#include <windows.h>
#include <dbt.h>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QThread>
#include <QString>
#include <QFileDialog>
#include <QDebug>
#include <QTimer>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QFuture>
#include <QToolBar>
#include <QObjectCleanupHandler>
#include <QtConcurrent/QtConcurrent>

#include "FX3ToolMainWin.h"
#include "Logger.h"
#include "DataConverters.h"

std::atomic<bool> FX3ToolMainWin::s_resourcesReleased{ false };

FX3ToolMainWin::FX3ToolMainWin(QWidget* parent)
    : QMainWindow(parent)
    , m_isClosing(false)
    , m_loggerInitialized(false)
    , m_deviceManager(nullptr)
    , m_uiStateHandler(nullptr)
    , m_layoutManager(nullptr)
    , m_deviceController(nullptr)
    , m_menuController(nullptr)
    , m_moduleManager(nullptr)
    , m_fileSaveController(nullptr)
    , m_saveFileBox(nullptr)
    , m_channelSelectWidget(nullptr)
    , m_dataAnalysisWidget(nullptr)
    , m_updataDeviceWidget(nullptr)
    , m_videoDisplayWidget(nullptr)
{
    ui.setupUi(this);

    try {
        // 初始化日志系统
        if (!initializeLogger()) {
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("日志系统初始化失败，应用程序无法继续"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序启动，Qt版本: %1").arg(QT_VERSION_STR));

        // 创建UI状态处理器
        m_uiStateHandler = new UIStateHandler(ui, this);

        // 创建设备管理器
        m_deviceManager = new FX3DeviceManager(this);

        // 创建布局管理器
        m_layoutManager = new FX3UILayoutManager(this);
        m_layoutManager->initializeMainLayout();

        // 创建设备控制器
        m_deviceController = new FX3DeviceController(this, m_deviceManager);

        // 创建菜单控制器
        m_menuController = new FX3MenuController(this);

        // 创建模块管理器
        m_moduleManager = new FX3ModuleManager(this);

        // 创建文件保存控制器
        m_fileSaveController = new FileSaveController(this);
        if (!m_fileSaveController->initialize()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存控制器初始化失败"));
        }

        // 初始化所有信号连接
        initializeConnections();

        // 启动设备检测
        registerDeviceNotification();

        // 设置初始状态并初始化设备
        AppStateMachine::instance().processEvent(StateEvent::APP_INIT,
            LocalQTCompat::fromLocal8Bit("应用程序初始化完成"));

        if (!m_deviceManager->initializeDeviceAndManager((HWND)this->winId())) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("设备初始化失败，应用将以离线模式运行"));
            QMessageBox::warning(this, LocalQTCompat::fromLocal8Bit("警告"),
                LocalQTCompat::fromLocal8Bit("设备初始化失败，将以离线模式运行"));
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3ToolMainWin构造函数完成..."));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("初始化过程中发生异常: %1").arg(e.what()));
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化异常: %1").arg(e.what()));
        QTimer::singleShot(0, this, &FX3ToolMainWin::close);
    }
    catch (...) {
        QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("初始化过程中发生未知异常"));
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化未知异常"));
        QTimer::singleShot(0, this, &FX3ToolMainWin::close);
    }
}

FX3ToolMainWin::~FX3ToolMainWin() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3ToolMainWin析构函数入口"));
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置关闭标志"));
    m_isClosing = true;

    // 检查资源是否已在closeEvent中释放
    if (!s_resourcesReleased) {
        if (m_deviceManager) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除设备管理器"));
            delete m_deviceManager;
            m_deviceManager = nullptr;
        }

        if (m_uiStateHandler) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除UI状态处理器"));
            delete m_uiStateHandler;
            m_uiStateHandler = nullptr;
        }

        if (m_fileSaveController) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除文件保存控制器"));
            delete m_fileSaveController;
            m_fileSaveController = nullptr;
        }

        // 释放其他子窗口资源
        if (m_channelSelectWidget) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除通道选择窗口"));
            delete m_channelSelectWidget;
            m_channelSelectWidget = nullptr;
        }

        if (m_dataAnalysisWidget) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除数据分析窗口"));
            delete m_dataAnalysisWidget;
            m_dataAnalysisWidget = nullptr;
        }

        if (m_updataDeviceWidget) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除设备升级窗口"));
            delete m_updataDeviceWidget;
            m_updataDeviceWidget = nullptr;
        }

        if (m_videoDisplayWidget) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除视频显示窗口"));
            delete m_videoDisplayWidget;
            m_videoDisplayWidget = nullptr;
        }
    }
    else {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("资源已在closeEvent中释放"));
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3ToolMainWin析构函数退出 - 成功"));
}

