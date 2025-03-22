// Source/MVC/Controllers/FX3MainController.cpp

#include "FX3MainController.h"
#include "FX3MainView.h"
#include "FX3MainModel.h"
#include "MenuController.h"
#include "DeviceController.h"

#include "DeviceView.h"
#include "ChannelSelectModel.h"

#include "DataPacket.h"
#include "Logger.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QThread>
#include <QTimer>
#include <QCoreApplication>
#include <windows.h>
#include <dbt.h>

FX3MainController::FX3MainController(FX3MainView* mainView)
    : QObject(nullptr)
    , m_mainView(mainView)
    , m_mainModel(nullptr)
    , m_initialized(false)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主控制器构造函数入口"));
    m_mainModel = FX3MainModel::getInstance();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主控制器构造函数完成"));
}

FX3MainController::~FX3MainController()
{
    if (m_initialized) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主控制器析构函数入口"));
        // 确保资源被释放
        shutdown();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主控制器析构函数完成"));
}

bool FX3MainController::initialize()
{
    if (m_initialized) {
        return true;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始初始化FX3主控制器..."));

    try {
        // 创建设备控制器
        m_DeviceView = std::make_unique<DeviceView>(this);
        m_DeviceView->initUIComponents(
            m_mainView->getUi()->imageWIdth,
            m_mainView->getUi()->imageHeight,
            m_mainView->getUi()->imageType,
            m_mainView->getUi()->usbSpeedLabel,
            m_mainView->getUi()->usbStatusLabel,
            m_mainView->getUi()->transferStatusLabel,
            m_mainView->getUi()->transferRateLabel,
            m_mainView->getUi()->totalBytesLabel,
            m_mainView->getUi()->totalTimeLabel,
            m_mainView->getUi()->actionStartTransfer,
            m_mainView->getUi()->actionStopTransfer,
            m_mainView->getUi()->actionResetDevice
        );
        m_deviceController = std::make_unique<DeviceController>(m_DeviceView.get(), this);
        if (!m_deviceController->initialize(m_mainView->getWindowHandle())) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备控制器初始化失败"));
            return false;
        }

        // 创建菜单控制器
        m_menuController = std::make_unique<MenuController>(m_mainView, this);
        if (!m_menuController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("菜单控制器初始化失败"));
            return false;
        }

        // 创建模块管理器
        m_moduleManager = std::make_unique<ModuleManager>(m_mainView);
        if (!m_moduleManager->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("模块管理器初始化失败"));
            return false;
        }

        // 连接信号
        connectSignals();

        // 注册设备通知
        registerDeviceNotification();

        // 设置初始状态并初始化设备
        AppStateMachine::instance().processEvent(StateEvent::APP_INIT,
            LocalQTCompat::fromLocal8Bit("应用程序初始化完成"));

        // 初始化设备
        if (!initializeDevice()) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("设备初始化失败，应用将以离线模式运行"));
            if (m_mainView) {
                m_mainView->showWarningMessage(LocalQTCompat::fromLocal8Bit("警告"),
                    LocalQTCompat::fromLocal8Bit("设备初始化失败，将以离线模式运行"));
            }
        }

        // After device controller is initialized, check for command directory
        if (m_deviceController && !m_mainModel->getCommandDirectory().isEmpty()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化时加载命令目录: %1").arg(m_mainModel->getCommandDirectory()));
            m_deviceController->setCommandDirectory(m_mainModel->getCommandDirectory());
        }

        m_initialized = true;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主控制器初始化完成"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("控制器初始化异常: %1").arg(e.what()));
        if (m_mainView) {
            m_mainView->showErrorMessage(LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("初始化过程中发生异常: %1").arg(e.what()));
        }
        return false;
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("控制器初始化未知异常"));
        if (m_mainView) {
            m_mainView->showErrorMessage(LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("初始化过程中发生未知异常"));
        }
        return false;
    }
}

