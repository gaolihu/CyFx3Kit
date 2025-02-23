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
#include <algorithm>

#include "FX3TestTool.h"
#include "CommandManager.h"
#include "Logger.h"

// Cypress FX3 Device GUID
// 注意：这个GUID需要与您的设备实际GUID匹配: CYUSBDRV_GUID

FX3TestTool::FX3TestTool(QWidget* parent)
    : QMainWindow(parent)
    , m_deviceInitializing(false)
    , m_loggerInitialized(false)
    , m_commandsLoaded(false)  // 确保初始化为false
    , m_deviceReady(false)     // 确保初始化为false
{
    // Setup UI from the form
    ui.setupUi(this);

    // 在任何其他初始化之前，先禁用所有控制按钮
    ui.startButton->setEnabled(false);
    ui.stopButton->setEnabled(false);
    ui.resetButton->setEnabled(false);

    setupUI();

    // 初始化Logger
    if (!initializeLogger()) {
        QMessageBox::critical(this, "错误", "日志系统初始化失败");
        return;
    }

    // 初始化设备和采集管理器
    initializeDeviceAndManager();

    // 注册设备通知
    registerDeviceNotification();

    // 初始化信号连接
    initConnections();

    // 检查命令文件状态
    updateCommandStatus(false);  // 确保命令状态为未加载

    // 检查设备状态 - 即使设备就绪也不会启用开始按钮，因为命令未加载
    checkAndOpenDevice();
}

FX3TestTool::~FX3TestTool()
{
}

bool FX3TestTool::nativeEvent(const QByteArray& eventType, void* message, long* result)
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
                    handleDeviceArrival();
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
                    handleDeviceRemoval();
                }
            }
            break;
        }
        }
    }

    return false;
}

void FX3TestTool::onDeviceInitialize()
{
    LOG_DEBUG("Starting device initialization...");

    if (m_usbDevice) {
        m_usbDevice->close();
    }

    m_usbDevice = std::make_unique<USBDevice>((HWND)this->winId());
    initConnections();

    if (!checkAndOpenDevice()) {
        LOG_ERROR("Device initialization failed");
    }

    m_deviceInitializing = false;
}

void FX3TestTool::onStartTransfer()
{
    if (!m_usbDevice || !m_acquisitionManager) {
        LOG_ERROR("Device or acquisition manager not initialized");
        return;
    }

    // 配置采集参数
    const uint16_t width = 1920;
    const uint16_t height = 1080;
    const uint8_t capType = 0x39;

    // 先启动采集管理器
    if (!m_acquisitionManager->startAcquisition(width, height, capType)) {
        LOG_ERROR("Failed to start acquisition manager");
        return;
    }

    // 再启动USB传输
    if (!m_usbDevice->startTransfer()) {
        LOG_ERROR("Failed to start USB transfer");
        m_acquisitionManager->stopAcquisition();
        return;
    }

    ui.startButton->setEnabled(false);
    ui.stopButton->setEnabled(true);
    LOG_INFO("Data acquisition started successfully");
}

void FX3TestTool::onStopTransfer()
{
    LOG_INFO("Transfer stopping");

    m_stopRequested = true;
    m_deviceReady = false;
    updateButtonStates();

    QThread* stopThread = new QThread(this);
    StopWorker* worker = new StopWorker(m_acquisitionManager, m_usbDevice);
    worker->moveToThread(stopThread);

    connect(stopThread, &QThread::started, worker, &StopWorker::doStop);
#if 0
    connect(worker, &StopWorker::stopCompleted, this, [this]() {
        m_stopRequested = false;
        m_deviceReady = true;
        updateButtonStates();
        LOG_INFO("Transfer stopped successfully");
        });
#endif

    connect(worker, &StopWorker::stopError, this, [this](const QString& error) {
        LOG_ERROR(QString("Stop operation failed: %1").arg(error));
        m_stopRequested = false;
        m_deviceReady = true;
        updateButtonStates();
        });

    connect(worker, &StopWorker::stopCompleted, worker, &StopWorker::deleteLater);
    connect(worker, &StopWorker::stopCompleted, stopThread, &QThread::quit);
    connect(stopThread, &QThread::finished, stopThread, &QThread::deleteLater);

    QTimer::singleShot(5000, this, [this, worker]() {
        if (m_stopRequested) {
            LOG_WARN("Stop operation timeout, forcing completion");
            worker->forceStop();
        }
        });

    stopThread->start();
}

