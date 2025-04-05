// Source/Core/ModuleManager.cpp

#include <QObject>
#include "ModuleManager.h"
#include "FX3MainView.h"
#include "ChannelSelectModel.h"
#include "ChannelSelectView.h"
#include "ChannelSelectController.h"
#include "DataAnalysisView.h"
#include "DataAnalysisController.h"
#include "VideoDisplayView.h"
#include "VideoDisplayController.h"
#include "WaveformAnalysisView.h"
#include "WaveformAnalysisController.h"
#include "FileOperationView.h"
#include "FileOperationController.h"
#include "UpdateDeviceView.h"
#include "UpdateDeviceController.h"
#include "Logger.h"

ModuleManager::ModuleManager(FX3MainView* mainView)
    : m_mainView(mainView)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器已创建"));

    // 确保主视图和UI状态管理器都有效
    if (m_mainView && !m_mainView->getUiStateManager()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("UI状态管理器未初始化，模块管理可能无法正常工作"));
    }
}

ModuleManager::~ModuleManager()
{
    if (!m_shuttingDown) {
        prepareForShutdown();
    }
    LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器已销毁"));
}

bool ModuleManager::initialize()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化模块管理器"));

        // 初始化模块可见性和初始化状态
        m_moduleVisibility[ModuleType::CHANNEL_CONFIG] = false;
        m_moduleVisibility[ModuleType::DATA_ANALYSIS] = false;
        m_moduleVisibility[ModuleType::VIDEO_DISPLAY] = false;
        m_moduleVisibility[ModuleType::WAVEFORM_ANALYSIS] = false;
        m_moduleVisibility[ModuleType::FILE_OPTIONS] = false;
        m_moduleVisibility[ModuleType::DEVICE_UPDATE] = false;

        m_moduleInitialized[ModuleType::CHANNEL_CONFIG] = false;
        m_moduleInitialized[ModuleType::DATA_ANALYSIS] = false;
        m_moduleInitialized[ModuleType::VIDEO_DISPLAY] = false;
        m_moduleInitialized[ModuleType::WAVEFORM_ANALYSIS] = false;
        m_moduleInitialized[ModuleType::FILE_OPTIONS] = false;
        m_moduleInitialized[ModuleType::DEVICE_UPDATE] = false;

        // 清空标签索引映射表
        m_tabIndexToModule.clear();

        // 设置标签索引为无效值
        m_channelConfigTabIndex = -1;
        m_dataAnalysisTabIndex = -1;
        m_videoDisplayTabIndex = -1;
        m_waveformAnalysisTabIndex = -1;
        m_fileOperationTabIndex = -1;
        m_UpdateDeviceTabIndex = -1;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器初始化完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化模块管理器异常: %1").arg(e.what()));
        return false;
    }
}

void ModuleManager::prepareForShutdown()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器准备关闭"));

    m_shuttingDown = true;

    // 关闭所有模块
    closeAllModules();

    // 释放所有控制器资源
    if (m_channelConfigController) {
        m_channelConfigController.reset();
    }

    if (m_dataAnalysisController) {
        m_dataAnalysisController.reset();
    }

    if (m_videoDisplayController) {
        m_videoDisplayController.reset();
    }

    if (m_waveformAnalysisController) {
        m_waveformAnalysisController.reset();
    }

    if (m_fileOperationController) {
        m_fileOperationController.reset();
    }

    if (m_UpdateDeviceController) {
        m_UpdateDeviceController.reset();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器清理完成"));
}

