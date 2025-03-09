// Source/MVC/Views/UpdateDeviceView.h
#pragma once

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include "ui_UpdateDevice.h"
#include "UpdateDeviceModel.h"

/**
 * @brief 设备升级视图类
 *
 * 负责显示设备升级界面并处理用户交互
 */
class UpdateDeviceView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit UpdateDeviceView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~UpdateDeviceView();

    /**
     * @brief 显示文件选择对话框
     * @param deviceType 设备类型
     * @return 选中的文件路径，如果取消则返回空字符串
     */
    QString showFileSelectDialog(DeviceType deviceType);

    /**
     * @brief 显示确认对话框
     * @param message 确认消息
     * @return 用户选择，true表示确认，false表示取消
     */
    bool showConfirmDialog(const QString& message);

    /**
     * @brief 显示消息对话框
     * @param title 标题
     * @param message 消息内容
     * @param isError 是否为错误消息
     */
    void showMessageDialog(const QString& title, const QString& message, bool isError = false);

signals:
    /**
     * @brief SOC文件选择按钮点击信号
     */
    void socFileSelectClicked();

    /**
     * @brief PHY文件选择按钮点击信号
     */
    void phyFileSelectClicked();

    /**
     * @brief SOC升级按钮点击信号
     */
    void socUpdateClicked();

    /**
     * @brief PHY升级按钮点击信号
     */
    void phyUpdateClicked();

public slots:
    /**
     * @brief 更新SOC文件路径显示
     * @param filePath 文件路径
     */
    void updateSOCFilePath(const QString& filePath);

    /**
     * @brief 更新PHY文件路径显示
     * @param filePath 文件路径
     */
    void updatePHYFilePath(const QString& filePath);

    /**
     * @brief 更新SOC进度条
     * @param progress 进度值(0-100)
     */
    void updateSOCProgress(int progress);

    /**
     * @brief 更新PHY进度条
     * @param progress 进度值(0-100)
     */
    void updatePHYProgress(int progress);

    /**
     * @brief 更新状态提示
     * @param message 状态消息
     */
    void updateStatusMessage(const QString& message);

    /**
     * @brief 更新SOC升级按钮状态
     * @param isUpdating 是否正在升级
     */
    void updateSOCButtonState(bool isUpdating);

    /**
     * @brief 更新PHY升级按钮状态
     * @param isUpdating 是否正在升级
     */
    void updatePHYButtonState(bool isUpdating);

    /**
     * @brief 更新UI状态
     * @param isUpdating 是否正在升级
     * @param currentDevice 当前升级设备类型
     */
    void updateUIState(bool isUpdating, DeviceType currentDevice);

private slots:
    /**
     * @brief SOC文件选择按钮点击处理
     */
    void onSOCFileOpenButtonClicked();

    /**
     * @brief PHY文件选择按钮点击处理
     */
    void onPHYFileOpenButtonClicked();

    /**
     * @brief SOC升级按钮点击处理
     */
    void onSOCUpdateButtonClicked();

    /**
     * @brief PHY升级按钮点击处理
     */
    void onPHYUpdateButtonClicked();

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

    /**
     * @brief 连接信号槽
     */
    void connectSignals();

private:
    Ui::UpdataDevice ui;
};