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

// ���ݰ��ṹ
struct DataPacket {
    std::vector<uint8_t> data;
    size_t size;
    uint64_t timestamp;
};

// ���ݴ������ӿ�
class IDataProcessor {
public:
    virtual ~IDataProcessor() = default;
    virtual void processData(const DataPacket& packet) = 0;
};

// ���ݲɼ�������
class DataAcquisitionManager : public QObject {
    Q_OBJECT

public:
    explicit DataAcquisitionManager(std::shared_ptr<USBDevice> device);
    ~DataAcquisitionManager();

    // ���úͿ��ƽӿ�
    void setDataProcessor(std::shared_ptr<IDataProcessor> processor);
    bool startAcquisition(uint16_t width, uint16_t height, uint8_t capType);
    void stopAcquisition();

    // ����������
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
    // �ڲ��� - ѭ��������������
    class CircularBuffer {
    public:
        CircularBuffer(size_t bufferCount, size_t bufferSize);

        // ��ȡ��һ����д��Ļ�����
        std::pair<uint8_t*, size_t> getWriteBuffer();
        // �ύд����ɵĻ�����
        void commitBuffer(size_t bytesWritten);
        // ��ȡ�ɶ�ȡ�Ļ�����
        std::optional<DataPacket> getReadBuffer();

    private:
        std::vector<std::vector<uint8_t>> m_buffers;
        std::queue<DataPacket> m_readyBuffers;
        size_t m_currentWriteBuffer;
        std::mutex m_mutex;
    };

private:
    // ���ݲɼ��߳�
    void acquisitionThread();
    // ���ݴ����߳�
    void processingThread();
    // ͳ����Ϣ����
    void updateStats();

    // �ɼ�״̬����
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

    // �߳̿���
    std::atomic<bool> m_running{ false };
    std::thread m_acquisitionThread;
    std::thread m_processingThread;

    // ͬ���Ϳ���
    std::mutex m_mutex;
    std::condition_variable m_dataReady;

    // ���ò���
    size_t m_bufferSize{ 16 * 1024 };  // 18KB per buffer
    size_t m_queueSize{ 32 };            // 32 buffers in queue

    static constexpr size_t BUFFER_SIZE = 16 * 1024;     // 16KB per buffer
    static constexpr size_t BUFFER_COUNT = 32;           // 32 buffers in queue

    // ͳ����Ϣ
    std::atomic<uint64_t> m_totalBytes{ 0 };
    std::chrono::steady_clock::time_point m_startTime;

    std::atomic<AcquisitionState> m_acquisitionState;
    std::mutex m_stateMutex;

    // �ɼ�����
    struct AcquisitionParams {
        uint16_t width;
        uint16_t height;
        uint8_t format;
        uint32_t frameRate;
        bool continuous;
    } m_params;

    // �ɼ�ͳ��
    struct AcquisitionStats {
        uint64_t totalFrames;
        uint64_t droppedFrames;
        double currentFPS;
        std::chrono::steady_clock::time_point lastFrameTime;
    } m_stats;
};