void FX3MainController::shutdown()
{
    if (!m_initialized) {
        return;
    }

    LOG_WARN(LocalQTCompat::fromLocal8Bit("关闭FX3主控制器..."));
    m_mainModel->setClosing(true);

    try {
        // 停止并释放资源
        stopAndReleaseResources();

        // 发送关闭事件到状态机
        AppStateMachine::instance().processEvent(StateEvent::APP_SHUTDOWN,
            LocalQTCompat::fromLocal8Bit("应用程序正在关闭"));

        if (m_deviceController) {
            m_deviceController->prepareForShutdown();
        }

        m_initialized = false;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主控制器关闭完成"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("控制器关闭异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("控制器关闭未知异常"));
    }
}

bool FX3MainController::initializeDevice()
{
    if (!m_deviceController) {
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化设备..."));
    return m_deviceController->checkAndOpenDevice();
}

void FX3MainController::stopAndReleaseResources()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止并释放资源..."));

    // 1. 确保设备控制器停止所有传输
    if (m_deviceController && m_deviceController->isTransferring()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止正在进行的数据传输"));
        m_deviceController->stopTransfer();

        // 给一定时间让设备控制器完成停止操作
        QElapsedTimer timer;
        timer.start();
        while (m_deviceController->isTransferring() && timer.elapsed() < 300) {
            QThread::msleep(10);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }

#if 0
    // 2. 停止文件保存（如果正在进行）
    if (m_fileSaveController && m_fileSaveController->isSaving()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
        m_fileSaveController->stopSaving();

        // 短暂等待让保存操作完成
        QThread::msleep(100);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
#endif

    // 3. 关闭所有模块窗口
    if (m_moduleManager) {
        m_moduleManager->closeAllModules();
    }

    // 4. 断开信号连接，避免在关闭过程中的信号干扰
    disconnect(&AppStateMachine::instance(), nullptr, this, nullptr);
    disconnect(this, nullptr, nullptr, nullptr); // 断开控制器所有信号
    disconnect(m_mainModel, nullptr, nullptr, nullptr); // 断开模型所有信号

    LOG_INFO(LocalQTCompat::fromLocal8Bit("所有资源已释放"));
}

bool FX3MainController::registerDeviceNotification()
{
    if (!m_mainView) {
        return false;
    }

    try {
        DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
        ZeroMemory(&notificationFilter, sizeof(notificationFilter));
        notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        notificationFilter.dbcc_classguid = CYUSBDRV_GUID;

        HWND hwnd = m_mainView->getWindowHandle();
        HDEVNOTIFY hDevNotify = RegisterDeviceNotification(
            hwnd,
            &notificationFilter,
            DEVICE_NOTIFY_WINDOW_HANDLE
        );

        if (!hDevNotify) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("注册Fx3 USB设备通知失败: %1")
                .arg(GetLastError()));
            return false;
        }
        else {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("Fx3 USB设备通知注册成功"));
            return true;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("注册设备通知异常: %1").arg(e.what()));
        return false;
    }
}

