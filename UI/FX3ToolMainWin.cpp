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
#include <QObjectCleanupHandler>
#include <QtConcurrent/QtConcurrent>

#include "FX3ToolMainWin.h"
#include "Logger.h"
#include "DataConverters.h"

std::atomic<bool> FX3ToolMainWin::s_resourcesReleased{ false };

// 在主窗口构造函数中初始化子窗口指针

FX3ToolMainWin::FX3ToolMainWin(QWidget* parent)
    : QMainWindow(parent)
    , m_isClosing(false)
    , m_loggerInitialized(false)
    , m_deviceManager(nullptr)
    , m_uiStateHandler(nullptr)
    , m_fileSavePanel(nullptr)
    , m_saveFileBox(nullptr)
    , m_channelSelectWidget(nullptr)
    , m_dataAnalysisWidget(nullptr)
    , m_updataDeviceWidget(nullptr)
    , m_videoDisplayWidget(nullptr)
{
    ui.setupUi(this);

    // 设置菜单栏
    setupMenuBar();

    try {
        // 1. 首先初始化日志系统&UI
        if (!initializeLogger()) {
            QMessageBox::critical(this, fromLocal8Bit("错误"),
                fromLocal8Bit("日志系统初始化失败，应用程序无法继续"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }
        LOG_INFO(fromLocal8Bit("应用程序启动，Qt版本: %1").arg(QT_VERSION_STR));
        slot_initializeUI();

        // 2. 创建UI状态处理器
        m_uiStateHandler = new UIStateHandler(ui, this);
        if (!m_uiStateHandler) {
            LOG_ERROR(fromLocal8Bit("创建UI状态处理器失败"));
            QMessageBox::critical(this, fromLocal8Bit("错误"),
                fromLocal8Bit("UI状态处理器创建失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 3. 创建设备管理器
        m_deviceManager = new FX3DeviceManager(this);
        if (!m_deviceManager) {
            LOG_ERROR(fromLocal8Bit("创建设备管理器失败"));
            QMessageBox::critical(this, fromLocal8Bit("错误"),
                fromLocal8Bit("设备管理器创建失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 4. 初始化文件保存组件
        if (!initializeFileSaveComponents()) {
            LOG_ERROR(fromLocal8Bit("初始化文件保存组件失败"));
            QMessageBox::critical(this, fromLocal8Bit("错误"),
                fromLocal8Bit("初始化文件保存组件失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 5. 初始化所有信号连接
        initializeConnections();

        // 6. 启动设备检测
        registerDeviceNotification();

        // 7. 设置初始状态并初始化设备
        AppStateMachine::instance().processEvent(StateEvent::APP_INIT,
            fromLocal8Bit("应用程序初始化完成"));

        if (!m_deviceManager->initializeDeviceAndManager((HWND)this->winId())) {
            LOG_WARN(fromLocal8Bit("设备初始化失败，应用将以离线模式运行"));
            QMessageBox::warning(this, fromLocal8Bit("警告"),
                fromLocal8Bit("设备初始化失败，将以离线模式运行"));
        }

        LOG_INFO(fromLocal8Bit("FX3ToolMainWin构造函数完成..."));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, fromLocal8Bit("错误"),
            fromLocal8Bit("初始化过程中发生异常: %1").arg(e.what()));
        LOG_ERROR(fromLocal8Bit("初始化异常: %1").arg(e.what()));
        QTimer::singleShot(0, this, &FX3ToolMainWin::close);
    }
    catch (...) {
        QMessageBox::critical(this, fromLocal8Bit("错误"),
            fromLocal8Bit("初始化过程中发生未知异常"));
        LOG_ERROR(fromLocal8Bit("初始化未知异常"));
        QTimer::singleShot(0, this, &FX3ToolMainWin::close);
    }
}

FX3ToolMainWin::~FX3ToolMainWin() {
    LOG_INFO(fromLocal8Bit("FX3ToolMainWin析构函数入口"));
    LOG_INFO(fromLocal8Bit("设置关闭标志"));
    m_isClosing = true;

    // 检查资源是否已在closeEvent中释放
    if (!s_resourcesReleased) {
        if (m_deviceManager) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除设备管理器"));
            delete m_deviceManager;
            m_deviceManager = nullptr;
        }

        if (m_uiStateHandler) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除UI状态处理器"));
            delete m_uiStateHandler;
            m_uiStateHandler = nullptr;
        }

        if (m_saveFileBox) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除文件保存对话框"));
            delete m_saveFileBox;
            m_saveFileBox = nullptr;
        }

        if (m_fileSavePanel) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除文件保存面板"));
            delete m_fileSavePanel;
            m_fileSavePanel = nullptr;
        }

        // 释放其他子窗口资源
        if (m_channelSelectWidget) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除通道选择窗口"));
            delete m_channelSelectWidget;
            m_channelSelectWidget = nullptr;
        }

        if (m_dataAnalysisWidget) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除数据分析窗口"));
            delete m_dataAnalysisWidget;
            m_dataAnalysisWidget = nullptr;
        }

        if (m_updataDeviceWidget) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除设备升级窗口"));
            delete m_updataDeviceWidget;
            m_updataDeviceWidget = nullptr;
        }

        if (m_videoDisplayWidget) {
            LOG_INFO(fromLocal8Bit("在析构函数中删除视频显示窗口"));
            delete m_videoDisplayWidget;
            m_videoDisplayWidget = nullptr;
        }
    }
    else {
        LOG_INFO(fromLocal8Bit("资源已在closeEvent中释放"));
    }

    LOG_INFO(fromLocal8Bit("FX3ToolMainWin析构函数退出 - 成功"));
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
    LOG_INFO(fromLocal8Bit("应用程序关闭中，正在清理资源..."));

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
        if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
            LOG_INFO(fromLocal8Bit("停止文件保存"));
            m_fileSavePanel->stopSaving();
        }

        // 3. 关闭所有子窗口
        if (m_saveFileBox && m_saveFileBox->isVisible()) {
            LOG_INFO(fromLocal8Bit("关闭文件保存对话框"));
            m_saveFileBox->close();
        }

        if (m_channelSelectWidget && m_channelSelectWidget->isVisible()) {
            LOG_INFO(fromLocal8Bit("关闭通道选择窗口"));
            m_channelSelectWidget->close();
        }

        if (m_dataAnalysisWidget && m_dataAnalysisWidget->isVisible()) {
            LOG_INFO(fromLocal8Bit("关闭数据分析窗口"));
            m_dataAnalysisWidget->close();
        }

        if (m_updataDeviceWidget && m_updataDeviceWidget->isVisible()) {
            LOG_INFO(fromLocal8Bit("关闭设备升级窗口"));
            m_updataDeviceWidget->close();
        }

        if (m_videoDisplayWidget && m_videoDisplayWidget->isVisible()) {
            LOG_INFO(fromLocal8Bit("关闭视频显示窗口"));
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
            fromLocal8Bit("应用程序正在关闭"));

        // 7. 停止设备传输、采集和资源释放
        stopAndReleaseResources();

        // 接受关闭事件
        event->accept();
        LOG_INFO(fromLocal8Bit("关闭流程执行完成，释放互斥锁"));
        closeMutex.unlock();
    }
    catch (const std::exception& e) {
        LOG_ERROR(fromLocal8Bit("关闭过程异常: %1").arg(e.what()));
        event->accept();
        closeMutex.unlock();
    }
    catch (...) {
        LOG_ERROR(fromLocal8Bit("关闭过程未知异常"));
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
    // 先禁用所有控制按钮
    ui.startButton->setEnabled(false);
    ui.stopButton->setEnabled(false);

    // 重置按钮的状态设置
    ui.resetButton->setEnabled(false);

    // 立即检查按钮状态
    LOG_INFO(fromLocal8Bit("初始化UI后按钮状态: 开始=%1, 停止=%2, 重置=%3")
        .arg(ui.startButton->isEnabled() ? fromLocal8Bit("启用") : fromLocal8Bit("禁用"))
        .arg(ui.stopButton->isEnabled() ? fromLocal8Bit("启用") : fromLocal8Bit("禁用"))
        .arg(ui.resetButton->isEnabled() ? fromLocal8Bit("启用") : fromLocal8Bit("禁用")));

    // 设置状态栏自适应布局
    this->statusBar()->setSizeGripEnabled(false);

    // 为所有状态栏标签设置统一字体
    QFont statusFont("Microsoft YaHei", 9); // 使用雅黑字体或其他合适的系统字体

    // 设置所有标签的统一字体和样式
    ui.usbSpeedLabel->setFont(statusFont);
    ui.usbStatusLabel->setFont(statusFont);
    ui.transferStatusLabel->setFont(statusFont);
    ui.speedLabel->setFont(statusFont);
    ui.totalBytesLabel->setFont(statusFont);
    ui.totalTimeLabel->setFont(statusFont);

    // 设置窗口最小尺寸
    this->setMinimumSize(1000, 800);

    // 初始化命令状态
    ui.cmdStatusLabel->setText(fromLocal8Bit("命令文件未加载"));
    ui.cmdStatusLabel->setStyleSheet("color: red;");

    // 初始化图像参数UI
    ui.imageWIdth->setReadOnly(false);
    ui.imageHeight->setReadOnly(false);

    // 初始化图像类型下拉框
    ui.imageType->clear();
    ui.imageType->addItem("RAW8 (0x38)");
    ui.imageType->addItem("RAW10 (0x39)");
    ui.imageType->addItem("RAW12 (0x3A)");
    ui.imageType->setCurrentIndex(1); // 默认选择RAW10

    // 设置默认值
    ui.imageWIdth->setText("1920");
    ui.imageHeight->setText("1080");

    // 创建保存文件按钮
    QPushButton* saveFileButton = new QPushButton(fromLocal8Bit("保存文件"), this);
    ui.controlTransferLayout->addWidget(saveFileButton);

    // 连接保存文件按钮信号
    connect(saveFileButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);
}

void FX3ToolMainWin::slot_showAboutDialog() {
    QMessageBox::about(this,
        fromLocal8Bit("关于FX3传输测试工具"),
        fromLocal8Bit("FX3传输测试工具 v1.0\n\n用于FX3设备的数据传输和测试\n\n© 2025 公司名称")
    );
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

        LOG_INFO(fromLocal8Bit("日志: %1").arg(logPath));

        m_loggerInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "日志初始化失败:" << e.what();
        return false;
    }
}

void FX3ToolMainWin::setupMenuBar() {
    // 创建菜单
    QMenu* fileMenu = menuBar()->addMenu(fromLocal8Bit("文件"));
    QMenu* deviceMenu = menuBar()->addMenu(fromLocal8Bit("设备"));
    QMenu* toolsMenu = menuBar()->addMenu(fromLocal8Bit("工具"));
    QMenu* viewMenu = menuBar()->addMenu(fromLocal8Bit("视图"));
    QMenu* helpMenu = menuBar()->addMenu(fromLocal8Bit("帮助"));

    // 文件菜单项
    QAction* saveAction = new QAction(fromLocal8Bit("保存文件..."), this);
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    saveAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_S));
#else
    saveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
