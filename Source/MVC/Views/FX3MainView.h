// MVC/Views/FX3MainView.h
#pragma once

#include <QtWidgets/QMainWindow>
#include <QCloseEvent>
#include <QTabWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>
#include <QPushButton>

#include "ui_FX3ToolMainWin.h"
#include "MainUiStateManager.h"

// 前向声明
class FX3MainController;

/**
 * @brief FX3主视图类 - 负责UI显示和用户交互
 *
 * 作为MVC架构中的视图层，负责界面呈现和捕获用户操作
 */
class FX3MainView : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    FX3MainView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FX3MainView();

    /**
     * @brief 获取UI对象
     * @return UI对象
     */
    Ui::FX3ToolMainWin* getUi() { return &ui; }

    /**
     * @brief 获取窗口句柄
     * @return 窗口句柄
     */
    HWND getWindowHandle() const { return (HWND)this->winId(); }

    /**
     * @brief 获取UI状态管理器
     * @return UI状态管理器
     */
    MainUiStateManager* getUiStateManager() const { return m_uiStateManager.get(); }

    //--- UI显示方法 ---//

    /**
     * @brief 显示错误消息
     * @param title 标题
     * @param message 错误消息
     */
    void showErrorMessage(const QString& title, const QString& message);

    /**
     * @brief 显示警告消息
     * @param title 标题
     * @param message 错误消息
     */
    void showWarningMessage(const QString& title, const QString& message);

    /**
     * @brief 显示信息消息
     * @param title 标题
     * @param message 信息消息
     */
    void showInfoMessage(const QString& title, const QString& message);

    /**
     * @brief 显示关于对话框
     */
    void showAboutDialog();

    /**
     * @brief 清除日志
     */
    void clearLog();

    /**
     * @brief 更新状态栏
     * @param message 状态消息
     * @param timeout 超时时间（毫秒）
     */
    void updateStatusBar(const QString& message, int timeout = 0);

    /**
     * @brief 更新窗口标题
     * @param deviceInfo 设备信息
     */
    void updateWindowTitle(const QString& toolInfo);

    /**
     * @brief 更新传输统计显示
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率 (bytes/sec)
     * @param errorCount 错误计数
     */
    void updateTransferStatsDisplay(uint64_t bytesTransferred, double transferRate, uint32_t errorCount);

    /**
     * @brief 更新USB速度显示
     * @param speed USB速度
     */
    void updateUsbSpeedDisplay(const QString& speed);

    /**
     * @brief 设置命令文件目录显示
     * @param dir 目录路径
     */
    void setCommandDirDisplay(const QString& dir);

    /**
     * @brief 更新传输时间显示
     * @param timeMs 时间ms
     */
    void updateTransferTimeDisplay(const QString& timeMs);

    /**
     * @brief 设置视频参数显示
     * @param width 视频宽度
     * @param height 视频高度
     * @param format 视频格式索引
     */
    void setVideoParamsDisplay(uint16_t width, uint16_t height, int format);

    /**
     * @brief 更新设备信息显示
     * @param deviceName 设备名称
     * @param firmwareVersion 固件版本
     * @param serialNumber 序列号
     */
    void updateDeviceInfoDisplay(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber);

    /**
     * @brief 重置传输统计显示
     */
    void resetTransferStatsDisplay();

    //--- 模块管理方法 ---//

    /**
     * @brief 添加模块到主标签页
     * @param widget 模块窗口指针
     * @param tabName 标签名称
     * @param tabIndex 输出参数，标签索引
     * @param icon 标签图标（可选）
     */
    void addModuleToMainTab(QWidget* widget, const QString& tabName, int& tabIndex, const QIcon& icon = QIcon());

    /**
     * @brief 显示模块标签页
     * @param tabIndex 标签索引
     * @param widget 模块窗口指针
     * @param tabName 标签名称
     * @param icon 标签图标（可选）
     */
    void showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon = QIcon());

    /**
     * @brief 移除模块标签页
     * @param tabIndex 标签索引
     */
    void removeModuleTab(int& tabIndex);

protected:
    /**
     * @brief 本地事件处理
     * @param eventType 事件类型
     * @param message 消息
     * @param result 结果
     * @return 事件是否已处理
     */
    virtual bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

    /**
     * @brief 关闭事件处理
     * @param event 关闭事件
     */
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief 调整大小事件处理
     * @param event 调整大小事件
     */
    void resizeEvent(QResizeEvent* event) override;