bool FX3ToolMainWin::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    MSG* msg = reinterpret_cast<MSG*>(message);

    if (msg->message == WM_DEVICECHANGE) {
        switch (msg->wParam) {
        case DBT_DEVICEARRIVAL: {
            DEV_BROADCAST_HDR* deviceHeader = reinterpret_cast<DEV_BROADCAST_HDR*>(msg->lParam);
            if (deviceHeader->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                DEV_BROADCAST_DEVICEINTERFACE* deviceInterface =
                    reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(msg->lParam);

                // 检查是否是我们的FX3设备
                if (IsEqualGUID(deviceInterface->dbcc_classguid, CYUSBDRV_GUID)) {
                    if (m_deviceManager) {
                        m_deviceManager->onDeviceArrival();
                    }
                }
            }
            break;
        }
        case DBT_DEVICEREMOVECOMPLETE: {
            DEV_BROADCAST_HDR* deviceHeader = reinterpret_cast<DEV_BROADCAST_HDR*>(msg->lParam);
            if (deviceHeader->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                DEV_BROADCAST_DEVICEINTERFACE* deviceInterface =
                    reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(msg->lParam);

                // 检查是否是我们的FX3设备
                if (IsEqualGUID(deviceInterface->dbcc_classguid, CYUSBDRV_GUID)) {
                    if (m_deviceManager) {
                        m_deviceManager->onDeviceRemoval();
                    }
                }
            }
            break;
        }
        }
    }

    return false;
}

void FX3ToolMainWin::closeEvent(QCloseEvent* event) {
    // 使用互斥锁防止重复关闭
    static std::mutex closeMutex;
    if (!closeMutex.try_lock()) {
        // 如果已经在关闭中，直接接受事件
        event->accept();
        return;
    }

    // 设置关闭标志，防止新的操作
    m_isClosing = true;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序关闭中，正在清理资源..."));

    try {
        // 1. 首先停止所有定时器
        QList<QTimer*> timers = findChildren<QTimer*>();
        for (QTimer* timer : timers) {
            if (timer && timer->isActive()) {
                timer->stop();
                disconnect(timer, nullptr, nullptr, nullptr);
            }
        }

        // 2. 停止文件保存（如果正在进行）
        if (m_fileSaveController && m_fileSaveController->isSaving()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
            m_fileSaveController->stopSaving();

            // 短暂等待让保存操作完成
            QThread::msleep(100);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        // 3. 关闭所有子窗口
        if (m_saveFileBox && m_saveFileBox->isVisible()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭文件保存对话框"));
            m_saveFileBox->close();
        }

        if (m_channelSelectWidget && m_channelSelectWidget->isVisible()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭通道选择窗口"));
            m_channelSelectWidget->close();
        }

        if (m_dataAnalysisWidget && m_dataAnalysisWidget->isVisible()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭数据分析窗口"));
            m_dataAnalysisWidget->close();
        }

        if (m_updataDeviceWidget && m_updataDeviceWidget->isVisible()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭设备升级窗口"));
            m_updataDeviceWidget->close();
        }

        if (m_videoDisplayWidget && m_videoDisplayWidget->isVisible()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭视频显示窗口"));
            m_videoDisplayWidget->close();
        }

        // 4. 通知UI状态处理器准备关闭
        if (m_uiStateHandler) {
            m_uiStateHandler->prepareForClose();
        }

        // 5. 断开状态机信号连接，避免在关闭过程中的信号干扰
        disconnect(&AppStateMachine::instance(), nullptr, this, nullptr);
        if (m_uiStateHandler) {
            disconnect(&AppStateMachine::instance(), nullptr, m_uiStateHandler, nullptr);
        }

        // 6. 发送关闭事件到状态机
        AppStateMachine::instance().processEvent(StateEvent::APP_SHUTDOWN,
            LocalQTCompat::fromLocal8Bit("应用程序正在关闭"));

        // 7. 停止设备传输、采集和资源释放
        stopAndReleaseResources();

        // 接受关闭事件
        event->accept();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭流程执行完成，释放互斥锁"));
        closeMutex.unlock();
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("关闭过程异常: %1").arg(e.what()));
        event->accept();
        closeMutex.unlock();
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("关闭过程未知异常"));
        event->accept();
        closeMutex.unlock();
    }
}

