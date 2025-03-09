// Source/Core/AppStateMachine.cpp

#include "AppStateMachine.h"
#include "Logger.h"
#include <QDebug>

AppStateMachine& AppStateMachine::instance() {
    static AppStateMachine instance;
    return instance;
}

AppStateMachine::AppStateMachine() : QObject(nullptr), m_currentState(AppState::INITIALIZING) {
}

AppState AppStateMachine::currentState() const {
    return m_currentState.load();
}

bool AppStateMachine::processEvent(StateEvent event, const QString& reason) {
    std::lock_guard<std::mutex> lock(m_stateMutex);

    AppState currentState = m_currentState.load();
    QString eventStr = eventToString(event);
    QString stateStr = stateToString(currentState);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理状态事件: %1, 状态: %2, 原因: %3")
        .arg(eventStr)
        .arg(stateStr)
        .arg(reason));

    StateTransitionResult result = handleEvent(currentState, event, reason);

    if (result.newState != currentState) {
        executeStateChange(result.newState, result.message.isEmpty() ? reason : result.message, result.isError);
        return true;
    }

    return false;
}

void AppStateMachine::executeStateChange(AppState newState, const QString& reason, bool isError) {
    AppState oldState = m_currentState.exchange(newState);

    QString oldStateStr = stateToString(oldState);
    QString newStateStr = stateToString(newState);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("执行状态转换: %1 -> %2, 原因: %3")
        .arg(oldStateStr)
        .arg(newStateStr)
        .arg(reason));

    // 使用QMetaObject::invokeMethod确保信号在正确的线程上下文中发送
    QMetaObject::invokeMethod(this, [this, oldState, newState, reason]() {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("发出leavingState信号，新状态: %1, 旧状态: %2").arg(stateToString(newState)).arg(stateToString(oldState)));
        emit leavingState(oldState, reason);

        LOG_WARN(LocalQTCompat::fromLocal8Bit("发出stateChanged信号，新状态: %1, 旧状态: %2").arg(stateToString(newState)).arg(stateToString(oldState)));
        emit stateChanged(newState, oldState, reason);

        LOG_WARN(LocalQTCompat::fromLocal8Bit("发出enteringState信号，新状态: %1, 旧状态: %2").arg(stateToString(newState)).arg(stateToString(oldState)));
        emit enteringState(newState, reason);
        }, Qt::QueuedConnection);

    if (isError) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("发出errorOccurred信号，原因: %1").arg(reason));
        emit errorOccurred(reason);
    }
}