bool ModuleManager::showModule(ModuleType type)
{
    if (m_shuttingDown) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示模块请求"));
        return false;
    }

    try {
        switch (type) {
        case ModuleType::CHANNEL_CONFIG:
            if (!m_moduleInitialized[type]) {
                if (!createChannelConfigModule()) {
                    return false;
                }
            }

            if (m_mainView && m_channelConfigView) {
                if (m_channelConfigTabIndex >= 0) {
                    m_mainView->showModuleTab(m_channelConfigTabIndex,
                        m_channelConfigView.get(),
                        LocalQTCompat::fromLocal8Bit("通道配置"));
                }
                else {
                    m_channelConfigTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_channelConfigView.get(),
                        LocalQTCompat::fromLocal8Bit("通道配置"),
                        m_channelConfigTabIndex);
                }

                // 更新映射表（显示已有标签页）
                updateTabIndexMapping(m_channelConfigTabIndex, type);

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        case ModuleType::DATA_ANALYSIS:
            if (!m_moduleInitialized[type]) {
                if (!createDataAnalysisModule()) {
                    return false;
                }
            }

            if (m_mainView && m_dataAnalysisView) {
                if (m_dataAnalysisTabIndex >= 0) {
                    m_mainView->showModuleTab(m_dataAnalysisTabIndex,
                        m_dataAnalysisView.get(),
                        LocalQTCompat::fromLocal8Bit("数据分析"));
                }
                else {
                    m_dataAnalysisTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_dataAnalysisView.get(),
                        LocalQTCompat::fromLocal8Bit("数据分析"),
                        m_dataAnalysisTabIndex);
                }

                // 更新映射表（显示已有标签页）
                updateTabIndexMapping(m_dataAnalysisTabIndex, type);

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        case ModuleType::VIDEO_DISPLAY:
            if (!m_moduleInitialized[type]) {
                if (!createVideoDisplayModule()) {
                    return false;
                }
            }

            if (m_mainView && m_videoDisplayView) {
                if (m_videoDisplayTabIndex >= 0) {
                    m_mainView->showModuleTab(m_videoDisplayTabIndex,
                        m_videoDisplayView.get(),
                        LocalQTCompat::fromLocal8Bit("视频显示"));
                }
                else {
                    m_videoDisplayTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_videoDisplayView.get(),
                        LocalQTCompat::fromLocal8Bit("视频显示"),
                        m_videoDisplayTabIndex);
                }

                // 更新映射表（显示已有标签页）
                updateTabIndexMapping(m_videoDisplayTabIndex, type);

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        case ModuleType::WAVEFORM_ANALYSIS:
            if (!m_moduleInitialized[type]) {
                if (!createWaveformAnalysisModule()) {
                    return false;
                }
            }

            if (m_mainView && m_waveformAnalysisView) {
                if (m_waveformAnalysisTabIndex >= 0) {
                    m_mainView->showModuleTab(m_waveformAnalysisTabIndex,
                        m_waveformAnalysisView.get(),
                        LocalQTCompat::fromLocal8Bit("波形分析"));
                }
                else {
                    m_waveformAnalysisTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_waveformAnalysisView.get(),
                        LocalQTCompat::fromLocal8Bit("波形分析"),
                        m_waveformAnalysisTabIndex);
                }

                // 更新映射表（显示已有标签页）
                updateTabIndexMapping(m_waveformAnalysisTabIndex, type);

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        case ModuleType::FILE_OPTIONS:
            if (!m_moduleInitialized[type]) {
                if (!createFileOperationModule()) {
                    return false;
                }
            }

            if (m_mainView && m_fileOperationView) {
                if (m_fileOperationTabIndex >= 0) {
                    m_mainView->showModuleTab(m_fileOperationTabIndex,
                        m_fileOperationView.get(),
                        LocalQTCompat::fromLocal8Bit("文件保存"));
                }
                else {
                    m_fileOperationTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_fileOperationView.get(),
                        LocalQTCompat::fromLocal8Bit("文件保存"),
                        m_fileOperationTabIndex);
                }

                // 更新映射表（显示已有标签页）
                updateTabIndexMapping(m_fileOperationTabIndex, type);

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        case ModuleType::DEVICE_UPDATE:
            if (!m_moduleInitialized[type]) {
                if (!createUpdateDeviceModule()) {
                    return false;
                }
            }

            if (m_mainView && m_UpdateDeviceView) {
                if (m_UpdateDeviceTabIndex >= 0) {
                    m_mainView->showModuleTab(m_UpdateDeviceTabIndex,
                        m_UpdateDeviceView.get(),
                        LocalQTCompat::fromLocal8Bit("设备更新"));
                }
                else {
                    m_UpdateDeviceTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_UpdateDeviceView.get(),
                        LocalQTCompat::fromLocal8Bit("设备更新"),
                        m_UpdateDeviceTabIndex);
                }

                // 更新映射表（显示已有标签页）
                updateTabIndexMapping(m_UpdateDeviceTabIndex, type);

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        default:
            LOG_WARN(LocalQTCompat::fromLocal8Bit("未知的模块类型: %1").arg(static_cast<int>(type)));
            return false;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("显示模块异常: %1").arg(e.what()));
        return false;
    }

    return false;
}