void FX3MainController::connectSignals()
{
    // 连接视图信号到控制器槽
    if (m_mainView) {
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_startButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleStartTransfer);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_stopButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleStopTransfer);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_resetButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleResetDevice);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_channelConfigButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleChannelConfig);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_dataAnalysisButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleDataAnalysis);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_videoDisplayButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleVideoDisplay);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_waveformAnalysisButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleWaveformAnalysis);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_exportDataButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleDataExport);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_FileOptionsButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleFileOption);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_settingsTriggered, this, &FX3MainController::slot_FX3Main_C_handleSettings);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_clearLogTriggered, this, &FX3MainController::slot_FX3Main_C_handleClearLog);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_helpContentTriggered, this, &FX3MainController::slot_FX3Main_C_handleHelpContent);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_aboutDialogTriggered, this, &FX3MainController::slot_FX3Main_C_handleAboutDialog);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_selectCommandDirClicked, this, &FX3MainController::slot_FX3Main_C_handleSelectCommandDir);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_updateDeviceButtonClicked, this, &FX3MainController::slot_FX3Main_C_handleDeviceUpdate);
        connect(m_mainView, &FX3MainView::signal_FX3Main_V_moduleTabClosed, this, &FX3MainController::slot_FX3Main_C_handleModuleTabClosed);
    }

    // 连接模型信号到控制器槽
    connect(m_mainModel, &FX3MainModel::signal_FX3Main_M_transferStateChanged, this, &FX3MainController::slot_FX3Main_C_onTransferStateChanged);
    connect(m_mainModel, &FX3MainModel::signal_FX3Main_M_transferStatsUpdated, this, &FX3MainController::slot_FX3Main_C_onTransferStatsUpdated);
    connect(m_mainModel, &FX3MainModel::signal_FX3Main_M_appStateChanged, this, &FX3MainController::slot_FX3Main_C_onAppStateChanged);

    // 连接设备控制器信号
    if (m_deviceController) {
        connect(m_deviceController.get(), &DeviceController::signal_Dev_C_transferStateChanged,
            this, &FX3MainController::slot_FX3Main_C_handleTransferStateChanged);

        connect(m_deviceController.get(), &DeviceController::signal_Dev_C_transferStatsUpdated,
            this, &FX3MainController::slot_FX3Main_C_handleTransferStatsUpdated);

        connect(m_deviceController.get(), &DeviceController::signal_Dev_C_dataPacketAvailable,
            this, &FX3MainController::slot_FX3Main_C_handleDataPacketAvailable);

        connect(m_deviceController.get(), &DeviceController::signal_Dev_C_deviceError,
            this, [this](const QString& title, const QString& message) {
                if (m_mainView) {
                    m_mainView->showErrorMessage(title, message);
                }
            });

        connect(m_deviceController.get(), &DeviceController::signal_Dev_C_usbSpeedUpdated,
            this, &FX3MainController::slot_FX3Main_C_handleUsbSpeedUpdated);
    }

    // 连接菜单控制器信号
    if (m_menuController) {
        connect(m_menuController.get(), &MenuController::menuActionTriggered,
            this, [this](const QString& action) {
                // 处理菜单动作
                if (action == "startAction") {
                    slot_FX3Main_C_handleStartTransfer();
                }
                else if (action == "stopAction") {
                    slot_FX3Main_C_handleStopTransfer();
                }
                else if (action == "resetAction") {
                    slot_FX3Main_C_handleResetDevice();
                }
                else if (action == "channelAction") {
                    slot_FX3Main_C_handleChannelConfig();
                }
                else if (action == "dataAction") {
                    slot_FX3Main_C_handleDataAnalysis();
                }
                else if (action == "videoAction") {
                    slot_FX3Main_C_handleVideoDisplay();
                }
                else if (action == "waveformAction") {
                    slot_FX3Main_C_handleWaveformAnalysis();
                }
                else if (action == "saveAction") {
                    slot_FX3Main_C_handleFileSave();
                }
                else if (action == "exportAction") {
                    slot_FX3Main_C_handleDataExport();
                }
                else if (action == "fileOptions") {
                    slot_FX3Main_C_handleFileOption();
                }
                else if (action == "settingsAction") {
                    slot_FX3Main_C_handleSettings();
                }
                else if (action == "clearLogAction") {
                    slot_FX3Main_C_handleClearLog();
                }
                else if (action == "helpContentAction") {
                    slot_FX3Main_C_handleHelpContent();
                }
                else if (action == "aboutAction") {
                    slot_FX3Main_C_handleAboutDialog();
                }
                else if (action == "updateAction") {
                    slot_FX3Main_C_handleDeviceUpdate();
                }
            });
    }

    // 连接状态机信号到UI状态管理器
    MainUiStateManager* uiStateManager = m_mainView ? m_mainView->getUiStateManager() : nullptr;
    if (uiStateManager) {
        connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged,
            uiStateManager, &MainUiStateManager::slot_MainUI_STM_onStateChanged);
    }

    // 连接状态机信号到模型
    connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged,
        this, [this](AppState newState, AppState oldState, const QString& reason) {
            // 由FX3MainModel处理状态变更
            m_mainModel->signal_FX3Main_M_appStateChanged(newState, oldState, reason);
        });

    // 连接模块管理器信号
    if (m_moduleManager) {
        // 连接模块可见性变更信号
        connect(m_moduleManager.get(), &ModuleManager::signal_moduleVisibilityChanged,
            this, [this](ModuleManager::ModuleType type, bool visible) {
                // 更新UI状态可能在此处理
                LOG_INFO(LocalQTCompat::fromLocal8Bit("模块可见性变更: %1, 可见性: %2")
                    .arg(static_cast<int>(type)).arg(visible));
            });

        // 连接通道配置变更信号
        connect(m_moduleManager.get(), &ModuleManager::signal_channelConfigChanged,
            this, &FX3MainController::slot_FX3Main_C_onChannelConfigChanged);
    }

    // 连接设备信息变更信号到视图更新
    connect(m_mainModel, &FX3MainModel::signal_FX3Main_M_deviceInfoChanged,
        this, [this](const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber) {
            if (m_mainView) {
                m_mainView->updateDeviceInfoDisplay(deviceName, firmwareVersion, serialNumber);
            }
        });

    // Connect command directory changed signal from model
    connect(m_mainModel, &FX3MainModel::signal_FX3Main_M_commandDirectoryChanged,
        this, [this](const QString& dir) {
            if (!dir.isEmpty() && m_deviceController) {
                LOG_INFO(LocalQTCompat::fromLocal8Bit("处理命令目录变更: %1").arg(dir));
                m_deviceController->setCommandDirectory(dir);
            }
        });

    LOG_INFO(LocalQTCompat::fromLocal8Bit("完成信号连接"));
}

