// Source/MVC/Models/DeviceModel.h
#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <cstdint>
#include "DeviceState.h"

/**
 * @brief 设备模型类
 *
 * 负责存储设备相关的数据和状态，包括图像参数和设备状态。
 */
class DeviceModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的引用
     */
    static DeviceModel* getInstance();

    /**
     * @brief 获取图像宽度
     * @return 当前设置的图像宽度
     */
    uint16_t getImageWidth() const;

    /**
     * @brief 设置图像宽度
     * @param width 图像宽度值
     */
    void setImageWidth(uint16_t width);

    /**
     * @brief 获取图像高度
     * @return 当前设置的图像高度
     */
    uint16_t getImageHeight() const;

    /**
     * @brief 设置图像高度
     * @param height 图像高度值
     */
    void setImageHeight(uint16_t height);

    /**
     * @brief 获取图像捕获类型
     * @return 当前设置的捕获类型
     */
    uint8_t getCaptureType() const;

    /**
     * @brief 设置图像捕获类型
     * @param captureType 捕获类型值
     */
    void setCaptureType(uint8_t captureType);

    /**
     * @brief 获取设备状态
     * @return 当前设备状态
     */
    DeviceState getDeviceState() const;

    /**
     * @brief 设置设备状态
     * @param state 新的设备状态
     */
    void setDeviceState(DeviceState state);

    /**
     * @brief 获取USB速度
     * @return 当前USB速度（MB/s）
     */
    double getUsbSpeed() const;

    /**
     * @brief 设置USB速度
     * @param speed USB速度（MB/s）
     */
    void setUsbSpeed(double speed);

    /**
     * @brief 获取传输字节数
     * @return 已传输的字节数
     */
    uint64_t getTransferredBytes() const;

    /**
     * @brief 设置传输字节数
     * @param bytes 已传输的字节数
     */
    void setTransferredBytes(uint64_t bytes);

    /**
     * @brief 获取错误消息
     * @return 当前错误消息
     */
    QString getErrorMessage() const;

    /**
     * @brief 设置错误消息
     * @param message 错误消息内容
     */
    void setErrorMessage(const QString& message);

    /**
     * @brief 验证图像参数
     * @param errorMessage 错误消息输出参数
     * @return 验证结果，true表示验证通过
     */
    bool validateImageParameters(QString& errorMessage) const;

    /**
     * @brief 保存配置到系统设置
     * @return 保存结果，true表示成功
     */
    bool saveConfigToSettings();

    /**
     * @brief 从系统设置加载配置
     * @return 加载结果，true表示成功
     */
    bool loadConfigFromSettings();

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

signals:
    /**
     * @brief 图像参数变更信号
     * @param width 新的宽度
     * @param height 新的高度
     * @param captureType 新的捕获类型
     */
    void signal_Dev_M_imageParametersChanged(uint16_t width, uint16_t height, uint8_t captureType);

    /**
     * @brief 设备状态变更信号
     * @param state 新的设备状态
     */
    void signal_Dev_M_deviceStateChanged(DeviceState state);

    /**
     * @brief 传输统计更新信号
     * @param speed USB速度（MB/s）
     * @param bytes 已传输字节数
     */
    void signal_Dev_M_transferStatsUpdated(double speed, uint64_t bytes);

    /**
     * @brief 设备错误信号
     * @param message 错误消息
     */
    void signal_Dev_M_deviceError(const QString& message);

private:
    /**
     * @brief 构造函数（私有，用于单例模式）
     */
    DeviceModel();

    /**
     * @brief 析构函数（私有，用于单例模式）
     */
    ~DeviceModel();

    /**
     * @brief 禁用拷贝构造函数
     */
    DeviceModel(const DeviceModel&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    DeviceModel& operator=(const DeviceModel&) = delete;

private:
    uint16_t m_imageWidth;        ///< 图像宽度
    uint16_t m_imageHeight;       ///< 图像高度
    uint8_t m_captureType;        ///< 图像捕获类型
    DeviceState m_deviceState;    ///< 设备状态
    double m_usbSpeed;            ///< USB传输速度(MB/s)
    uint64_t m_transferredBytes;  ///< 已传输字节数
    QString m_errorMessage;       ///< 错误消息
    mutable QMutex m_dataMutex;   ///< 数据互斥锁
};