void FX3TestTool::onResetDevice()
{
    if (m_usbDevice->reset()) {
        m_deviceReady = true;
        // 不直接设置按钮状态，通过updateButtonStates统一处理
        updateButtonStates();
    }
}

void FX3TestTool::onUsbStatusChanged(const std::string& status)
{
    QString statusText;

    if (status == "ready") {
        statusText = QString::fromLocal8Bit("就绪");
        m_deviceReady = true;
        // 不直接设置按钮状态，而是通过updateButtonStates统一处理
        updateButtonStates();
    }
    else if (status == "transferring") {
        statusText = QString::fromLocal8Bit("传输中");
        updateButtonStates();
    }
    else if (status == "disconnected") {
        statusText = QString::fromLocal8Bit("已断开");
        m_deviceReady = false;
        updateButtonStates();
    }
    else if (status == "error") {
        statusText = QString::fromLocal8Bit("错误");
        m_deviceReady = false;
        updateButtonStates();
    }

    updateStatusBar(statusText);
}

void FX3TestTool::onTransferProgress(uint64_t transferred, int length, int success, int failed)
{
    static QElapsedTimer speedTimer;
    static uint64_t lastTransferred = 0;

    // 每次都要计算速度，不要跳过
    if (!speedTimer.isValid()) {
        speedTimer.start();
        lastTransferred = transferred;
        return;
    }

    // 去掉1000ms的等待，改为实时计算
    qint64 elapsed = speedTimer.elapsed();
    if (elapsed > 0) {  // 只要有时间流逝就计算速度
        double intervalBytes = static_cast<double>(transferred - lastTransferred);
        // 这里需要将毫秒转换为秒，所以要乘以1000
        double speed = (intervalBytes * 1000.0) / (elapsed * 1024.0 * 1024.0);

        updateTransferStatus(transferred, speed);

        // 更新基准值
        speedTimer.restart();
        lastTransferred = transferred;
    }
}

void FX3TestTool::onDeviceError(const QString& error)
{
    QMessageBox::warning(this,
        QString::fromLocal8Bit("错误"),
        error);
}

void FX3TestTool::onSelectCommandDirectory()
{
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
    if (!loadCommandFiles(dir)) {
        QMessageBox::warning(
            this,
            QString::fromLocal8Bit("错误"),
            QString::fromLocal8Bit("无法加载命令文件，请确保目录包含所需的所有命令文件")
        );
        ui.cmdDirEdit->clear();
        return;
    }

    // 更新命令状态标签
    updateCommandStatus(true);
}

void FX3TestTool::onStopComplete()
{
    m_stopRequested = false;
    m_deviceReady = true;

    // 通过updateButtonStates统一处理按钮状态
    updateButtonStates();

    LOG_INFO("Transfer stopped by user");
}

bool FX3TestTool::loadCommandFiles(const QString& dir)
{
    LOG_INFO(QString("Loading command files from directory: %1").arg(dir));

    // 设置命令目录
    if (!CommandManager::instance().setCommandDirectory(dir)) {
        LOG_ERROR("Failed to set command directory");
        m_commandsLoaded = false;
        updateButtonStates();
        return false;
    }

    // 验证所有必需的命令文件
    if (!CommandManager::instance().validateCommands()) {
        LOG_ERROR("Command validation failed");
        m_commandsLoaded = false;
        updateButtonStates();
        return false;
    }

    LOG_INFO("Command files loaded successfully");
    m_commandsLoaded = true;
    updateButtonStates();
    return true;
}

