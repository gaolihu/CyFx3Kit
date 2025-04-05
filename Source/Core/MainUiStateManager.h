// Source/Core/MainUiStateManager.h
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QTime>
#include <QElapsedTimer>
#include <QTabWidget>
#include <QIcon>
#include "ui_FX3ToolMainWin.h"
#include "AppStateMachine.h"
#include "DeviceState.h"

/**
 * @brief 增强的UI状态处理器类，负责集中管理UI状态
 *
 * 此类根据应用程序状态更新UI元素，处理状态转换和显示反馈。
 * 管理状态栏、快捷按钮和信息显示。
 */
class MainUiStateManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param ui UI对象引用
     * @param parent 父对象
     */
    explicit MainUiStateManager(Ui::FX3ToolMainWin& ui, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MainUiStateManager();

    //--- Tab页管理相关方法 ---//

    /**
     * @brief 初始化Tab管理
     * @param mainTabWidget 主Tab控件指针
     * @return 初始化是否成功
     */
    bool initializeTabManagement(QTabWidget* mainTabWidget);

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

    /**
     * @brief 获取主页标签索引
     * @return 主页标签索引
     */
    int getHomeTabIndex() const { return m_homeTabIndex; }

    //--- 状态栏管理 ---//

    /**
     * @brief 更新传输统计信息
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率 (bytes/sec)
     * @param errorCount 传输时间
     */
    void updateTransferStats(uint64_t bytesTransferred, double transferRate, uint64_t elapseMs);

    /**
     * @brief 更新USB速度显示
     * @param speedDesc USB速度描述文本
     * @param isUSB3 是否是USB3.0
     * @param isUSB3 是否已连接设备
     */
    void updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3, bool isConnected);

    /**
     * @brief 更新传输时间显示
     */
    void updateTransferTimeDisplay();

    /**
     * @brief 重置传输统计显示
     */
    void resetTransferStatsDisplay();

    /**
     * @brief 开始传输时间计时
     */
    void startTransferTimer();

    /**
     * @brief 停止传输时间计时
     */
    void stopTransferTimer();

    //--- 快捷按钮管理 ---//

    /**
     * @brief 根据应用状态更新按钮启用状态
     * @param state 应用状态
     */
    void updateButtonStates(AppState state);

    /**
     * @brief 根据传输状态更新快捷按钮显示
     * @param isTransferring 是否正在传输
     */
    void updateQuickButtonsForTransfer(bool isTransferring);

    //--- 设备信息显示管理 ---//

    /**
     * @brief 更新设备信息显示
     * @param deviceName 设备名称
     * @param firmwareVersion 固件版本
     * @param serialNumber 序列号
     */
    void updateDeviceInfoDisplay(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber);

    /**
     * @brief 更新设备参数显示
     * @param width 图像宽度
     * @param height 图像高度
     * @param captureType 捕获类型
     */
    void updateDeviceParameters(uint16_t width, uint16_t height, uint8_t captureType);

    /**
     * @brief 更新传输状态显示
     * @param isTransferring 是否正在传输
     * @param statusText 状态文本描述
     */
    void updateTransferStatus(bool isTransferring, const QString& statusText);

    /**
     * @brief 更新按钮状态
     * @param enableStart 是否启用开始按钮
     * @param enableStop 是否启用停止按钮
     * @param enableReset 是否启用重置按钮
     */
    void updateButtonStates(bool enableStart, bool enableStop, bool enableReset);

    /**
     * @brief 更新设备状态显示
     * @param state 设备状态
     */
    void updateDeviceState(DeviceState state);

    /**
     * @brief 更新视频参数显示
     * @param width 视频宽度
     * @param height 视频高度
     * @param format 视频格式索引
     */
    void setVideoParamsDisplay(uint16_t width, uint16_t height, int format);

    /**
     * @brief 更新命令目录显示
     * @param dir 命令目录路径
     */
    void setCommandDirDisplay(const QString& dir);

    //--- 错误和消息显示 ---//

    /**
     * @brief 显示错误消息
     * @param title 标题
     * @param message 错误消息
     */
    void showErrorMessage(const QString& title, const QString& message);

    /**
     * @brief 显示警告消息
     * @param title 标题
     * @param message 信息消息
     */
    void showWarnMessage(const QString& title, const QString& message);

    /**
     * @brief 显示信息消息
     * @param title 标题
     * @param message 信息消息
     */
    void showInfoMessage(const QString& title, const QString& message);

    /**
     * @brief 显示关于对话框
     * @param title 标题
     * @param message 信息消息
     */
    void showAboutMessage(const QString& title, const QString& message);

    /**
     * @brief 清除日志框
     */
    void clearLogbox();

    /**
     * @brief 准备关闭
     * 在应用程序关闭前调用，防止后续UI访问
     */
    void prepareForClose();

    /**
     * @brief 初始化信号连接
     * 建立UI元素和控制器之间的信号连接
     * @param parentWidget 父窗口用于连接信号
     * @return 初始化是否成功
     */
    bool initializeSignalConnections(QWidget* parentWidget);

    /**
     * @brief 设置标签页的样式
     * @param tabWidget 标签页控件
     * @param index 标签页索引
     * @param tabName 标签页名称
     */
    void applyTabStyle(QTabWidget* tabWidget, int index, const QString& tabName);

    /**
     * @brief 初始化标签栏全局样式
     * @param tabWidget 标签页控件
     */
    void initializeTabBarStyle(QTabWidget* tabWidget);

