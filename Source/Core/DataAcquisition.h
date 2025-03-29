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
//#define AQ_DBG
#ifdef AQ_DBG
#include "Logger.h"
#endif // AQ_DBG

class USBDevice;

/**
 * @brief 数据处理器接口
 *
 * 定义数据包处理的标准接口
 */
class IDataProcessor {
public:
    virtual ~IDataProcessor() = default;

    /**
     * @brief 处理数据包
     * @param packet 要处理的数据包
     */
    virtual void processData(const DataPacket& packet) = 0;

    /**
     * @brief 处理批量数据包（默认实现，可被具体处理器覆盖以提高性能）
     * @param packets 批量数据包
     */
    virtual void processBatchData(const DataPacketBatch& packets) {
        for (const auto& packet : packets) {
            processData(packet);
        }
    }
};

/**
 * @brief 数据采集管理器
 *
 * 负责管理数据采集过程，包括从USB设备读取数据、
 * 缓冲管理、数据处理和统计信息收集
 */
class DataAcquisitionManager : public QObject,
    public std::enable_shared_from_this<DataAcquisitionManager> {
    Q_OBJECT

public:
    /**
     * @brief 创建DataAcquisitionManager实例的静态工厂方法
     * @param device USB设备指针
     * @return 新创建的DataAcquisitionManager实例
     */
    static std::shared_ptr<DataAcquisitionManager> create(std::shared_ptr<USBDevice> device);

    /**
     * @brief 析构函数
     * 确保所有资源正确释放
     */
    ~DataAcquisitionManager();

    /**
     * @brief 禁用拷贝构造函数
     */
    DataAcquisitionManager(const DataAcquisitionManager&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    DataAcquisitionManager& operator=(const DataAcquisitionManager&) = delete;

    /**
     * @brief 设置数据处理器
     * @param processor 数据处理器实例
     */
    void setDataProcessor(std::shared_ptr<IDataProcessor> processor) {
        m_processor = processor;
    }

    /**
     * @brief 开始数据采集
     * @param width 图像宽度
     * @param height 图像高度
     * @param capType 捕获类型
     * @return 是否成功启动采集
     */
    bool startAcquisition(uint16_t width, uint16_t height, uint8_t capType);

    /**
     * @brief 停止数据采集
     */
    void stopAcquisition();

    /**
     * @brief 准备关闭
     * 在应用退出前调用，确保安全关闭所有资源
     */
    void prepareForShutdown() {
        m_isShuttingDown = true;
        stopAcquisition(); // 立即停止所有活动
        // 断开所有信号连接以防止新的异步调用
        disconnect(this, nullptr, nullptr, nullptr);
    }

    /**
     * @brief 获取采集运行时间（秒）
     * @return 运行时间
     */
    uint64_t getElapsedTimeSeconds() const {
        if (!m_running) return 0;
        return m_rateStats.getElapsedTimeMs() / 1000;
    }

    /**
     * @brief 检查采集是否正在运行
     * @return 采集状态
     */
    bool isRunning() const { return m_running; }

    /**
     * @brief 检查是否正在关闭
     * @return 关闭状态
     */
    bool isShuttingDown() const { return m_isShuttingDown; }

    /**
     * @brief 检查是否处于错误状态
     * @return 错误状态
     */
    bool isErrorState() const { return m_errorOccurred; }

    /**
     * @brief 获取总接收字节数
     * @return 总字节数
     */
    uint64_t getTotalBytes() const { return m_totalBytes; }

    /**
     * @brief 获取当前数据速率（MB/s）
     * @return 数据速率
     */
    double getDataRate() const { return m_dataRate; }

signals:
    /**
     * @brief 采集开始信号
     */
    void signal_AQ_acquisitionStarted();

    /**
     * @brief 采集停止信号
     */
    void signal_AQ_acquisitionStopped();

    /**
     * @brief 数据接收信号
     * @param packet 接收到的数据包
     */
    void signal_AQ_dataReceived(const DataPacket& packet);

    void signal_AQ_batchDataReceived(const std::vector<DataPacket>& packets);

    /**
     * @brief 错误发生信号
     * @param error 错误信息
     */
    void signal_AQ_errorOccurred(const QString& error);

    /**
     * @brief 统计信息更新信号
     * @param receivedBytes 已接收字节数
     * @param dataRate 数据速率（MB/s）
     * @param elapsedTimeMs 运行时间（毫秒）
     */
    void signal_AQ_statsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeMs);

    /**
     * @brief 采集状态变更信号
     * @param state 新状态描述
     */
    void signal_AQ_acquisitionStateChanged(const QString& state);

private:
    /**
     * @brief 私有构造函数，强制使用create静态方法
     * @param device USB设备指针
     */
    explicit DataAcquisitionManager(std::shared_ptr<USBDevice> device);

    /**
     * @brief 原子关闭标志
     */
    std::atomic<bool> m_isShuttingDown{ false };

    /**
     * @brief 安全发送信号辅助模板函数
     * @param emitFunc 信号发送函数
     */
    template<typename Func>
    void safeEmit(Func emitFunc) {
        if (!m_isShuttingDown) {
            emitFunc();
        }
    }

    /**
     * @brief 速率统计类
     *
     * 提供高效、精确的数据速率和时间统计计算
     */
    class RateStatistics {
    public:
        RateStatistics()
            : m_totalBytes(0)
            , m_startTime(std::chrono::steady_clock::now())
        {
        }

        void reset() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_totalBytes = 0;
            m_startTime = std::chrono::steady_clock::now();
        }

        void addBytes(uint64_t bytes) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_totalBytes += bytes;
        }

        double getDataRate() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_startTime).count();

            if (elapsedMs <= 0) return 0.0;

            // Calculate MB/s directly from total bytes and elapsed time
            return (static_cast<double>(m_totalBytes) / elapsedMs) * 1000.0 / (1024.0 * 1024.0);
        }

        uint64_t getTotalBytes() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_totalBytes;
        }

        uint64_t getElapsedTimeMs() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startTime).count();
        }

    private:
        std::mutex mutable m_mutex;                        // Mutex for thread safety
        uint64_t m_totalBytes;                             // Total bytes transferred
        std::chrono::steady_clock::time_point m_startTime; // Start time
    };

    /**
     * @brief 循环缓冲区管理器
     *
     * 管理数据缓冲池，实现高效的数据生产者-消费者模型
     */
    class CircularBuffer {
    public:
        /**
         * @brief 缓冲区状态警告级别
         */
        enum class WarningLevel {
            C_NORMAL,         // 正常
            C_WARNING,        // 警告（使用率超过75%）
            C_CRITICAL        // 严重（使用率超过90%）
        };

        /**
         * @brief 构造函数
         * @param bufferCount 缓冲区数量
         * @param bufferSize 每个缓冲区大小
         */
        CircularBuffer(size_t bufferCount, size_t bufferSize);

        /**
         * @brief 析构函数
         */
        ~CircularBuffer() = default;

        /**
         * @brief 检查缓冲区状态
         * @return 警告级别
         */
        WarningLevel checkBufferStatus();

        /**
         * @brief 获取写入缓冲区
         * @return 缓冲区指针和大小
         */
        std::pair<uint8_t*, size_t> getWriteBuffer();

        /**
         * @brief 提交已写入的缓冲区
         * @param bytesWritten 已写入的字节数
         */
        void commitBuffer(size_t bytesWritten);

        /**
         * @brief 设置批处理参数
         * @param maxPacketsPerBatch 每批最大数据包数量
         * @param maxBatchIntervalMs 最大批处理间隔(毫秒)
         */
        void setBatchingParams(size_t maxPacketsPerBatch, uint64_t maxBatchIntervalMs) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_maxPacketsPerBatch = maxPacketsPerBatch;
            m_maxBatchIntervalMs = maxBatchIntervalMs;
        }

        /**
         * @brief 获取批处理的数据包
         * @return 数据包批次的可选值
         */
        std::optional<DataPacketBatch> getReadyBatch() {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_readyBatches.empty()) {
#ifdef AQ_DBG
                LOG_DEBUG(LocalQTCompat::fromLocal8Bit("没有可用的批次数据"));
#endif // AQ_DBG
                return std::nullopt;
            }

            DataPacketBatch batch = std::move(m_readyBatches.front());
            m_readyBatches.pop();
