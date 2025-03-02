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
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("日志系统初始化失败，应用程序无法继续"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序启动，Qt版本: %1").arg(QT_VERSION_STR));
        slot_initializeUI();

        // 2. 创建UI状态处理器
        m_uiStateHandler = new UIStateHandler(ui, this);
        if (!m_uiStateHandler) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建UI状态处理器失败"));
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("UI状态处理器创建失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 3. 创建设备管理器
        m_deviceManager = new FX3DeviceManager(this);
        if (!m_deviceManager) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建设备管理器失败"));
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("设备管理器创建失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 4. 初始化文件保存组件
        if (!initializeFileSaveComponents()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化文件保存组件失败"));
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("初始化文件保存组件失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 5. 初始化所有信号连接
        initializeConnections();

        // 6. 启动设备检测
        registerDeviceNotification();

        // 7. 设置初始状态并初始化设备
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

        if (m_saveFileBox) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除文件保存对话框"));
            delete m_saveFileBox;
            m_saveFileBox = nullptr;
        }

        if (m_fileSavePanel) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("在析构函数中删除文件保存面板"));
            delete m_fileSavePanel;
            m_fileSavePanel = nullptr;
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
        if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
            m_fileSavePanel->stopSaving();
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
    // 创建主分割器
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧区域：控制面板
    QWidget* controlPanel = new QWidget(this);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);

    // 控制区域标题
    QLabel* controlTitle = new QLabel(LocalQTCompat::fromLocal8Bit("设备控制"), this);
    controlTitle->setFont(QFont(controlTitle->font().family(), controlTitle->font().pointSize() + 1, QFont::Bold));
    controlLayout->addWidget(controlTitle);

    // 重要：保留原始按钮但改变其父对象
    controlLayout->addWidget(ui.startButton);
    controlLayout->addWidget(ui.stopButton);
    controlLayout->addWidget(ui.resetButton);

    // 添加图像参数组
    QGroupBox* paramBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("图像参数"), this);
    QGridLayout* paramLayout = new QGridLayout(paramBox);
    paramLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("宽度:")), 0, 0);
    paramLayout->addWidget(ui.imageWIdth, 0, 1);
    paramLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("高度:")), 1, 0);
    paramLayout->addWidget(ui.imageHeight, 1, 1);
    paramLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("类型:")), 2, 0);
    paramLayout->addWidget(ui.imageType, 2, 1);
    controlLayout->addWidget(paramBox);

    // 添加命令文件区域
    QGroupBox* cmdBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("命令文件"), this);
    QHBoxLayout* cmdLayout = new QHBoxLayout(cmdBox);
    cmdLayout->addWidget(ui.cmdDirEdit);
    cmdLayout->addWidget(ui.cmdDirButton);
    controlLayout->addWidget(cmdBox);
    controlLayout->addWidget(ui.cmdStatusLabel);

    // 添加功能模块快速访问区
    QGroupBox* moduleBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("功能模块"), this);
    QVBoxLayout* moduleLayout = new QVBoxLayout(moduleBox);
    // 添加各功能按钮，连接槽函数
    QPushButton* channelBtn = new QPushButton(LocalQTCompat::fromLocal8Bit("通道配置"), this);
    QPushButton* dataBtn = new QPushButton(LocalQTCompat::fromLocal8Bit("数据分析"), this);
    QPushButton* videoBtn = new QPushButton(LocalQTCompat::fromLocal8Bit("视频显示"), this);
    QPushButton* saveBtn = new QPushButton(LocalQTCompat::fromLocal8Bit("文件保存"), this);
    moduleLayout->addWidget(channelBtn);
    moduleLayout->addWidget(dataBtn);
    moduleLayout->addWidget(videoBtn);
    moduleLayout->addWidget(saveBtn);
    controlLayout->addWidget(moduleBox);

    // 添加伸缩空间
    controlLayout->addStretch();

    // 创建中央区域：选项卡控件
    m_mainTabWidget = new QTabWidget(this);

    // 添加主页标签
    QWidget* homeTab = new QWidget(this);
    QVBoxLayout* homeLayout = new QVBoxLayout(homeTab);

    // 欢迎消息
    QLabel* welcomeLabel = new QLabel(LocalQTCompat::fromLocal8Bit("欢迎使用FX3传输测试工具"), this);
    welcomeLabel->setAlignment(Qt::AlignCenter);
    homeLayout->addWidget(welcomeLabel);

    // 状态信息面板
    QGroupBox* statusBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("设备状态"), this);
    QGridLayout* statusLayout = new QGridLayout(statusBox);
    statusLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("USB状态:")), 0, 0);
    statusLayout->addWidget(ui.usbStatusLabel, 0, 1);
    statusLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("USB速度:")), 1, 0);
    statusLayout->addWidget(ui.usbSpeedLabel, 1, 1);
    statusLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输状态:")), 2, 0);
    statusLayout->addWidget(ui.transferStatusLabel, 2, 1);
    statusLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输速率:")), 3, 0);
    statusLayout->addWidget(ui.speedLabel, 3, 1);
    statusLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("总字节数:")), 4, 0);
    statusLayout->addWidget(ui.totalBytesLabel, 4, 1);
    statusLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输时间:")), 5, 0);
    statusLayout->addWidget(ui.totalTimeLabel, 5, 1);
    homeLayout->addWidget(statusBox);

    // 添加波形显示预留区域
    QLabel* waveformPlaceholder = new QLabel(LocalQTCompat::fromLocal8Bit("波形显示区域 (功能待实现)"), this);
    waveformPlaceholder->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    waveformPlaceholder->setMinimumHeight(300);
    waveformPlaceholder->setAlignment(Qt::AlignCenter);
    homeLayout->addWidget(waveformPlaceholder);
    homeLayout->addStretch();

    // 添加标签页
    m_mainTabWidget->addTab(homeTab, LocalQTCompat::fromLocal8Bit("主页"));

    // 日志区域
    QWidget* logWidget = new QWidget(this);
    QVBoxLayout* logLayout = new QVBoxLayout(logWidget);
    QLabel* logLabel = new QLabel(LocalQTCompat::fromLocal8Bit("系统日志"), this);
    logLabel->setFont(QFont(logLabel->font().family(), logLabel->font().pointSize() + 1, QFont::Bold));
    logLayout->addWidget(logLabel);
    logLayout->addWidget(ui.logTextEdit);  // 重用原始日志控件

    // 创建垂直分割器
    QSplitter* vertSplitter = new QSplitter(Qt::Vertical, this);
    vertSplitter->addWidget(m_mainTabWidget);
    vertSplitter->addWidget(logWidget);
    vertSplitter->setStretchFactor(0, 3);  // 内容区域占更多空间
    vertSplitter->setStretchFactor(1, 1);  // 日志区域占较少空间

    // 添加到主分割器
    mainSplitter->addWidget(controlPanel);
    mainSplitter->addWidget(vertSplitter);
    mainSplitter->setStretchFactor(0, 1);  // 控制面板
    mainSplitter->setStretchFactor(1, 3);  // 内容区域

    // 设置中央控件
    setCentralWidget(mainSplitter);
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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化主界面布局"));

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 创建左侧分割器
    m_leftSplitter = new QSplitter(Qt::Vertical, this);

    // 左侧上部分: 控制面板
    QWidget* controlPanel = new QWidget(this);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(6, 6, 6, 6);
    controlLayout->setSpacing(8);

    // 控制面板标题
    QLabel* controlTitle = new QLabel(LocalQTCompat::fromLocal8Bit("设备控制"), this);
    QFont titleFont = controlTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    controlTitle->setFont(titleFont);
    controlTitle->setAlignment(Qt::AlignCenter);
    controlLayout->addWidget(controlTitle);

    // 添加设备控制按钮组
    QFrame* buttonFrame = new QFrame(this);
    buttonFrame->setFrameShape(QFrame::StyledPanel);
    buttonFrame->setFrameShadow(QFrame::Raised);
    QVBoxLayout* buttonLayout = new QVBoxLayout(buttonFrame);

    // 按钮样式设置
    QString buttonStyle = "QPushButton { min-height: 30px; }";
    ui.startButton->setStyleSheet(buttonStyle);
    ui.stopButton->setStyleSheet(buttonStyle);
    ui.resetButton->setStyleSheet(buttonStyle);

    // 添加按钮到布局
    buttonLayout->addWidget(ui.startButton);
    buttonLayout->addWidget(ui.stopButton);
    buttonLayout->addWidget(ui.resetButton);

    // 添加按钮框架到控制面板
    controlLayout->addWidget(buttonFrame);

    // 参数设置组
    QGroupBox* paramBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("图像参数"), this);
    QGridLayout* paramLayout = new QGridLayout(paramBox);

    // 水平布局包装每组参数
    QHBoxLayout* widthLayout = new QHBoxLayout();
    QLabel* widthLabel = new QLabel(LocalQTCompat::fromLocal8Bit("宽度:"), this);
    widthLayout->addWidget(widthLabel);
    widthLayout->addWidget(ui.imageWIdth);
    paramLayout->addLayout(widthLayout, 0, 0);

    QHBoxLayout* heightLayout = new QHBoxLayout();
    QLabel* heightLabel = new QLabel(LocalQTCompat::fromLocal8Bit("高度:"), this);
    heightLayout->addWidget(heightLabel);
    heightLayout->addWidget(ui.imageHeight);
    paramLayout->addLayout(heightLayout, 1, 0);

    QHBoxLayout* typeLayout = new QHBoxLayout();
    QLabel* typeLabel = new QLabel(LocalQTCompat::fromLocal8Bit("类型:"), this);
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(ui.imageType);
    paramLayout->addLayout(typeLayout, 2, 0);

    controlLayout->addWidget(paramBox);

    // 命令文件区域
    QGroupBox* cmdBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("命令文件"), this);
    QVBoxLayout* cmdLayout = new QVBoxLayout(cmdBox);

    QHBoxLayout* cmdDirLayout = new QHBoxLayout();
    cmdDirLayout->addWidget(ui.cmdDirEdit);
    cmdDirLayout->addWidget(ui.cmdDirButton);
    cmdLayout->addLayout(cmdDirLayout);
    cmdLayout->addWidget(ui.cmdStatusLabel);

    controlLayout->addWidget(cmdBox);

    // 功能模块快速访问区
    QGroupBox* quickAccessBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("功能模块"), this);
    QGridLayout* quickAccessLayout = new QGridLayout(quickAccessBox);

    // 创建功能按钮
    QPushButton* channelBtn = new QPushButton(QIcon(":/icons/channel.png"), LocalQTCompat::fromLocal8Bit("通道配置"), this);
    QPushButton* dataAnalysisBtn = new QPushButton(QIcon(":/icons/analysis.png"), LocalQTCompat::fromLocal8Bit("数据分析"), this);
    QPushButton* videoDisplayBtn = new QPushButton(QIcon(":/icons/video.png"), LocalQTCompat::fromLocal8Bit("视频显示"), this);
    QPushButton* waveformBtn = new QPushButton(QIcon(":/icons/waveform.png"), LocalQTCompat::fromLocal8Bit("波形分析"), this);
    QPushButton* saveFileBtn = new QPushButton(QIcon(":/icons/save.png"), LocalQTCompat::fromLocal8Bit("文件保存"), this);

    // 设置按钮样式
    channelBtn->setStyleSheet(buttonStyle);
    dataAnalysisBtn->setStyleSheet(buttonStyle);
    videoDisplayBtn->setStyleSheet(buttonStyle);
    waveformBtn->setStyleSheet(buttonStyle);
    saveFileBtn->setStyleSheet(buttonStyle);

    // 连接信号
    connect(channelBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    connect(dataAnalysisBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    connect(videoDisplayBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    connect(saveFileBtn, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

    // 添加按钮到网格布局
    quickAccessLayout->addWidget(channelBtn, 0, 0);
    quickAccessLayout->addWidget(dataAnalysisBtn, 0, 1);
    quickAccessLayout->addWidget(videoDisplayBtn, 1, 0);
    quickAccessLayout->addWidget(waveformBtn, 1, 1);
    quickAccessLayout->addWidget(saveFileBtn, 2, 0, 1, 2);

    controlLayout->addWidget(quickAccessBox);

    // 添加状态面板
    m_statusPanel = new QWidget(this);
    QVBoxLayout* statusLayout = new QVBoxLayout(m_statusPanel);
    statusLayout->setContentsMargins(4, 4, 4, 4);
    statusLayout->setSpacing(4);

    QLabel* statusTitle = new QLabel(LocalQTCompat::fromLocal8Bit("设备状态"), this);
    statusTitle->setFont(titleFont);
    statusTitle->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(statusTitle);

    // 创建网格布局来整齐显示状态信息
    QGridLayout* statusGridLayout = new QGridLayout();
    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("USB状态:")), 0, 0);
    statusGridLayout->addWidget(ui.usbStatusLabel, 0, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("USB速度:")), 1, 0);
    statusGridLayout->addWidget(ui.usbSpeedLabel, 1, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输状态:")), 2, 0);
    statusGridLayout->addWidget(ui.transferStatusLabel, 2, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输速率:")), 3, 0);
    statusGridLayout->addWidget(ui.speedLabel, 3, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("总字节数:")), 4, 0);
    statusGridLayout->addWidget(ui.totalBytesLabel, 4, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输时间:")), 5, 0);
    statusGridLayout->addWidget(ui.totalTimeLabel, 5, 1);

    statusLayout->addLayout(statusGridLayout);
    statusLayout->addStretch();

    // 添加控制面板和状态面板到左侧分割器
    m_leftSplitter->addWidget(controlPanel);
    m_leftSplitter->addWidget(m_statusPanel);

    // 设置左侧分割比例
    m_leftSplitter->setStretchFactor(0, 3);
    m_leftSplitter->setStretchFactor(1, 2);

    // 创建右侧内容区域: 标签页控件
    m_mainTabWidget = new QTabWidget(this);
    m_mainTabWidget->setTabsClosable(true);  // 允许关闭标签页
    m_mainTabWidget->setDocumentMode(true);  // 使用文档模式外观
    m_mainTabWidget->setMovable(true);       // 允许拖动标签页

    // 主页标签页
    QWidget* homeWidget = createHomeTabContent();
    m_homeTabIndex = m_mainTabWidget->addTab(homeWidget, QIcon(":/icons/home.png"), LocalQTCompat::fromLocal8Bit("主页"));

    // 连接标签页关闭信号
    connect(m_mainTabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        // 主页不能关闭
        if (index == m_homeTabIndex) {
            return;
        }

        // 根据索引确定关闭的是哪个模块
        if (index == m_channelTabIndex) {
            m_channelTabIndex = -1;
        }
        else if (index == m_dataAnalysisTabIndex) {
            m_dataAnalysisTabIndex = -1;
        }
        else if (index == m_videoDisplayTabIndex) {
            m_videoDisplayTabIndex = -1;
        }
        else if (index == m_waveformTabIndex) {
            m_waveformTabIndex = -1;
        }

        // 移除标签页
        m_mainTabWidget->removeTab(index);
        });

    // 日志区域占用更小的空间
    QWidget* logContainer = new QWidget(this);
    QVBoxLayout* logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(4, 4, 4, 4);

    QLabel* logTitle = new QLabel(LocalQTCompat::fromLocal8Bit("系统日志"), this);
    logTitle->setFont(titleFont);
    logTitle->setAlignment(Qt::AlignCenter);
    logLayout->addWidget(logTitle);
    logLayout->addWidget(ui.logTextEdit);

    // 添加主要部分到主分割器
    m_mainSplitter->addWidget(m_leftSplitter);
    m_mainSplitter->addWidget(m_mainTabWidget);
    m_mainSplitter->addWidget(logContainer);

    // 设置主分割比例
    m_mainSplitter->setStretchFactor(0, 2);  // 左侧控制面板
    m_mainSplitter->setStretchFactor(1, 5);  // 中间内容区域
    m_mainSplitter->setStretchFactor(2, 3);  // 右侧日志区域

    // 设置为中央控件
    setCentralWidget(m_mainSplitter);

    // 创建工具栏
    createMainToolBar();

    // 初始化模块标签索引
    m_channelTabIndex = -1;
    m_dataAnalysisTabIndex = -1;
    m_videoDisplayTabIndex = -1;
    m_waveformTabIndex = -1;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("主界面布局初始化完成"));
}