bool ModuleManager::showModuleIfNotVisible(ModuleType type) {
    if (isModuleVisible(type)) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("模块已可见: %1").arg(getModuleTypeName(type)));
        return true;
    }
    return showModule(type);
}

void ModuleManager::closeModule(ModuleType type)
{
    try {
        switch (type) {
        case ModuleType::CHANNEL_CONFIG:
            if (m_mainView && m_channelConfigTabIndex >= 0) {
                // 先保存索引值，以便后续移除映射
                int oldIndex = m_channelConfigTabIndex;

                // 移除标签页
                m_mainView->removeModuleTab(m_channelConfigTabIndex);

                // 移除映射表中的记录
                removeTabIndexMapping(oldIndex);

                // 重置索引和可见性
                m_channelConfigTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::DATA_ANALYSIS:
            if (m_mainView && m_dataAnalysisTabIndex >= 0) {
                int oldIndex = m_dataAnalysisTabIndex;
                m_mainView->removeModuleTab(m_dataAnalysisTabIndex);
                removeTabIndexMapping(oldIndex);
                m_dataAnalysisTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::VIDEO_DISPLAY:
            if (m_mainView && m_videoDisplayTabIndex >= 0) {
                int oldIndex = m_channelConfigTabIndex;
                m_mainView->removeModuleTab(m_videoDisplayTabIndex);
                removeTabIndexMapping(oldIndex);
                m_videoDisplayTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::WAVEFORM_ANALYSIS:
            if (m_mainView && m_waveformAnalysisTabIndex >= 0) {
                int oldIndex = m_channelConfigTabIndex;
                m_mainView->removeModuleTab(m_waveformAnalysisTabIndex);
                removeTabIndexMapping(oldIndex);
                m_waveformAnalysisTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::FILE_OPTIONS:
            if (m_mainView && m_fileOperationTabIndex >= 0) {
                int oldIndex = m_channelConfigTabIndex;
                m_mainView->removeModuleTab(m_fileOperationTabIndex);
                removeTabIndexMapping(oldIndex);
                m_fileOperationTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::DEVICE_UPDATE:
            if (m_mainView && m_UpdateDeviceTabIndex >= 0) {
                int oldIndex = m_channelConfigTabIndex;
                m_mainView->removeModuleTab(m_UpdateDeviceTabIndex);
                removeTabIndexMapping(oldIndex);
                m_UpdateDeviceTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        default:
            LOG_WARN(LocalQTCompat::fromLocal8Bit("未知的模块类型: %1").arg(static_cast<int>(type)));
            break;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("关闭模块异常: %1").arg(e.what()));
    }
}

void ModuleManager::closeAllModules()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭所有模块"));

    closeModule(ModuleType::CHANNEL_CONFIG);
    closeModule(ModuleType::DATA_ANALYSIS);
    closeModule(ModuleType::VIDEO_DISPLAY);
    closeModule(ModuleType::WAVEFORM_ANALYSIS);
    closeModule(ModuleType::FILE_OPTIONS);
    closeModule(ModuleType::DEVICE_UPDATE);
}

bool ModuleManager::isModuleVisible(ModuleType type) const
{
    auto it = m_moduleVisibility.find(type);
    return (it != m_moduleVisibility.end()) ? it->second : false;
}

bool ModuleManager::isModuleInitialized(ModuleType type) const
{
    auto it = m_moduleInitialized.find(type);
    return (it != m_moduleInitialized.end()) ? it->second : false;
}

bool ModuleManager::isAnyModuleActive() const {
    for (const auto& pair : m_moduleVisibility) {
        if (pair.second) {
            return true;
        }
    }
    return false;
}

QString ModuleManager::getModuleTypeName(ModuleType type) {
    switch (type) {
    case ModuleType::CHANNEL_CONFIG:
        return LocalQTCompat::fromLocal8Bit("通道配置");
    case ModuleType::DATA_ANALYSIS:
        return LocalQTCompat::fromLocal8Bit("数据分析");
    case ModuleType::VIDEO_DISPLAY:
        return LocalQTCompat::fromLocal8Bit("视频显示");
    case ModuleType::WAVEFORM_ANALYSIS:
        return LocalQTCompat::fromLocal8Bit("波形分析");
    case ModuleType::FILE_OPTIONS:
        return LocalQTCompat::fromLocal8Bit("文件保存");
    case ModuleType::DEVICE_UPDATE:
        return LocalQTCompat::fromLocal8Bit("设备更新");
    default:
        return LocalQTCompat::fromLocal8Bit("未知模块");
    }
}

void ModuleManager::notifyAllModules(ModuleEvent event, const QVariant& data) {
    //LOG_INFO(LocalQTCompat::fromLocal8Bit("通知所有模块事件: %1").arg(static_cast<int>(event)));

    // 发送通用模块事件信号
    emit signal_moduleEvent(event, data);

    // 处理特定事件
    switch (event) {
    case ModuleEvent::DEVICE_CONNECTED:
        notifyDeviceConnection(true);
        break;
    case ModuleEvent::DEVICE_DISCONNECTED:
        notifyDeviceConnection(false);
        break;
    case ModuleEvent::TRANSFER_STARTED:
        notifyTransferState(true);
        break;
    case ModuleEvent::TRANSFER_STOPPED:
        notifyTransferState(false);
        break;
    case ModuleEvent::APP_CLOSING:
        prepareForShutdown();
        break;
    case ModuleEvent::DATA_AVAILABLE:
        if (data.canConvert<std::vector<DataPacket>>()) {
            processDataPacket(data.value<std::vector<DataPacket>>());
        }
        break;
    case ModuleEvent::CONFIG_CHANGED:
        if (data.canConvert<ChannelConfig>()) {
            // emit signal_channelConfigChanged(data.value<ChannelConfig>());
        }
        break;
    default:
        break;
    }
}

void ModuleManager::notifyDeviceConnection(bool connected)
{
    // 通知各个模块设备连接状态变化
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通知所有模块设备连接状态: %1")
        .arg(connected ? "已连接" : "已断开"));

    // 通知视频显示模块
    if (m_moduleInitialized[ModuleType::VIDEO_DISPLAY] && m_videoDisplayController) {
        // 假设控制器提供了设备连接状态通知方法
        // m_videoDisplayController->notifyDeviceConnection(connected);
    }

    // 通知波形分析模块
    if (m_moduleInitialized[ModuleType::WAVEFORM_ANALYSIS] && m_waveformAnalysisController) {
        // 假设控制器提供了设备连接状态通知方法
        // m_waveformAnalysisController->notifyDeviceConnection(connected);
    }

    // 通知文件保存模块
    if (m_moduleInitialized[ModuleType::FILE_OPTIONS] && m_fileOperationController) {
        // 文件保存模块可能不需要直接响应设备连接状态
    }

    // 通知设备更新模块
    if (m_moduleInitialized[ModuleType::DEVICE_UPDATE] && m_UpdateDeviceController) {
        // 假设控制器提供了设备连接状态通知方法
        // m_UpdateDeviceController->notifyDeviceConnection(connected);
    }
}

void ModuleManager::notifyTransferState(bool transferring)
{
    // 通知各个模块传输状态变化
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通知所有模块传输状态: %1")
        .arg(transferring ? "传输中" : "已停止"));

    // 通知视频显示模块
    if (m_moduleInitialized[ModuleType::VIDEO_DISPLAY] && m_videoDisplayController) {
        // 假设控制器提供了传输状态通知方法
        // m_videoDisplayController->notifyTransferState(transferring);
    }

    // 通知波形分析模块
    if (m_moduleInitialized[ModuleType::WAVEFORM_ANALYSIS] && m_waveformAnalysisController) {
        // 假设控制器提供了传输状态通知方法
        // m_waveformAnalysisController->notifyTransferState(transferring);
    }

    // 通知文件保存模块
    if (m_moduleInitialized[ModuleType::FILE_OPTIONS] && m_fileOperationController) {
        // 假设控制器提供了传输状态通知方法，实际上可能是开始或停止保存
        if (transferring) {
            // 仅在设置了自动保存时才自动开始保存
            // bool autoSave = FileOperationModel::getInstance()->getOption("autoSaveOnTransfer", false).toBool();
            // if (autoSave) {
            //     m_fileOperationController->startSaving();
            // }
        }
        else {
            // 传输停止时可能需要停止保存
            // bool autoStop = FileOperationModel::getInstance()->getOption("autoStopOnTransferEnd", false).toBool();
            // if (autoStop && m_fileOperationController->isSaving()) {
            //     m_fileOperationController->stopSaving();
            // }
        }
    }
}

void ModuleManager::handleModuleTabClosed(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理模块标签页关闭，索引: %1").arg(index));

    // 使用映射表查找对应的模块类型
    ModuleType moduleType = findModuleTypeByTabIndex(index);

    if (moduleType == static_cast<ModuleType>(-1)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("收到未知标签页关闭请求，索引: %1").arg(index));

        // 尝试恢复逻辑：直接从UI中移除标签
        if (m_mainView && m_mainView->getUi() &&
            m_mainView->getUi()->mainTabWidget &&
            index >= 0 && index < m_mainView->getUi()->mainTabWidget->count()) {

            LOG_INFO(LocalQTCompat::fromLocal8Bit("尝试直接关闭未知标签页: %1").arg(index));
            m_mainView->getUi()->mainTabWidget->removeTab(index);

            // 更新所有标签索引
            updateTabIndicesAfterClose(index);
        }
        return;
    }

    // 记录关闭前的状态，用于更新其他标签索引
    int closedIndex = index;

    // 根据模块类型更新状态
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭模块 %1 (索引: %2)").arg(getModuleTypeName(moduleType)).arg(index));

    // 先移除映射，防止更新索引时重复处理
    removeTabIndexMapping(index);

    if (m_mainView) {
        // 确保先从UI移除标签，再更新内部状态
        m_mainView->getUi()->mainTabWidget->removeTab(index);
    }

    // 更新对应模块的状态
    switch (moduleType) {
    case ModuleType::CHANNEL_CONFIG:
        m_channelConfigTabIndex = -1;
        m_moduleVisibility[moduleType] = false;
        break;

    case ModuleType::DATA_ANALYSIS:
        m_dataAnalysisTabIndex = -1;
        m_moduleVisibility[moduleType] = false;
        break;

    case ModuleType::VIDEO_DISPLAY:
        m_videoDisplayTabIndex = -1;
        m_moduleVisibility[moduleType] = false;
        break;

    case ModuleType::WAVEFORM_ANALYSIS:
        m_waveformAnalysisTabIndex = -1;
        m_moduleVisibility[moduleType] = false;
        break;

    case ModuleType::FILE_OPTIONS:
        m_fileOperationTabIndex = -1;
        m_moduleVisibility[moduleType] = false;
        break;

    case ModuleType::DEVICE_UPDATE:
        m_UpdateDeviceTabIndex = -1;
        m_moduleVisibility[moduleType] = false;
        break;

    default:
        LOG_WARN(LocalQTCompat::fromLocal8Bit("未知的模块类型: %1").arg(static_cast<int>(moduleType)));
        return;
    }

    // 更新剩余标签的索引
    updateTabIndicesAfterClose(closedIndex);

    // 发送可见性变更信号
    emit signal_moduleVisibilityChanged(moduleType, false);
}

void ModuleManager::processDataPacket(const std::vector<DataPacket>& packets)
{
    if (packets.empty()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("空包，忽略"));
        return;
    }

    if (m_moduleInitialized[ModuleType::FILE_OPTIONS]) {
        if (m_fileOperationController) {
            // 必须实时保存文件，这是数据分析、视频显示的基础
            if (m_fileOperationController->isSaving()) {
#if 0
                LOG_INFO(QString("保存状态：正在保存中，处理%1个数据包").arg(packets.size()));
#endif
                // 处理所有数据包
                for (const auto& packet : packets) {
                    m_fileOperationController->slot_FS_C_processDataPacket(packet);
                }
            }
            else {
                // 检查是否启用了自动保存
                bool autoSave = m_fileOperationController->slot_FS_C_isAutoSaveEnabled();
                if (autoSave) {
                    LOG_INFO("自动保存已启用，启动保存");

                    // 将文件目录设置到数据分析模块中
                    m_dataAnalysisController->setDataSource(m_fileOperationController->getCurrentFileName());

                    if (m_fileOperationController->slot_FS_C_startSaving()) {
                        // 延迟100ms确保保存状态已更新
                        QTimer::singleShot(100, [this, packets]() {
                            for (const auto& packet : packets) {
                                m_fileOperationController->slot_FS_C_processDataPacket(packet);
                            }
                            });
                    }
                }
                else {
                    LOG_INFO("自动保存未启用，数据包未保存");
                }
            }
        }
        else {
            LOG_WARN("文件保存控制器未初始化");
        }
    }
    else {
        LOG_WARN("文件保存模块未初始化");
    }

    // 数据分析模块处理
    if (m_moduleInitialized[ModuleType::DATA_ANALYSIS] &&
        m_moduleVisibility[ModuleType::DATA_ANALYSIS] &&
        m_dataAnalysisController) {
        // 必须实时分析，生成索引信息
        m_dataAnalysisController->processDataPackets(packets);
    }

#if 0
    // 视频显示模块
    if (m_moduleInitialized[ModuleType::VIDEO_DISPLAY] &&
        m_moduleVisibility[ModuleType::VIDEO_DISPLAY] &&
        m_videoDisplayController) {
        // 什么也不干，视频显示模块采取Lazy的模式，只有被观察才会采取动作
    }

    // 波形分析模块
    if (m_moduleInitialized[ModuleType::WAVEFORM_ANALYSIS] &&
        m_moduleVisibility[ModuleType::WAVEFORM_ANALYSIS] &&
        m_waveformAnalysisController) {
        // 什么也不干，波形分析模块采取Lazy的模式，只有被观察才会采取动作
    }
#endif
}

bool ModuleManager::createChannelConfigModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建通道配置模块"));

        // 创建视图
        m_channelConfigView = std::make_unique<ChannelSelectView>(m_mainView);

        // 获取控制器
        m_channelConfigController = std::make_shared<ChannelSelectController>(m_channelConfigView.get());

        // 初始化控制器
        m_channelConfigController->initialize();

        // 连接通道配置变更信号
        connect(m_channelConfigView.get(), &ChannelSelectView::signal_CHN_V_configChanged,
            this, &ModuleManager::signal_channelConfigChanged);

        // 标记为已初始化
        m_moduleInitialized[ModuleType::CHANNEL_CONFIG] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建通道配置模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_channelConfigView.reset();
        m_channelConfigController.reset();

        return false;
    }
}

