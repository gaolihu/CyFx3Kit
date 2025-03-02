#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <functional>

// 定义应用程序可能的状态
enum class AppState {
    INITIALIZING,     // 应用初始化中
    DEVICE_ABSENT,    // 设备不存在
    DEVICE_ERROR,     // 设备错误
    IDLE,             // 设备就绪，空闲
    COMMANDS_MISSING, // 命令文件未加载
    CONFIGURED,       // 配置完成，可以开始传输
    STARTING,         // 正在启动传输
    TRANSFERRING,     // 传输中
    STOPPING,         // 正在停止传输
    SHUTDOWN          // 应用关闭中
};

// 定义可能的状态事件
enum class StateEvent {
    APP_INIT,           // 应用程序初始化
    DEVICE_CONNECTED,   // 设备连接
    DEVICE_DISCONNECTED,// 设备断开
    ERROR_OCCURRED,     // 发生错误
    COMMANDS_LOADED,    // 命令文件加载
    COMMANDS_UNLOADED,  // 命令文件卸载
    START_REQUESTED,    // 请求开始传输
    START_SUCCEEDED,    // 开始传输成功
    START_FAILED,       // 开始传输失败
    STOP_REQUESTED,     // 请求停止传输
    STOP_SUCCEEDED,     // 停止传输成功
    STOP_FAILED,        // 停止传输失败
    APP_SHUTDOWN        // 应用程序关闭
};

// 状态转换结果，包含新状态和可选的附加信息
struct StateTransitionResult {
    AppState newState;
    QString message;
    bool isError;

    StateTransitionResult(AppState state, const QString& msg = QString(), bool error = false)
        : newState(state), message(msg), isError(error) {}
};

// 应用状态机类
class AppStateMachine : public QObject {
    Q_OBJECT

public:
    static AppStateMachine& instance();

    // 获取当前状态
    AppState currentState() const;

    // 状态转换处理
    bool processEvent(StateEvent event, const QString& reason = QString());

    // 将状态转换为字符串（用于日志和调试）
    static QString stateToString(AppState state);
    static QString eventToString(StateEvent event);

signals:
    // 当状态改变时发出信号
    void stateChanged(AppState newState, AppState oldState, const QString& reason);

    // 用于特定状态的信号
    void enteringState(AppState state, const QString& reason);
    void leavingState(AppState state, const QString& reason);

    // 错误信号
    void errorOccurred(const QString& error);

private:
    AppStateMachine();
    ~AppStateMachine() = default;
    AppStateMachine(const AppStateMachine&) = delete;
    AppStateMachine& operator=(const AppStateMachine&) = delete;

    // 状态转换函数
    StateTransitionResult handleEvent(AppState currentState, StateEvent event, const QString& reason);

    // 执行状态转换
    void executeStateChange(AppState newState, const QString& reason, bool isError);

    // 当前状态
    std::atomic<AppState> m_currentState;

    // 状态转换锁
    std::mutex m_stateMutex;
};