QWidget* FX3ToolMainWin::createHomeTabContent()
{
    QWidget* homeWidget = new QWidget(this);
    QVBoxLayout* homeLayout = new QVBoxLayout(homeWidget);

    // 添加欢迎标题
    QLabel* welcomeTitle = new QLabel(LocalQTCompat::fromLocal8Bit("FX3传输测试工具"), this);
    QFont titleFont;
    titleFont.setBold(true);
    titleFont.setPointSize(16);
    welcomeTitle->setFont(titleFont);
    welcomeTitle->setAlignment(Qt::AlignCenter);
    homeLayout->addWidget(welcomeTitle);

    // 添加版本信息
    QLabel* versionLabel = new QLabel(LocalQTCompat::fromLocal8Bit("V1.0.0 (2025-03)"), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    homeLayout->addWidget(versionLabel);

    // 添加分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    homeLayout->addWidget(line);

    // 添加说明文本
    QTextEdit* descriptionEdit = new QTextEdit(this);
    descriptionEdit->setReadOnly(true);
    descriptionEdit->setHtml(LocalQTCompat::fromLocal8Bit(
        "<h3>欢迎使用FX3传输测试工具</h3>"
        "<p>本工具用于FX3设备的数据传输和测试，提供以下功能：</p>"
        "<ul>"
        "<li><b>通道配置：</b> 设置通道参数和使能状态</li>"
        "<li><b>数据分析：</b> 分析采集的数据，提供统计和图表</li>"
        "<li><b>视频显示：</b> 实时显示视频流并调整参数</li>"
        "<li><b>波形分析：</b> 分析信号波形（开发中）</li>"
        "<li><b>文件保存：</b> 保存采集的数据到本地文件</li>"
        "</ul>"
        "<p>使用左侧控制面板控制设备或打开相应的功能模块。设备和传输状态信息将显示在状态栏和左下方状态面板中。</p>"
        "<p>常见设备状态：</p>"
        "<table border='1' cellspacing='0' cellpadding='3'>"
        "<tr><th>状态</th><th>说明</th></tr>"
        "<tr><td>已连接</td><td>设备已连接，可以开始传输</td></tr>"
        "<tr><td>传输中</td><td>设备正在传输数据</td></tr>"
        "<tr><td>已断开</td><td>设备未连接，请检查连接</td></tr>"
        "<tr><td>错误</td><td>设备出现错误，请查看日志</td></tr>"
        "</table>"
    ));
    homeLayout->addWidget(descriptionEdit);

    // 添加功能快速链接
    QGroupBox* quickLinksBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("快速链接"), this);
    QGridLayout* linksLayout = new QGridLayout(quickLinksBox);

    // 创建功能链接按钮
    QPushButton* channelLink = new QPushButton(QIcon(":/icons/channel.png"), LocalQTCompat::fromLocal8Bit("通道配置"), this);
    QPushButton* analysisLink = new QPushButton(QIcon(":/icons/analysis.png"), LocalQTCompat::fromLocal8Bit("数据分析"), this);
    QPushButton* videoLink = new QPushButton(QIcon(":/icons/video.png"), LocalQTCompat::fromLocal8Bit("视频显示"), this);
    QPushButton* saveLink = new QPushButton(QIcon(":/icons/save.png"), LocalQTCompat::fromLocal8Bit("文件保存"), this);

    // 连接信号
    connect(channelLink, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    connect(analysisLink, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    connect(videoLink, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    connect(saveLink, &QPushButton::clicked, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

    // 添加到布局
    linksLayout->addWidget(channelLink, 0, 0);
    linksLayout->addWidget(analysisLink, 0, 1);
    linksLayout->addWidget(videoLink, 1, 0);
    linksLayout->addWidget(saveLink, 1, 1);

    homeLayout->addWidget(quickLinksBox);
    homeLayout->addStretch();

    return homeWidget;
}

void FX3ToolMainWin::updateStatusPanel()
{
    // 这里可以添加额外的状态更新逻辑，比如仪表盘显示等
}

void FX3ToolMainWin::createMainToolBar()
{
    m_mainToolBar = new QToolBar(LocalQTCompat::fromLocal8Bit("主工具栏"), this);
    m_mainToolBar->setIconSize(QSize(24, 24));
    m_mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // 添加工具栏按钮
    QAction* startAction = m_mainToolBar->addAction(QIcon(":/icons/start.png"), LocalQTCompat::fromLocal8Bit("开始传输"));
    QAction* stopAction = m_mainToolBar->addAction(QIcon(":/icons/stop.png"), LocalQTCompat::fromLocal8Bit("停止传输"));
    QAction* resetAction = m_mainToolBar->addAction(QIcon(":/icons/reset.png"), LocalQTCompat::fromLocal8Bit("重置设备"));
    m_mainToolBar->addSeparator();

    QAction* channelAction = m_mainToolBar->addAction(QIcon(":/icons/channel.png"), LocalQTCompat::fromLocal8Bit("通道配置"));
    QAction* dataAction = m_mainToolBar->addAction(QIcon(":/icons/analysis.png"), LocalQTCompat::fromLocal8Bit("数据分析"));
    QAction* videoAction = m_mainToolBar->addAction(QIcon(":/icons/video.png"), LocalQTCompat::fromLocal8Bit("视频显示"));
    QAction* waveformAction = m_mainToolBar->addAction(QIcon(":/icons/waveform.png"), LocalQTCompat::fromLocal8Bit("波形分析"));
    m_mainToolBar->addSeparator();

    QAction* saveAction = m_mainToolBar->addAction(QIcon(":/icons/save.png"), LocalQTCompat::fromLocal8Bit("保存文件"));

    // 连接信号
    connect(startAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onStartButtonClicked);
    connect(stopAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onStopButtonClicked);
    connect(resetAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onResetButtonClicked);

    connect(channelAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowChannelSelectTriggered);
    connect(dataAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowDataAnalysisTriggered);
    connect(videoAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);
    connect(saveAction, &QAction::triggered, this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);

    // 添加工具栏到主窗口
    addToolBar(Qt::TopToolBarArea, m_mainToolBar);
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
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("注册Fx3 USB设备通知失败: %1")
            .arg(GetLastError()));
    }
    else {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("Fx3 USB设备通知注册成功"));
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
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备管理器或UI状态处理器为空，无法连接信号"));
        if (!m_deviceManager) LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备管理器为空"));
        if (!m_uiStateHandler) LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI状态处理器为空"));
    }

    // 连接文件保存管理器信号
    connect(&FileSaveManager::instance(), &FileSaveManager::saveCompleted,
        this, &FX3ToolMainWin::slot_onSaveCompleted);
    connect(&FileSaveManager::instance(), &FileSaveManager::saveError,
        this, &FX3ToolMainWin::slot_onSaveError);

    // 连接状态机事件
    connect(&AppStateMachine::instance(), &AppStateMachine::enteringState, this, &FX3ToolMainWin::slot_onEnteringState);

    // 添加状态变化时更新菜单栏
    connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged,
        this, [this](AppState newState, AppState oldState, const QString& reason) {
            updateMenuBarState(newState);
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
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放视频显示窗口"));
    videoDisplayPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放设备升级窗口"));
    updataDevicePtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放数据分析窗口"));
    dataAnalysisPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放通道选择窗口"));
    channelSelectPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放文件保存对话框"));
    saveFileBoxPtr.reset();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放文件保存面板"));
    fileSavePanelPtr.reset();

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略停止请求"));
        return;
    }

    // 调用设备管理器停止传输
    if (m_deviceManager) {
        m_deviceManager->stopTransfer();
    }
}

void FX3ToolMainWin::slot_onResetButtonClicked() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置设备按钮点击"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略重置请求"));
        return;
    }

    // 调用设备管理器重置设备
    if (m_deviceManager) {
        m_deviceManager->resetDevice();
    }
}

