// MVC/Controllers/FX3MainController.h
#pragma once

#include <QObject>
#include <memory>
#include <atomic>
#include "AppStateMachine.h"
#include "ModuleManager.h"
#include "DataPacket.h"

// 前向声明，减少头文件依赖
class FX3MainView;
class FX3MainModel;
class MenuController;
class FileOperationController;
class ModuleManager;
class DeviceView;
class DeviceController;

struct ChannelConfig;
struct DataPacket;

/**
 * @brief FX3主控制器类 - 应用程序的核心控制器
 *
 * 负责协调其他控制器、处理用户操作请求，管理应用程序生命周期
 */
class FX3MainController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param mainView 主视图
     */
    explicit FX3MainController(FX3MainView* mainView);

    /**
     * @brief 析构函数
     */
    ~FX3MainController();

    /**
     * @brief 初始化控制器
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 关闭控制器
     */
    void shutdown();

    /**
     * @brief 处理设备到达事件
     */
    void handleDeviceArrival();

    /**
     * @brief 处理设备移除事件
     */
    void handleDeviceRemoval();

    /**
     * @brief 处理应用程序关闭事件
     */
    void handleClose();

public slots:
    // 用户操作处理槽函数

    /**
     * @brief 处理开始传输请求
     */
    void slot_FX3Main_C_handleStartTransfer();

    /**
     * @brief 处理停止传输请求
     */
    void slot_FX3Main_C_handleStopTransfer();

    /**
     * @brief 处理重置设备请求
     */
    void slot_FX3Main_C_handleResetDevice();

    /**
     * @brief 处理通道配置请求
     */
    void slot_FX3Main_C_handleChannelConfig();

    /**
     * @brief 处理数据分析请求
     */
    void slot_FX3Main_C_handleDataAnalysis();

    /**
     * @brief 处理视频显示请求
     */
    void slot_FX3Main_C_handleVideoDisplay();

    /**
     * @brief 处理波形分析请求
     */
    void slot_FX3Main_C_handleWaveformAnalysis();

    /**
     * @brief 处理文件保存请求
     */
    void slot_FX3Main_C_handleFileOperation();

    /**
     * @brief 处理数据导出请求
     */
    void slot_FX3Main_C_handleDataExport();

    /**
     * @brief 处理文件相关设置
     */
    void slot_FX3Main_C_handleFileOption();

    /**
     * @brief 处理设置请求
     */
    void slot_FX3Main_C_handleSettings();

    /**
     * @brief 处理清除日志请求
     */
    void slot_FX3Main_C_handleClearLog();

    /**
     * @brief 处理帮助内容请求
     */
    void slot_FX3Main_C_handleHelpContent();

    /**
     * @brief 处理关于对话框请求
     */
    void slot_FX3Main_C_handleAboutDialog();

    /**
     * @brief 处理命令目录选择请求
     */
    void slot_FX3Main_C_handleSelectCommandDir();

    /**
     * @brief 处理设备升级请求
     */
    void slot_FX3Main_C_handleDeviceUpdate();

    /**
     * @brief 处理模块标签页关闭请求
     * @param index 标签页索引
     */
    void slot_FX3Main_C_handleModuleTabClosed(int index);

private slots:
    // 内部槽函数用于处理各种事件和信号

    /**
     * @brief 处理通道配置变更
     * @param config 通道配置
     */
    void slot_FX3Main_C_onChannelConfigChanged(const ChannelConfig& config);

    /**
     * @brief 处理视频显示状态变更
     * @param isRunning 是否运行中
     */
    void slot_FX3Main_C_onVideoDisplayStatusChanged(bool isRunning);

    /**
     * @brief 处理文件保存完成
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void slot_FX3Main_C_onSaveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 处理文件保存错误
     * @param error 错误信息
     */
    void slot_FX3Main_C_onSaveError(const QString& error);

    /**
     * @brief 处理应用程序状态变更
     * @param state 新状态
     * @param oldState 旧状态
     * @param reason 变更原因
     */
    void slot_FX3Main_C_onAppStateChanged(AppState state, AppState oldState, const QString& reason);

    /**
     * @brief 处理传输状态变更
     * @param transferring 是否传输中
     */
    void slot_FX3Main_C_onTransferStateChanged(bool transferring);

    /**
     * @brief 处理传输统计更新
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率
     * @param errorCount 传输时间ms
     */
    void slot_FX3Main_C_onTransferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint64_t elapseMs);

    // UI事件处理槽函数
    void slot_FX3Main_C_handleStartButtonClicked();
    void slot_FX3Main_C_handleStopButtonClicked();
    void slot_FX3Main_C_handleResetButtonClicked();

    // 设备控制器事件处理槽函数
    void slot_FX3Main_C_handleTransferStateChanged(bool transferring);
    void slot_FX3Main_C_handleTransferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint32_t elapseMs);
    void slot_FX3Main_C_handleUsbSpeedUpdated(const QString& speedDesc, bool isUSB3, bool isConnected);
    void slot_FX3Main_C_handleDeviceError(const QString& title, const QString& message);
    void slot_FX3Main_C_handleDataPacketAvailable(const std::vector<DataPacket>& packets);

private:
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 初始化设备
     * @return 初始化是否成功
     */
    bool initializeDevice();

    /**
     * @brief 停止并释放资源
     */
    void stopAndReleaseResources();

    /**
     * @brief 注册设备通知
     * @return 注册是否成功
     */
    bool registerDeviceNotification();

    /**
     * @brief 验证图像参数
     * @param width 输出参数，图像宽度
     * @param height 输出参数，图像高度
     * @param format 输出参数，图像格式
     * @return 验证是否通过
     */
    bool validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& format);

    int formatToIndex(uint8_t format) const;
    void initializeConnections();
    void updateUIFromModel();

    /**
     * @brief 通用模块显示请求处理
     * @param type 要显示的模块类型
     */
    void handleModuleDisplay(ModuleManager::ModuleType type);

private:
    // 视图和模型引用（不负责生命周期）
    FX3MainView* m_mainView;                              ///< 主视图
    FX3MainModel* m_mainModel;                            ///< 主模型

    // 控制器和管理器（负责生命周期）
    std::unique_ptr<DeviceController> m_deviceController; ///< 设备控制器
    std::unique_ptr<MenuController> m_menuController;     ///< 菜单控制器

    std::unique_ptr<ModuleManager> m_moduleManager;       ///< 模块管理器
    std::unique_ptr<DeviceView> m_DeviceView;             ///< 设备视图

    // 状态标志
    std::atomic<bool> m_initialized;                      ///< 是否已初始化
};