void FX3ToolMainWin::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    slot_adjustStatusBar();
}

void FX3ToolMainWin::slot_adjustStatusBar() {
    QStatusBar* statusBar = this->statusBar();
    if (!statusBar) return;

    statusBar->setMinimumWidth(40);
    statusBar->setMinimumHeight(30);
}

void FX3ToolMainWin::slot_initializeUI() {
    // 这个方法可能已被layoutManager替代，保留方法以防需要
}

void FX3ToolMainWin::slot_showAboutDialog() {
    QMessageBox::about(this,
        LocalQTCompat::fromLocal8Bit("关于FX3传输测试工具"),
        LocalQTCompat::fromLocal8Bit("FX3传输测试工具 v1.0\n\n用于FX3设备的数据传输和测试\n\n© 2025 公司名称")
    );
}

void FX3ToolMainWin::slot_onClearLogTriggered() {
    if (ui.logTextEdit) {
        ui.logTextEdit->clear();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("日志已清除"));
    }
}

void FX3ToolMainWin::slot_onExportDataTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("导出数据功能尚未实现"));
    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("提示"),
        LocalQTCompat::fromLocal8Bit("导出数据功能正在开发中"));
}

void FX3ToolMainWin::slot_onSettingsTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("应用设置功能尚未实现"));
    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("提示"),
        LocalQTCompat::fromLocal8Bit("应用设置功能正在开发中"));
}

void FX3ToolMainWin::slot_onHelpContentTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("帮助内容功能尚未实现"));
    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("提示"),
        LocalQTCompat::fromLocal8Bit("帮助文档正在编写中"));
}

bool FX3ToolMainWin::initializeLogger() {
    if (m_loggerInitialized) {
        return true;
    }

    try {
        QString logPath = QApplication::applicationDirPath() + "/fx3_t.log";
        Logger::instance().setLogFile(logPath);
        if (ui.logTextEdit) {
            Logger::instance().setLogWidget(ui.logTextEdit);
        }
        else {
            throw std::runtime_error("未找到日志控件");
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("日志: %1").arg(logPath));

        m_loggerInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "日志初始化失败:" << e.what();
        return false;
    }
}

void FX3ToolMainWin::setupMenuBar() {
    // 动态设置菜单项状态和属性
    updateMenuBarState(AppStateMachine::instance().currentState());

    // 可能的版本检查和动态菜单项
    checkForUpdates();
}

void FX3ToolMainWin::updateMenuBarState(AppState state) {
    // 获取菜单动作
    QAction* startAction = ui.actionStartTransfer;
    QAction* stopAction = ui.actionStopTransfer;
    QAction* resetAction = ui.actionResetDevice;
    QAction* channelAction = ui.actionChannelConfig;
    QAction* dataAction = ui.actionDataAnalysis;
    QAction* videoAction = ui.actionVideoDisplay;
    QAction* saveAction = ui.actionSaveFile;
    QAction* exportAction = ui.actionExportData;

    // 根据状态设置菜单项启用/禁用
    bool transferring = (state == AppState::TRANSFERRING);
    bool deviceConnected = (state != AppState::DEVICE_ABSENT &&
        state != AppState::DEVICE_ERROR);
    bool idle = (state == AppState::IDLE || state == AppState::CONFIGURED);

    // 传输控制
    startAction->setEnabled(idle && deviceConnected);
    stopAction->setEnabled(transferring);
    resetAction->setEnabled(deviceConnected && !transferring);

    // 功能模块
    channelAction->setEnabled(deviceConnected && !transferring);
    dataAction->setEnabled(deviceConnected);
    videoAction->setEnabled(deviceConnected);

    // 文件操作
    saveAction->setEnabled(idle);
    exportAction->setEnabled(idle);
}