// 通道选择窗口触发函数
void FX3ToolMainWin::slot_onShowChannelSelectTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示通道配置窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 延迟创建通道选择窗口（单例模式）
    if (!m_channelSelectWidget) {
        m_channelSelectWidget = new ChannelSelect(this);

        // 修改窗口标记，使其嵌入而非独立窗口
        m_channelSelectWidget->setWindowFlags(Qt::Widget);

        // 连接信号
        connect(m_channelSelectWidget, &ChannelSelect::channelConfigChanged,
            this, &FX3ToolMainWin::slot_onChannelConfigChanged);

        // 设置初始值（如果有）
        ChannelConfig initialConfig;
        // 获取当前UI中的参数
        bool ok = false;
        int width = ui.imageWIdth->text().toInt(&ok);
        if (ok && width > 0) initialConfig.videoWidth = width;

        int height = ui.imageHeight->text().toInt(&ok);
        if (ok && height > 0) initialConfig.videoHeight = height;

        // 设置到通道选择窗口
        m_channelSelectWidget->setConfigToUI(initialConfig);
    }

    // 将通道选择模块添加到主标签页
    showModuleTab(m_channelTabIndex, m_channelSelectWidget,
        LocalQTCompat::fromLocal8Bit("通道配置"),
        QIcon(":/icons/channel.png"));
}