#ifdef AQ_DBG
            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("获取批次数据，包含 %1 个数据包").arg(batch.size()));
#endif // AQ_DBG
            return batch;
        }

        /**
         * @brief 获取读取缓冲区
         * @return 可选的数据包
         */
        std::optional<DataPacket> getReadBuffer();

        /**
         * @brief 重置缓冲区
         */
        void reset();

    private:
        std::vector<std::vector<uint8_t>> m_buffers;   // 缓冲区数组
        std::queue<DataPacket> m_readyBuffers;         // 就绪缓冲区队列
        size_t m_currentWriteBuffer;                   // 当前写入缓冲区索引
        std::mutex m_mutex;                            // 互斥锁
        const size_t m_warningThreshold;               // 警告阈值
        const size_t m_criticalThreshold;              // 严重阈值
        WarningLevel m_lastWarningLevel{ WarningLevel::C_NORMAL }; // 上次警告级别

        size_t m_maxPacketsPerBatch = 10;               // 每批最多包数量
        uint64_t m_maxBatchIntervalMs = 50;             // 最大批处理间隔(ms)
        uint32_t m_currentBatchId = 0;                  // 当前批次ID
        size_t m_packetsInCurrentBatch = 0;             // 当前批次中的包数量
        std::chrono::steady_clock::time_point m_batchStartTime; // 批次开始时间
        std::queue<DataPacketBatch> m_readyBatches;     // 就绪批次队列
        std::vector<DataPacket> m_currentBatch;         // 当前构建中的批次
    };