bool ModuleManager::createDataAnalysisModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建数据分析模块"));

        // 创建视图
        m_dataAnalysisView = std::make_unique<DataAnalysisView>(m_mainView);

        // 获取控制器
        m_dataAnalysisController = std::make_shared<DataAnalysisController>(m_dataAnalysisView.get());

        // 初始化控制器
        m_dataAnalysisController->initialize();

        // Connect file loading signal to controller
        connect(m_dataAnalysisView.get(), &DataAnalysisView::signal_DA_V_loadDataFromFileRequested,
            m_dataAnalysisController.get(), &DataAnalysisController::loadDataFromFile);

        // 标记为已初始化
        m_moduleInitialized[ModuleType::DATA_ANALYSIS] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建数据分析模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_dataAnalysisView.reset();
        m_dataAnalysisController.reset();

        return false;
    }
}

bool ModuleManager::createVideoDisplayModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建视频显示模块"));

        // 创建视图
        m_videoDisplayView = std::make_unique<VideoDisplayView>(m_mainView);

        // 获取控制器
        m_videoDisplayController = std::make_shared<VideoDisplayController>(m_videoDisplayView.get());

        m_videoDisplayView->setVidioDisplayController(m_videoDisplayController.get());
        // 初始化控制器
        if (!m_videoDisplayController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化视频显示控制器失败"));
            throw std::runtime_error("初始化视频显示控制器失败");
        }

        // 标记为已初始化
        m_moduleInitialized[ModuleType::VIDEO_DISPLAY] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("视频显示模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建视频显示模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_videoDisplayView.reset();
        m_videoDisplayController.reset();

        return false;
    }
}