#endif
    connect(saveAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);
    fileMenu->addAction(saveAction);

    QAction* exportAction = new QAction(fromLocal8Bit("导出数据..."), this);
    fileMenu->addAction(exportAction);
    fileMenu->addSeparator();

    QAction* exitAction = new QAction(fromLocal8Bit("退出"), this);
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    exitAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_F4));
#else
    exitAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F4));
#endif
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);

    // 设备菜单项
    QAction* startAction = new QAction(fromLocal8Bit("开始传输"), this);
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    startAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_B));
#else
    startAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
#endif
    connect(startAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onStartButtonClicked);
    deviceMenu->addAction(startAction);

    QAction* stopAction = new QAction(fromLocal8Bit("停止传输"), this);
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    stopAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_E));
#else
    stopAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
#endif
    connect(stopAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onStopButtonClicked);
    deviceMenu->addAction(stopAction);

    QAction* resetAction = new QAction(fromLocal8Bit("重置设备"), this);
    connect(resetAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onResetButtonClicked);
    deviceMenu->addAction(resetAction);
    deviceMenu->addSeparator();

    QAction* channelAction = new QAction(fromLocal8Bit("通道配置..."), this);
    connect(channelAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    deviceMenu->addAction(channelAction);

    QAction* updataAction = new QAction(fromLocal8Bit("设备升级..."), this);
    connect(updataAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowUpdataDeviceTriggered);
    deviceMenu->addAction(updataAction);

    // 工具菜单项
    QAction* analysisAction = new QAction(fromLocal8Bit("数据分析..."), this);
    connect(analysisAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    toolsMenu->addAction(analysisAction);

    QAction* videoAction = new QAction(fromLocal8Bit("视频显示..."), this);
    connect(videoAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    toolsMenu->addAction(videoAction);

    QAction* settingsAction = new QAction(fromLocal8Bit("应用设置..."), this);
    toolsMenu->addAction(settingsAction);

    // 视图菜单项
    QAction* clearLogAction = new QAction(fromLocal8Bit("清除日志"), this);
    connect(clearLogAction, &QAction::triggered, this, [this]() {
        if (ui.logTextEdit) {
            ui.logTextEdit->clear();
            LOG_INFO(fromLocal8Bit("日志已清除"));
        }
        });
    viewMenu->addAction(clearLogAction);

    // 帮助菜单项
    QAction* helpAction = new QAction(fromLocal8Bit("使用帮助"), this);
    helpMenu->addAction(helpAction);
    helpMenu->addSeparator();

    QAction* aboutAction = new QAction(fromLocal8Bit("关于"), this);
    connect(aboutAction, &QAction::triggered, this, &FX3ToolMainWin::slot_showAboutDialog);
    helpMenu->addAction(aboutAction);
}

bool FX3ToolMainWin::initializeFileSaveComponents() {
    return true;
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
        LOG_ERROR(fromLocal8Bit("注册Fx3 USB设备通知失败: %1")
            .arg(GetLastError()));
    }
    else {
        LOG_INFO(fromLocal8Bit("Fx3 USB设备通知注册成功"));
    }
}

void FX3ToolMainWin::initializeConnections() {
    // 按钮信号连接
    connect(ui.startButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onStartButtonClicked);
    connect(ui.stopButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onStopButtonClicked);
    connect(ui.resetButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onResetButtonClicked);

    // 命令目录按钮连接
    connect(ui.cmdDirButton, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onSelectCommandDirectory);

    if (m_deviceManager && m_uiStateHandler) {
        // 设备管理器连接
        connect(m_deviceManager, &FX3DeviceManager::transferStatsUpdated, m_uiStateHandler, &UIStateHandler::updateTransferStats);
        connect(m_deviceManager, &FX3DeviceManager::usbSpeedUpdated, m_uiStateHandler, &UIStateHandler::updateUsbSpeedDisplay);
        connect(m_deviceManager, &FX3DeviceManager::deviceError, m_uiStateHandler, &UIStateHandler::showErrorMessage);

        // 添加数据包处理连接
        //connect(m_deviceManager, &FX3DeviceManager::dataPacketAvailable, this, &FX3ToolMainWin::slot_onDataPacketAvailable);

        // 状态机连接
        connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged, m_uiStateHandler, &UIStateHandler::onStateChanged, Qt::DirectConnection);
    }
    else {
        LOG_ERROR(fromLocal8Bit("设备管理器或UI状态处理器为空，无法连接信号"));
        if (!m_deviceManager) LOG_ERROR(fromLocal8Bit("设备管理器为空"));
        if (!m_uiStateHandler) LOG_ERROR(fromLocal8Bit("UI状态处理器为空"));
    }

    // 连接文件保存管理器信号
    connect(&FileSaveManager::instance(), &FileSaveManager::saveCompleted,
        this, &FX3ToolMainWin::slot_onSaveCompleted);
    connect(&FileSaveManager::instance(), &FileSaveManager::saveError,
        this, &FX3ToolMainWin::slot_onSaveError);

    // 连接状态机事件
    connect(&AppStateMachine::instance(), &AppStateMachine::enteringState, this, &FX3ToolMainWin::slot_onEnteringState);
}

