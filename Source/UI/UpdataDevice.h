#pragma once

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include "ui_UpdataDevice.h"

class UpdataDevice : public QWidget
{
    Q_OBJECT

public:
    explicit UpdataDevice(QWidget* parent = nullptr);
    ~UpdataDevice();

signals:
    // 升级完成信号
    void updateCompleted(bool success, const QString& message);

private slots:
    // SOC文件选择按钮点击事件
    void onSOCFileOpenButtonClicked();

    // ISO文件选择按钮点击事件
    void onISOFileOpenButtonClicked();

    // SOC升级按钮点击事件
    void onSOCUpdateButtonClicked();

    // PHY升级按钮点击事件
    void onPHYUpdateButtonClicked();

    // 升级进度更新
    void onUpdateProgressChanged(int progress);

    // 升级状态更新
    void onUpdateStatusChanged(bool success, const QString& message);

private:
    Ui::UpdataDeviceClass ui;

    // 选中的SOC文件路径
    QString m_socFilePath;

    // 选中的ISO文件路径
    QString m_isoFilePath;

    // 是否正在升级
    bool m_isUpdating;

    // 初始化UI
    void initializeUI();

    // 连接信号槽
    void connectSignals();

    // 更新UI状态
    void updateUIState();

    // 验证文件
    bool validateFile(const QString& filePath, const QString& fileType);
};