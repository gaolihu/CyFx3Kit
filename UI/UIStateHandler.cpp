#include "UIStateHandler.h"
#include "Logger.h"
#include <QMessageBox>

UIStateHandler::UIStateHandler(Ui::FX3ToolMainWinClass& ui, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_isClosing(false)
    , m_lastTransferred(0)
    , m_lastSpeed(0.0)
{
    LOG_INFO(QString::fromLocal8Bit("UIStateHandler构造函数 - 初始化"));

    // 主动获取当前状态并更新UI
    AppState currentState = AppStateMachine::instance().currentState();
    LOG_INFO(QString::fromLocal8Bit("UIStateHandler构造函数 - 初始化UI状态为: %1")
        .arg(AppStateMachine::stateToString(currentState)));

    // 确保UI初始状态正确
    updateButtonStates(currentState);
    updateStatusTexts(currentState);
}

void UIStateHandler::onStateChanged(AppState newState, AppState oldState, const QString& reason) {
    // 检查是否可以更新UI
    if (!canUpdateUI()) {
        LOG_INFO(QString::fromLocal8Bit("UI处理器准备关闭或应用正在退出，忽略状态更新"));
        return;
    }

    LOG_INFO(QString::fromLocal8Bit("UI状态处理器收到状态变化: %1 -> %2, 原因: %3")
        .arg(AppStateMachine::stateToString(oldState))
        .arg(AppStateMachine::stateToString(newState))
        .arg(reason));

    // 使用invokeMethod确保在主线程中更新UI
    QMetaObject::invokeMethod(this, [this, newState, oldState, reason]() {
        if (!canUpdateUI()) return;

        // 更新按钮状态
        updateButtonStates(newState);

        // 更新状态文本
        updateStatusTexts(newState, reason);
        }, Qt::QueuedConnection);
}

void UIStateHandler::updateButtonStates(AppState state) {
    // 检查是否可以更新UI
    if (!canUpdateUI()) return;

    // 根据状态确定按钮启用/禁用
    bool startEnabled = false;
    bool stopEnabled = false;
    bool resetEnabled = false;
    bool cmdDirEnabled = false;
    bool imageParamsEnabled = false;

    // 根据状态设置按钮状态
    switch (state) {
    case AppState::INITIALIZING:
    case AppState::STARTING:
    case AppState::STOPPING:
    case AppState::SHUTDOWN:
        // 所有按钮禁用
        break;

    case AppState::DEVICE_ABSENT:
        // 只有命令目录按钮可用
        cmdDirEnabled = true;
        break;

    case AppState::DEVICE_ERROR:
        // 可以重置设备和选择命令目录
        resetEnabled = true;
        cmdDirEnabled = true;
        break;

    case AppState::COMMANDS_MISSING:
        // 可以重置设备和选择命令目录
        resetEnabled = true;
        cmdDirEnabled = true;
        break;

    case AppState::CONFIGURED:
        // 可以开始传输、重置设备和选择命令目录
        startEnabled = true;
        resetEnabled = true;
        cmdDirEnabled = true;
        imageParamsEnabled = true;
        break;

    case AppState::TRANSFERRING:
        // 只能停止传输
        stopEnabled = true;
        break;

    case AppState::IDLE:
        // 可以重置设备和选择命令目录
        resetEnabled = true;
        cmdDirEnabled = true;
        imageParamsEnabled = true;
        break;

    default:
        LOG_WARN(QString::fromLocal8Bit("updateButtonStates - 未处理的状态: %1")
            .arg(AppStateMachine::stateToString(state)));
        break;
    }

    // 线程安全地更新UI
    QMetaObject::invokeMethod(QApplication::instance(), [=, this]() {
        if (!canUpdateUI()) return;

        // 直接更新按钮状态
        if (m_ui.startButton) m_ui.startButton->setEnabled(startEnabled);
        if (m_ui.stopButton) m_ui.stopButton->setEnabled(stopEnabled);
        if (m_ui.resetButton) m_ui.resetButton->setEnabled(resetEnabled);
        if (m_ui.cmdDirButton) m_ui.cmdDirButton->setEnabled(cmdDirEnabled);

        // 更新图像参数控件状态
        if (m_ui.imageWIdth) m_ui.imageWIdth->setReadOnly(!imageParamsEnabled);
        if (m_ui.imageHeight) m_ui.imageHeight->setReadOnly(!imageParamsEnabled);
        if (m_ui.imageType) m_ui.imageType->setEnabled(imageParamsEnabled);

        LOG_DEBUG(QString::fromLocal8Bit("按钮状态已更新 - 开始: %1, 停止: %2, 重置: %3, 命令目录: %4")
            .arg(startEnabled ? QString::fromLocal8Bit("启用") : QString::fromLocal8Bit("禁用"))
            .arg(stopEnabled ? QString::fromLocal8Bit("启用") : QString::fromLocal8Bit("禁用"))
            .arg(resetEnabled ? QString::fromLocal8Bit("启用") : QString::fromLocal8Bit("禁用"))
            .arg(cmdDirEnabled ? QString::fromLocal8Bit("启用") : QString::fromLocal8Bit("禁用")));
        }, Qt::QueuedConnection);
}

void UIStateHandler::updateStatusTexts(AppState state, const QString& additionalInfo) {
    QString statusText;
    QString transferStatusText;

    if (!canUpdateUI()) return;

    // 根据状态设置状态文本
    switch (state) {
    case AppState::INITIALIZING:
        statusText = QString::fromLocal8Bit("初始化中");
        transferStatusText = QString::fromLocal8Bit("初始化中");
        break;

    case AppState::DEVICE_ABSENT:
        statusText = QString::fromLocal8Bit("未连接设备");
        transferStatusText = QString::fromLocal8Bit("未连接");
        // 清空USB速度信息
        m_ui.usbSpeedLabel->setText(QString::fromLocal8Bit("设备: 未连接"));
        m_ui.usbSpeedLabel->setStyleSheet("");
        break;

    case AppState::DEVICE_ERROR:
        statusText = QString::fromLocal8Bit("设备错误");
        transferStatusText = QString::fromLocal8Bit("错误");
        m_ui.usbSpeedLabel->setStyleSheet("color: red;");
        break;

    case AppState::COMMANDS_MISSING:
        statusText = QString::fromLocal8Bit("命令文件未加载");
        transferStatusText = QString::fromLocal8Bit("空闲");
        // 更新命令状态标签
        m_ui.cmdStatusLabel->setText(QString::fromLocal8Bit("命令文件未加载"));
        m_ui.cmdStatusLabel->setStyleSheet("color: red;");
        break;

    case AppState::CONFIGURED:
        statusText = QString::fromLocal8Bit("就绪");
        transferStatusText = QString::fromLocal8Bit("已配置");
        // 更新命令状态标签
        m_ui.cmdStatusLabel->setText(QString::fromLocal8Bit("命令文件加载成功"));
        m_ui.cmdStatusLabel->setStyleSheet("color: green;");
        break;

    case AppState::STARTING:
        statusText = QString::fromLocal8Bit("启动中");
        transferStatusText = QString::fromLocal8Bit("启动中");
        break;

    case AppState::TRANSFERRING:
        statusText = QString::fromLocal8Bit("传输中");
        transferStatusText = QString::fromLocal8Bit("传输中");
        break;

    case AppState::STOPPING:
        statusText = QString::fromLocal8Bit("停止中");
        transferStatusText = QString::fromLocal8Bit("停止中");
        break;

    case AppState::IDLE:
        statusText = QString::fromLocal8Bit("就绪");
        transferStatusText = QString::fromLocal8Bit("空闲");
        break;

    case AppState::SHUTDOWN:
        statusText = QString::fromLocal8Bit("关闭中");
        transferStatusText = QString::fromLocal8Bit("关闭中");
        break;

    default:
        statusText = QString::fromLocal8Bit("未知状态");
        transferStatusText = QString::fromLocal8Bit("未知");
        break;
    }

    // 更新状态标签
    m_ui.usbStatusLabel->setText(QString::fromLocal8Bit("USB状态: %1").arg(statusText));
    m_ui.transferStatusLabel->setText(QString::fromLocal8Bit("传输状态: %1").arg(transferStatusText));
}

void UIStateHandler::updateTransferStats(uint64_t transferred, double speed, uint64_t elapsedTimeSeconds) {
    m_lastTransferred = transferred;
    m_lastSpeed = speed;

    if (!canUpdateUI()) return;

    // 更新速度显示
    QString speedText = QString::fromLocal8Bit("速度: 0 MB/s");
    if (speed > 0) {
        if (speed >= 1024) {
            speedText = QString::fromLocal8Bit("速度: %1 GB/s")
                .arg(speed / 1024.0, 0, 'f', 2);
        }
        else {
            speedText = QString::fromLocal8Bit("速度: %1 MB/s")
                .arg(speed, 0, 'f', 2);
        }
    }
    m_ui.speedLabel->setText(speedText);

    // 更新总传输量显示
    m_ui.totalBytesLabel->setText(QString::fromLocal8Bit("总计: %1").arg(formatDataSize(transferred)));

    // Update elapsed time display
    QString timeText;
    if (elapsedTimeSeconds > 0) {
        timeText = QString::fromLocal8Bit("采集时长: %1").arg(formatElapsedTime(elapsedTimeSeconds));
    }
    else {
        timeText = QString::fromLocal8Bit("采集时长: 00:00:00");
    }
    m_ui.totalTimeLabel->setText(timeText);

    LOG_DEBUG(speedText);
}

void UIStateHandler::updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3) {
    m_ui.usbSpeedLabel->setText(QString::fromLocal8Bit("设备: %1").arg(speedDesc));

    if (!canUpdateUI()) return;

    // 根据USB速度设置不同样式
    if (isUSB3) {
        m_ui.usbSpeedLabel->setStyleSheet("color: blue;");
    }
    else if (!speedDesc.contains(QString::fromLocal8Bit("未连接"))) {
        m_ui.usbSpeedLabel->setStyleSheet("color: green;");
    }
    else {
        m_ui.usbSpeedLabel->setStyleSheet("");
    }

    LOG_INFO(QString::fromLocal8Bit("接收信号，USB速度更新: %1").arg(speedDesc));
}

void UIStateHandler::showErrorMessage(const QString& title, const QString& message) {
    LOG_ERROR(QString::fromLocal8Bit("错误对话框: %1 - %2").arg(title).arg(message));
    if (!canUpdateUI()) return;

    QMessageBox::critical(nullptr, title, message);
}

QString UIStateHandler::formatDataSize(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        return QString("%1 GB").arg(gb, 0, 'f', 2);
    }
    else if (bytes >= 1024ULL * 1024ULL) {
        double mb = bytes / (1024.0 * 1024.0);
        return QString("%1 MB").arg(mb, 0, 'f', 2);
    }
    else if (bytes >= 1024ULL) {
        double kb = bytes / 1024.0;
        return QString("%1 KB").arg(kb, 0, 'f', 2);
    }
    else {
        return QString("%1 B").arg(bytes);
    }
}