void FX3ToolMainWin::stopAndReleaseResources() {
    // 1. 确保设备管理器停止所有传输
    if (m_deviceManager) {
        if (m_deviceManager->isTransferring()) {
            LOG_INFO(fromLocal8Bit("停止正在进行的数据传输"));
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
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        LOG_INFO(fromLocal8Bit("停止文件保存"));
        m_fileSavePanel->stopSaving();

        // 短暂等待让保存操作完成
        QThread::msleep(100);
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    // 3. 创建单独的智能指针保存各个组件，以便在类析构前释放
    std::unique_ptr<FX3DeviceManager> deviceManagerPtr(m_deviceManager);
    std::unique_ptr<UIStateHandler> uiHandlerPtr(m_uiStateHandler);
    std::unique_ptr<FileSavePanel> fileSavePanelPtr(m_fileSavePanel);
    std::unique_ptr<SaveFileBox> saveFileBoxPtr(m_saveFileBox);
    std::unique_ptr<ChannelSelect> channelSelectPtr(m_channelSelectWidget);
    std::unique_ptr<DataAnalysis> dataAnalysisPtr(m_dataAnalysisWidget);
    std::unique_ptr<UpdataDevice> updataDevicePtr(m_updataDeviceWidget);
    std::unique_ptr<VideoDisplay> videoDisplayPtr(m_videoDisplayWidget);

    // 将类成员设为nullptr防止析构函数重复释放
    m_deviceManager = nullptr;
    m_uiStateHandler = nullptr;
    m_fileSavePanel = nullptr;
    m_saveFileBox = nullptr;
    m_channelSelectWidget = nullptr;
    m_dataAnalysisWidget = nullptr;
    m_updataDeviceWidget = nullptr;
    m_videoDisplayWidget = nullptr;

    // 4. 先释放子窗口
    LOG_INFO(fromLocal8Bit("释放视频显示窗口"));
    videoDisplayPtr.reset();

    LOG_INFO(fromLocal8Bit("释放设备升级窗口"));
    updataDevicePtr.reset();

    LOG_INFO(fromLocal8Bit("释放数据分析窗口"));
    dataAnalysisPtr.reset();

    LOG_INFO(fromLocal8Bit("释放通道选择窗口"));
    channelSelectPtr.reset();

    LOG_INFO(fromLocal8Bit("释放文件保存对话框"));
    saveFileBoxPtr.reset();

    LOG_INFO(fromLocal8Bit("释放文件保存面板"));
    fileSavePanelPtr.reset();

    // 5. 然后释放UI状态处理器
    LOG_INFO(fromLocal8Bit("释放UI状态处理器"));
    uiHandlerPtr.reset();

    // 6. 最后释放设备管理器 - 这会同时清理USB设备和采集管理器
    LOG_INFO(fromLocal8Bit("释放设备管理器"));
    deviceManagerPtr.reset();

    // 设置静态资源释放标志
    s_resourcesReleased = true;
    LOG_INFO(fromLocal8Bit("所有资源已释放完成"));
}

void FX3ToolMainWin::slot_onStartButtonClicked() {
    LOG_INFO(fromLocal8Bit("开始传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略开始请求"));
        return;
    }

    // 验证参数
    uint16_t width;
    uint16_t height;
    uint8_t capType;

    if (!validateImageParameters(width, height, capType)) {
        return;
    }

    // 调用设备管理器启动传输
    if (m_deviceManager) {
        m_deviceManager->startTransfer(width, height, capType);
    }
}

void FX3ToolMainWin::slot_onStopButtonClicked() {
    LOG_INFO(fromLocal8Bit("停止传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略停止请求"));
        return;
    }

    // 调用设备管理器停止传输
    if (m_deviceManager) {
        m_deviceManager->stopTransfer();
    }
}

