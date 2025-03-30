// Source/MVC/Controllers/UpdateDeviceController.h
#pragma once

#include <QObject>
#include "UpdateDeviceModel.h"
#include "UpdateDeviceView.h"

/**
 * @brief 设备升级控制器类
 *
 * 负责协调设备升级模型和视图之间的交互，
 * 处理用户输入和业务逻辑
 */
class UpdateDeviceController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param model 设备升级模型
     * @param view 设备升级视图
     * @param parent 父对象
     */
    explicit UpdateDeviceController(UpdateDeviceView* view, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~UpdateDeviceController();

    /**
     * @brief 初始化控制器
     */
    bool initialize();

signals:
    /**
     * @brief 升级完成信号
     * @param success 是否成功
     * @param message 完成消息
     */
    void signal_UD_C_updateCompleted(bool success, const QString& message);

private slots:
    /**
     * @brief 处理SOC文件选择
     */
    void slot_UD_C_handleSOCFileSelect();

    /**
     * @brief 处理PHY文件选择
     */
    void slot_UD_C_handlePHYFileSelect();

    /**
     * @brief 处理SOC升级
     */
    void slot_UD_C_handleSOCUpdate();

    /**
     * @brief 处理PHY升级
     */
    void slot_UD_C_handlePHYUpdate();

    /**
     * @brief 处理模型状态变更
     * @param status 新的状态
     */
    void slot_UD_C_handleModelStatusChanged(UpdateStatus status);

    /**
     * @brief 处理模型进度变更
     * @param progress 新的进度
     */
    void slot_UD_C_handleModelProgressChanged(int progress);

    /**
     * @brief 处理模型升级完成
     * @param success 是否成功
     * @param message 完成消息
     */
    void slot_UD_C_handleModelUpdateCompleted(bool success, const QString& message);

    /**
     * @brief 处理模型文件路径变更
     * @param deviceType 设备类型
     * @param filePath 新的文件路径
     */
    void slot_UD_C_handleModelFilePathChanged(DeviceType deviceType, const QString& filePath);

private:
    /**
     * @brief 连接模型和视图信号
     */
    void connectSignals();

    /**
     * @brief 更新视图UI状态
     */
    void updateViewState();

private:
    UpdateDeviceModel* m_model; // 设备升级模型
    UpdateDeviceView* m_view;   // 设备升级视图
};