void FX3MainController::handleDeviceArrival()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备到达事件"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略设备到达事件"));
        return;
    }

    // 使用设备控制器处理设备到达事件
    if (m_deviceController) {
        m_deviceController->checkAndOpenDevice();
    }
}

void FX3MainController::handleDeviceRemoval()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备移除事件"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略设备移除事件"));
        return;
    }

    // 更新应用状态机状态
    AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED,
        LocalQTCompat::fromLocal8Bit("设备已移除"));
}

void FX3MainController::handleClose()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理关闭事件"));
    m_mainModel->setClosing(true);

    if (m_moduleManager) {
        m_moduleManager->notifyAllModules(ModuleManager::ModuleEvent::APP_CLOSING);
    }

    shutdown();
}

bool FX3MainController::validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& format)
{
    if (!m_mainView) {
        return false;
    }

    try {
        // 从模型获取图像参数
        m_mainModel->getVideoConfig(width, height, format);

        // 验证参数合法性
        if (width == 0 || width > 4096) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像宽度"));
            m_mainView->showErrorMessage(
                LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值"));
            return false;
        }

        if (height == 0 || height > 4096) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像高度"));
            m_mainView->showErrorMessage(
                LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("无效的图像高度，请输入1-4096之间的值"));
            return false;
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("图像参数验证通过 - 宽度: %1, 高度: %2, 类型: 0x%3")
            .arg(width).arg(height).arg(format, 2, 16, QChar('0')));

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("验证图像参数异常: %1").arg(e.what()));
        return false;
    }
}

//---------------------- 用户操作处理槽函数 ----------------------

void FX3MainController::slot_FX3Main_C_handleStartTransfer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理开始传输请求"));

    if (m_moduleManager && !m_moduleManager->isModuleInitialized(ModuleManager::ModuleType::FILE_OPTIONS)) {
        // 检查文件保存模块是否已初始化
        LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化文件保存模块"));

        // 使用现有的TAB管理机制显示模块
        handleModuleDisplay(ModuleManager::ModuleType::FILE_OPTIONS);
        return;
    }

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略开始请求"));
        return;
    }

    // 获取图像参数
    uint16_t width;
    uint16_t height;
    uint8_t format;

    if (validateImageParameters(width, height, format)) {
        // 使用设备控制器启动传输
        if (m_deviceController) {
            // 设置设备控制器中的图像参数
            m_deviceController->setImageParameters(width, height, format);
            m_deviceController->startTransfer();
        }
    }
}

