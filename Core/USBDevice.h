#pragma once

#include <Windows.h>

#include <memory>
#include <string>
#include <functional>
#include <QObject>
#include <CyAPI.h>

class USBDevice : public QObject {
    Q_OBJECT

public:
    explicit USBDevice(HWND hwnd);
    ~USBDevice();

    bool isConnected() const;
    bool open();
    void close();
    bool reset();

    // Transfer control
    bool startTransfer();
    bool stopTransfer();

    bool readData(PUCHAR buffer, LONG& length);

    // Device properties
    QString getDeviceInfo() const;
    bool isUSB3() const;

    void setTransferSize(int size);
    void setQueueSize(int size);
    bool isTransferring() const { return m_isTransferring; }

    // 新增传输状态查询
    double getTransferRate() const;
    uint64_t getTotalTransferred() const;

signals:
    void statusChanged(const std::string& status);
    void transferProgress(uint64_t transferred, int length, int success, int failed);
    void deviceError(const QString& error);

private:
    bool initEndpoints();
    bool validateDevice();
    void emitError(const QString& error);

private:
    // 命令发送方法
    bool sendCommand(const UCHAR* cmd, int length);
    bool configureTransfer(ULONG frameSize);

    // 命令处理方法
    bool prepareCommandBuffer(PUCHAR buffer, const UCHAR* cmdTemplate);
    void updateTransferStats();

    std::shared_ptr<CCyUSBDevice> m_device;
    CCyBulkEndPoint* m_inEndpoint;
    CCyBulkEndPoint* m_outEndpoint;

    HWND m_hwnd;

    static const int TRANSFER_TIMEOUT = 1000;  // ms
    static const int MAX_PACKET_SIZE = 1024;   // bytes

    static const int DEFAULT_TRANSFER_SIZE = 512 * 1024;  // 512KB per transfer
    static const int DEFAULT_QUEUE_SIZE = 64;             // 64 queues

    static const ULONG READ_TIMEOUT = 1000;  // 读取超时时间ms
    static const ULONG MAX_TRANSFER_SIZE = 1024 * 1024;  // 最大传输大小

    int m_transferSize;
    int m_queueSize;
    uint64_t m_totalTransferred{0};

    // 传输状态
    std::atomic<bool> m_isTransferring;
    std::chrono::steady_clock::time_point m_transferStartTime;

    // FPGA 命令定义
    static const int CMD_BUFFER_SIZE = 512;
    static const UCHAR CMD_START[];
    static const UCHAR CMD_FRAME_SIZE[];
    static const UCHAR CMD_STOP[];

    // 配置参数
    uint16_t m_width;
    uint16_t m_height;
    uint8_t m_capType;
    uint8_t m_lanSeq;
    uint8_t m_channelMode;
    uint8_t m_invertPn;

    // 命令超时设置
    static const ULONG CMD_TIMEOUT = 1000;  // ms

    // 当前传输配置
    ULONG m_frameSize;
    bool m_isConfigured;
};