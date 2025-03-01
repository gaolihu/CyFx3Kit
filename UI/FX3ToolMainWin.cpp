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

std::atomic<bool> FX3ToolMainWin::s_resourcesReleased{ false };

// FX3ToolMainWin.cpp中初始化部分优化

FX3ToolMainWin::FX3ToolMainWin(QWidget* parent)
    : QMainWindow(parent)
    , m_isClosing(false)
    , m_loggerInitialized(false)
    , m_deviceManager(nullptr)
    , m_uiStateHandler(nullptr)
{
    ui.setupUi(this);

    // 明确的初始化顺序，带有错误处理
    try {
        // 1. 首先初始化UI和日志系统
        initializeUI();
        if (!initializeLogger()) {
            QMessageBox::critical(this, QString::fromLocal8Bit("错误"),
                QString::fromLocal8Bit("日志系统初始化失败，应用程序无法继续"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        LOG_INFO(QString::fromLocal8Bit("应用程序启动，Qt版本: %1").arg(QT_VERSION_STR));

        // 2. 创建UI状态处理器
        m_uiStateHandler = new UIStateHandler(ui, this);
        if (!m_uiStateHandler) {
            LOG_ERROR(QString::fromLocal8Bit("创建UI状态处理器失败"));
            QMessageBox::critical(this, QString::fromLocal8Bit("错误"),
                QString::fromLocal8Bit("UI状态处理器创建失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 3. 创建设备管理器
        m_deviceManager = new FX3DeviceManager(this);
        if (!m_deviceManager) {
            LOG_ERROR(QString::fromLocal8Bit("创建设备管理器失败"));
            QMessageBox::critical(this, QString::fromLocal8Bit("错误"),
                QString::fromLocal8Bit("设备管理器创建失败"));
            QTimer::singleShot(0, this, &FX3ToolMainWin::close);
            return;
        }

        // 4. 初始化所有信号连接
        initializeConnections();

        // 5. 启动设备检测
        registerDeviceNotification();

        // 6. 设置初始状态并初始化设备
        AppStateMachine::instance().processEvent(StateEvent::APP_INIT,
            QString::fromLocal8Bit("应用程序初始化完成"));

        if (!m_deviceManager->initializeDeviceAndManager((HWND)this->winId())) {
            LOG_WARN(QString::fromLocal8Bit("设备初始化失败，应用将以离线模式运行"));
            QMessageBox::warning(this, QString::fromLocal8Bit("警告"),
                QString::fromLocal8Bit("设备初始化失败，将以离线模式运行"));
            // 继续运行，而不是退出应用
        }

        LOG_INFO(QString::fromLocal8Bit("FX3ToolMainWin构造函数完成..."));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, QString::fromLocal8Bit("错误"),
            QString::fromLocal8Bit("初始化过程中发生异常: %1").arg(e.what()));
        LOG_ERROR(QString::fromLocal8Bit("初始化异常: %1").arg(e.what()));
        QTimer::singleShot(0, this, &FX3ToolMainWin::close);
    }
    catch (...) {
        QMessageBox::critical(this, QString::fromLocal8Bit("错误"),
            QString::fromLocal8Bit("初始化过程中发生未知异常"));
        LOG_ERROR(QString::fromLocal8Bit("初始化未知异常"));
        QTimer::singleShot(0, this, &FX3ToolMainWin::close);
    }
}

FX3ToolMainWin::~FX3ToolMainWin() {
    LOG_INFO(QString::fromLocal8Bit("FX3ToolMainWin析构函数入口"));
    LOG_INFO(QString::fromLocal8Bit("设置关闭标志"));
    m_isClosing = true;

    // 检查资源是否已在closeEvent中释放
    if (!s_resourcesReleased) {
        if (m_deviceManager) {
            LOG_INFO(QString::fromLocal8Bit("在析构函数中删除设备管理器"));
            delete m_deviceManager;
            m_deviceManager = nullptr;
        }

        if (m_uiStateHandler) {
            LOG_INFO(QString::fromLocal8Bit("在析构函数中删除UI状态处理器"));
            delete m_uiStateHandler;
            m_uiStateHandler = nullptr;
        }
    }
    else {
        LOG_INFO(QString::fromLocal8Bit("资源已在closeEvent中释放"));
    }

    LOG_INFO(QString::fromLocal8Bit("FX3ToolMainWin析构函数退出 - 成功"));
}

bool FX3ToolMainWin::nativeEvent(const QByteArray& eventType, void* message, long* result)
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

// FX3ToolMainWin.cpp中closeEvent优化

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
    LOG_INFO(QString::fromLocal8Bit("应用程序关闭中，正在清理资源..."));

    try {
        // 1. 首先停止所有定时器
        QList<QTimer*> timers = findChildren<QTimer*>();
        for (QTimer* timer : timers) {
            if (timer && timer->isActive()) {
                timer->stop();
                disconnect(timer, nullptr, nullptr, nullptr);
            }
        }

        // 2. 通知UI状态处理器准备关闭
        if (m_uiStateHandler) {
            m_uiStateHandler->prepareForClose();
        }

        // 3. 断开状态机信号连接，避免在关闭过程中的信号干扰
        disconnect(&AppStateMachine::instance(), nullptr, this, nullptr);
        if (m_uiStateHandler) {
            disconnect(&AppStateMachine::instance(), nullptr, m_uiStateHandler, nullptr);
        }

        // 4. 发送关闭事件到状态机
        AppStateMachine::instance().processEvent(StateEvent::APP_SHUTDOWN,
            QString::fromLocal8Bit("应用程序正在关闭"));

        // 5. 停止设备传输、采集和资源释放
        stopAndReleaseResources();

        // 接受关闭事件
        event->accept();
        LOG_INFO(QString::fromLocal8Bit("关闭流程执行完成，释放互斥锁"));
        closeMutex.unlock();
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString::fromLocal8Bit("关闭过程异常: %1").arg(e.what()));
        event->accept();
        closeMutex.unlock();
    }
    catch (...) {
        LOG_ERROR(QString::fromLocal8Bit("关闭过程未知异常"));
        event->accept();
        closeMutex.unlock();
    }
}

void FX3ToolMainWin::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    adjustStatusBar();
}

void FX3ToolMainWin::adjustStatusBar() {
    QStatusBar* statusBar = this->statusBar();
    if (!statusBar) return;

    statusBar->setMinimumWidth(40);
    statusBar->setMinimumHeight(30);
}

// 在FX3ToolMainWin::initializeUI()中修改初始按钮状态设置

void FX3ToolMainWin::initializeUI() {
    LOG_INFO(QString::fromLocal8Bit("初始化UI组件"));

    // 在任何其他初始化之前，先禁用所有控制按钮
    ui.startButton->setEnabled(false);
    ui.stopButton->setEnabled(false);

    // 重要：检查重置按钮的状态设置
    LOG_INFO(QString::fromLocal8Bit("设置重置按钮状态为禁用"));
    ui.resetButton->setEnabled(false);

    // 立即检查按钮状态
    LOG_INFO(QString::fromLocal8Bit("初始化UI后按钮状态: 开始=%1, 停止=%2, 重置=%3")
        .arg(ui.startButton->isEnabled() ? "启用" : "禁用")
        .arg(ui.stopButton->isEnabled() ? "启用" : "禁用")
        .arg(ui.resetButton->isEnabled() ? "启用" : "禁用"));

    // 设置状态栏自适应布局
    QStatusBar* statusBar = this->statusBar();
    statusBar->setSizeGripEnabled(false);

    // 为所有状态栏标签设置统一字体
    QFont statusFont("Microsoft YaHei", 9); // 使用雅黑字体或其他合适的系统字体

    // 设置所有标签的统一字体和样式
    ui.usbSpeedLabel->setFont(statusFont);
    ui.usbStatusLabel->setFont(statusFont);
    ui.transferStatusLabel->setFont(statusFont);
    ui.speedLabel->setFont(statusFont);
    ui.totalBytesLabel->setFont(statusFont);
    ui.totalTimeLabel->setFont(statusFont);

    // 使用QLabel替代原有标签，并设置最小宽度
    ui.usbSpeedLabel->setMinimumWidth(200);
    ui.usbStatusLabel->setMinimumWidth(200);
    ui.transferStatusLabel->setMinimumWidth(200);
    ui.speedLabel->setMinimumWidth(250);
    ui.totalBytesLabel->setMinimumWidth(200);

    // 设置窗口最小尺寸
    this->setMinimumSize(1000, 800);

    // 初始化命令状态
    ui.cmdStatusLabel->setText(QString::fromLocal8Bit("命令文件未加载"));
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
}

bool FX3ToolMainWin::initializeLogger() {
    if (m_loggerInitialized) {
        return true;
    }

    QString logPath = QApplication::applicationDirPath() + "/fx3_t.log";

    try {
        // 初始化日志文件
        Logger::instance().setLogFile(logPath);

        // 设置日志控件
        if (ui.logTextEdit) {
            Logger::instance().setLogWidget(ui.logTextEdit);
        }
        else {
            throw std::runtime_error("未找到日志控件");
        }

        // 添加启动日志
        LOG_INFO(QString::fromLocal8Bit("日志: %1").arg(logPath));

        m_loggerInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "日志初始化失败:" << e.what();
        return false;
    }
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
        LOG_ERROR(QString::fromLocal8Bit("注册Fx3 USB设备通知失败: %1")
            .arg(GetLastError()));
    }
    else {
        LOG_DEBUG(QString::fromLocal8Bit("Fx3 USB设备通知注册成功"));
    }
}

void FX3ToolMainWin::initializeConnections() {
    // 按钮信号连接
    connect(ui.startButton, &QPushButton::clicked, this, &FX3ToolMainWin::onStartButtonClicked);
    connect(ui.stopButton, &QPushButton::clicked, this, &FX3ToolMainWin::onStopButtonClicked);
    connect(ui.resetButton, &QPushButton::clicked, this, &FX3ToolMainWin::onResetButtonClicked);

    // 命令目录按钮连接
    connect(ui.cmdDirButton, &QPushButton::clicked, this, &FX3ToolMainWin::onSelectCommandDirectory);

    // 确保对象已经创建并初始化
    if (m_deviceManager && m_uiStateHandler) {
        // 设备管理器连接
        connect(m_deviceManager, &FX3DeviceManager::transferStatsUpdated,
            m_uiStateHandler, &UIStateHandler::updateTransferStats);
        connect(m_deviceManager, &FX3DeviceManager::usbSpeedUpdated,
            m_uiStateHandler, &UIStateHandler::updateUsbSpeedDisplay);
        connect(m_deviceManager, &FX3DeviceManager::deviceError,
            m_uiStateHandler, &UIStateHandler::showErrorMessage);

        // 尝试直接连接状态机信号，不使用引用
        AppStateMachine& stateMachine = AppStateMachine::instance();

        // 强制删除之前的连接，避免重复
        disconnect(&stateMachine, &AppStateMachine::stateChanged,
            m_uiStateHandler, &UIStateHandler::onStateChanged);

        // 重新建立连接，使用直接连接而非队列连接，确保立即处理
        bool connected = connect(&stateMachine, &AppStateMachine::stateChanged,
            m_uiStateHandler, &UIStateHandler::onStateChanged, Qt::DirectConnection);

        LOG_INFO(QString::fromLocal8Bit("状态机stateChanged信号连接到UI处理器 - 连接状态: %1")
            .arg(connected ? QString::fromLocal8Bit("成功") : QString::fromLocal8Bit("失败")));

        // 如果连接失败，尝试替代方法
        if (!connected) {
            LOG_ERROR(QString::fromLocal8Bit("信号连接失败，尝试替代方法"));

            // 使用lambda表达式作为中介
            connect(&stateMachine, &AppStateMachine::stateChanged,
                this, [this](AppState newState, AppState oldState, const QString& reason) {
                    m_uiStateHandler->onStateChanged(newState, oldState, reason);
                }, Qt::DirectConnection);
        }
    }
    else {
        LOG_ERROR(QString::fromLocal8Bit("设备管理器或UI状态处理器为空，无法连接信号"));
        if (!m_deviceManager) LOG_ERROR(QString::fromLocal8Bit("设备管理器为空"));
        if (!m_uiStateHandler) LOG_ERROR(QString::fromLocal8Bit("UI状态处理器为空"));
    }

    // 连接状态机事件
    bool enteringConnected = connect(&AppStateMachine::instance(), &AppStateMachine::enteringState,
        this, &FX3ToolMainWin::onEnteringState);
    LOG_INFO(QString::fromLocal8Bit("状态机enteringState信号连接到FX3ToolMainWin - 连接状态: %1")
        .arg(enteringConnected ? QString::fromLocal8Bit("成功") : QString::fromLocal8Bit("失败")));
}

void FX3ToolMainWin::stopAndReleaseResources() {
    // 1. 确保设备管理器停止所有传输
    if (m_deviceManager) {
        if (m_deviceManager->isTransferring()) {
            LOG_INFO(QString::fromLocal8Bit("停止正在进行的数据传输"));
            m_deviceManager->stopTransfer();

            // 给一定时间让设备管理器完成停止操作
            QElapsedTimer timer;
            timer.start();
            const int MAX_WAIT_MS = 300;

            while (m_deviceManager->isTransferring() && timer.elapsed() < MAX_WAIT_MS) {
                QThread::msleep(10);
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }
    }

    // 2. 创建单独的智能指针保存设备管理器和UI状态处理器，以便在类析构前释放
    std::unique_ptr<FX3DeviceManager> deviceManagerPtr(m_deviceManager);
    std::unique_ptr<UIStateHandler> uiHandlerPtr(m_uiStateHandler);

    // 将类成员设为nullptr防止析构函数重复释放
    m_deviceManager = nullptr;
    m_uiStateHandler = nullptr;

    // 3. 先释放设备管理器 - 这会同时清理USB设备和采集管理器
    if (deviceManagerPtr) {
        LOG_INFO(QString::fromLocal8Bit("释放设备管理器"));
        deviceManagerPtr.reset();
    }

    // 4. 然后释放UI状态处理器
    if (uiHandlerPtr) {
        LOG_INFO(QString::fromLocal8Bit("释放UI状态处理器"));
        uiHandlerPtr.reset();
    }

    // 设置静态资源释放标志
    s_resourcesReleased = true;
    LOG_INFO(QString::fromLocal8Bit("所有资源已释放完成"));
}

void FX3ToolMainWin::onStartButtonClicked() {
    LOG_INFO(QString::fromLocal8Bit("开始传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(QString::fromLocal8Bit("应用程序正在关闭，忽略开始请求"));
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

void FX3ToolMainWin::onStopButtonClicked() {
    LOG_INFO(QString::fromLocal8Bit("停止传输按钮点击"));

    if (m_isClosing) {
        LOG_INFO(QString::fromLocal8Bit("应用程序正在关闭，忽略停止请求"));
        return;
    }

    // 调用设备管理器停止传输
    if (m_deviceManager) {
        m_deviceManager->stopTransfer();
    }
}

void FX3ToolMainWin::onResetButtonClicked() {
    LOG_INFO(QString::fromLocal8Bit("重置设备按钮点击"));

    if (m_isClosing) {
        LOG_INFO(QString::fromLocal8Bit("应用程序正在关闭，忽略重置请求"));
        return;
    }

    // 调用设备管理器重置设备
    if (m_deviceManager) {
        m_deviceManager->resetDevice();
    }
}

void FX3ToolMainWin::onSelectCommandDirectory() {
    LOG_INFO(QString::fromLocal8Bit("选择命令文件目录"));

    if (m_isClosing) {
        return;
    }

    QString dir = QFileDialog::getExistingDirectory(
        this,
        QString::fromLocal8Bit("选择命令文件目录"),
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
            QString::fromLocal8Bit("错误"),
            QString::fromLocal8Bit("无法加载命令文件，请确保目录包含所需的所有命令文件")
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
        QMessageBox::warning(this, QString::fromLocal8Bit("错误"),
            QString::fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值"));
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
        QMessageBox::warning(this, QString::fromLocal8Bit("错误"),
            QString::fromLocal8Bit("无效的图像高度，请输入1-4096之间的值"));
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

    LOG_INFO(QString::fromLocal8Bit("图像参数验证通过 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    return true;
}

void FX3ToolMainWin::onEnteringState(AppState state, const QString& reason) {
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