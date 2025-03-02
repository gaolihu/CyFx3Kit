// FX3ToolMainWin.h
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
#include "FX3DeviceController.h"
#include "FX3MenuController.h"
#include "FX3UILayoutManager.h"
#include "FX3ModuleManager.h"
#include "FileSaveManager.h"
#include "FileSaveController.h"
#include "ChannelSelect.h"
#include "DataAnalysis.h"
#include "UpdataDevice.h"
#include "VideoDisplay.h"

class FX3ToolMainWin : public QMainWindow {
    Q_OBJECT

public:
    FX3ToolMainWin(QWidget* parent = nullptr);
    ~FX3ToolMainWin();

protected:
    // 本地事件处理
    virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    // UI事件处理
    void slot_onStartButtonClicked();
    void slot_onStopButtonClicked();
    void slot_onResetButtonClicked();
    void slot_onSelectCommandDirectory();

    // 状态机处理
    void slot_onEnteringState(AppState state, const QString& reason);

    // 窗口和初始化
    void slot_adjustStatusBar();
    void slot_initializeUI();

    // 数据处理相关槽函数
    void slot_onDataPacketAvailable(const DataPacket& packet);

    // 文件保存相关槽函数
    void slot_onShowSaveFileBoxTriggered();
    void slot_onSaveCompleted(const QString& path, uint64_t totalBytes);
    void slot_onSaveError(const QString& error);

    // 通道选择窗口相关槽函数
    void slot_onShowChannelSelectTriggered();
    void slot_onChannelConfigChanged(const ChannelConfig& config);

    // 数据分析窗口相关槽函数
    void slot_onShowDataAnalysisTriggered();

    // 设备升级窗口相关槽函数
    void slot_onShowUpdataDeviceTriggered();

    // 视频显示窗口相关槽函数
    void slot_onShowVideoDisplayTriggered();
    void slot_onShowWaveformAnalysisTriggered();
    void slot_onVideoDisplayStatusChanged(bool isRunning);

    // 关于对话框
    void slot_showAboutDialog();

    // 菜单处理槽
    void slot_onClearLogTriggered();
    void slot_onExportDataTriggered();
    void slot_onSettingsTriggered();
    void slot_onHelpContentTriggered();

private:
    // 初始化函数
    bool initializeLogger();
    void registerDeviceNotification();
    void initializeConnections();
    void stopAndReleaseResources();
    void setupMenuBar();
    void updateMenuBarState(AppState state);
    void checkForUpdates();
    void initializeMainUI();
    QWidget* createHomeTabContent();
    void updateStatusPanel();
    void createMainToolBar();
    bool validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType);

    // 模块管理函数
    void addModuleToMainTab(QWidget* widget, const QString& tabName, int& tabIndex, const QIcon& icon = QIcon());
    void showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon = QIcon());
    void removeModuleTab(int& tabIndex);
    void showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName);

    // UI成员
    Ui::FX3ToolMainWinClass ui;
    UIStateHandler* m_uiStateHandler;

    // 设备管理器
    FX3DeviceManager* m_deviceManager;

    // 控制器类
    FX3UILayoutManager* m_layoutManager;
    FX3DeviceController* m_deviceController;
    FX3MenuController* m_menuController;
    FX3ModuleManager* m_moduleManager;
    FileSaveController* m_fileSaveController;

    // UI组件
    QTabWidget* m_mainTabWidget;
    QSplitter* m_mainSplitter;
    QSplitter* m_leftSplitter;
    QWidget* m_statusPanel;
    QToolBar* m_mainToolBar;

    // 模块标签索引
    int m_homeTabIndex;
    int m_channelTabIndex;
    int m_dataAnalysisTabIndex;
    int m_videoDisplayTabIndex;
    int m_waveformTabIndex;

    // 子窗口指针 (弱引用，不负责生命周期)
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