void FX3MainController::slot_FX3Main_C_handleStopTransfer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主控制器处理停止传输请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略停止请求"));
        return;
    }

    if (m_deviceController) {
        m_deviceController->stopTransfer();
    }
}

void FX3MainController::slot_FX3Main_C_handleResetDevice()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理重置设备请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略重置请求"));
        return;
    }

    if (m_deviceController) {
        m_deviceController->resetDevice();
    }
}

void FX3MainController::slot_FX3Main_C_handleChannelConfig()
{
    handleModuleDisplay(ModuleManager::ModuleType::CHANNEL_CONFIG);
}

void FX3MainController::slot_FX3Main_C_handleDataAnalysis()
{
    handleModuleDisplay(ModuleManager::ModuleType::DATA_ANALYSIS);
}

void FX3MainController::slot_FX3Main_C_handleVideoDisplay()
{
    handleModuleDisplay(ModuleManager::ModuleType::VIDEO_DISPLAY);
}

void FX3MainController::slot_FX3Main_C_handleWaveformAnalysis()
{
    handleModuleDisplay(ModuleManager::ModuleType::WAVEFORM_ANALYSIS);
}

void FX3MainController::slot_FX3Main_C_handleFileSave()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理保存文件请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 获取图像参数
    uint16_t width;
    uint16_t height;
    uint8_t format;

    if (validateImageParameters(width, height, format)) {
        // 设置模型中的视频配置
        m_mainModel->setVideoConfig(width, height, format);

        if (m_moduleManager) {
            //m_moduleManager->TODO ();
        }
    }
}

void FX3MainController::slot_FX3Main_C_handleDataExport()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理导出数据请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略导出请求"));
        return;
    }

    if (m_mainView) {
        m_mainView->showInfoMessage(
            LocalQTCompat::fromLocal8Bit("提示"),
            LocalQTCompat::fromLocal8Bit("导出数据功能正在开发中")
        );
    }
}

void FX3MainController::slot_FX3Main_C_handleFileOption()
{
    handleModuleDisplay(ModuleManager::ModuleType::FILE_OPTIONS);
}

void FX3MainController::slot_FX3Main_C_handleSettings()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理设置请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略设置请求"));
        return;
    }

    if (m_mainView) {
        m_mainView->showInfoMessage(
            LocalQTCompat::fromLocal8Bit("提示"),
            LocalQTCompat::fromLocal8Bit("应用设置功能正在开发中")
        );
    }
}

void FX3MainController::slot_FX3Main_C_handleClearLog()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理清除日志请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略清除日志请求"));
        return;
    }

    if (m_mainView) {
        m_mainView->clearLogbox();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("日志已清除"));
    }
}

void FX3MainController::slot_FX3Main_C_handleHelpContent()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理帮助内容请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略帮助请求"));
        return;
    }

    if (m_mainView) {
        m_mainView->showInfoMessage(
            LocalQTCompat::fromLocal8Bit("提示"),
            LocalQTCompat::fromLocal8Bit("帮助文档正在编写中")
        );
    }
}

void FX3MainController::slot_FX3Main_C_handleAboutDialog()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理关于对话框请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略关于对话框请求"));
        return;
    }

    if (m_mainView) {
        m_mainView->showAboutDialog();
    }
}

void FX3MainController::slot_FX3Main_C_handleSelectCommandDir()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理选择命令目录请求"));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略选择目录请求"));
        return;
    }

    if (m_mainView) {
        QString dir = QFileDialog::getExistingDirectory(
            m_mainView,
            LocalQTCompat::fromLocal8Bit("选择命令文件目录"),
            QDir::currentPath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

        if (dir.isEmpty()) {
            return;
        }

        // 更新模型中的命令目录
        m_mainModel->setCommandDirectory(dir);

        // 更新UI显示选中的目录
        m_mainView->setCommandDirDisplay(dir);

        // 尝试加载命令文件
        if (m_deviceController && !m_deviceController->setCommandDirectory(dir)) {
            m_mainView->showErrorMessage(
                LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("无法加载命令文件，请确保目录包含所需的所有命令文件")
            );
            m_mainView->setCommandDirDisplay("");
            m_mainModel->setCommandDirectory("");
        }
    }
}