void FX3ToolMainWin::checkForUpdates() {
    // 检查更新，可能在帮助菜单中添加"检查更新"项
    QMenu* helpMenu = menuBar()->findChild<QMenu*>("menuHelp");
    if (helpMenu) {
        // 如有必要，动态添加更新检查
        // helpMenu->addAction(...);
    }
}

void FX3ToolMainWin::initializeMainUI()
{
    // 这个方法可能已被layoutManager替代，保留方法以防需要
}

QWidget* FX3ToolMainWin::createHomeTabContent()
{
    // 这个方法可能已被layoutManager替代，保留方法以防需要
    return new QWidget(this);
}

void FX3ToolMainWin::updateStatusPanel()
{
    // 这个方法可能已被layoutManager替代，保留方法以防需要
}

void FX3ToolMainWin::createMainToolBar()
{
    // 这个方法可能已被layoutManager替代，保留方法以防需要
}

void FX3ToolMainWin::registerDeviceNotification() {
    DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
    ZeroMemory(&notificationFilter, sizeof(notificationFilter));
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notificationFilter.dbcc_classguid = CYUSBDRV_GUID;

    HWND hwnd = (HWND)this->winId();
    HDEVNOTIFY hDevNotify = RegisterDeviceNotification(
        hwnd,
        &notificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    if (!hDevNotify) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("注册Fx3 USB设备通知失败: %1")
            .arg(GetLastError()));
    }
    else {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("Fx3 USB设备通知注册成功"));
    }
}