bool ModuleManager::createWaveformAnalysisModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建波形分析模块"));

        // 创建视图
        m_waveformAnalysisView = std::make_unique<WaveformAnalysisView>(m_mainView);

        // 获取控制器
        m_waveformAnalysisController = std::make_shared<WaveformAnalysisController>(m_waveformAnalysisView.get());

        // 设置视图的控制器引用
        m_waveformAnalysisView->setController(m_waveformAnalysisController.get());

        // 初始化控制器
        if (!m_waveformAnalysisController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化波形分析控制器失败"));
            throw std::runtime_error("初始化波形分析控制器失败");
        }

        // 连接垂直缩放信号
        connect(m_waveformAnalysisView.get(), &WaveformAnalysisView::signal_WA_V_verticalScaleChanged,
            [this](int value) {
                if (m_waveformAnalysisController) {
                    // 将滑块值(1-10)转换为缩放系数(0.5-2.0)
                    double scale = 0.5 + (value / 10.0) * 1.5;
                    m_waveformAnalysisController->slot_WA_C_setVerticalScale(scale);
                }
            });

        connect(m_mainView->getUi()->mainTabWidget, &QTabWidget::currentChanged,
            [this](int index) {
                // Check if the current tab is the waveform analysis tab
                if (index == m_waveformAnalysisTabIndex && m_waveformAnalysisController) {
                    m_waveformAnalysisController->setTabVisible(true);
                }
                else if (m_waveformAnalysisController) {
                    m_waveformAnalysisController->setTabVisible(false);
                }
            });

        // 标记为已初始化
        m_moduleInitialized[ModuleType::WAVEFORM_ANALYSIS] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建波形分析模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_waveformAnalysisView.reset();
        m_waveformAnalysisController.reset();

        return false;
    }
}

