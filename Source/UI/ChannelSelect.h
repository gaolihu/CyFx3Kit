// ChannelSelect.h
#ifndef CHANNELSELECT_H
#define CHANNELSELECT_H

#include <QWidget>
#include <QMessageBox>
#include <QMap>
#include "ui_ChannelSelect.h"

// 通道配置结构体
struct ChannelConfig {
    int videoWidth = 1920;
    int videoHeight = 1080;
    int captureType = 0;
    bool channelEnabled[4] = { true, false, false, false };
    bool pnSwapped[4] = { false, false, false, false };
    QMap<QString, QVariant> additionalParams;
};

class ChannelSelect : public QWidget
{
    Q_OBJECT

public:
    explicit ChannelSelect(QWidget* parent = nullptr);
    ~ChannelSelect();

    // 获取当前配置
    ChannelConfig getCurrentConfig();

    // 设置UI以匹配配置
    void setConfigToUI(const ChannelConfig& config);

signals:
    // 配置变更信号
    void channelConfigChanged(const ChannelConfig& config);

private slots:
    // 按钮点击事件
    void onSaveButtonClicked();
    void onCancelButtonClicked();

    // 通道和PN状态变更事件
    void onChannelEnableChanged(bool checked);
    void onPNStatusChanged(bool checked);
    void onCaptureTypeChanged(int index);

private:
    Ui::ChannelSelectClass ui;  // UI对象

    // 初始化UI
    void initializeUI();

    // 连接信号与槽
    void connectSignals();

    // 更新UI状态
    void updateUIState();

    // 验证参数
    bool validateParameters();
};

#endif // CHANNELSELECT_H