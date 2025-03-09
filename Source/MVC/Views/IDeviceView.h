// Source/MVC/Views/IDeviceView.h
#pragma once

#include <QObject>
#include <QString>
#include "DeviceModel.h"

/**
 * @brief 设备视图接口类
 *
 * 定义了设备控制视图应该实现的接口方法，
 * 使控制器可以与视图交互，而无需了解具体实现。
 */
class IDeviceView
{
public:
    /**
     * @brief 析构函数
     */
    virtual ~IDeviceView() {}

    /**
     * @brief 获取图像宽度
     * @param ok 输出参数，表示转换是否成功
     * @return 图像宽度值
     */
    virtual uint16_t getImageWidth(bool* ok = nullptr) const = 0;

    /**
     * @brief 获取图像高度
     * @param ok 输出参数，表示转换是否成功
     * @return 图像高度值
     */
    virtual uint16_t getImageHeight(bool* ok = nullptr) const = 0;

    /**
     * @brief 获取图像捕获类型
     * @return 捕获类型值
     */
    virtual uint8_t getCaptureType() const = 0;

    /**
     * @brief 更新图像宽度显示
     * @param width 宽度值
     */
    virtual void updateImageWidth(uint16_t width) = 0;

    /**
     * @brief 更新图像高度显示
     * @param height 高度值
     */
    virtual void updateImageHeight(uint16_t height) = 0;

    /**
     * @brief 更新捕获类型显示
     * @param captureType 捕获类型值
     */
    virtual void updateCaptureType(uint8_t captureType) = 0;

    /**
     * @brief 更新USB速度显示
     * @param speed USB速度（MB/s）
     */
    virtual void updateUsbSpeed(double speed) = 0;

    /**
     * @brief 更新传输字节数显示
     * @param bytes 已传输字节数
     */
    virtual void updateTransferredBytes(uint64_t bytes) = 0;

    /**
     * @brief 更新设备状态显示
     * @param state 设备状态
     */
    virtual void updateDeviceState(DeviceState state) = 0;

    /**
     * @brief 显示错误消息
     * @param message 错误消息内容
     */
    virtual void showErrorMessage(const QString& message) = 0;

    /**
     * @brief 显示确认对话框
     * @param title 标题
     * @param message 消息内容
     * @return 用户选择，true表示确认，false表示取消
     */
    virtual bool showConfirmDialog(const QString& title, const QString& message) = 0;

signals:
    /**
     * @brief 启动传输信号
     */
    virtual void startTransferRequested() = 0;

    /**
     * @brief 停止传输信号
     */
    virtual void stopTransferRequested() = 0;

    /**
     * @brief 重置设备信号
     */
    virtual void resetDeviceRequested() = 0;

    /**
     * @brief 图像参数变更信号
     */
    virtual void imageParametersChanged() = 0;
};