// Source/Core/USBDevice.h
#pragma once

#include <Windows.h>

#include <memory>
#include <string>
#include <functional>
#include <QObject>
#include <CyAPI.h>

/**
 * @brief USB设备类
 *
 * 封装Cypress FX3 USB设备的操作，提供数据传输、命令发送和设备状态管理
 */
class USBDevice : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param hwnd 窗口句柄，用于USB设备通知
     */
    explicit USBDevice(HWND hwnd);

    /**
     * @brief 析构函数
     */
    ~USBDevice();

    /**
     * @brief 检查设备是否已连接
     * @return 连接状态
     */
    bool isConnected() const;

    /**
     * @brief 打开设备
     * @return 操作是否成功
     */
    bool open();

    /**
     * @brief 关闭设备
     */
    void close();

    /**
     * @brief 重置设备
     * @return 操作是否成功
     */
    bool reset();

    /**
     * @brief 启动数据传输
     * @return 操作是否成功
     */
    bool startTransfer();

    /**
     * @brief 停止数据传输
     * @return 操作是否成功
     */
    bool stopTransfer();

    /**
     * @brief 读取数据
     * @param buffer 数据缓冲区
     * @param length 输入/输出参数，期望读取长度/实际读取长度
     * @return 操作是否成功
     */
    bool readData(PUCHAR buffer, LONG& length);

    /**
     * @brief 获取设备信息
     * @return 设备信息描述
     */
    QString getDeviceInfo() const;

    /**
     * @brief 检查设备是否是USB3连接
     * @return USB3连接状态
     */
    bool isUSB3() const;

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param capType 捕获类型
     */
    void setImageParams(uint16_t width, uint16_t height, uint8_t capType) {
        m_width = width;
        m_height = height;
        m_capType = capType;
    }

    /**
     * @brief 设置传输队列大小
     * @param size 队列大小
     */
    void setQueueSize(int size);

    /**
     * @brief 检查是否正在传输
     * @return 传输状态
     */
    bool isTransferring() const { return m_isTransferring; }

    /**
     * @brief USB连接速度类型枚举
     */
    enum class USBSpeedType {
        UNKNOWN,        // 未知
        LOW_SPEED,      // 低速 (USB 1.0) - 1.5 Mbps
        FULL_SPEED,     // 全速 (USB 1.1) - 12 Mbps
        HIGH_SPEED,     // 高速 (USB 2.0) - 480 Mbps
        SUPER_SPEED,    // 超速 (USB 3.0) - 5 Gbps
        SUPER_SPEED_P,  // 超速+ (USB 3.1+) - 10+ Gbps
        NOT_CONNECTED   // 未连接
    };

    /**
     * @brief 获取USB速度类型
     * @return 速度类型
     */
    USBSpeedType getUsbSpeedType() const;

    /**
     * @brief 获取USB速度描述文本
     * @return 速度描述
     */
    QString getUsbSpeedDescription() const;

signals:
    /**
     * @brief 设备状态变更信号
     * @param status 新状态
     */
    void signal_USB_statusChanged(const std::string& status);

    /**
     * @brief 传输进度信号
     * @param transferred 已传输字节数
     * @param length 当前包长度
     * @param success 成功传输计数
     * @param failed 失败传输计数
     */
    void signal_USB_transferProgress(uint64_t transferred, int length, int success, int failed);

    /**
     * @brief 设备错误信号
     * @param error 错误描述
     */
    void signal_USB_deviceError(const QString& error);

private:
    /**
     * @brief 初始化端点
     * @return 初始化是否成功
     */
    bool initEndpoints();

    /**
     * @brief 验证设备
     * @return 验证是否成功
     */
    bool validateDevice();

    /**
     * @brief 发送错误信号
     * @param error 错误描述
     */
    void emitError(const QString& error);

    /**
     * @brief 发送命令
     * @param cmd 命令数据
     * @param length 命令长度
     * @return 发送是否成功
     */
    bool sendCommand(const UCHAR* cmd, size_t length);

    /**
     * @brief 配置传输
     * @param frameSize 帧大小
     * @return 配置是否成功
     */
    bool configureTransfer(ULONG frameSize);

    /**
     * @brief 准备命令缓冲区
     * @param buffer 输出缓冲区
     * @param cmdTemplate 命令模板
     * @return 准备是否成功
     */
    bool prepareCommandBuffer(PUCHAR buffer, const UCHAR* cmdTemplate);

    std::shared_ptr<CCyUSBDevice> m_device;            // Cypress USB设备
    CCyBulkEndPoint* m_inEndpoint;                     // 数据输入端点
    CCyBulkEndPoint* m_outEndpoint;                    // 命令输出端点

    HWND m_hwnd;                                       // 窗口句柄

    static const int TRANSFER_TIMEOUT = 1000;          // 传输超时(ms)
    static const int MAX_PACKET_SIZE = 1024;           // 最大包大小(bytes)

    static const int DEFAULT_TRANSFER_SIZE = 512 * 1024; // 默认传输大小
    static const int DEFAULT_QUEUE_SIZE = 64;          // 默认队列大小

    static const ULONG READ_TIMEOUT = 1000;            // 读取超时(ms)
    static const ULONG MAX_TRANSFER_SIZE = 1024 * 1024; // 最大传输大小

    int m_transferSize;                                // 传输大小
    int m_queueSize;                                   // 队列大小

    // 传输状态
    std::atomic<bool> m_isTransferring;                // 传输标志
    std::chrono::steady_clock::time_point m_transferStartTime; // 传输开始时间

    // 基本字节计数 - 不进行速率计算
    std::atomic<uint64_t> m_totalBytes{ 0 };           // 总字节数

    // FPGA 命令定义
    static const int CMD_BUFFER_SIZE = 512;            // 命令缓冲区大小

    // 配置参数
    uint16_t m_width;                                  // 图像宽度
    uint16_t m_height;                                 // 图像高度
    uint8_t m_capType;                                 // 捕获类型
    uint8_t m_lanSeq{ 1 };                             // 通道序列
    uint8_t m_channelMode{ 0 };                        // 通道模式
    uint8_t m_invertPn{ 0 };                           // 反转PN

    // 命令超时设置
    static const ULONG CMD_TIMEOUT = 1000;             // 命令超时(ms)

    // 当前传输配置
    ULONG m_frameSize;                                 // 帧大小
    bool m_isConfigured;                               // 配置标志

    // 进度更新控制
    static constexpr int PROGRESS_UPDATE_INTERVAL_MS = 500; // 进度更新间隔
    std::chrono::steady_clock::time_point m_lastProgressUpdate; // 上次进度更新时间
};