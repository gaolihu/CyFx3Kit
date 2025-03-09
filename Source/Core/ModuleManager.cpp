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
#include "FileSaveView.h"
#include "FileSaveController.h"
#include "UpdateDeviceView.h"
#include "UpdateDeviceController.h"
#include "Logger.h"

ModuleManager::ModuleManager(FX3MainView* mainView)
    : m_mainView(mainView)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器已创建"));
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

    if (m_fileSaveController) {
        m_fileSaveController.reset();
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

                m_moduleVisibility[type] = true;
                emit signal_moduleVisibilityChanged(type, true);
                return true;
            }
            break;

        case ModuleType::FILE_OPTIONS:
            if (!m_moduleInitialized[type]) {
                if (!createFileSaveModule()) {
                    return false;
                }
            }

            if (m_mainView && m_fileSaveView) {
                if (m_fileSaveTabIndex >= 0) {
                    m_mainView->showModuleTab(m_fileSaveTabIndex,
                        m_fileSaveView.get(),
                        LocalQTCompat::fromLocal8Bit("文件保存"));
                }
                else {
                    m_fileSaveTabIndex = -1;
                    m_mainView->addModuleToMainTab(m_fileSaveView.get(),
                        LocalQTCompat::fromLocal8Bit("文件保存"),
                        m_fileSaveTabIndex);
                }

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
                m_mainView->removeModuleTab(m_channelConfigTabIndex);
                m_channelConfigTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::DATA_ANALYSIS:
            if (m_mainView && m_dataAnalysisTabIndex >= 0) {
                m_mainView->removeModuleTab(m_dataAnalysisTabIndex);
                m_dataAnalysisTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::VIDEO_DISPLAY:
            if (m_mainView && m_videoDisplayTabIndex >= 0) {
                m_mainView->removeModuleTab(m_videoDisplayTabIndex);
                m_videoDisplayTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::WAVEFORM_ANALYSIS:
            if (m_mainView && m_waveformAnalysisTabIndex >= 0) {
                m_mainView->removeModuleTab(m_waveformAnalysisTabIndex);
                m_waveformAnalysisTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::FILE_OPTIONS:
            if (m_mainView && m_fileSaveTabIndex >= 0) {
                m_mainView->removeModuleTab(m_fileSaveTabIndex);
                m_fileSaveTabIndex = -1;
                m_moduleVisibility[type] = false;
                emit signal_moduleVisibilityChanged(type, false);
            }
            break;

        case ModuleType::DEVICE_UPDATE:
            if (m_mainView && m_UpdateDeviceTabIndex >= 0) {
                m_mainView->removeModuleTab(m_UpdateDeviceTabIndex);
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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通知所有模块事件: %1").arg(static_cast<int>(event)));

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
        if (data.canConvert<DataPacket>()) {
            processDataPacket(data.value<DataPacket>());
        }
        break;
    case ModuleEvent::CONFIG_CHANGED:
        if (data.canConvert<ChannelConfig>()) {
            emit signal_channelConfigChanged(data.value<ChannelConfig>());
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
    if (m_moduleInitialized[ModuleType::FILE_OPTIONS] && m_fileSaveController) {
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
    if (m_moduleInitialized[ModuleType::FILE_OPTIONS] && m_fileSaveController) {
        // 假设控制器提供了传输状态通知方法，实际上可能是开始或停止保存
        if (transferring) {
            // 仅在设置了自动保存时才自动开始保存
            // bool autoSave = FileSaveModel::getInstance()->getOption("autoSaveOnTransfer", false).toBool();
            // if (autoSave) {
            //     m_fileSaveController->startSaving();
            // }
        }
        else {
            // 传输停止时可能需要停止保存
            // bool autoStop = FileSaveModel::getInstance()->getOption("autoStopOnTransferEnd", false).toBool();
            // if (autoStop && m_fileSaveController->isSaving()) {
            //     m_fileSaveController->stopSaving();
            // }
        }
    }
}

void ModuleManager::handleModuleTabClosed(int index)
{
    if (index == m_channelConfigTabIndex) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置标签页已关闭"));
        m_channelConfigTabIndex = -1;
        m_moduleVisibility[ModuleType::CHANNEL_CONFIG] = false;
        emit signal_moduleVisibilityChanged(ModuleType::CHANNEL_CONFIG, false);
    }
    else if (index == m_dataAnalysisTabIndex) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析标签页已关闭"));
        m_dataAnalysisTabIndex = -1;
        m_moduleVisibility[ModuleType::DATA_ANALYSIS] = false;
        emit signal_moduleVisibilityChanged(ModuleType::DATA_ANALYSIS, false);
    }
    else if (index == m_videoDisplayTabIndex) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("视频显示标签页已关闭"));
        m_videoDisplayTabIndex = -1;
        m_moduleVisibility[ModuleType::VIDEO_DISPLAY] = false;
        emit signal_moduleVisibilityChanged(ModuleType::VIDEO_DISPLAY, false);
    }
    else if (index == m_waveformAnalysisTabIndex) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析标签页已关闭"));
        m_waveformAnalysisTabIndex = -1;
        m_moduleVisibility[ModuleType::WAVEFORM_ANALYSIS] = false;
        emit signal_moduleVisibilityChanged(ModuleType::WAVEFORM_ANALYSIS, false);
    }
    else if (index == m_fileSaveTabIndex) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存标签页已关闭"));
        m_fileSaveTabIndex = -1;
        m_moduleVisibility[ModuleType::FILE_OPTIONS] = false;
        emit signal_moduleVisibilityChanged(ModuleType::FILE_OPTIONS, false);
    }
    else if (index == m_UpdateDeviceTabIndex) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设备更新标签页已关闭"));
        m_UpdateDeviceTabIndex = -1;
        m_moduleVisibility[ModuleType::DEVICE_UPDATE] = false;
        emit signal_moduleVisibilityChanged(ModuleType::DEVICE_UPDATE, false);
    }
}

