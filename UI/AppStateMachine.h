#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <functional>

// ����Ӧ�ó�����ܵ�״̬
enum class AppState {
    INITIALIZING,     // Ӧ�ó�ʼ����
    DEVICE_ABSENT,    // �豸������
    DEVICE_ERROR,     // �豸����
    IDLE,             // �豸����������
    COMMANDS_MISSING, // �����ļ�δ����
    CONFIGURED,       // ������ɣ����Կ�ʼ����
    STARTING,         // ������������
    TRANSFERRING,     // ������
    STOPPING,         // ����ֹͣ����
    SHUTDOWN          // Ӧ�ùر���
};

// ������ܵ�״̬�¼�
enum class StateEvent {
    APP_INIT,           // Ӧ�ó����ʼ��
    DEVICE_CONNECTED,   // �豸����
    DEVICE_DISCONNECTED,// �豸�Ͽ�
    ERROR_OCCURRED,     // ��������
    COMMANDS_LOADED,    // �����ļ�����
    COMMANDS_UNLOADED,  // �����ļ�ж��
    START_REQUESTED,    // ����ʼ����
    START_SUCCEEDED,    // ��ʼ����ɹ�
    START_FAILED,       // ��ʼ����ʧ��
    STOP_REQUESTED,     // ����ֹͣ����
    STOP_SUCCEEDED,     // ֹͣ����ɹ�
    STOP_FAILED,        // ֹͣ����ʧ��
    APP_SHUTDOWN        // Ӧ�ó���ر�
};

// ״̬ת�������������״̬�Ϳ�ѡ�ĸ�����Ϣ
struct StateTransitionResult {
    AppState newState;
    QString message;
    bool isError;

    StateTransitionResult(AppState state, const QString& msg = QString(), bool error = false)
        : newState(state), message(msg), isError(error) {}
};

// Ӧ��״̬����
class AppStateMachine : public QObject {
    Q_OBJECT

public:
    static AppStateMachine& instance();

    // ��ȡ��ǰ״̬
    AppState currentState() const;

    // ״̬ת������
    bool processEvent(StateEvent event, const QString& reason = QString());

    // ��״̬ת��Ϊ�ַ�����������־�͵��ԣ�
    static QString stateToString(AppState state);
    static QString eventToString(StateEvent event);

signals:
    // ��״̬�ı�ʱ�����ź�
    void stateChanged(AppState newState, AppState oldState, const QString& reason);

    // �����ض�״̬���ź�
    void enteringState(AppState state, const QString& reason);
    void leavingState(AppState state, const QString& reason);

    // �����ź�
    void errorOccurred(const QString& error);

private:
    AppStateMachine();
    ~AppStateMachine() = default;
    AppStateMachine(const AppStateMachine&) = delete;
    AppStateMachine& operator=(const AppStateMachine&) = delete;

    // ״̬ת������
    StateTransitionResult handleEvent(AppState currentState, StateEvent event, const QString& reason);

    // ִ��״̬ת��
    void executeStateChange(AppState newState, const QString& reason, bool isError);

    // ��ǰ״̬
    std::atomic<AppState> m_currentState;

    // ״̬ת����
    std::mutex m_stateMutex;
};