void FX3ToolMainWin::initializeConnections()
{
    // 按钮信号连接
    connect(ui.startButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onStartButtonClicked);
    connect(ui.stopButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onStopButtonClicked);
    connect(ui.resetButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onResetButtonClicked);

    // 工具栏动作连接
    QAction* startAction = findChild<QAction*>("toolbarStartAction");
    QAction* stopAction = findChild<QAction*>("toolbarStopAction");
    QAction* resetAction = findChild<QAction*>("toolbarResetAction");
    QAction* channelAction = findChild<QAction*>("toolbarChannelAction");
    QAction* dataAction = findChild<QAction*>("toolbarDataAction");
    QAction* videoAction = findChild<QAction*>("toolbarVideoAction");
    QAction* waveformAction = findChild<QAction*>("toolbarWaveformAction");
    QAction* saveAction = findChild<QAction*>("toolbarSaveAction");

    if (startAction) connect(startAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onStartButtonClicked);
    if (stopAction) connect(stopAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onStopButtonClicked);
    if (resetAction) connect(resetAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onResetButtonClicked);
    if (channelAction) connect(channelAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    if (dataAction) connect(dataAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    if (videoAction) connect(videoAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    if (waveformAction) connect(waveformAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowWaveformAnalysisTriggered);
    if (saveAction) connect(saveAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

    // 快捷按钮连接
    QPushButton* quickChannelBtn = findChild<QPushButton*>("quickChannelBtn");
    QPushButton* quickDataBtn = findChild<QPushButton*>("quickDataBtn");
    QPushButton* quickVideoBtn = findChild<QPushButton*>("quickVideoBtn");
    QPushButton* quickWaveformBtn = findChild<QPushButton*>("quickWaveformBtn");
    QPushButton* quickSaveBtn = findChild<QPushButton*>("quickSaveBtn");

    if (quickChannelBtn) connect(quickChannelBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    if (quickDataBtn) connect(quickDataBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    if (quickVideoBtn) connect(quickVideoBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    if (quickWaveformBtn) connect(quickWaveformBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowWaveformAnalysisTriggered);
    if (quickSaveBtn) connect(quickSaveBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

    // 主页快捷按钮连接
    QPushButton* homeChannelBtn = findChild<QPushButton*>("homeChannelBtn");
    QPushButton* homeDataBtn = findChild<QPushButton*>("homeDataBtn");
    QPushButton* homeVideoBtn = findChild<QPushButton*>("homeVideoBtn");
    QPushButton* homeSaveBtn = findChild<QPushButton*>("homeSaveBtn");

    if (homeChannelBtn) connect(homeChannelBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    if (homeDataBtn) connect(homeDataBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    if (homeVideoBtn) connect(homeVideoBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    if (homeSaveBtn) connect(homeSaveBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

    // 菜单控制器连接
    if (m_menuController) {
        connect(m_menuController, &FX3MenuController::menuActionTriggered, this,
            [this](const QString& action) {
                if (action == "start") {
                    slot_onStartButtonClicked();
                }
                else if (action == "stop") {
                    slot_onStopButtonClicked();
                }
                else if (action == "reset") {
                    slot_onResetButtonClicked();
                }
                else if (action == "channel") {
                    slot_onShowChannelSelectTriggered();
                }
                else if (action == "data") {
                    slot_onShowDataAnalysisTriggered();
                }
                else if (action == "video") {
                    slot_onShowVideoDisplayTriggered();
                }
                else if (action == "waveform") {
                    slot_onShowWaveformAnalysisTriggered();
                }
                else if (action == "save") {
                    slot_onShowSaveFileBoxTriggered();
                }
                else if (action == "export") {
                    slot_onExportDataTriggered();
                }
                else if (action == "settings") {
                    slot_onSettingsTriggered();
                }
                else if (action == "clearLog") {
                    slot_onClearLogTriggered();
                }
                else if (action == "help") {
                    slot_onHelpContentTriggered();
                }
                else if (action == "about") {
                    slot_showAboutDialog();
                }
            });
    }

    // 模块管理器连接
    if (m_moduleManager) {
        connect(m_moduleManager, &FX3ModuleManager::moduleSignal, this,
            [this](const QString& signal, const QVariant& data) {
                if (signal == "channelConfigChanged") {
                    ChannelConfig config = data.value<ChannelConfig>();
                    slot_onChannelConfigChanged(config);
                }
                else if (signal == "showSaveFileBox") {
                    slot_onShowSaveFileBoxTriggered();
                }
                else if (signal == "showVideoDisplay") {
                    slot_onShowVideoDisplayTriggered();
                }
                else if (signal == "videoDisplayStatusChanged") {
                    slot_onVideoDisplayStatusChanged(data.toBool());
                }
                else if (signal == "saveCompleted") {
                    QPair<QString, uint64_t> result = data.value<QPair<QString, uint64_t>>();
                    slot_onSaveCompleted(result.first, result.second);
                }
                else if (signal == "saveError") {
                    slot_onSaveError(data.toString());
                }
            });
    }

    // 设备管理器连接
    if (m_deviceManager && m_uiStateHandler) {
        connect(m_deviceManager, &FX3DeviceManager::transferStatsUpdated,
            m_uiStateHandler, &UIStateHandler::updateTransferStats);
        connect(m_deviceManager, &FX3DeviceManager::usbSpeedUpdated,
            m_uiStateHandler, &UIStateHandler::updateUsbSpeedDisplay);
        connect(m_deviceManager, &FX3DeviceManager::deviceError,
            m_uiStateHandler, &UIStateHandler::showErrorMessage);
    }

    // 文件保存控制器连接
    if (m_fileSaveController) {
        connect(m_fileSaveController, &FileSaveController::saveCompleted,
            this, &FX3ToolMainWin::slot_onSaveCompleted);
        connect(m_fileSaveController, &FileSaveController::saveError,
            this, &FX3ToolMainWin::slot_onSaveError);
    }

    // 状态机连接
    connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged, this,
        [this](AppState newState, AppState oldState, const QString& reason) {
            if (m_menuController) {
                m_menuController->updateMenuBarState(newState);
            }
            if (m_uiStateHandler) {
                m_uiStateHandler->onStateChanged(newState, oldState, reason);
            }
        });
}

void FX3ToolMainWin::stopAndReleaseResources() {
    // 1. 确保设备管理器停止所有传输
    if (m_deviceManager) {
        if (m_deviceManager->isTransferring()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("停止正在进行的数据传输"));
            m_deviceManager->stopTransfer();

            // 给一定时间让设备管理器完成停止操作
            QElapsedTimer timer;
            timer.start();
            while (m_deviceManager->isTransferring() && timer.elapsed() < 300) {
                QThread::msleep(10);
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
    }

    // 2. 停止文件保存（如果正在进行）
    if (m_fileSaveController && m_fileSaveController->isSaving()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
        m_fileSaveController->stopSaving();

        // 短暂等待让保存操作完成
        QThread::msleep(100);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // 3. 创建单独的智能指针保存各个组件，以便在类析构前释放
    std::unique_ptr<FX3DeviceManager> deviceManagerPtr(m_deviceManager);
    std::unique_ptr<UIStateHandler> uiHandlerPtr(m_uiStateHandler);
    std::unique_ptr<FileSaveController> fileSaveControllerPtr(m_fileSaveController);
    std::unique_ptr<FX3DeviceController> deviceControllerPtr(m_deviceController);
    std::unique_ptr<FX3MenuController> menuControllerPtr(m_menuController);
    std::unique_ptr<FX3ModuleManager> moduleManagerPtr(m_moduleManager);
    std::unique_ptr<FX3UILayoutManager> layoutManagerPtr(m_layoutManager);

    // 将类成员设为nullptr防止析构函数重复释放
    m_deviceManager = nullptr;
    m_uiStateHandler = nullptr;
    m_fileSaveController = nullptr;
    m_deviceController = nullptr;
    m_menuController = nullptr;
    m_moduleManager = nullptr;
    m_layoutManager = nullptr;

    // 不持有这些窗口的所有权，不需要释放
    m_saveFileBox = nullptr;
    m_channelSelectWidget = nullptr;
    m_dataAnalysisWidget = nullptr;
    m_updataDeviceWidget = nullptr;
    m_videoDisplayWidget = nullptr;

    // 4. 先释放控制器和管理器
    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放模块管理器"));
    moduleManagerPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放菜单控制器"));
    menuControllerPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放设备控制器"));
    deviceControllerPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放文件保存控制器"));
    fileSaveControllerPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放UI布局管理器"));
    layoutManagerPtr.reset();

    // 5. 然后释放UI状态处理器
    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放UI状态处理器"));
    uiHandlerPtr.reset();

    // 6. 最后释放设备管理器 - 这会同时清理USB设备和采集管理器
    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放设备管理器"));
    deviceManagerPtr.reset();

    // 设置静态资源释放标志
    s_resourcesReleased = true;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("所有资源已释放完成"));
}

void FX3ToolMainWin::slot_onStartButtonClicked() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略开始请求"));
        return;
    }

    if (m_deviceController) {
        m_deviceController->startTransfer();
    }
}

void FX3ToolMainWin::slot_onStopButtonClicked() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略停止请求"));
        return;
    }

    if (m_deviceController) {
        m_deviceController->stopTransfer();
    }
}

void FX3ToolMainWin::slot_onResetButtonClicked() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置设备按钮点击"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略重置请求"));
        return;
    }

    if (m_deviceController) {
        m_deviceController->resetDevice();
    }
}

void FX3ToolMainWin::slot_onShowChannelSelectTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示通道配置窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    if (m_moduleManager) {
        m_moduleManager->showChannelConfigModule();
    }
}

void FX3ToolMainWin::slot_onShowDataAnalysisTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示数据分析窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    if (m_moduleManager) {
        m_moduleManager->showDataAnalysisModule();
    }
}

// 设备升级窗口触发函数
void FX3ToolMainWin::slot_onShowUpdataDeviceTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示设备升级窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 这里可以通过moduleManager处理，或者保持原来的实现
    if (m_moduleManager) {
        // 如果moduleManager有对应方法，使用moduleManager
        // m_moduleManager->showUpdateDeviceModule();
    }
    else {
        // 否则使用原始实现
        // 延迟创建设备升级窗口（单例模式）
        if (!m_updataDeviceWidget) {
            m_updataDeviceWidget = new UpdataDevice(this);

            // 连接升级完成信号
            connect(m_updataDeviceWidget, &UpdataDevice::updateCompleted,
                this, [this](bool success, const QString& message) {
                    // 更新状态显示
                    if (success) {
                        LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级成功: %1").arg(message));
                    }
                    else {
                        if (m_uiStateHandler) {
                            m_uiStateHandler->showErrorMessage(
                                LocalQTCompat::fromLocal8Bit("升级失败: ") + message, "");
                        }
                        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备升级失败: %1").arg(message));
                    }
                });
        }

        // 设备升级保持独立窗口，因为它是模态操作
        m_updataDeviceWidget->show();
        m_updataDeviceWidget->raise();
        m_updataDeviceWidget->activateWindow();
    }
}

void FX3ToolMainWin::slot_onShowVideoDisplayTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示视频窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    if (m_moduleManager) {
        m_moduleManager->showVideoDisplayModule();
    }
}