bool ModuleManager::createFileOperationModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建文件保存模块"));

        // 创建视图
        m_fileOperationView = std::make_unique<FileOperationView>(m_mainView);

        // 获取控制器（可能已经是一个单例或需要参数）
        m_fileOperationController = std::make_shared<FileOperationController>(m_mainView);

        // 初始化控制器
        if (!m_fileOperationController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化文件保存控制器失败"));
            throw std::runtime_error("初始化文件保存控制器失败");
        }

        // 连接视图与控制器的信号
        connect(m_fileOperationView.get(), &FileOperationView::signal_FS_V_startSaveRequested,
            m_fileOperationController.get(), &FileOperationController::slot_FS_C_startSaving);
        connect(m_fileOperationView.get(), &FileOperationView::signal_FS_V_stopSaveRequested,
            m_fileOperationController.get(), &FileOperationController::slot_FS_C_stopSaving);
        connect(m_fileOperationView.get(), &FileOperationView::signal_FS_V_saveParametersChanged,
            m_fileOperationController.get(), &FileOperationController::slot_FS_C_onViewParametersChanged);

        // 连接控制器到视图的信号
        connect(m_fileOperationController.get(), &FileOperationController::signal_FS_C_saveStarted,
            m_fileOperationView.get(), &FileOperationView::slot_FS_V_onSaveStarted);
        connect(m_fileOperationController.get(), &FileOperationController::signal_FS_C_saveStopped,
            m_fileOperationView.get(), &FileOperationView::slot_FS_V_onSaveStopped);
        connect(m_fileOperationController.get(), &FileOperationController::signal_FS_C_saveCompleted,
            m_fileOperationView.get(), &FileOperationView::slot_FS_V_onSaveCompleted);
        connect(m_fileOperationController.get(), &FileOperationController::signal_FS_C_saveError,
            m_fileOperationView.get(), &FileOperationView::slot_FS_V_onSaveError);

        // 标记为已初始化
        m_moduleInitialized[ModuleType::FILE_OPTIONS] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建文件保存模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_fileOperationView.reset();
        m_fileOperationController.reset();

        return false;
    }
}

