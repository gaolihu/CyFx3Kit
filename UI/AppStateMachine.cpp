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

    LOG_INFO(QString::fromLocal8Bit("����״̬�¼�: %1, ״̬: %2, ԭ��: %3")
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

    LOG_INFO(QString::fromLocal8Bit("ִ��״̬ת��: %1 -> %2, ԭ��: %3")
        .arg(oldStateStr)
        .arg(newStateStr)
        .arg(reason));

    // ʹ��QMetaObject::invokeMethodȷ���ź�����ȷ���߳��������з���
    QMetaObject::invokeMethod(this, [this, oldState, newState, reason]() {
        LOG_WARN(QString::fromLocal8Bit("����leavingState�źţ���״̬: %1, ��״̬: %2").arg(stateToString(newState)).arg(stateToString(oldState)));
        emit leavingState(oldState, reason);

        LOG_WARN(QString::fromLocal8Bit("����stateChanged�źţ���״̬: %1, ��״̬: %2").arg(stateToString(newState)).arg(stateToString(oldState)));
        emit stateChanged(newState, oldState, reason);

        LOG_WARN(QString::fromLocal8Bit("����enteringState�źţ���״̬: %1, ��״̬: %2").arg(stateToString(newState)).arg(stateToString(oldState)));
        emit enteringState(newState, reason);
        }, Qt::QueuedConnection);

    if (isError) {
        LOG_ERROR(QString::fromLocal8Bit("����errorOccurred�źţ�ԭ��: %1").arg(reason));
        emit errorOccurred(reason);
    }
}

StateTransitionResult AppStateMachine::handleEvent(AppState currentState, StateEvent event, const QString& reason) {
    // ����ر�Ӧ���¼� - ���κ�״̬�¶����Թر�
    if (event == StateEvent::APP_SHUTDOWN) {
        return StateTransitionResult(AppState::SHUTDOWN, QString::fromLocal8Bit("Ӧ�ó������ڹر�"));
    }

    // ���ڵ�ǰ״̬���յ����¼���״̬ת���߼�
    switch (currentState) {
    case AppState::INITIALIZING:
        switch (event) {
        case StateEvent::DEVICE_CONNECTED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, QString::fromLocal8Bit("�豸�����ӣ��ȴ������ļ�"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::DEVICE_ABSENT:
        switch (event) {
        case StateEvent::DEVICE_CONNECTED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, QString::fromLocal8Bit("�豸�����ӣ��ȴ������ļ�"));
        default:
            break;
        }
        break;

    case AppState::DEVICE_ERROR:
        switch (event) {
        case StateEvent::DEVICE_CONNECTED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, QString::fromLocal8Bit("�豸���������ӣ��ȴ������ļ�"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        default:
            break;
        }
        break;

    case AppState::COMMANDS_MISSING:
        switch (event) {
        case StateEvent::COMMANDS_LOADED:
            return StateTransitionResult(AppState::CONFIGURED, QString::fromLocal8Bit("�����ļ��Ѽ��أ�ϵͳ������"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::CONFIGURED:
        switch (event) {
        case StateEvent::START_REQUESTED:
            return StateTransitionResult(AppState::STARTING, QString::fromLocal8Bit("�����������ݴ���"));
        case StateEvent::COMMANDS_UNLOADED:
            return StateTransitionResult(AppState::COMMANDS_MISSING, QString::fromLocal8Bit("�����ļ���ж��"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::STARTING:
        switch (event) {
        case StateEvent::START_SUCCEEDED:
            return StateTransitionResult(AppState::TRANSFERRING, QString::fromLocal8Bit("���ݴ����ѿ�ʼ"));
        case StateEvent::START_FAILED:
            return StateTransitionResult(AppState::DEVICE_ERROR, QString::fromLocal8Bit("�������ݴ���ʧ��: ") + reason, true);
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::TRANSFERRING:
        switch (event) {
        case StateEvent::STOP_REQUESTED:
            return StateTransitionResult(AppState::STOPPING, QString::fromLocal8Bit("����ֹͣ���ݴ���"));
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::STOPPING:
        switch (event) {
        case StateEvent::STOP_SUCCEEDED:
            return StateTransitionResult(AppState::CONFIGURED, QString::fromLocal8Bit("���ݴ�����ֹͣ"));
        case StateEvent::STOP_FAILED:
            return StateTransitionResult(AppState::DEVICE_ERROR, QString::fromLocal8Bit("ֹͣ���ݴ���ʧ��: ") + reason, true);
        case StateEvent::DEVICE_DISCONNECTED:
            return StateTransitionResult(AppState::DEVICE_ABSENT, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        case StateEvent::ERROR_OCCURRED:
            return StateTransitionResult(AppState::DEVICE_ERROR, reason, true);
        default:
            break;
        }
        break;

    case AppState::SHUTDOWN:
        // �Ѿ����ڹر�״̬�����������¼�
        break;

    default:
        LOG_ERROR(QString::fromLocal8Bit("δ֪״̬: %1").arg(static_cast<int>(currentState)));
        break;
    }

    // ���û����Ч��ת�������ص�ǰ״̬
    return StateTransitionResult(currentState);
}

QString AppStateMachine::stateToString(AppState state) {
    switch (state) {
    case AppState::INITIALIZING: return QString::fromLocal8Bit("��ʼ����");
    case AppState::DEVICE_ABSENT: return QString::fromLocal8Bit("�豸δ����");
    case AppState::DEVICE_ERROR: return QString::fromLocal8Bit("�豸����");
    case AppState::IDLE: return QString::fromLocal8Bit("����");
    case AppState::COMMANDS_MISSING: return QString::fromLocal8Bit("����δ����");
    case AppState::CONFIGURED: return QString::fromLocal8Bit("������");
    case AppState::STARTING: return QString::fromLocal8Bit("������");
    case AppState::TRANSFERRING: return QString::fromLocal8Bit("������");
    case AppState::STOPPING: return QString::fromLocal8Bit("ֹͣ��");
    case AppState::SHUTDOWN: return QString::fromLocal8Bit("�ر���");
    default: return QString::fromLocal8Bit("δ֪״̬");
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
    default: return QString::fromLocal8Bit("δ֪�¼�");
    }
}