void FX3ToolMainWin::slot_onShowWaveformAnalysisTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示波形分析窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    if (m_moduleManager) {
        m_moduleManager->showWaveformModule();
    }
}

void FX3ToolMainWin::slot_onVideoDisplayStatusChanged(bool isRunning) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频显示状态变更: %1").arg(isRunning ? "运行中" : "已停止"));

    // 更新UI状态
    if (m_uiStateHandler) {
        // 可能需要实现对应方法
    }

    // 当视频显示启动时，添加相关数据源连接
    if (isRunning && m_deviceManager) {
        // 这里可以添加连接设备数据到视频显示的逻辑
        // 例如开始发送数据包到转换函数
        // 注意：实际连接应该在connectModuleDataExchange中建立
    }
    else if (!isRunning && m_deviceManager) {
        // 视频显示停止时，可以考虑减少不必要的处理
    }

    // 更新标签页图标或文本，指示状态
    if (m_mainTabWidget && m_videoDisplayTabIndex >= 0) {
        // 为视频显示标签添加运行状态指示
        QString tabText = LocalQTCompat::fromLocal8Bit("视频显示");
        if (isRunning) {
            tabText += LocalQTCompat::fromLocal8Bit(" [运行中]");
            // 可以考虑更换为"正在运行"的图标
        }
        m_mainTabWidget->setTabText(m_videoDisplayTabIndex, tabText);
    }
}