bool ModuleManager::createUpdateDeviceModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建设备更新模块"));

        // 创建视图
        m_UpdateDeviceView = std::make_unique<UpdateDeviceView>(m_mainView);

        // 获取控制器
        m_UpdateDeviceController = std::make_shared<UpdateDeviceController>(m_UpdateDeviceView.get());

        // 初始化控制器
        if (!m_UpdateDeviceController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化设备更新控制器失败"));
            throw std::runtime_error("初始化设备更新控制器失败");
        }

        // 标记为已初始化
        m_moduleInitialized[ModuleType::DEVICE_UPDATE] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("设备更新模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建设备更新模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_UpdateDeviceView.reset();
        m_UpdateDeviceController.reset();

        return false;
    }
}

void ModuleManager::updateTabIndexMapping(int index, ModuleType type)
{
    if (index < 0) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试更新无效的标签索引映射: %1 -> %2")
            .arg(index).arg(static_cast<int>(type)));
        return;
    }

    // 检查是否存在索引冲突
    for (const auto& pair : m_tabIndexToModule) {
        if (pair.first == index && pair.second != type) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("索引映射冲突: 索引 %1 已映射到 %2，正在被重新映射到 %3")
                .arg(index).arg(static_cast<int>(pair.second)).arg(static_cast<int>(type)));
        }
    }

    // 更新映射
    m_tabIndexToModule[index] = type;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新标签索引映射: %1 -> %2 (%3)")
        .arg(index).arg(static_cast<int>(type)).arg(getModuleTypeName(type)));
}