// 数据分析窗口触发函数
void FX3ToolMainWin::slot_onShowDataAnalysisTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示数据分析窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 延迟创建数据分析窗口（单例模式）
    if (!m_dataAnalysisWidget) {
        m_dataAnalysisWidget = new DataAnalysis(this);

        // 修改窗口标记，使其嵌入而非独立窗口
        m_dataAnalysisWidget->setWindowFlags(Qt::Widget);

        // 连接保存数据和视频预览信号
        connect(m_dataAnalysisWidget, &DataAnalysis::saveDataRequested,
            this, &FX3ToolMainWin::slot_onShowSaveFileBoxTriggered);
        connect(m_dataAnalysisWidget, &DataAnalysis::videoDisplayRequested,
            this, &FX3ToolMainWin::slot_onShowVideoDisplayTriggered);

        // 如果已有数据，可以设置到分析窗口
        // 示例：if (m_deviceManager && m_deviceManager->hasData()) {
        //    m_dataAnalysisWidget->setAnalysisData(m_deviceManager->getCollectedData());
        // }
    }

    // 将数据分析模块添加到主标签页
    showModuleTab(m_dataAnalysisTabIndex, m_dataAnalysisWidget,
        LocalQTCompat::fromLocal8Bit("数据分析"),
        QIcon(":/icons/analysis.png"));
}

