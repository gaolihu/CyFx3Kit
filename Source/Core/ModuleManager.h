// Source/Core/ModuleManager.h
#pragma once

#include <QObject>
#include <QVariant>
#include <map>
#include <memory>

// Forward declarations
class FX3MainView;
class ChannelSelectView;
class DataAnalysisView;
class VideoDisplayView;
class WaveformAnalysisView;
class FileSaveView;
class UpdateDeviceView;

class ChannelSelectController;
class DataAnalysisController;
class VideoDisplayController;
class WaveformAnalysisController;
class FileSaveController;
class UpdateDeviceController;

struct ChannelConfig;
struct DataPacket;

/**
 * @brief 模块管理系统
 * 负责应用程序中各个功能模块的生命周期管理
 */
class ModuleManager : public QObject {
    Q_OBJECT

public:
    enum class ModuleType {
        CHANNEL_CONFIG,
        DATA_ANALYSIS,
        VIDEO_DISPLAY,
        WAVEFORM_ANALYSIS,
        FILE_OPTIONS,
        DEVICE_UPDATE
    };

    /**
     * @brief 模块事件类型
     * 用于在模块间传递统一事件
     */
    enum class ModuleEvent {
        DEVICE_CONNECTED,
        DEVICE_DISCONNECTED,
        TRANSFER_STARTED,
        TRANSFER_STOPPED,
        CONFIG_CHANGED,
        DATA_AVAILABLE,
        APP_CLOSING
    };

    ModuleManager(FX3MainView* mainView);
    ~ModuleManager();

    /**
     * @brief 初始化模块管理器
     */
    bool initialize();

    /**
     * @brief 准备关闭模块管理器
     */
    void prepareForShutdown();

    /**
     * @brief 显示指定模块
     * @param type 模块类型
     * @return 是否成功显示
     */
    bool showModule(ModuleType type);

    /**
     * @brief 如果模块未显示，则显示模块
     * @param type 模块类型
     * @return 是否成功显示
     */
    bool showModuleIfNotVisible(ModuleType type);

    /**
     * @brief 关闭指定模块
     * @param type 模块类型
     */
    void closeModule(ModuleType type);

    /**
     * @brief 关闭所有模块
     */
    void closeAllModules();

    /**
     * @brief 获取模块是否可见
     * @param type 模块类型
     * @return 模块是否可见
     */
    bool isModuleVisible(ModuleType type) const;

    /**
     * @brief 获取模块是否已初始化
     * @param type 模块类型
     * @return 模块是否已初始化
     */
    bool isModuleInitialized(ModuleType type) const;

    /**
     * @brief 检查是否有任何模块处于活动状态
     * @return 是否有活动模块
     */
    bool isAnyModuleActive() const;

    /**
     * @brief 获取模块类型的名称
     * @param type 模块类型
     * @return 模块类型的显示名称
     */
    static QString getModuleTypeName(ModuleType type);

    /**
     * @brief 通知所有模块特定事件
     * @param event 事件类型
     * @param data 事件数据（可选）
     */
    void notifyAllModules(ModuleEvent event, const QVariant& data = QVariant());

    /**
     * @brief 通知模块设备连接状态
     * @param connected 设备是否已连接
     */
    void notifyDeviceConnection(bool connected);

    /**
     * @brief 通知模块传输状态
     * @param transferring 是否正在传输
     */
    void notifyTransferState(bool transferring);

    /**
     * @brief 处理模块标签页关闭
     * @param index 标签页索引
     */
    void handleModuleTabClosed(int index);

    /**
     * @brief 处理数据包
     * @param packet 数据包
     */
    void processDataPacket(const std::vector<DataPacket>& packets);

signals:
    /**
     * @brief 通道配置变更信号
     * @param config 新的通道配置
     */
    void signal_channelConfigChanged(const ChannelConfig& config);

    /**
     * @brief 模块可见性变更信号
     * @param type 模块类型
     * @param visible 是否可见
     */
    void signal_moduleVisibilityChanged(ModuleType type, bool visible);

    /**
     * @brief 模块事件信号
     * @param event 事件类型
     * @param data 事件数据
     */
    void signal_moduleEvent(ModuleEvent event, const QVariant& data);

private:
    /**
     * @brief 创建通道配置模块
     * @return 是否成功创建
     */
    bool createChannelConfigModule();

    /**
     * @brief 创建数据分析模块
     * @return 是否成功创建
     */
    bool createDataAnalysisModule();

    /**
     * @brief 创建视频显示模块
     * @return 是否成功创建
     */
    bool createVideoDisplayModule();

    /**
     * @brief 创建波形分析模块
     * @return 是否成功创建
     */
    bool createWaveformAnalysisModule();

    /**
     * @brief 创建文件保存模块
     * @return 是否成功创建
     */
    bool createFileSaveModule();

    /**
     * @brief 创建设备更新模块
     * @return 是否成功创建
     */
    bool createUpdateDeviceModule();

    /**
     * @brief 更新标签索引与模块类型的映射
     * @param index 标签索引
     * @param type 模块类型
     */
    void updateTabIndexMapping(int index, ModuleType type);

    /**
     * @brief 从映射中移除标签索引
     * @param index 标签索引
     */
    void removeTabIndexMapping(int index);

    /**
     * @brief 根据标签索引查找对应的模块类型
     * @param index 标签索引
     * @return 找到的模块类型，如果未找到返回指定的默认值
     */
    ModuleType findModuleTypeByTabIndex(int index) const;

    /**
     * @brief 更新关闭后TAB索引
     * @param closedIndex 关闭的标签索引
     */
    void updateTabIndicesAfterClose(int closedIndex);

private:
    FX3MainView* m_mainView;
    bool m_shuttingDown{ false };

    // Module views
    std::unique_ptr<ChannelSelectView> m_channelConfigView;
    std::unique_ptr<DataAnalysisView> m_dataAnalysisView;
    std::unique_ptr<VideoDisplayView> m_videoDisplayView;
    std::unique_ptr<WaveformAnalysisView> m_waveformAnalysisView;
    std::unique_ptr<FileSaveView> m_fileSaveView;
    std::unique_ptr<UpdateDeviceView> m_UpdateDeviceView;

    // Module controllers
    std::shared_ptr<ChannelSelectController> m_channelConfigController;
    std::shared_ptr<DataAnalysisController> m_dataAnalysisController;
    std::shared_ptr<VideoDisplayController> m_videoDisplayController;
    std::shared_ptr<WaveformAnalysisController> m_waveformAnalysisController;
    std::shared_ptr<FileSaveController> m_fileSaveController;
    std::shared_ptr<UpdateDeviceController> m_UpdateDeviceController;

    // Module visibility and initialization state
    std::map<ModuleType, bool> m_moduleVisibility;
    std::map<ModuleType, bool> m_moduleInitialized;

    // 标签索引与模块类型映射表
    std::map<int, ModuleType> m_tabIndexToModule;

    // Tab indices
    int m_channelConfigTabIndex{ -1 };
    int m_dataAnalysisTabIndex{ -1 };
    int m_videoDisplayTabIndex{ -1 };
    int m_waveformAnalysisTabIndex{ -1 };
    int m_fileSaveTabIndex{ -1 };
    int m_UpdateDeviceTabIndex{ -1 };
};