// 通道配置变更处理函数
void FX3ToolMainWin::slot_onChannelConfigChanged(const ChannelConfig& config) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置已更新"));

    // 更新主界面上的相关参数
    if (config.videoWidth > 0 && config.videoHeight > 0) {
        ui.imageWIdth->setText(QString::number(config.videoWidth));
        ui.imageHeight->setText(QString::number(config.videoHeight));
        LOG_INFO(LocalQTCompat::fromLocal8Bit("从通道配置更新图像尺寸：%1x%2")
            .arg(config.videoWidth).arg(config.videoHeight));
    }

    // 如果有设备管理器，更新设备配置
    if (m_deviceManager) {
        //m_deviceManager->updateChannelConfig(config);
    }
}

void FX3ToolMainWin::slot_onSelectCommandDirectory() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择命令文件目录"));

    if (m_isClosing) {
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(
        this,
        LocalQTCompat::fromLocal8Bit("选择命令文件目录"),
        QDir::currentPath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (dir.isEmpty()) {
        return;
    }

    // 更新UI显示选中的目录
    ui.cmdDirEdit->setText(dir);

    // 尝试加载命令文件
    if (m_deviceManager && !m_deviceManager->loadCommandFiles(dir)) {
        QMessageBox::warning(
            this,
            LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("无法加载命令文件，请确保目录包含所需的所有命令文件")
        );
        ui.cmdDirEdit->clear();
    }
}

bool FX3ToolMainWin::validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType) {
    bool conversionOk = false;

    // 获取宽度
    QString widthText = ui.imageWIdth->text();
    if (widthText.contains("Width")) {
        widthText = widthText.remove("Width").trimmed();
    }
    width = widthText.toUInt(&conversionOk);
    if (!conversionOk || width == 0 || width > 4096) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像宽度"));
        QMessageBox::warning(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值"));
        return false;
    }

    // 获取高度
    QString heightText = ui.imageHeight->text();
    if (heightText.contains("Height")) {
        heightText = heightText.remove("Height").trimmed();
    }
    height = heightText.toUInt(&conversionOk);
    if (!conversionOk || height == 0 || height > 4096) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像高度"));
        QMessageBox::warning(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("无效的图像高度，请输入1-4096之间的值"));
        return false;
    }

    // 获取采集类型
    capType = 0x39; // 默认为RAW10
    int typeIndex = ui.imageType->currentIndex();
    switch (typeIndex) {
    case 0: capType = 0x38; break; // RAW8
    case 1: capType = 0x39; break; // RAW10
    case 2: capType = 0x3A; break; // RAW12
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("图像参数验证通过 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    return true;
}

void FX3ToolMainWin::addModuleToMainTab(QWidget* widget, const QString& tabName, int& tabIndex, const QIcon& icon)
{
    if (!m_mainTabWidget || !widget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("添加模块失败：标签控件或模块窗口为空"));
        return;
    }

    // 检查是否已添加
    if (tabIndex >= 0 && tabIndex < m_mainTabWidget->count()) {
        m_mainTabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("模块标签页已存在，切换到标签页: %1").arg(tabName));
        return;
    }

    // 添加新标签页
    tabIndex = icon.isNull() ?
        m_mainTabWidget->addTab(widget, tabName) :
        m_mainTabWidget->addTab(widget, icon, tabName);

    m_mainTabWidget->setCurrentIndex(tabIndex);

    // 设置关闭按钮，除了主页标签
    if (tabIndex != m_homeTabIndex) {
        m_mainTabWidget->setTabsClosable(true);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已添加模块标签页: %1，索引: %2").arg(tabName).arg(tabIndex));
}