public slots:
    /**
     * @brief 处理应用状态变化
     * @param newState 新状态
     * @param oldState 旧状态
     * @param reason 变更原因
     */
    void slot_MainUI_STM_onStateChanged(AppState newState, AppState oldState, const QString& reason);

    /**
     * @brief 处理传输状态变更
     * @param transferring 是否传输中
     */
    void slot_MainUI_STM_onTransferStateChanged(bool transferring);

    /**
     * @brief 处理Tab关闭请求
     * @param index 要关闭的Tab索引
     */
    void slot_MainUI_STM_onTabCloseRequested(int index);

    /**
    * @brief 响应设备状态变化
    * @param state 新的设备状态
    */
    void slot_MainUI_STM_onDeviceStateChanged(DeviceState state);

    /**
     * @brief 初始化UI状态
     * 设置所有UI元素的初始状态
     */
    void slot_MainUI_STM_initializeUIState();

    /**
     * @brief 初始化视频参数
     * 设置默认的视频参数(1920x1080)
     */
    void slot_MainUI_STM_initializeVideoParameters();

signals:
    /**
     * @brief 开始按钮点击信号
     */
    void signal_MainUI_STM_startButtonClicked();

    /**
     * @brief 停止按钮点击信号
     */
    void signal_MainUI_STM_stopButtonClicked();

    /**
     * @brief 重置按钮点击信号
     */
    void signal_MainUI_STM_resetButtonClicked();

    /**
     * @brief 通道配置按钮点击信号
     */
    void signal_MainUI_STM_channelConfigButtonClicked();

    /**
     * @brief 数据分析按钮点击信号
     */
    void signal_MainUI_STM_dataAnalysisButtonClicked();

    /**
     * @brief 视频显示按钮点击信号
     */
    void signal_MainUI_STM_videoDisplayButtonClicked();

    /**
     * @brief 波形分析按钮点击信号
     */
    void signal_MainUI_STM_waveformAnalysisButtonClicked();

    /**
     * @brief 保存文件按钮点击信号
     */
    void signal_MainUI_STM_saveFileButtonClicked();

    /**
     * @brief 数据导出按钮点击信号
     */
    void signal_MainUI_STM_exportDataButtonClicked();

    /**
     * @brief 文件选项按钮点击信号
     */
    void signal_MainUI_STM_fileOptionsButtonClicked();

    /**
     * @brief 设置菜单触发信号
     */
    void signal_MainUI_STM_settingsTriggered();

    /**
     * @brief 选择命令目录按钮点击信号
     */
    void signal_MainUI_STM_selectCommandDirClicked();

    /**
     * @brief 设备升级按钮点击信号
     */
    void signal_MainUI_STM_updateDeviceButtonClicked();

    /**
     * @brief 模块选项卡关闭信号
     * @param index 选项卡索引
     */
    void signal_MainUI_STM_moduleTabClosed(int index);

private slots:
    /**
     * @brief 定时更新传输时间显示
     */
    void slot_MainUI_STM_updateTransferTime();

private:
    /**
     * @brief 更新状态标签
     * @param state 应用状态
     */
    void updateStatusLabels(AppState state);

    /**
     * @brief 更新状态文本
     * @param statusText USB状态文本
     * @param transferStatusText 传输状态文本
     */
    void updateStatusLabels(const QString& statusText, const QString& transferStatusText);

    /**
     * @brief 获取设备状态对应的文本描述
     * @param state 设备状态
     * @return 设备状态文本
     */
    QString getDeviceStateText(DeviceState state) const;

    /**
     * @brief 更新UI控件启用/禁用状态
     * @param enableStart 是否启用开始按钮
     * @param enableStop 是否启用停止按钮
     * @param enableReset 是否启用重置按钮
     */
    void updateButtons(bool enableStart, bool enableStop, bool enableReset);

    /**
     * @brief 检查UI是否有效
     * @return 如果UI有效返回true，否则返回false
     */
    bool validUI() const;

    /**
     * @brief 格式化字节大小为可读格式
     * @param bytes 字节数量
     * @return 格式化后的字符串 (如 "1.25 MB")
     */
    QString formatByteSize(uint64_t bytes) const;

    /**
     * @brief 格式化时间为可读格式
     * @param milliseconds 毫秒数
     * @return 格式化后的字符串 (如 "01:23:45.032 ms")
     */
    QString formatTime(qint64 milliseconds) const;

private:
    Ui::FX3ToolMainWin& m_ui;       ///< UI引用
    bool m_validUI;                 ///< UI有效性标志
    bool m_isTransferring;          ///< 传输状态
    bool m_isDeviceConnected;       ///< 设备连接状态

    QTimer m_transferTimer;         ///< 传输时间更新定时器
    QElapsedTimer m_elapsedTimer;   ///< 传输时间计时器
    qint64 m_totalElapsedTime;      ///< 总传输时间(毫秒)

    uint64_t m_bytesTransferred;    ///< 已传输字节
    double m_transferRate;          ///< 传输速率
    uint32_t m_errorCount;          ///< 错误计数

    // Tab管理相关
    QTabWidget* m_mainTabWidget;    ///< 主Tab控件指针
    int m_homeTabIndex;             ///< 主页标签索引
    int m_channelTabIndex;          ///< 通道配置标签索引
    int m_dataAnalysisTabIndex;     ///< 数据分析标签索引
    int m_videoDisplayTabIndex;     ///< 视频显示标签索引
    int m_waveformTabIndex;         ///< 波形分析标签索引
    int m_fileOperationTabIndex;    ///< 文件操作标签索引
};