void FX3ToolMainWin::slot_onResetButtonClicked() {
    LOG_INFO(fromLocal8Bit("重置设备按钮点击"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略重置请求"));
        return;
    }

    // 调用设备管理器重置设备
    if (m_deviceManager) {
        m_deviceManager->resetDevice();
    }
}

// 通道选择窗口触发函数
void FX3ToolMainWin::slot_onShowChannelSelectTriggered() {
    LOG_INFO(fromLocal8Bit("显示通道配置窗口"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 延迟创建通道选择窗口（单例模式）
    if (!m_channelSelectWidget) {
        m_channelSelectWidget = new ChannelSelect(this);
        connect(m_channelSelectWidget, &ChannelSelect::channelConfigChanged,
            this, &FX3ToolMainWin::slot_onChannelConfigChanged);
    }

    // 显示窗口
    m_channelSelectWidget->show();
    m_channelSelectWidget->raise();
    m_channelSelectWidget->activateWindow();
}

// 数据分析窗口触发函数
void FX3ToolMainWin::slot_onShowDataAnalysisTriggered() {
    LOG_INFO(fromLocal8Bit("显示数据分析窗口"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 延迟创建数据分析窗口（单例模式）
    if (!m_dataAnalysisWidget) {
        m_dataAnalysisWidget = new DataAnalysis(this);

        // 连接保存数据信号
        connect(m_dataAnalysisWidget, &DataAnalysis::saveDataRequested,
            this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

        // 连接视频预览信号
        connect(m_dataAnalysisWidget, &DataAnalysis::videoDisplayRequested,
            this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    }

    // 显示窗口
    m_dataAnalysisWidget->show();
    m_dataAnalysisWidget->raise();
    m_dataAnalysisWidget->activateWindow();
}

// 设备升级窗口触发函数
void FX3ToolMainWin::slot_onShowUpdataDeviceTriggered() {
    LOG_INFO(fromLocal8Bit("显示设备升级窗口"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 延迟创建设备升级窗口（单例模式）
    if (!m_updataDeviceWidget) {
        m_updataDeviceWidget = new UpdataDevice(this);
    }

    // 显示窗口
    m_updataDeviceWidget->show();
    m_updataDeviceWidget->raise();
    m_updataDeviceWidget->activateWindow();
}

// 视频显示窗口触发函数
void FX3ToolMainWin::slot_onShowVideoDisplayTriggered() {
    LOG_INFO(fromLocal8Bit("显示视频窗口"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 验证图像参数
    uint16_t width;
    uint16_t height;
    uint8_t capType;

    if (!validateImageParameters(width, height, capType)) {
        QMessageBox::warning(this,
            fromLocal8Bit("警告"),
            fromLocal8Bit("图像参数无效，请先设置正确的图像参数"));
        return;
    }

    // 延迟创建视频显示窗口（单例模式）
    if (!m_videoDisplayWidget) {
        m_videoDisplayWidget = new VideoDisplay(this);
    }

    // 设置图像参数
    m_videoDisplayWidget->setImageParameters(width, height, capType);

    // 显示窗口
    m_videoDisplayWidget->show();
    m_videoDisplayWidget->raise();
    m_videoDisplayWidget->activateWindow();
}

// 通道配置变更处理函数
void FX3ToolMainWin::slot_onChannelConfigChanged(const ChannelConfig& config) {
    LOG_INFO(fromLocal8Bit("通道配置已更新"));

    // 更新主界面上的相关参数
    if (config.videoWidth > 0 && config.videoHeight > 0) {
        ui.imageWIdth->setText(QString::number(config.videoWidth));
        ui.imageHeight->setText(QString::number(config.videoHeight));
        LOG_INFO(fromLocal8Bit("从通道配置更新图像尺寸：%1x%2")
            .arg(config.videoWidth).arg(config.videoHeight));
    }

    // 如果有设备管理器，更新设备配置
    if (m_deviceManager) {
        //m_deviceManager->updateChannelConfig(config);
    }
}

void FX3ToolMainWin::slot_onSaveButtonClicked() {
    LOG_INFO(fromLocal8Bit("保存按钮点击"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略保存请求"));
        return;
    }

    // 检查文件保存面板是否存在
    if (!m_fileSavePanel) {
        LOG_ERROR(fromLocal8Bit("文件保存面板未初始化"));
        QMessageBox::warning(this, fromLocal8Bit("错误"),
            fromLocal8Bit("文件保存功能未初始化"));
        return;
    }

    // 如果保存已经在进行中，则停止保存
    if (m_fileSavePanel->isSaving()) {
        m_fileSavePanel->stopSaving();
        LOG_INFO(fromLocal8Bit("停止文件保存"));
    }
    else {
        // 否则，启动保存

        // 验证参数并更新文件保存选项
        uint16_t width;
        uint16_t height;
        uint8_t capType;

        if (!validateImageParameters(width, height, capType)) {
            return;
        }

        // 获取当前保存参数并更新选项
        SaveParameters params = FileSaveManager::instance().getSaveParameters();
        params.options["width"] = width;
        params.options["height"] = height;
        params.options["format"] = capType;

        // 更新保存参数
        FileSaveManager::instance().setSaveParameters(params);

        // 启动保存
        m_fileSavePanel->startSaving();
        LOG_INFO(fromLocal8Bit("开始文件保存，参数：宽度=%1，高度=%2，格式=0x%3")
            .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));
    }
}

void FX3ToolMainWin::slot_onSelectCommandDirectory() {
    LOG_INFO(fromLocal8Bit("选择命令文件目录"));

    if (m_isClosing) {
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(
        this,
        fromLocal8Bit("选择命令文件目录"),
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
            fromLocal8Bit("错误"),
            fromLocal8Bit("无法加载命令文件，请确保目录包含所需的所有命令文件")
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
        LOG_ERROR("无效的图像宽度");
        QMessageBox::warning(this, fromLocal8Bit("错误"),
            fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值"));
        return false;
    }

    // 获取高度
    QString heightText = ui.imageHeight->text();
    if (heightText.contains("Height")) {
        heightText = heightText.remove("Height").trimmed();
    }
    height = heightText.toUInt(&conversionOk);
    if (!conversionOk || height == 0 || height > 4096) {
        LOG_ERROR("无效的图像高度");
        QMessageBox::warning(this, fromLocal8Bit("错误"),
            fromLocal8Bit("无效的图像高度，请输入1-4096之间的值"));
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

    LOG_INFO(fromLocal8Bit("图像参数验证通过 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    return true;
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

    // 检查文件保存面板是否正在保存数据
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        // 将数据包传递给文件保存管理器
        FileSaveManager::instance().processDataPacket(packet);
    }
}

void FX3ToolMainWin::slot_onSaveCompleted(const QString& path, uint64_t totalBytes) {
    LOG_INFO(fromLocal8Bit("文件保存完成: 路径=%1, 总大小=%2 字节")
        .arg(path).arg(totalBytes));

    // 这里可以添加任何保存完成后的逻辑，比如更新UI等
}

void FX3ToolMainWin::slot_onSaveError(const QString& error) {
    LOG_ERROR(fromLocal8Bit("文件保存错误: %1").arg(error));

    // 这里可以添加保存错误处理逻辑
    // 例如在状态栏显示错误信息等
}

void FX3ToolMainWin::slot_onShowSaveFileBoxTriggered() {
    LOG_INFO(fromLocal8Bit("显示文件保存对话框"));

    if (m_isClosing) {
        LOG_INFO(fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 验证参数
    uint16_t width;
    uint16_t height;
    uint8_t capType;

    if (!validateImageParameters(width, height, capType)) {
        return;
    }

    // 延迟创建 SaveFileBox
    if (!m_saveFileBox) {
        m_saveFileBox = new SaveFileBox(this);

        // 连接信号
        connect(m_saveFileBox, &SaveFileBox::saveCompleted,
            this, &FX3ToolMainWin::slot_onSaveFileBoxCompleted);
        connect(m_saveFileBox, &SaveFileBox::saveError,
            this, &FX3ToolMainWin::slot_onSaveFileBoxError);
    }

    // 设置图像参数
    m_saveFileBox->setImageParameters(width, height, capType);

    // 准备显示
    m_saveFileBox->prepareForShow();

    // 显示对话框
    m_saveFileBox->show();
    m_saveFileBox->raise();
    m_saveFileBox->activateWindow();
}

// 添加文件保存完成槽函数
void FX3ToolMainWin::slot_onSaveFileBoxCompleted(const QString& path, uint64_t totalBytes) {
    LOG_INFO(fromLocal8Bit("文件保存完成：路径=%1，总大小=%2字节")
        .arg(path).arg(totalBytes));

    // 在这里可以添加保存完成后的处理逻辑
    // 例如更新状态栏显示等
}

// 添加文件保存错误槽函数
void FX3ToolMainWin::slot_onSaveFileBoxError(const QString& error) {
    LOG_ERROR(fromLocal8Bit("文件保存错误：%1").arg(error));

    // 在这里可以添加错误处理逻辑
}