void FX3TestTool::updateCommandStatus(bool valid)
{
    m_commandsLoaded = valid;

    if (valid) {
        ui.cmdStatusLabel->setText(QString::fromLocal8Bit("命令文件加载成功"));
        ui.cmdStatusLabel->setStyleSheet("color: green;");
    }
    else {
        ui.cmdStatusLabel->setText(QString::fromLocal8Bit("命令文件未加载"));
        ui.cmdStatusLabel->setStyleSheet("color: red;");
    }

    updateButtonStates();
}

void FX3TestTool::setupUI()
{
    // 设置状态栏自适应布局
    QStatusBar* statusBar = this->statusBar();
    statusBar->setSizeGripEnabled(false);

    // 使用QLabel替代原有标签，并设置最小宽度
    ui.usbStatusLabel->setMinimumWidth(200);
    ui.transferStatusLabel->setMinimumWidth(200);
    ui.speedLabel->setMinimumWidth(150);
    ui.totalBytesLabel->setMinimumWidth(200);

    // 将标签添加到状态栏
    statusBar->addWidget(ui.usbStatusLabel);
    statusBar->addWidget(ui.transferStatusLabel);
    statusBar->addWidget(ui.speedLabel);
    statusBar->addWidget(ui.totalBytesLabel);

    // 设置窗口最小尺寸
    this->setMinimumSize(800, 600);

    // 初始化命令状态
    updateCommandStatus(false);
}

void FX3TestTool::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    adjustStatusBar();
}

void FX3TestTool::adjustStatusBar()
{
    QStatusBar* statusBar = this->statusBar();
    if (!statusBar) return;

    statusBar->setMinimumWidth(40);
    statusBar->setMinimumHeight(30);
}

void FX3TestTool::handleAcquiredData(const DataPacket& packet)
{
    // 这里处理采集到的数据
    // 例如：显示、保存、处理等
    LOG_DEBUG(QString("Received data packet, size: %1 bytes").arg(packet.size));
}

void FX3TestTool::updateStats(uint64_t receivedBytes, double dataRate)
{
    // 更新UI显示的统计信息
    /*
    QString speedText = QString::fromLocal8Bit("速度: %1 MB/s").arg(dataRate, 0, 'f', 2);
    QString totalText = QString::fromLocal8Bit("总计: %1 MB").arg(receivedBytes / (1024.0 * 1024.0), 0, 'f', 2);

    ui.speedLabel->setText(speedText);
    ui.totalBytesLabel->setText(totalText);
    */
}

bool FX3TestTool::initializeLogger()
{
    if (m_loggerInitialized) {
        return true;
    }

    QString logPath = QApplication::applicationDirPath() + "/fx3_test.log";

    try {
        // 初始化日志文件
        Logger::instance().setLogFile(logPath);

        // 设置日志控件
        if (ui.logTextEdit) {
            Logger::instance().setLogWidget(ui.logTextEdit);
        }
        else {
            throw std::runtime_error("Log widget not found");
        }

        // 添加启动日志
        LOG_INFO("Application starting...");
        LOG_INFO(QString("Log file path: %1").arg(logPath));
        LOG_INFO("Logger initialization completed");

        // 记录应用程序信息
        LOG_INFO(QString("Application path: %1").arg(QApplication::applicationDirPath()));
        LOG_INFO(QString("Qt version: %1").arg(QT_VERSION_STR));

        m_loggerInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Logger initialization failed:" << e.what();
        return false;
    }
}