private:
    /**
     * @brief 停止原因枚举
     */
    enum class StopReason {
        USER_REQUEST,     // 用户请求
        READ_ERROR,       // 读取错误
        DEVICE_ERROR,     // 设备错误
        BUFFER_OVERFLOW   // 缓冲区溢出
    };

    /**
     * @brief 采集状态枚举
     */
    enum class AcquisitionState {
        AC_IDLE,          // 空闲
        AC_CONFIGURING,   // 配置中
        AC_RUNNING,       // 运行中
        AC_STOPPING,      // 停止中
        AC_ERROR          // 错误
    };

    /**
     * @brief 采集线程主函数
     */
    void acquisitionThread();

    /**
     * @brief 处理线程主函数
     */
    void processingThread();

    /**
     * @brief 发送停止信号
     * @param reason 停止原因
     */
    void signalStop(StopReason reason);

    /**
     * @brief 更新统计信息
     */
    void updateStats();

    /**
     * @brief 验证采集参数
     * @return 参数是否有效
     */
    bool validateAcquisitionParams() const;

    /**
     * @brief 更新采集状态
     * @param newState 新状态
     */
    void updateAcquisitionState(AcquisitionState newState);

    // 设备和处理器
    std::weak_ptr<USBDevice> m_deviceWeak;             // 设备弱引用
    std::shared_ptr<IDataProcessor> m_processor;       // 数据处理器
    std::unique_ptr<CircularBuffer> m_buffer;          // 循环缓冲区
    RateStatistics m_rateStats;                        // 速率统计

    // 线程控制
    std::atomic<bool> m_running{ false };              // 运行标志
    std::atomic<bool> m_errorOccurred{ false };        // 错误标志
    std::thread m_acquisitionThread;                   // 采集线程
    std::thread m_processingThread;                    // 处理线程

    // 同步和控制
    std::mutex m_mutex;                                // 主互斥锁
    std::mutex m_stopMutex;                            // 停止互斥锁
    std::condition_variable m_dataReady;               // 数据就绪条件变量
    std::atomic<AcquisitionState> m_acquisitionState{ AcquisitionState::AC_IDLE }; // 采集状态

    // 配置参数
    struct AcquisitionParams {
        uint16_t width{ 0 };                           // 图像宽度
        uint16_t height{ 0 };                          // 图像高度
        uint8_t format{ 0 };                           // 格式
        uint32_t frameRate{ 0 };                       // 帧率
        bool continuous{ true };                       // 连续模式
    } m_params;

    // 统计信息
    struct AcquisitionStats {
        uint64_t totalFrames{ 0 };                     // 总帧数
        uint64_t droppedFrames{ 0 };                   // 丢帧数
        double currentFPS{ 0.0 };                      // 当前帧率
        std::chrono::steady_clock::time_point lastFrameTime; // 上一帧时间
    } m_stats;

    // 流控制和错误处理参数
    static constexpr size_t MAX_PACKET_SIZE = 16 * 16 * 1024;     // 16KB 每包
    static constexpr size_t BUFFER_SIZE = 16 * 16 * 1024;         // 16KB 每缓冲区
    static constexpr size_t BUFFER_COUNT = 64;               // 64 个缓冲区
    static constexpr int MAX_READ_RETRIES = 3;               // 最大重试次数
    static constexpr int READ_RETRY_DELAY_MS = 100;          // 重试延迟
    static constexpr int MAX_CONSECUTIVE_FAILURES = 10;      // 最大连续失败次数
    static constexpr int STOP_CHECK_INTERVAL_MS = 100;       // 停止检查间隔
    static constexpr int STATS_UPDATE_INTERVAL_MS = 200;     // 统计更新间隔

    // 状态追踪
    std::atomic<uint64_t> m_totalBytes{ 0 };           // 总字节数
    std::atomic<double> m_dataRate{ 0.0 };             // 数据速率
    std::atomic<int> m_failedReads{ 0 };               // 失败读取计数
    std::chrono::steady_clock::time_point m_startTime; // 开始时间
};