// 设备升级窗口触发函数
void FX3ToolMainWin::slot_onShowUpdataDeviceTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示设备升级窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 延迟创建设备升级窗口（单例模式）
    if (!m_updataDeviceWidget) {
        m_updataDeviceWidget = new UpdataDevice(this);

        // 连接升级完成信号
        connect(m_updataDeviceWidget, &UpdataDevice::updateCompleted,
            this, [this](bool success, const QString& message) {
                // 更新状态显示
                if (success) {
                    if (m_uiStateHandler) {
                    }
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

// 视频显示窗口触发函数
void FX3ToolMainWin::slot_onShowVideoDisplayTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示视频窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 验证图像参数
    uint16_t width;
    uint16_t height;
    uint8_t capType;

    if (!validateImageParameters(width, height, capType)) {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("警告"),
            LocalQTCompat::fromLocal8Bit("图像参数无效，请先设置正确的图像参数"));
        return;
    }

    // 延迟创建视频显示窗口（单例模式）
    if (!m_videoDisplayWidget) {
        m_videoDisplayWidget = new VideoDisplay(this);

        // 修改窗口标记，使其嵌入而非独立窗口
        m_videoDisplayWidget->setWindowFlags(Qt::Widget);

        // 连接视频状态变更信号
        connect(m_videoDisplayWidget, &VideoDisplay::videoDisplayStatusChanged,
            this, &FX3ToolMainWin::slot_onVideoDisplayStatusChanged);
    }

    // 设置图像参数
    m_videoDisplayWidget->setImageParameters(width, height, capType);

    // 将视频显示模块添加到主标签页
    showModuleTab(m_videoDisplayTabIndex, m_videoDisplayWidget,
        LocalQTCompat::fromLocal8Bit("视频显示"),
        QIcon(":/icons/video.png"));
}

void FX3ToolMainWin::slot_onShowWaveformAnalysisTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示波形分析窗口"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 这里只添加一个简单的提示，表明功能正在开发中
    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("功能开发中"),
        LocalQTCompat::fromLocal8Bit("波形分析功能正在开发中，敬请期待！"));

    // 此处可以根据需要实现简单的波形分析框架
    // 暂不实现完整功能
}