bool FX3TestTool::checkAndOpenDevice()
{
    LOG_INFO("Checking device connection status...");

    if (!m_usbDevice) {
        LOG_ERROR("USB device object not initialized");
        updateUIState(false);
        return false;
    }

    // 检查设备连接状态
    if (!m_usbDevice->isConnected()) {
        LOG_WARN("No device connected");
        updateUIState(false);
        return false;
    }

    // 获取并记录设备信息
    QString deviceInfo = m_usbDevice->getDeviceInfo();
    LOG_INFO(QString("Found device: %1").arg(deviceInfo));

    // 尝试打开设备
    if (!m_usbDevice->open()) {
        LOG_ERROR("Failed to open device");
        updateUIState(false);
        return false;
    }

    LOG_INFO("Device opened successfully");

    // 更新UI状态
    updateUIState(true);
    updateStatusBar(QString::fromLocal8Bit("就绪"));

    // 发送设备就绪信号
    emit m_usbDevice->statusChanged("ready");

    return true;
}

void FX3TestTool::initConnections()
{
    // 初始化防抖定时器
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(DEBOUNCE_DELAY);
    connect(&m_debounceTimer, &QTimer::timeout, this, &FX3TestTool::onDeviceInitialize);

    // Button signal connections
    connect(ui.startButton, &QPushButton::clicked, this, &FX3TestTool::onStartTransfer);
    connect(ui.stopButton, &QPushButton::clicked, this, &FX3TestTool::onStopTransfer);
    connect(ui.resetButton, &QPushButton::clicked, this, &FX3TestTool::onResetDevice);

    // USB device signal connections
    connect(m_usbDevice.get(), &USBDevice::statusChanged, this, &FX3TestTool::onUsbStatusChanged);
    connect(m_usbDevice.get(), &USBDevice::transferProgress, this, &FX3TestTool::onTransferProgress);
    connect(m_usbDevice.get(), &USBDevice::deviceError, this, &FX3TestTool::onDeviceError);

    // 连接DataAcquisitionManager的信号
    connect(m_acquisitionManager.get(), &DataAcquisitionManager::dataReceived,
        this, &FX3TestTool::handleAcquiredData);
    connect(m_acquisitionManager.get(), &DataAcquisitionManager::errorOccurred,
        this, &FX3TestTool::onDeviceError);
    connect(m_acquisitionManager.get(), &DataAcquisitionManager::statsUpdated,
        this, &FX3TestTool::updateStats);

    // 添加命令目录选择按钮的信号连接
    connect(ui.cmdDirButton, &QPushButton::clicked, this, &FX3TestTool::onSelectCommandDirectory);
}

void FX3TestTool::handleDeviceArrival()
{
    static QElapsedTimer lastArrivalTime;
    const int MIN_EVENT_INTERVAL = 500; // 最小事件间隔(ms)

    // 防止事件重复触发
    if (lastArrivalTime.isValid() &&
        lastArrivalTime.elapsed() < MIN_EVENT_INTERVAL) {
        LOG_DEBUG("Ignoring duplicate device arrival event");
        return;
    }
    lastArrivalTime.restart();

    LOG_WARN("USB device arrival detected");

    if (m_deviceInitializing) {
        LOG_DEBUG("Device initialization already in progress, ignoring arrival event");
        return;
    }

    m_deviceInitializing = true;

    // 使用定时器延迟初始化，给设备足够的枚举时间
    QTimer::singleShot(1000, this, [this]() {
        LOG_DEBUG("Starting device initialization...");

        if (m_usbDevice) {
            m_usbDevice->close();
        }

        m_usbDevice = std::make_unique<USBDevice>((HWND)this->winId());
        initConnections();

        if (!checkAndOpenDevice()) {
            LOG_ERROR("Device initialization failed");
        }

        m_deviceInitializing = false;
        });
}

void FX3TestTool::handleDeviceRemoval()
{
    static QElapsedTimer lastRemovalTime;
    const int MIN_EVENT_INTERVAL = 500; // 最小事件间隔(ms)

    // 防止事件重复触发
    if (lastRemovalTime.isValid() &&
        lastRemovalTime.elapsed() < MIN_EVENT_INTERVAL) {
        LOG_DEBUG("Ignoring duplicate device removal event");
        return;
    }
    lastRemovalTime.restart();

    LOG_WARN("USB device removal detected");

    // 确保设备正确关闭
    if (m_usbDevice) {
        m_usbDevice->close();
    }

    // 更新UI状态
    updateUIState(false);
}

