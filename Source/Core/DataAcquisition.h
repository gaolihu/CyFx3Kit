// Source/Core/DataAcquisitionManager.h
#pragma once

#include <QObject>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <optional>
#include <condition_variable>
#include <queue>
#include "USBDevice.h"
#include "DataPacket.h"

class USBDevice;

// 数据处理器接口
class IDataProcessor {
public:
    virtual ~IDataProcessor() = default;
    virtual void processData(const DataPacket& packet) = 0;
};

// 数据采集管理器
class DataAcquisitionManager : public QObject,
    public std::enable_shared_from_this<DataAcquisitionManager> {
    Q_OBJECT

public:
    // 静态工厂方法，用于创建DataAcquisitionManager实例
    static std::shared_ptr<DataAcquisitionManager> create(std::shared_ptr<USBDevice> device);

    // 析构函数
    ~DataAcquisitionManager();

    // 禁用拷贝操作
    DataAcquisitionManager(const DataAcquisitionManager&) = delete;
    DataAcquisitionManager& operator=(const DataAcquisitionManager&) = delete;

    // 配置和控制接口
    void setDataProcessor(std::shared_ptr<IDataProcessor> processor) {
        m_processor = processor;
    }
    bool startAcquisition(uint16_t width, uint16_t height, uint8_t capType);
    void stopAcquisition();
    void prepareForShutdown() {
        m_isShuttingDown = true;
        stopAcquisition(); // 立即停止所有活动
        // 断开所有信号连接以防止新的异步调用
        disconnect(this, nullptr, nullptr, nullptr);
    }

    uint64_t getElapsedTimeSeconds() const {
        if (!m_running) return 0;

        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(
            now - m_startTime).count();
    }

    // 状态查询
    bool isRunning() const { return m_running; }
    bool isShuttingDown() const { return m_isShuttingDown; }
    bool isErrorState() const { return m_errorOccurred; }
    uint64_t getTotalBytes() const { return m_totalBytes; }
    double getDataRate() const { return m_dataRate; }

signals:
    void acquisitionStarted();
    void acquisitionStopped();
    void dataReceived(const DataPacket& packet);
    void errorOccurred(const QString& error);
    void statsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeSeconds);
    void acquisitionStateChanged(const QString& state);

private:
    // 私有构造函数 - 强制使用create静态方法创建实例
    explicit DataAcquisitionManager(std::shared_ptr<USBDevice> device);

    std::atomic<bool> m_isShuttingDown{ false };

    template<typename Func>
    void safeEmit(Func emitFunc) {
        if (!m_isShuttingDown) {
            emitFunc();
        }
    }

    // 内部类 - 循环缓冲区管理器
    class CircularBuffer {
    public:
        enum class WarningLevel {
            C_NORMAL,         // 正常
            C_WARNING,        // 警告（使用率超过75%）
            C_CRITICAL        // 严重（使用率超过90%）
        };

        CircularBuffer(size_t bufferCount, size_t bufferSize);
        ~CircularBuffer() = default;

        WarningLevel checkBufferStatus();
        std::pair<uint8_t*, size_t> getWriteBuffer();
        void commitBuffer(size_t bytesWritten);
        std::optional<DataPacket> getReadBuffer();
        void reset();

    private:
        std::vector<std::vector<uint8_t>> m_buffers;
        std::queue<DataPacket> m_readyBuffers;
        size_t m_currentWriteBuffer;
        std::mutex m_mutex;
        const size_t m_warningThreshold;
        const size_t m_criticalThreshold;
        WarningLevel m_lastWarningLevel{ WarningLevel::C_NORMAL };
    };