void ModuleManager::processDataPacket(const DataPacket& packet)
{
    // 将数据包转发到需要的模块

    // 数据分析模块
    if (m_moduleInitialized[ModuleType::DATA_ANALYSIS] &&
        m_moduleVisibility[ModuleType::DATA_ANALYSIS] &&
        m_dataAnalysisController) {
        // 假设控制器提供处理数据包的方法
        // m_dataAnalysisController->processDataPacket(packet);
    }

    // 视频显示模块
    if (m_moduleInitialized[ModuleType::VIDEO_DISPLAY] &&
        m_moduleVisibility[ModuleType::VIDEO_DISPLAY] &&
        m_videoDisplayController) {
        // 假设控制器提供处理数据包的方法
        // m_videoDisplayController->processDataPacket(packet);
    }

    // 波形分析模块
    if (m_moduleInitialized[ModuleType::WAVEFORM_ANALYSIS] &&
        m_moduleVisibility[ModuleType::WAVEFORM_ANALYSIS] &&
        m_waveformAnalysisController) {
        // 假设控制器提供处理数据包的方法
        // m_waveformAnalysisController->processDataPacket(packet);
    }

    // 文件保存模块
    if (m_moduleInitialized[ModuleType::FILE_OPTIONS] &&
        m_fileSaveController && m_fileSaveController->isSaving()) {
        // 文件保存控制器中已经有处理数据包的方法
        m_fileSaveController->processDataPacket(packet);
    }
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
        connect(m_channelConfigView.get(), &ChannelSelectView::configChanged,
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

        // 初始化控制器
        if (!m_waveformAnalysisController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化波形分析控制器失败"));
            throw std::runtime_error("初始化波形分析控制器失败");
        }

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

bool ModuleManager::createFileSaveModule()
{
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("创建文件保存模块"));

        // 创建视图
        m_fileSaveView = std::make_unique<FileSaveView>(m_mainView);

        // 获取控制器（可能已经是一个单例或需要参数）
        m_fileSaveController = std::make_shared<FileSaveController>(m_mainView);

        // 初始化控制器
        if (!m_fileSaveController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化文件保存控制器失败"));
            throw std::runtime_error("初始化文件保存控制器失败");
        }

        // 连接视图与控制器的信号
        connect(m_fileSaveView.get(), &FileSaveView::startSaveRequested,
            m_fileSaveController.get(), &FileSaveController::startSaving);
        connect(m_fileSaveView.get(), &FileSaveView::stopSaveRequested,
            m_fileSaveController.get(), &FileSaveController::stopSaving);
#if 0
        connect(m_fileSaveView.get(), &FileSaveView::saveParametersChanged,
            m_fileSaveController.get(), &FileSaveController::onViewParametersChanged);
#endif //TODO

        // 连接控制器到视图的信号
        connect(m_fileSaveController.get(), &FileSaveController::saveStarted,
            m_fileSaveView.get(), &FileSaveView::onSaveStarted);
        connect(m_fileSaveController.get(), &FileSaveController::saveStopped,
            m_fileSaveView.get(), &FileSaveView::onSaveStopped);
        connect(m_fileSaveController.get(), &FileSaveController::saveCompleted,
            m_fileSaveView.get(), &FileSaveView::onSaveCompleted);
        connect(m_fileSaveController.get(), &FileSaveController::saveError,
            m_fileSaveView.get(), &FileSaveView::onSaveError);

        // 标记为已初始化
        m_moduleInitialized[ModuleType::FILE_OPTIONS] = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存模块创建完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建文件保存模块异常: %1").arg(e.what()));

        // 清理失败的资源
        m_fileSaveView.reset();
        m_fileSaveController.reset();

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