void ModuleManager::removeTabIndexMapping(int index)
{
    auto it = m_tabIndexToModule.find(index);
    if (it != m_tabIndexToModule.end()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("移除标签索引映射: %1 -> %2 (%3)")
            .arg(index).arg(static_cast<int>(it->second)).arg(getModuleTypeName(it->second)));
        m_tabIndexToModule.erase(it);
    }
}

ModuleManager::ModuleType ModuleManager::findModuleTypeByTabIndex(int index) const
{
    auto it = m_tabIndexToModule.find(index);
    if (it != m_tabIndexToModule.end()) {
        return it->second;
    }

    // 使用存储的索引变量进行额外检查（向后兼容）
    if (index == m_channelConfigTabIndex) {
        return ModuleType::CHANNEL_CONFIG;
    }
    else if (index == m_dataAnalysisTabIndex) {
        return ModuleType::DATA_ANALYSIS;
    }
    else if (index == m_videoDisplayTabIndex) {
        return ModuleType::VIDEO_DISPLAY;
    }
    else if (index == m_waveformAnalysisTabIndex) {
        return ModuleType::WAVEFORM_ANALYSIS;
    }
    else if (index == m_fileOperationTabIndex) {
        return ModuleType::FILE_OPTIONS;
    }
    else if (index == m_UpdateDeviceTabIndex) {
        return ModuleType::DEVICE_UPDATE;
    }

    // 如果未找到映射，返回一个无效值（使用最大值表示无效）
    return static_cast<ModuleType>(-1);
}

void ModuleManager::updateTabIndicesAfterClose(int closedIndex)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新标签索引，已关闭索引: %1").arg(closedIndex));

    // 调整模块索引变量
    if (m_channelConfigTabIndex > closedIndex) m_channelConfigTabIndex--;
    if (m_dataAnalysisTabIndex > closedIndex) m_dataAnalysisTabIndex--;
    if (m_videoDisplayTabIndex > closedIndex) m_videoDisplayTabIndex--;
    if (m_waveformAnalysisTabIndex > closedIndex) m_waveformAnalysisTabIndex--;
    if (m_fileOperationTabIndex > closedIndex) m_fileOperationTabIndex--;
    if (m_UpdateDeviceTabIndex > closedIndex) m_UpdateDeviceTabIndex--;

    // 更新映射表中的索引
    std::map<int, ModuleType> updatedMap;
    for (const auto& pair : m_tabIndexToModule) {
        int idx = pair.first;
        if (idx > closedIndex) {
            updatedMap[idx - 1] = pair.second;
            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("标签索引映射调整: %1 -> %2，模块: %3")
                .arg(idx).arg(idx - 1).arg(getModuleTypeName(pair.second)));
        }
        else if (idx < closedIndex) {
            updatedMap[idx] = pair.second;
        }
        // 忽略closedIndex，它已经被移除
    }
    m_tabIndexToModule = std::move(updatedMap);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("标签索引更新完成，映射表大小: %1").arg(m_tabIndexToModule.size()));
}