void FX3MainController::slot_FX3Main_C_handleDeviceUpdate()
{
    handleModuleDisplay(ModuleManager::ModuleType::DEVICE_UPDATE);
}

void FX3MainController::slot_FX3Main_C_handleModuleTabClosed(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理模块标签页关闭，索引: %1").arg(index));

    if (m_moduleManager) {
        // 确保模块管理器知道索引并能正确处理
        m_moduleManager->handleModuleTabClosed(index);
    }
}

//---------------------- 状态和事件处理槽函数 ----------------------

void FX3MainController::slot_FX3Main_C_onChannelConfigChanged(const ChannelConfig& config) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置已更新"));

    // 更新模型
    m_mainModel->setVideoConfig(config.videoWidth, config.videoHeight, 0x39);

    // 更新视图
    if (m_mainView) {
        m_mainView->setVideoParamsDisplay(config.videoWidth, config.videoHeight, 1);
    }

    // 更新设备控制器
    if (m_deviceController) {
        m_deviceController->setImageParameters(config.videoWidth, config.videoHeight, 0x39);
    }

    // 通知所有模块配置已更改
    if (m_moduleManager) {
        m_moduleManager->notifyAllModules(ModuleManager::ModuleEvent::CONFIG_CHANGED, QVariant::fromValue(config));
    }
}

void FX3MainController::slot_FX3Main_C_onVideoDisplayStatusChanged(bool isRunning)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频显示状态变更: %1").arg(isRunning ? "运行中" : "已停止"));

    // 通知模块管理器更新视频显示状态
    // 当视频显示启动时，添加相关数据源连接
    if (isRunning && m_deviceController) {
        // 这里可以添加连接设备数据到视频显示的逻辑
    }
    else if (!isRunning && m_deviceController) {
        // 视频显示停止时，可以考虑减少不必要的处理
    }
}

void FX3MainController::slot_FX3Main_C_onSaveCompleted(const QString& path, uint64_t totalBytes)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存完成: 路径=%1, 总大小=%2 字节")
        .arg(path).arg(totalBytes));

    // 通知视图显示成功消息
    if (m_mainView) {
        m_mainView->showInfoMessage(
            LocalQTCompat::fromLocal8Bit("保存成功"),
            LocalQTCompat::fromLocal8Bit("文件已保存到: %1\n总大小: %2 字节")
            .arg(path).arg(totalBytes)
        );
    }
}

void FX3MainController::slot_FX3Main_C_onSaveError(const QString& error)
{
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误: %1").arg(error));

    // 通知视图显示错误消息
    if (m_mainView) {
        m_mainView->showErrorMessage(
            LocalQTCompat::fromLocal8Bit("保存错误"),
            error
        );
    }
}

void FX3MainController::slot_FX3Main_C_onAppStateChanged(AppState state, AppState oldState, const QString& reason)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序状态从 %1 变更为 %2，原因: %3")
        .arg(static_cast<int>(oldState))
        .arg(static_cast<int>(state))
        .arg(reason));

    // 更新菜单控制器状态
    if (m_menuController) {
        m_menuController->updateMenuStateForAppState(state);
    }

    // 在这里可以添加特定状态的处理逻辑
    switch (state) {
    case AppState::DEVICE_ABSENT:
        // 设备不存在时的处理
        break;
    case AppState::IDLE:
        // 空闲状态处理
        break;
    case AppState::TRANSFERRING:
        // 传输状态处理
        break;
    case AppState::CONFIGURED:
        // 已配置状态处理
        break;
    case AppState::DEVICE_ERROR:
        // 设备错误状态处理
        break;
    case AppState::SHUTDOWN:
        // 关闭状态处理
        m_mainModel->setClosing(true);
        break;
    default:
        break;
    }
}

