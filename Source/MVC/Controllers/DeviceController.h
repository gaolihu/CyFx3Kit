// Source/MVC/Controllers/DeviceController.h
#pragma once

#include <QObject>
#include <memory>
#include "DeviceModel.h"
#include "IDeviceView.h"
#include "FX3DeviceManager.h"

/**
 * @brief 设备控制器类
 *
 * 作为设备MVC架构中的控制器，隔离主控制器与设备管理器
 * 管理设备模型和设备视图之间的交互，处理与设备相关的所有业务逻辑
 */
class DeviceController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param deviceView 设备视图接口
     * @param parent 父对象
     */
    explicit DeviceController(IDeviceView* deviceView, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DeviceController();

    /**
     * @brief 初始化设备控制器
     * @param windowHandle 窗口句柄，用于设备管理器初始化
     * @return 初始化是否成功
     */
    bool initialize(HWND windowHandle);

    /**
     * @brief 检查并打开设备
     * @return 操作是否成功
     */
    bool checkAndOpenDevice();

    /**
     * @brief 重置设备
     * @return 操作是否成功
     */
    bool resetDevice();

    /**
     * @brief 设置命令文件目录
     * @param dirPath 目录路径
     * @return 操作是否成功
     */
    bool setCommandDirectory(const QString& dirPath);

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
     * @brief 更新图像参数
     * @return 操作是否成功
     */
    bool updateImageParameters();

    /**
     * @brief 获取当前图像参数
     * @param width 输出参数，图像宽度
     * @param height 输出参数，图像高度
     * @param captureType 输出参数，捕获类型
     */
    void getImageParameters(uint16_t& width, uint16_t& height, uint8_t& captureType);

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param captureType 捕获类型
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t captureType);

    /**
     * @brief 准备关闭
     * 在应用程序退出前调用，确保安全关闭设备
     */
    void prepareForShutdown();

    /**
     * @brief 设备是否已连接
     * @return 连接状态
     */
    bool isDeviceConnected() const;

    /**
     * @brief 设备是否正在传输数据
     * @return 传输状态
     */
    bool isTransferring() const;

signals:
    /**
     * @brief 传输状态变更信号
     * @param transferring 是否正在传输
     */
    void signal_Dev_C_transferStateChanged(bool transferring);

    /**
     * @brief 传输统计更新信号
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率(MB/s)
     * @param errorCount 错误计数
     */
    void signal_Dev_C_transferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint32_t errorCount);

    /**
     * @brief USB速度更新信号
     * @param speedDesc 速度描述
     * @param isUSB3 是否是USB3连接
     * @param isConnected 是否已连接
     */
    void signal_Dev_C_usbSpeedUpdated(const QString& speedDesc, bool isUSB3, bool isConnected);

    /**
     * @brief 设备错误信号
     * @param title 错误标题
     * @param message 错误信息
     */
    void signal_Dev_C_deviceError(const QString& title, const QString& message);

    /**
     * @brief 数据包可用信号
     * @param packet 数据包
     */
    void signal_Dev_C_dataPacketAvailable(const std::vector<DataPacket>& packets);

private slots:
    /**
     * @brief 处理开始传输请求
     */
    void slot_Dev_C_handleStartTransferRequest();

    /**
     * @brief 处理停止传输请求
     */
    void slot_Dev_C_handleStopTransferRequest();

    /**
     * @brief 处理重置设备请求
     */
    void slot_Dev_C_handleResetDeviceRequest();

    /**
     * @brief 处理图像参数变更
     */
    void slot_Dev_C_handleImageParametersChanged();

    /**
     * @brief 处理传输状态变更
     * @param transferring 是否正在传输
     */
    void slot_handleTransferStateChanged(bool transferring);

    /**
     * @brief 处理传输统计更新
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率(MB/s)
     * @param errorCount 传输时间ms
     */
    void slot_Dev_C_handleTransferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint32_t elapseMs);

    /**
     * @brief 处理USB速度更新
     * @param speedDesc 速度描述
     * @param isUSB3 是否是USB3连接
     * @param isConnected 是否已连接
     */
    void slot_Dev_C_handleUsbSpeedUpdated(const QString& speedDesc, bool isUSB3, bool isConnected);

    /**
     * @brief 处理设备错误
     * @param title 错误标题
     * @param message 错误信息
     */
    void slot_Dev_C_handleDeviceError(const QString& title, const QString& message);

private:
    /**
     * @brief 初始化连接
     * 连接视图信号和设备管理器信号
     */
    void initializeConnections();

    /**
     * @brief 更新视图状态
     * 根据设备状态更新视图显示
     */
    void updateViewState();

    /**
     * @brief 验证图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param captureType 捕获类型
     * @param errorMessage 输出参数，错误消息
     * @return 验证是否通过
     */
    bool validateImageParameters(uint16_t width, uint16_t height, uint8_t captureType, QString& errorMessage);

private:
    DeviceModel* m_deviceModel;                     ///< 设备模型指针（单例）
    IDeviceView* m_deviceView;                      ///< 设备视图指针
    std::unique_ptr<FX3DeviceManager> m_deviceManager; ///< 设备管理器指针
    bool m_initialized;                             ///< 控制器是否已初始化
};