void FX3ToolMainWin::slot_onVideoDisplayStatusChanged(bool isRunning) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频显示状态变更: %1").arg(isRunning ? "运行中" : "已停止"));

    // 更新UI状态
    if (m_uiStateHandler) {
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

void FX3ToolMainWin::slot_onSaveButtonClicked() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存按钮点击"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略保存请求"));
        return;
    }

    // 检查文件保存面板是否存在
    if (!m_fileSavePanel) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存面板未初始化"));
        QMessageBox::warning(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("文件保存功能未初始化"));
        return;
    }

    // 如果保存已经在进行中，则停止保存
    if (m_fileSavePanel->isSaving()) {
        m_fileSavePanel->stopSaving();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
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
        LOG_INFO(LocalQTCompat::fromLocal8Bit("开始文件保存，参数：宽度=%1，高度=%2，格式=0x%3")
            .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));
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
        LOG_ERROR("无效的图像宽度");
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
        LOG_ERROR("无效的图像高度");
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

    // 检查文件保存面板是否正在保存数据
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        // 将数据包传递给文件保存管理器
        FileSaveManager::instance().processDataPacket(packet);
    }
}

void FX3ToolMainWin::slot_onSaveCompleted(const QString& path, uint64_t totalBytes) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存完成: 路径=%1, 总大小=%2 字节")
        .arg(path).arg(totalBytes));

    // 这里可以添加任何保存完成后的逻辑，比如更新UI等
}

