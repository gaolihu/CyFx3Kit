#pragma once

#include <QObject>
#include <memory>
#include <chrono>
#include <QTimer>
#include <QElapsedTimer>
#include "USBDevice.h"
#include "DataAcquisition.h"
#include "CommandManager.h"
#include "AppStateMachine.h"

// 设备管理类 - 处理与FX3设备的所有交互
class FX3DeviceManager : public QObject {
    Q_OBJECT

public:
    explicit FX3DeviceManager(QObject* parent = nullptr);
    ~FX3DeviceManager();

    // 初始化设备和采集管理器
    bool initializeDeviceAndManager(HWND windowHandle);

    // 设备操作
    bool checkAndOpenDevice();
    bool resetDevice();

    // 命令操作
    bool loadCommandFiles(const QString& directoryPath);

    // 传输操作
    bool startTransfer(uint16_t width, uint16_t height, uint8_t capType);
    bool stopTransfer();
    void stopAllTransfers();
    void releaseResources();

    // 状态查询
    bool isDeviceConnected() const;
    bool isTransferring() const;
    QString getDeviceInfo() const;
    QString getUsbSpeedDescription() const;
    bool isUSB3() const;

public slots:
    // 设备事件处理
    void onDeviceArrival();
    void onDeviceRemoval();

    // 设备信号处理
    void onUsbStatusChanged(const std::string& status);
    void onTransferProgress(uint64_t transferred, int length, int success, int failed);
    void onDeviceError(const QString& error);

    // 采集管理器信号处理
    void onAcquisitionStarted();
    void onAcquisitionStopped();
    void onDataReceived(const DataPacket& packet);
    void onAcquisitionError(const QString& error);
    void onStatsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeSeconds);
    void onAcquisitionStateChanged(const QString& state);

    // 信号传递 - 用于UI更新
signals:
    void transferStatsUpdated(uint64_t transferred, double speed, uint64_t elapsedTimeSeconds);
    void usbSpeedUpdated(const QString& speedDesc, bool isUSB3);
    void deviceError(const QString& title, const QString& error);
    void dataProcessed(const DataPacket& packet);

private:
    // 初始化信号连接
    void initConnections();

    // 防抖处理
    void debounceDeviceEvent(std::function<void()> action);

    // 设备和采集管理器指针
    std::shared_ptr<USBDevice> m_usbDevice;
    std::shared_ptr<DataAcquisitionManager> m_acquisitionManager;

    // 防抖相关
    QTimer m_debounceTimer;
    static const int DEBOUNCE_DELAY = 500; // ms
    QElapsedTimer m_lastDeviceEventTime;

    // 传输统计
    std::chrono::steady_clock::time_point m_transferStartTime;
    uint64_t m_lastTransferred = 0;

    // 锁
    std::mutex m_shutdownMutex;

    // 避免在关闭期间处理事件
    std::atomic<bool> m_shuttingDown{ false };
    std::atomic<bool> m_isTransferring{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_errorOccurred{false};
    std::atomic<bool> m_stoppingInProgress{false};
    std::atomic<bool> m_commandsLoaded{false};
};