signals:
    // 主视图发出的信号，由控制器响应

    /**
     * @brief 开始按钮点击信号
     */
    void signal_startButtonClicked();

    /**
     * @brief 停止按钮点击信号
     */
    void signal_stopButtonClicked();

    /**
     * @brief 重置按钮点击信号
     */
    void signal_resetButtonClicked();

    /**
     * @brief 通道配置按钮点击信号
     */
    void signal_channelConfigButtonClicked();

    /**
     * @brief 数据分析按钮点击信号
     */
    void signal_dataAnalysisButtonClicked();

    /**
     * @brief 视频显示按钮点击信号
     */
    void signal_videoDisplayButtonClicked();

    /**
     * @brief 波形分析按钮点击信号
     */
    void signal_waveformAnalysisButtonClicked();

    /**
     * @brief 保存文件按钮点击信号
     */
    void signal_saveFileButtonClicked();

    /**
     * @brief 导出数据按钮点击信号
     */
    void signal_exportDataButtonClicked();

    /**
     * @brief 设置文件选项按钮点击信号
     */
    void signal_FileOptionsButtonClicked();

    /**
     * @brief 设置菜单触发信号
     */
    void signal_settingsTriggered();

    /**
     * @brief 清除日志按钮点击信号
     */
    void signal_clearLogTriggered();

    /**
     * @brief 帮助内容菜单触发信号
     */
    void signal_helpContentTriggered();

    /**
     * @brief 关于对话框菜单触发信号
     */
    void signal_aboutDialogTriggered();

    /**
     * @brief 选择命令目录按钮点击信号
     */
    void signal_selectCommandDirClicked();

    /**
     * @brief 设备升级按钮点击信号
     */
    void signal_updateDeviceButtonClicked();

    /**
     * @brief 模块选项卡关闭信号
     * @param index 选项卡索引
     */
    void signal_moduleTabClosed(int index);

private slots:
    void slot_onStartButtonClicked();
    void slot_onStopButtonClicked();
    void slot_onResetButtonClicked();
    void slot_onSelectCommandDirClicked();
    void slot_onChannelConfigButtonClicked();
    void slot_onDataAnalysisButtonClicked();
    void slot_onVideoDisplayButtonClicked();
    void slot_onWaveformAnalysisButtonClicked();
    void slot_onSaveFileButtonClicked();
    void slot_onExportDataButtonClicked();
    void slot_onFileOptionsButtonClicked();
    void slot_onSettingsTriggered();
    void slot_onClearLogTriggered();
    void slot_onHelpContentTriggered();
    void slot_onAboutDialogTriggered();
    void slot_onUpdateDeviceButtonClicked();
    void slot_adjustStatusBar();
    void slot_onTabCloseRequested(int index);
    void slot_onShowChannelSelectTriggered();
    void slot_onShowUpdataDeviceTriggered();
    void slot_onShowDataAnalysisTriggered();
    void slot_onShowVideoDisplayTriggered();
    void slot_onShowWaveformAnalysisTriggered();
    void slot_onShowSaveFileBoxTriggered();
    void slot_onSaveButtonClicked();
    void slot_onSelectCommandDirectory();
    void slot_showAboutDialog();
    void slot_onExportDataTriggered();
    void slot_onFileOptionsTriggered();

    // 连接 UI 状态管理器信号
    void slot_forwardStartButtonSignal();
    void slot_forwardStopButtonSignal();
    void slot_forwardResetButtonSignal();
    void slot_forwardChannelConfigButtonSignal();
    void slot_forwardDataAnalysisButtonSignal();
    void slot_forwardVideoDisplayButtonSignal();
    void slot_forwardWaveformAnalysisButtonSignal();
    void slot_forwardSaveFileButtonSignal();
    void slot_forwardExportDataButtonSignal();
    void slot_forwardFileOptionsButtonSignal();
    void slot_forwardSettingsTriggeredSignal();
    void slot_forwardSelectCommandDirSignal();
    void slot_forwardUpdateDeviceButtonSignal();

private:
    /**
     * @brief 初始化日志系统
     * @return 初始化是否成功
     */
    bool initializeLogger();

    /**
     * @brief 初始化信号连接
     */
    void initializeSignalConnections();

    /**
     * @brief 初始化UI状态管理器
     * @return 初始化是否成功
     */
    bool initializeUiStateManager();

    /**
     * @brief 设置模块按钮与信号的映射
     */
    void setupModuleButtonSignalMapping();

private:
    Ui::FX3ToolMainWin ui;                        ///< UI对象
    std::unique_ptr<FX3MainController> m_controller;///< 控制器对象
    std::unique_ptr<MainUiStateManager> m_uiStateManager; ///< UI状态管理器

    // UI组件
    QTabWidget* m_mainTabWidget;                  ///< 主标签页控件
    QSplitter* m_mainSplitter;                    ///< 主分割器
    QSplitter* m_leftSplitter;                    ///< 左侧分割器
    QWidget* m_statusPanel;                       ///< 状态面板
    QToolBar* m_mainToolBar;                      ///< 主工具栏

    // 模块标签索引
    int m_homeTabIndex;                           ///< 主页标签索引
    int m_channelTabIndex;                        ///< 通道配置标签索引
    int m_dataAnalysisTabIndex;                   ///< 数据分析标签索引
    int m_videoDisplayTabIndex;                   ///< 视频显示标签索引
    int m_waveformTabIndex;                       ///< 波形分析标签索引
    int m_fileSaveTabIndex;                       ///< 文件保存标签索引

    // 状态标志
    bool m_loggerInitialized;                     ///< 日志系统是否已初始化
};