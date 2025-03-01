#pragma once

#include <QtWidgets/QMainWindow>
#include <QThread>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QCloseEvent>
#include <atomic>

#include "ui_FX3ToolMainWin.h"
#include "FX3DeviceManager.h"
#include "UIStateHandler.h"
#include "AppStateMachine.h"

// FX3测试工具主窗口类 - 重构版
class FX3ToolMainWin : public QMainWindow {
    Q_OBJECT

public:
    FX3ToolMainWin(QWidget* parent = nullptr);
    ~FX3ToolMainWin();

protected:
    // 添加本地事件处理器用于USB设备检测
    virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // UI事件处理
    void onStartButtonClicked();
    void onStopButtonClicked();
    void onResetButtonClicked();
    void onSelectCommandDirectory();

    // 状态机事件处理
    void onEnteringState(AppState state, const QString& reason);

    // 窗口和UI事件
    void adjustStatusBar();
    void initializeUI();

private:
    // 初始化和加载
    bool initializeLogger();
    void registerDeviceNotification();
    void initializeConnections();
    void stopAndReleaseResources();

    // 传输控制
    bool validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType);

    // UI对象
    Ui::FX3ToolMainWinClass ui;
    UIStateHandler* m_uiStateHandler;

    // 设备管理器
    FX3DeviceManager* m_deviceManager;

    // 状态标志
    std::atomic<bool> m_isClosing{ false };
    static std::atomic<bool> s_resourcesReleased;
    bool m_loggerInitialized{ false };
};