void FX3TestTool::updateUIState(bool deviceReady)
{
    m_deviceReady = deviceReady;
    updateButtonStates();
}

void FX3TestTool::updateStatusBar(const QString& usbStatus)
{
    if (!usbStatus.isEmpty()) {
        ui.usbStatusLabel->setText(QString::fromLocal8Bit("USB状态: %1").arg(usbStatus));
        updateTransferStatus(0, 0.0);
    }
}

// In FX3TestTool.cpp, fix the updateTransferStatus method
void FX3TestTool::updateTransferStatus(uint64_t transferred, double speed) {
    QMetaObject::invokeMethod(this, [this, transferred, speed]() {
        // 更新传输状态
        QString status = m_usbDevice && m_usbDevice->isTransferring() ?
            QString::fromLocal8Bit("传输中") :
            QString::fromLocal8Bit("空闲");

        ui.transferStatusLabel->setText(
            QString::fromLocal8Bit("传输状态: %1").arg(status));

        // 更新速度显示
        QString speedText = QString::fromLocal8Bit("速度: 0 MB/s");
        if (speed > 0) {
            if (speed >= 1024) {
                speedText = QString::fromLocal8Bit("速度: %1 GB/s")
                    .arg(speed / 1024.0, 0, 'f', 2);
            }
            else {
                speedText = QString::fromLocal8Bit("速度: %1 MB/s")
                    .arg(speed, 0, 'f', 2);
            }
        }
        ui.speedLabel->setText(speedText);
        LOG_DEBUG(speedText);

        // 更新总传输量显示
        QString sizeText;
        double displaySize;

        if (transferred >= 1024ULL * 1024ULL * 1024ULL) {
            displaySize = transferred / (1024.0 * 1024.0 * 1024.0);
            sizeText = QString::fromLocal8Bit("总计: %1 GB")
                .arg(displaySize, 0, 'f', 2);
        }
        else if (transferred >= 1024ULL * 1024ULL) {
            displaySize = transferred / (1024.0 * 1024.0);
            sizeText = QString::fromLocal8Bit("总计: %1 MB")
                .arg(displaySize, 0, 'f', 2);
        }
        else if (transferred >= 1024ULL) {
            displaySize = transferred / 1024.0;
            sizeText = QString::fromLocal8Bit("总计: %1 KB")
                .arg(displaySize, 0, 'f', 2);
        }
        else {
            sizeText = QString::fromLocal8Bit("总计: %1 B")
                .arg(transferred);
        }
        ui.totalBytesLabel->setText(sizeText);
        }, Qt::QueuedConnection);
}

void FX3TestTool::initializeDeviceAndManager()
{
    LOG_INFO("Initializing device and manager...");

    HWND hwnd = (HWND)this->winId();
    m_usbDevice = std::make_shared<USBDevice>(hwnd);
    m_acquisitionManager = std::make_shared<DataAcquisitionManager>(m_usbDevice);
}

void FX3TestTool::registerDeviceNotification()
{
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
        LOG_ERROR(QString("Failed to register device notification: %1")
            .arg(GetLastError()));
    }
    else {
        LOG_INFO("Device notification registered successfully");
    }
}

void FX3TestTool::updateButtonStates()
{
    // 统一管理所有按钮状态
    bool startEnabled = m_commandsLoaded && m_deviceReady && !m_stopRequested;
    bool stopEnabled = m_deviceReady && m_usbDevice && m_usbDevice->isTransferring();
    bool resetEnabled = m_deviceReady && !m_stopRequested;

    // 在UI线程中更新按钮状态
    QMetaObject::invokeMethod(this, [this, startEnabled, stopEnabled, resetEnabled]() {
        ui.startButton->setEnabled(startEnabled);
        ui.stopButton->setEnabled(stopEnabled);
        ui.resetButton->setEnabled(resetEnabled);
        }, Qt::QueuedConnection);
}