void FX3MainController::slot_FX3Main_C_onTransferStateChanged(bool transferring)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("传输状态变更: %1").
        arg(transferring ? "传输中" : "已停止"));

    // 在这里处理传输状态变更
    if (transferring) {
        // 传输开始
        // 重置传输统计信息
        m_mainModel->resetTransferStats();
    }
    else {
        // 传输停止
        // 可能需要处理数据关闭或清理工作
    }

    if (m_mainView) {
        if (MainUiStateManager* uiStateManager = m_mainView->getUiStateManager()) {
            uiStateManager->slot_MainUI_STM_onTransferStateChanged(transferring);
        }
        else {
            m_mainView->updateStatusBar(
                transferring ? LocalQTCompat::fromLocal8Bit("数据传输中...") : LocalQTCompat::fromLocal8Bit("传输已停止"),
                3000
            );
        }
    }
}

void FX3MainController::slot_FX3Main_C_handleTransferStateChanged(bool transferring) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主设备控制器处理传输状态变更: %1").arg(transferring ? "传输中" : "已停止"));

    // 通知模型
    m_mainModel->setTransferring(transferring);

    // 通知模块管理器 - 使用新的事件系统
    if (m_moduleManager) {
        ModuleManager::ModuleEvent event = transferring ?
            ModuleManager::ModuleEvent::TRANSFER_STARTED :
            ModuleManager::ModuleEvent::TRANSFER_STOPPED;
        m_moduleManager->notifyAllModules(event);
    }

    // 更新UI和应用状态机
    if (m_mainView && m_mainView->getUiStateManager()) {
        m_mainView->getUiStateManager()->slot_MainUI_STM_onTransferStateChanged(transferring);
    }

    StateEvent stateEvent = transferring ? StateEvent::TRANSFER_STARTED : StateEvent::STOP_SUCCEEDED;
    QString reason = transferring ? LocalQTCompat::fromLocal8Bit("传输已开始") : LocalQTCompat::fromLocal8Bit("传输已停止");
    AppStateMachine::instance().processEvent(stateEvent, reason);
}

void FX3MainController::slot_FX3Main_C_handleTransferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint32_t elapseMs)
{
    // 更新模型
    if (m_mainModel) {
        m_mainModel->updateTransferStats(bytesTransferred, transferRate, elapseMs);
    }
}

void FX3MainController::slot_FX3Main_C_handleUsbSpeedUpdated(const QString& speedDesc, bool isUSB3, bool isConnected)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主控制器中USB速度更新: %1, %2, %3").arg(speedDesc).arg(isUSB3 ? "u3" : "no-u3").arg(isConnected ? "已连接" : "未连接"));

    // 更新UI
    if (m_mainView) {
        m_mainView->updateUsbSpeedDisplay(speedDesc, isUSB3, isConnected);
    }
}

void FX3MainController::slot_FX3Main_C_handleDeviceError(const QString& title, const QString& message)
{
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));

    // 显示错误消息
    if (m_mainView) {
        m_mainView->showErrorMessage(title, message);
    }
}

void FX3MainController::slot_FX3Main_C_onTransferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint64_t elapseMs)
{
    // 更新UI显示
    if (m_mainView) {
        m_mainView->updateTransferStatsDisplay(bytesTransferred, transferRate, elapseMs);
    }

    static uint64_t lastLoggedBytes = 0;
    static QTime lastLogTime = QTime::currentTime();
    QTime currentTime = QTime::currentTime();

    // 每7秒或每800Mb记录一次
    if (lastLoggedBytes == 0 ||
        bytesTransferred - lastLoggedBytes > 800 * 1024 * 1024 ||
        lastLogTime.msecsTo(currentTime) > 10000) {

        LOG_INFO(LocalQTCompat::fromLocal8Bit("传输统计 - 总数据: %1 Bytes, 速率: %2 MB/s, 时间: %3 ms")
            .arg(bytesTransferred)
            .arg(transferRate, 0, 'f', 2)
            .arg(elapseMs));

        lastLoggedBytes = bytesTransferred;
        lastLogTime = currentTime;
    }
}