void FX3ToolMainWin::showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon)
{
    if (!m_mainTabWidget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("标签控件为空，无法显示模块"));
        return;
    }

    // 如果标签页已存在
    if (tabIndex >= 0 && tabIndex < m_mainTabWidget->count()) {
        m_mainTabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("切换到模块标签页: %1").arg(tabName));
    }
    // 否则添加新标签页
    else {
        addModuleToMainTab(widget, tabName, tabIndex, icon);
    }
}

void FX3ToolMainWin::removeModuleTab(int& tabIndex)
{
    if (!m_mainTabWidget || tabIndex < 0 || tabIndex >= m_mainTabWidget->count()) {
        return;
    }

    // 保存要删除的标签名称
    QString tabName = m_mainTabWidget->tabText(tabIndex);

    // 移除标签页
    m_mainTabWidget->removeTab(tabIndex);
    tabIndex = -1;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已移除模块标签页: %1").arg(tabName));
}

void FX3ToolMainWin::showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName)
{
    if (!m_mainTabWidget) {
        return;
    }

    // 如果标签页已存在
    if (tabIndex >= 0 && tabIndex < m_mainTabWidget->count()) {
        m_mainTabWidget->setCurrentIndex(tabIndex);
    }
    // 否则添加新标签页
    else {
        addModuleToMainTab(widget, tabName, tabIndex);
    }
}

void FX3ToolMainWin::slot_onEnteringState(AppState state, const QString& reason) {
    // 这里可以处理特定状态的进入逻辑
    // 目前大部分状态处理已经委托给UIStateHandler

    switch (state) {
    case AppState::SHUTDOWN:
        // 确保关闭流程已经启动
        m_isClosing = true;
        break;

    default:
        // 其他状态处理已经由UIStateHandler完成
        break;
    }
}

void FX3ToolMainWin::slot_onDataPacketAvailable(const DataPacket& packet) {
    // 如果应用正在关闭，忽略数据包
    if (m_isClosing) {
        return;
    }

    // 将数据包发送到文件保存控制器
    if (m_fileSaveController) {
        m_fileSaveController->processDataPacket(packet);
    }
}

void FX3ToolMainWin::slot_onSaveCompleted(const QString& path, uint64_t totalBytes) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存完成: 路径=%1, 总大小=%2 字节")
        .arg(path).arg(totalBytes));

    // 这里可以添加保存完成后的UI更新或其他处理
}

void FX3ToolMainWin::slot_onSaveError(const QString& error) {
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误: %1").arg(error));

    // 在状态栏或对话框中显示错误信息
    if (m_uiStateHandler) {
        m_uiStateHandler->showErrorMessage(
            LocalQTCompat::fromLocal8Bit("文件保存错误"), error);
    }
}

void FX3ToolMainWin::slot_onShowSaveFileBoxTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示文件保存对话框"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 获取图像参数
    uint16_t width;
    uint16_t height;
    uint8_t format;

    if (validateImageParameters(width, height, format)) {
        // 设置文件保存控制器的图像参数
        m_fileSaveController->setImageParameters(width, height, format);

        // 创建或显示保存对话框
        if (!m_saveFileBox) {
            m_saveFileBox = m_fileSaveController->createSaveFileBox(this);

            // 连接信号 - FileSaveController已经连接了信号到this
        }

        // 准备并显示对话框
        m_saveFileBox->prepareForShow();
        m_saveFileBox->show();
        m_saveFileBox->raise();
        m_saveFileBox->activateWindow();
    }
}