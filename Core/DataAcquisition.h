#pragma once

#include <QObject>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <optional>
#include <condition_variable>
#include <queue>
#include <vector>
#include "USBDevice.h"

class USBDevice;

// 数据包结构
struct DataPacket {
    std::vector<uint8_t> data;
    size_t size;
    uint64_t timestamp;
};

// 数据处理器接口
class IDataProcessor {
public:
    virtual ~IDataProcessor() = default;
    virtual void processData(const DataPacket& packet) = 0;
};

// 数据采集管理器
class DataAcquisitionManager : public QObject {
    Q_OBJECT

public:
    explicit DataAcquisitionManager(std::shared_ptr<USBDevice> device);
    ~DataAcquisitionManager();

    // 配置和控制接口
    void setDataProcessor(std::shared_ptr<IDataProcessor> processor);
    bool startAcquisition(uint16_t width, uint16_t height, uint8_t capType);
    void stopAcquisition();

    // 缓冲区配置
    void setBufferSize(size_t size);
    void setQueueSize(size_t size);

signals:
    void acquisitionStarted();
    void acquisitionStopped();
    void dataReceived(const DataPacket& packet);
    void errorOccurred(const QString& error);
    void statsUpdated(uint64_t receivedBytes, double dataRate);
    void acquisitionStateChanged(const QString& state);

private:
    // 内部类 - 循环缓冲区管理器
    class CircularBuffer {
    public:
        CircularBuffer(size_t bufferCount, size_t bufferSize);

        // 获取下一个可写入的缓冲区
        std::pair<uint8_t*, size_t> getWriteBuffer();
        // 提交写入完成的缓冲区
        void commitBuffer(size_t bytesWritten);
        // 获取可读取的缓冲区
        std::optional<DataPacket> getReadBuffer();

    private:
        std::vector<std::vector<uint8_t>> m_buffers;
        std::queue<DataPacket> m_readyBuffers;
        size_t m_currentWriteBuffer;
        std::mutex m_mutex;
    };

private:
    // 数据采集线程
    void acquisitionThread();
    // 数据处理线程
    void processingThread();
    // 统计信息更新
    void updateStats();

    // 采集状态管理
    enum class AcquisitionState {
        AC_IDLE,
        AC_CONFIGURING,
        AC_RUNNING,
        AC_PAUSED,
        AC_ERROR
    };

    void updateAcquisitionState(AcquisitionState newState);
    bool validateAcquisitionParams();
    void handleAcquisitionError(const std::string& error);

private:
    std::shared_ptr<USBDevice> m_device;
    std::shared_ptr<IDataProcessor> m_processor;
    std::unique_ptr<CircularBuffer> m_buffer;

    // 线程控制
    std::atomic<bool> m_running{ false };
    std::thread m_acquisitionThread;
    std::thread m_processingThread;

    // 同步和控制
    std::mutex m_mutex;
    std::condition_variable m_dataReady;

    // 配置参数
    size_t m_bufferSize{ 16 * 1024 };  // 18KB per buffer
    size_t m_queueSize{ 32 };            // 32 buffers in queue

    static constexpr size_t BUFFER_SIZE = 16 * 1024;     // 16KB per buffer
    static constexpr size_t BUFFER_COUNT = 32;           // 32 buffers in queue

    // 统计信息
    std::atomic<uint64_t> m_totalBytes{ 0 };
    std::chrono::steady_clock::time_point m_startTime;

    std::atomic<AcquisitionState> m_acquisitionState;
    std::mutex m_stateMutex;

    // 采集参数
    struct AcquisitionParams {
        uint16_t width;
        uint16_t height;
        uint8_t format;
        uint32_t frameRate;
        bool continuous;
    } m_params;

    // 采集统计
    struct AcquisitionStats {
        uint64_t totalFrames;
        uint64_t droppedFrames;
        double currentFPS;
        std::chrono::steady_clock::time_point lastFrameTime;
    } m_stats;
};