void FX3MainController::slot_FX3Main_C_handleStartButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主控制器: 处理开始按钮点击"));
    if (m_deviceController) {
        m_deviceController->startTransfer();
    }
}

void FX3MainController::slot_FX3Main_C_handleStopButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主控制器: 处理停止按钮点击"));
    if (m_deviceController) {
        m_deviceController->stopTransfer();
    }
}

void FX3MainController::slot_FX3Main_C_handleResetButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主控制器: 处理重置按钮点击"));
    if (m_deviceController) {
        m_deviceController->resetDevice();
    }
}

void FX3MainController::slot_FX3Main_C_handleDataPacketAvailable(const std::vector<DataPacket>& packets)
{
    // LOG_INFO(LocalQTCompat::fromLocal8Bit("主控制器: 数据来了: %1").arg(packets.size()));
    // 数据包处理逻辑
    // 例如转发到文件保存控制器、视频显示模块等
    // 转发到模块管理器
    if (m_moduleManager) {
        // Use ModuleManager's notification system
        // m_moduleManager->notifyAllModules(ModuleManager::ModuleEvent::DATA_AVAILABLE, QVariant::fromValue(packet));
        m_moduleManager->notifyAllModules(ModuleManager::ModuleEvent::DATA_AVAILABLE, QVariant::fromValue(packets));
    }
}

//---------------------- 辅助方法 ----------------------

int FX3MainController::formatToIndex(uint8_t format) const
{
    // 转换格式代码到UI索引
    switch (format) {
    case 0x38: return 0; // RAW8
    case 0x39: return 1; // RAW10
    case 0x3A: return 2; // RAW12
    default: return 1;   // 默认RAW10
    }
}

void FX3MainController::initializeConnections()
{
    // 从设备控制器获取初始状态和参数
    if (m_deviceController) {
        // 获取图像参数
        uint16_t width, height;
        uint8_t captureType;
        m_deviceController->getImageParameters(width, height, captureType);

        // 更新模型
        m_mainModel->setVideoConfig(width, height, captureType);

        // 更新视图
        if (m_mainView) {
            m_mainView->setVideoParamsDisplay(width, height, formatToIndex(captureType));
        }
    }

    // 连接DeviceModel状态变化到MainUiStateManager
    connect(DeviceModel::getInstance(), &DeviceModel::signal_Dev_M_deviceStateChanged,
        m_mainView->getUiStateManager(), &MainUiStateManager::slot_MainUI_STM_onDeviceStateChanged);

    // 更新UI状态
    updateUIFromModel();
}

void FX3MainController::updateUIFromModel()
{
    if (!m_mainView) {
        return;
    }

    // 获取设备信息
    QString deviceName, firmwareVersion, serialNumber;
    m_mainModel->getDeviceInfo(deviceName, firmwareVersion, serialNumber);
    m_mainView->updateDeviceInfoDisplay(deviceName, firmwareVersion, serialNumber);

    // 获取传输统计
    uint64_t bytesTransferred;
    double transferRate;
    uint32_t errorCount;
    m_mainModel->getTransferStats(bytesTransferred, transferRate, errorCount);
    m_mainView->updateTransferStatsDisplay(bytesTransferred, transferRate, errorCount);

    // 获取视频配置
    uint16_t width, height;
    uint8_t format;
    m_mainModel->getVideoConfig(width, height, format);
    m_mainView->setVideoParamsDisplay(width, height, formatToIndex(format));

    // 获取命令目录
    m_mainView->setCommandDirDisplay(m_mainModel->getCommandDirectory());
}

void FX3MainController::handleModuleDisplay(ModuleManager::ModuleType type) {
    QString moduleName = ModuleManager::getModuleTypeName(type);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理%1模块显示请求").arg(moduleName));

    if (m_mainModel->isClosing()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    if (m_moduleManager) {
        m_moduleManager->showModule(type);
    }
}