void FX3ToolMainWin::slot_onSaveError(const QString& error) {
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误: %1").arg(error));

    // 这里可以添加保存错误处理逻辑
    // 例如在状态栏显示错误信息等
}

void FX3ToolMainWin::slot_onShowSaveFileBoxTriggered() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示文件保存对话框"));

    if (m_isClosing) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，忽略显示请求"));
        return;
    }

    // 验证参数
    uint16_t width;
    uint16_t height;
    uint8_t capType;

    if (!validateImageParameters(width, height, capType)) {
        return;
    }

    // 延迟创建文件保存对话框
    if (!m_saveFileBox) {
        m_saveFileBox = new SaveFileBox(this);

        // 连接信号
        connect(m_saveFileBox, &SaveFileBox::saveCompleted,
            this, [this](const QString& path, uint64_t totalBytes) {
                slot_onSaveFileBoxCompleted(path, totalBytes);
                // 通知可能的数据分析模块
                if (m_dataAnalysisWidget) {
                    QMessageBox::information(m_dataAnalysisWidget,
                        LocalQTCompat::fromLocal8Bit("保存完成"),
                        LocalQTCompat::fromLocal8Bit("数据已保存到: %1\n总大小: %2 MB")
                        .arg(path)
                        .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2));
                }
            });

        connect(m_saveFileBox, &SaveFileBox::saveError,
            this, [this](const QString& error) {
                slot_onSaveFileBoxError(error);
                // 通知可能的数据分析模块
                if (m_dataAnalysisWidget) {
                    QMessageBox::critical(m_dataAnalysisWidget,
                        LocalQTCompat::fromLocal8Bit("保存错误"),
                        LocalQTCompat::fromLocal8Bit("保存数据时出错: %1").arg(error));
                }
            });
    }

    // 设置图像参数
    m_saveFileBox->setImageParameters(width, height, capType);

    // 准备显示
    m_saveFileBox->prepareForShow();

    // 将保存对话框保持为独立窗口
    m_saveFileBox->show();
    m_saveFileBox->raise();
    m_saveFileBox->activateWindow();
}

// 添加文件保存完成槽函数
void FX3ToolMainWin::slot_onSaveFileBoxCompleted(const QString& path, uint64_t totalBytes) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存完成：路径=%1，总大小=%2字节")
        .arg(path).arg(totalBytes));

    // 在这里可以添加保存完成后的处理逻辑
    // 例如更新状态栏显示等
}

// 添加文件保存错误槽函数
void FX3ToolMainWin::slot_onSaveFileBoxError(const QString& error) {
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误：%1").arg(error));

    // 在这里可以添加错误处理逻辑
}