private:
    // 停止原因枚举
    enum class StopReason {
        USER_REQUEST,
        READ_ERROR,
        DEVICE_ERROR,
        BUFFER_OVERFLOW
    };

    // 采集状态枚举
    enum class AcquisitionState {
        AC_IDLE,
        AC_CONFIGURING,
        AC_RUNNING,
        AC_STOPPING,
        AC_ERROR
    };

    // 线程函数
    void acquisitionThread();
    void processingThread();

    // 内部控制函数
    void signalStop(StopReason reason);
    void updateStats();
    bool validateAcquisitionParams() const;
    void updateAcquisitionState(AcquisitionState newState);

    // 设备和处理器
    std::weak_ptr<USBDevice> m_deviceWeak;
    std::shared_ptr<IDataProcessor> m_processor;
    std::unique_ptr<CircularBuffer> m_buffer;

    // 线程控制
    std::atomic<bool> m_running{ false };
    std::atomic<bool> m_errorOccurred{ false };
    std::thread m_acquisitionThread;
    std::thread m_processingThread;

    // 同步和控制
    std::mutex m_mutex;
    std::mutex m_stopMutex;
    std::condition_variable m_dataReady;
    std::atomic<AcquisitionState> m_acquisitionState{ AcquisitionState::AC_IDLE };

    // 配置参数
    struct AcquisitionParams {
        uint16_t width{ 0 };
        uint16_t height{ 0 };
        uint8_t format{ 0 };
        uint32_t frameRate{ 0 };
        bool continuous{ true };
    } m_params;

    // 统计信息
    struct AcquisitionStats {
        uint64_t totalFrames{ 0 };
        uint64_t droppedFrames{ 0 };
        double currentFPS{ 0.0 };
        std::chrono::steady_clock::time_point lastFrameTime;
    } m_stats;

    // 传输统计
    class TransferStats {
    public:
        TransferStats() :
            totalBytes(0),
            successCount(0),
            failureCount(0),
            currentSpeed(0.0) {}

        void reset() {
            totalBytes.store(0);
            successCount.store(0);
            failureCount.store(0);
            currentSpeed.store(0.0);
            startTime = std::chrono::steady_clock::now();
        }

        void addBytes(uint64_t bytes) {
            totalBytes.fetch_add(bytes);
        }

        void incrementSuccess() {
            successCount.fetch_add(1);
        }

        void incrementFailure() {
            failureCount.fetch_add(1);
        }

        void updateSpeed() {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                now - startTime).count();
            if (duration > 0) {
                currentSpeed.store(static_cast<double>(totalBytes) /
                    duration / (1024.0 * 1024.0));
            }
        }

        uint64_t getTotalBytes() const { return totalBytes.load(); }
        uint64_t getSuccessCount() const { return successCount.load(); }
        uint64_t getFailureCount() const { return failureCount.load(); }
        double getCurrentSpeed() const { return currentSpeed.load(); }
        std::chrono::steady_clock::time_point getStartTime() const { return startTime; }

    private:
        std::atomic<uint64_t> totalBytes;
        std::atomic<uint64_t> successCount;
        std::atomic<uint64_t> failureCount;
        std::atomic<double> currentSpeed;
        std::chrono::steady_clock::time_point startTime;
    };
    TransferStats m_transferStats;

    // 流控制和错误处理参数
    static constexpr size_t MAX_PACKET_SIZE = 16 * 1024;     // 16KB 每包
    static constexpr size_t BUFFER_SIZE = 16 * 1024;         // 16KB 每缓冲区
    static constexpr size_t BUFFER_COUNT = 32;               // 32 个缓冲区
    static constexpr int MAX_READ_RETRIES = 3;               // 最大重试次数
    static constexpr int READ_RETRY_DELAY_MS = 100;          // 重试延迟
    static constexpr int MAX_CONSECUTIVE_FAILURES = 10;      // 从100改为10
    static constexpr int STOP_CHECK_INTERVAL_MS = 100;       // 停止检查间隔
    static constexpr int STATS_UPDATE_INTERVAL_MS = 200;    // 统计更新间隔

    // 状态追踪
    std::atomic<uint64_t> m_totalBytes{ 0 };
    std::atomic<double> m_dataRate{ 0.0 };
    std::atomic<int> m_failedReads{ 0 };
    std::chrono::steady_clock::time_point m_startTime;
};