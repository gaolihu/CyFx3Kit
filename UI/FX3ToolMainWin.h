#pragma once

#include <QtWidgets/QMainWindow>
#include <QThread>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <atomic>

#include "ui_FX3ToolMainWin.h"
#include "FX3DeviceManager.h"
#include "UIStateHandler.h"
#include "AppStateMachine.h"
#include "FileSaveManager.h"
#include "FileSavePanel.h"
#include "SaveFileBox.h"
#include "ChannelSelect.h"
#include "DataAnalysis.h"
#include "UpdataDevice.h"
#include "VideoDisplay.h"

// 前置声明其他窗口类
class ChannelSelect;
class DataAnalysis;
class UpdataDevice;
class VideoDisplay;

class FX3ToolMainWin : public QMainWindow {
    Q_OBJECT

public:
    FX3ToolMainWin(QWidget* parent = nullptr);
    ~FX3ToolMainWin();

protected:
    // 本地事件处理
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;
#else
    virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // UI事件处理
    void slot_onStartButtonClicked();
    void slot_onStopButtonClicked();
    void slot_onResetButtonClicked();
    void slot_onSaveButtonClicked();

    // 命令目录选择
    void slot_onSelectCommandDirectory();

    // 状态机处理
    void slot_onEnteringState(AppState state, const QString& reason);

    // 窗口和初始化
    void slot_adjustStatusBar();
    void slot_initializeUI();

    // 数据处理相关槽函数
    void slot_onDataPacketAvailable(const DataPacket& packet);
    void slot_onSaveCompleted(const QString& path, uint64_t totalBytes);
    void slot_onSaveError(const QString& error);

    // 文件保存对话框相关槽函数
    void slot_onShowSaveFileBoxTriggered();
    void slot_onSaveFileBoxCompleted(const QString& path, uint64_t totalBytes);
    void slot_onSaveFileBoxError(const QString& error);

    // 通道选择窗口相关槽函数
    void slot_onShowChannelSelectTriggered();
    void slot_onChannelConfigChanged(const ChannelConfig& config);

    // 数据分析窗口相关槽函数
    void slot_onShowDataAnalysisTriggered();

    // 设备升级窗口相关槽函数
    void slot_onShowUpdataDeviceTriggered();

    // 视频显示窗口相关槽函数
    void slot_onShowVideoDisplayTriggered();

    // 关于对话框
    void slot_showAboutDialog();

private:
    // 初始化函数
    bool initializeLogger();
    void registerDeviceNotification();
    void initializeConnections();
    void stopAndReleaseResources();
    bool initializeFileSaveComponents();
    void setupMenuBar();

    // 传输参数验证
    bool validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType);

    // UI成员
    Ui::FX3ToolMainWinClass ui;
    UIStateHandler* m_uiStateHandler;

    // 设备管理器
    FX3DeviceManager* m_deviceManager;

    // 文件保存面板
    FileSavePanel* m_fileSavePanel;

    // 各子窗口
    SaveFileBox* m_saveFileBox;
    ChannelSelect* m_channelSelectWidget;
    DataAnalysis* m_dataAnalysisWidget;
    UpdataDevice* m_updataDeviceWidget;
    VideoDisplay* m_videoDisplayWidget;

    // 标志
    std::atomic<bool> m_isClosing{ false };
    bool m_loggerInitialized{ false };
    static std::atomic<bool> s_resourcesReleased;
};