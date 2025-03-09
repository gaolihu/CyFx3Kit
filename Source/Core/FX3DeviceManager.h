// Source/Core/FX3DeviceManager.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <queue>
#include <condition_variable>

#include "USBDevice.h"
#include "DataAcquisition.h"
#include "CommandManager.h"
#include "AppStateMachine.h"
#include "DeviceTransferStats.h"

/**
 * @brief FX3设备管理器类
 *
 * 负责与FX3设备的所有交互，包括设备连接/断开检测、数据传输控制等
 * 实现了资源安全管理和线程安全操作
 */
class FX3DeviceManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit FX3DeviceManager(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     * 实现安全的资源释放
     */
    ~FX3DeviceManager();

    /**
     * @brief 初始化设备和管理器
     * @param windowHandle 窗口句柄，用于USB设备通知
     * @return 初始化是否成功
     */
    bool initializeDeviceAndManager(HWND windowHandle);

    /**
     * @brief 检查并打开设备
     * 检测设备连接状态并尝试打开设备
     * @return 检查和打开是否成功
     */
    bool checkAndOpenDevice();

    /**
     * @brief 重置设备
     * 重置USB设备到初始状态
     * @return 重置是否成功
     */
    bool resetDevice();

    /**
     * @brief 加载命令文件
     * @param directoryPath 命令文件目录
     * @return 加载是否成功
     */
    bool loadCommandFiles(const QString& directoryPath);

    /**
     * @brief 启动数据传输
     * @param width 图像宽度
     * @param height 图像高度
     * @param captureType 捕获类型
     * @return 启动是否成功
     */
    bool startTransfer(uint16_t width, uint16_t height, uint8_t captureType);

    /**
     * @brief 停止数据传输
     * @return 停止操作是否成功发起
     */
    bool stopTransfer();

    /**
     * @brief 准备关闭
     * 在应用退出前调用，确保安全关闭所有资源
     */
    void prepareForShutdown();

    /**
     * @brief 设备是否已连接
     * @return 连接状态
     */
    bool isDeviceConnected() const;

    /**
     * @brief 是否正在传输数据
     * @return 传输状态
     */
    bool isTransferring() const;

    /**
     * @brief 获取设备信息
     * @return 设备信息描述
     */
    QString getDeviceInfo() const;

    /**
     * @brief 获取USB速度描述
     * @return USB速度描述文本
     */
    QString getUsbSpeedDescription() const;

    /**
     * @brief 是否是USB3连接
     * @return USB3连接状态
     */
    bool isUSB3() const;

    /**
     * @brief 获取当前传输统计
     * @return 传输统计数据
     */
    DeviceTransferStats getTransferStats() const;

public slots:
    /**
     * @brief 处理设备接入事件
     */
    void onDeviceArrival();

    /**
     * @brief 处理设备移除事件
     */
    void onDeviceRemoval();

signals:
    /**
     * @brief 设备连接状态变更信号
     * @param connected 是否已连接
     */
    void deviceConnectionChanged(bool connected);

    /**
     * @brief 传输状态变更信号
     * @param transferring 是否正在传输
     */
    void transferStateChanged(bool transferring);

    /**
     * @brief 传输统计更新信号
     * @param transferred 已传输字节数
     * @param speed 传输速率(MB/s)
     * @param errors 错误计数
     */
    void transferStatsUpdated(uint64_t transferred, double speed, uint32_t errors);

    /**
     * @brief USB速度更新信号
     * @param speedDesc 速度描述
     * @param isUSB3 是否是USB3连接
     */
    void usbSpeedUpdated(const QString& speedDesc, bool isUSB3);

    /**
     * @brief 设备错误信号
     * @param title 错误标题
     * @param message 错误详细信息
     */
    void deviceError(const QString& title, const QString& message);

    /**
     * @brief 数据包可用信号
     * @param packet 数据包
     */
    void dataPacketAvailable(const DataPacket& packet);

private slots:
    /**
     * @brief 处理USB状态变更
     * @param status 新状态
     */
    void handleUsbStatusChanged(const std::string& status);

    /**
     * @brief 处理传输进度更新
     * @param transferred 已传输字节数
     * @param length 当前包长度
     * @param success 成功计数
     * @param failed 失败计数
     */
    void handleTransferProgress(uint64_t transferred, int length, int success, int failed);

    /**
     * @brief 处理USB设备错误
     * @param error 错误信息
     */
    void handleDeviceError(const QString& error);

    /**
     * @brief 处理采集开始事件
     */
    void handleAcquisitionStarted();

    /**
     * @brief 处理采集停止事件
     */
    void handleAcquisitionStopped();

    /**
     * @brief 处理数据接收事件
     * @param packet 数据包
     */
    void handleDataReceived(const DataPacket& packet);

    /**
     * @brief 处理采集错误事件
     * @param error 错误信息
     */
    void handleAcquisitionError(const QString& error);

    /**
     * @brief 处理统计更新事件
     * @param receivedBytes 接收字节数
     * @param dataRate 数据速率
     * @param elapsedTimeSeconds 已用时间(秒)
     */
    void handleStatsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeSeconds);

    /**
     * @brief 处理采集状态变更事件
     * @param state 新状态
     */
    void handleAcquisitionStateChanged(const QString& state);

private:
    /**
     * @brief 初始化信号连接
     */
    void initConnections();

    /**
     * @brief 安全停止所有传输
     * 确保所有传输安全停止，并释放资源
     */
    void stopAllTransfers();

    /**
     * @brief 释放资源
     * 安全释放所有设备资源
     */
    void releaseResources();

    /**
     * @brief 防抖处理设备事件
     * @param action 事件处理函数
     */
    void debounceDeviceEvent(std::function<void()> action);

    /**
     * @brief 更新传输统计
     * @param transferred 已传输字节数
     * @param speed 传输速率
     * @param errors 错误计数
     */
    void updateTransferStats(uint64_t transferred, double speed, uint32_t errors);

    /**
     * @brief 处理关键错误
     * @param title 错误标题
     * @param message 错误信息
     * @param eventType 状态机事件类型
     * @param eventReason 事件原因
     */
    void handleCriticalError(const QString& title, const QString& message,
        StateEvent eventType = StateEvent::ERROR_OCCURRED,
        const QString& eventReason = QString());

private:
    // 设备和采集管理器
    std::shared_ptr<USBDevice> m_usbDevice;
    std::shared_ptr<DataAcquisitionManager> m_acquisitionManager;

    // 防抖设置
    QTimer m_debounceTimer;
    static const int DEBOUNCE_DELAY = 300; // ms
    QElapsedTimer m_lastDeviceEventTime;

    // 传输统计
    mutable std::mutex m_statsMutex;
    DeviceTransferStats m_transferStats;
    std::chrono::steady_clock::time_point m_transferStartTime;
    uint64_t m_lastTransferred;

    // 状态标志
    std::mutex m_shutdownMutex;
    std::atomic<bool> m_shuttingDown{ false };
    std::atomic<bool> m_isTransferring{ false };
    std::atomic<bool> m_stoppingInProgress{ false };
    std::atomic<bool> m_commandsLoaded{ false };
    std::atomic<bool> m_deviceInitialized{ false };
    std::atomic<bool> m_errorOccurred{ false };

    // 线程安全队列相关
    std::mutex m_eventQueueMutex;
    std::condition_variable m_eventQueueCV;
    bool m_eventProcessingActive{ true };
};