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
    virtual void signal_Dev_V_startTransferRequested() = 0;

    /**
     * @brief 停止传输信号
     */
    virtual void signal_Dev_V_stopTransferRequested() = 0;

    /**
     * @brief 重置设备信号
     */
    virtual void signal_Dev_V_resetDeviceRequested() = 0;

    /**
     * @brief 图像参数变更信号
     */
    virtual void signal_Dev_V_imageParametersChanged() = 0;
};