StateTransitionResult AppStateMachine::handleEvent(AppState currentState, StateEvent event, const QString& reason) {
    // 处理关闭应用事件 - 在任何状态下都可以关闭
    if (event == StateEvent::APP_SHUTDOWN) {
        return StateTransitionResult(AppState::SHUTDOWN, LocalQTCompat::fromLocal8Bit("应用程序正在关闭"));
    }

    // 基于当前状态和收到的事件的状态转换逻辑
    switch (currentState) {
    case AppState::INITIALIZING:
        switch (event) {
        case StateEvent::DEVICE_CONNECTED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, LocalQTCompat::fromLocal8Bit("设备已连接，等待命令文件"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::DEVICE_ABSENT:
        switch (event) {
        case StateEvent::DEVICE_CONNECTED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, LocalQTCompat::fromLocal8Bit("设备已连接，等待命令文件"));
        default:
            break;
        }
        break;

    case AppState::DEVICE_ERROR:
        switch (event) {
        case StateEvent::DEVICE_CONNECTED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, LocalQTCompat::fromLocal8Bit("设备已重新连接，等待命令文件"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        default:
            break;
        }
        break;

    case AppState::COMMANDS_MISSING:
        switch (event) {
        case StateEvent::COMMANDS_LOADED:
            return StateTransitionResult(AppState::CONFIGURED, LocalQTCompat::fromLocal8Bit("命令文件已加载，系统已配置"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::CONFIGURED:
        switch (event) {
        case StateEvent::START_REQUESTED:
            return StateTransitionResult(AppState::STARTING, LocalQTCompat::fromLocal8Bit("正在启动数据传输"));
        case StateEvent::COMMANDS_UNLOADED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, LocalQTCompat::fromLocal8Bit("命令文件已卸载"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::STARTING:
        switch (event) {
        case StateEvent::START_SUCCEEDED:
            return StateTransitionResult(AppState::TRANSFERRING, LocalQTCompat::fromLocal8Bit("数据传输已开始"));
        case StateEvent::START_FAILED:
            return StateTransitionResult(AppState::DEVICE_ERROR, LocalQTCompat::fromLocal8Bit("启动数据传输失败: ") + reason, true);
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::TRANSFERRING:
        switch (event) {
        case StateEvent::STOP_REQUESTED:
            return StateTransitionResult(AppState::STOPPING, LocalQTCompat::fromLocal8Bit("正在停止数据传输"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::STOPPING:
        switch (event) {
        case StateEvent::STOP_SUCCEEDED:
            return StateTransitionResult(AppState::CONFIGURED, LocalQTCompat::fromLocal8Bit("数据传输已停止"));
        case StateEvent::STOP_FAILED:
            return StateTransitionResult(AppState::DEVICE_ERROR, LocalQTCompat::fromLocal8Bit("停止数据传输失败: ") + reason, true);
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::SHUTDOWN:
        // 已经处于关闭状态，忽略所有事件
        break;

    default:
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("未知状态: %1").arg(static_cast<int>(currentState)));
        break;
    }

    // 如果没有有效的转换，返回当前状态
    return StateTransitionResult(currentState);
}

QString AppStateMachine::stateToString(AppState state) {
    switch (state) {
    case AppState::INITIALIZING: return LocalQTCompat::fromLocal8Bit("初始化中");
    case AppState::DEVICE_ABSENT: return LocalQTCompat::fromLocal8Bit("设备未连接");
    case AppState::DEVICE_ERROR: return LocalQTCompat::fromLocal8Bit("设备错误");
    case AppState::IDLE: return LocalQTCompat::fromLocal8Bit("空闲");
    case AppState::COMMANDS_MISSING: return LocalQTCompat::fromLocal8Bit("命令未加载");
    case AppState::CONFIGURED: return LocalQTCompat::fromLocal8Bit("已配置");
    case AppState::STARTING: return LocalQTCompat::fromLocal8Bit("启动中");
    case AppState::TRANSFERRING: return LocalQTCompat::fromLocal8Bit("传输中");
    case AppState::STOPPING: return LocalQTCompat::fromLocal8Bit("停止中");
    case AppState::SHUTDOWN: return LocalQTCompat::fromLocal8Bit("关闭中");
    default: return LocalQTCompat::fromLocal8Bit("未知状态");
    }
}

QString AppStateMachine::eventToString(StateEvent event) {
    switch (event) {
    case StateEvent::APP_INIT: return "APP_INIT";
    case StateEvent::DEVICE_CONNECTED: return "DEVICE_CONNECTED";
    case StateEvent::DEVICE_DISCONNECTED: return "DEVICE_DISCONNECTED";
    case StateEvent::ERROR_OCCURRED: return "ERROR_OCCURRED";
    case StateEvent::COMMANDS_LOADED: return "COMMANDS_LOADED";
    case StateEvent::COMMANDS_UNLOADED: return "COMMANDS_UNLOADED";
    case StateEvent::START_REQUESTED: return "START_REQUESTED";
    case StateEvent::START_SUCCEEDED: return "START_SUCCEEDED";
    case StateEvent::START_FAILED: return "START_FAILED";
    case StateEvent::STOP_REQUESTED: return "STOP_REQUESTED";
    case StateEvent::STOP_SUCCEEDED: return "STOP_SUCCEEDED";
    case StateEvent::STOP_FAILED: return "STOP_FAILED";
    case StateEvent::APP_SHUTDOWN: return "APP_SHUTDOWN";
    default: return LocalQTCompat::fromLocal8Bit("未知事件");
    }
}