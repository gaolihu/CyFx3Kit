#include <windows.h>
#include <dbt.h>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QThread>
#include <QString>
#include <QDebug>
#include <QTimer>

#include "FX3TestTool.h"
#include "Logger.h"

// Cypress FX3 Device GUID
// 注意：这个GUID需要与您的设备实际GUID匹配: CYUSBDRV_GUID

FX3TestTool::FX3TestTool(QWidget* parent)
    : QMainWindow(parent)
{
    // Setup UI from the form
    ui.setupUi(this);

    // 注册设备通知
    DEV_BROADCAST_DEVICEINTERFACE notificationFilter;
    ZeroMemory(&notificationFilter, sizeof(notificationFilter));
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    notificationFilter.dbcc_classguid = CYUSBDRV_GUID;

    LOG_DEBUG("FX3TestTool construct");

    HWND hwnd = (HWND)this->winId();
    HDEVNOTIFY hDevNotify = RegisterDeviceNotification(hwnd,
        &notificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE);

    if (hDevNotify == NULL) {
        Logger::instance().error(QString("Failed to register device notification: %1")
            .arg(GetLastError()));
    }
    else {
        Logger::instance().debug("Device notification registered successfully");
    }

    m_usbDevice = std::make_shared<USBDevice>(hwnd);
    m_acquisitionManager = std::make_unique<DataAcquisitionManager>(m_usbDevice);

    initConnections();

    Logger::instance().debug("Checking initial device state...");
    checkAndOpenDevice();
}

FX3TestTool::~FX3TestTool()
{
}

void FX3TestTool::initLogger()
{
    QString logPath = QApplication::applicationDirPath() + "/fx3_test.log";

    // 同步初始化日志文件和控件
    Logger::instance().setLogFile(logPath);
    Logger::instance().setLogWidget(ui.logTextEdit);

    // 添加启动日志
    LOG_INFO("Application starting...");
    LOG_INFO(QString("Log file path: %1").arg(logPath));
    LOG_INFO("Logger initialization completed");

    // 记录应用程序信息
    LOG_INFO(QString("Application path: %1").arg(QApplication::applicationDirPath()));
    LOG_INFO(QString("Qt version: %1").arg(QT_VERSION_STR));
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
    if (m_usbDevice->stopTransfer()) {
        ui.startButton->setEnabled(true);
        ui.stopButton->setEnabled(false);
        LOG_INFO("USB transfer stoped");
    }
    if (m_acquisitionManager) {
        m_acquisitionManager->stopAcquisition();
        LOG_INFO("Data acquisition stopped");
    }
}

void FX3TestTool::onResetDevice()
{
    if (m_usbDevice->reset()) {
        ui.startButton->setEnabled(true);
        ui.stopButton->setEnabled(false);
    }
}

void FX3TestTool::onUsbStatusChanged(const std::string& status)
{
    QString statusText;

    if (status == "ready") {
        statusText = QString::fromLocal8Bit("就绪");
        ui.startButton->setEnabled(true);
    }
    else if (status == "transferring") {
        statusText = QString::fromLocal8Bit("传输中");
    }
    else if (status == "disconnected") {
        statusText = QString::fromLocal8Bit("已断开");
        ui.startButton->setEnabled(false);
        ui.stopButton->setEnabled(false);
    }
    else if (status == "error") {
        statusText = QString::fromLocal8Bit("错误");
    }

    updateStatusBar(statusText);
}

void FX3TestTool::onTransferProgress(uint64_t transferred, int length, int success, int failed)
{
    // 计算传输速度
    static QElapsedTimer speedTimer;
    static uint64_t lastTransferred = 0;

    if (!speedTimer.isValid()) {
        speedTimer.start();
        lastTransferred = transferred;
    }

    int elapsed = speedTimer.elapsed();
    if (elapsed >= 1000) {  // 每秒更新一次速度
        double speed = (transferred - lastTransferred) / (elapsed / 1000.0) / 1024.0; // KB/s
        updateTransferStatus(transferred, speed);

        speedTimer.restart();
        lastTransferred = transferred;
    }
    else {
        updateTransferStatus(transferred);
    }
}

void FX3TestTool::onDeviceError(const QString& error)
{
    QMessageBox::warning(this,
        QString::fromLocal8Bit("错误"),
        error);
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
    QString speedText = QString::fromLocal8Bit("速度: %1 MB/s").arg(dataRate, 0, 'f', 2);
    QString totalText = QString::fromLocal8Bit("总计: %1 MB").arg(receivedBytes / (1024.0 * 1024.0), 0, 'f', 2);

    ui.speedLabel->setText(speedText);
    ui.totalBytesLabel->setText(totalText);
}

bool FX3TestTool::checkAndOpenDevice()
{
    LOG_DEBUG("Checking device connection status...");

    if (!m_usbDevice->isConnected()) {
        LOG_WARN("No device connected");
        updateUIState(false);
        return false;
    }

    LOG_INFO(QString("Device info: %1").arg(m_usbDevice->getDeviceInfo()));

    if (!m_usbDevice->open()) {
        LOG_ERROR("Failed to open device");
        updateUIState(false);
        return false;
    }

    LOG_INFO("Device opened successfully");
    updateUIState(true);
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

    LOG_INFO("USB device arrival detected");

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

    LOG_INFO("USB device removal detected");

    // 确保设备正确关闭
    if (m_usbDevice) {
        m_usbDevice->close();
        LOG_DEBUG("Device closed");
    }

    // 更新UI状态
    updateUIState(false);
}

void FX3TestTool::updateUIState(bool deviceReady)
{
    ui.startButton->setEnabled(deviceReady);
    ui.stopButton->setEnabled(false);
    ui.resetButton->setEnabled(deviceReady);
}

void FX3TestTool::updateStatusBar(const QString& usbStatus)
{
    if (!usbStatus.isEmpty()) {
        ui.usbStatusLabel->setText(QString::fromLocal8Bit("USB状态: %1").arg(usbStatus));
        updateTransferStatus(0, 0.0);
    }
}

void FX3TestTool::updateTransferStatus(uint64_t transferred, double speed)
{
    // 更新传输状态
    QString transferStatus = m_usbDevice && m_usbDevice->isTransferring() ?
        QString::fromLocal8Bit("传输中") :
        QString::fromLocal8Bit("空闲");
    ui.transferStatusLabel->setText(QString::fromLocal8Bit("传输状态: %1").arg(transferStatus));

    // 更新传输速度
    if (speed >= 0) {
        QString speedText;
        if (speed >= 1024) {
            speedText = QString::fromLocal8Bit("速度: %1 KB/s").arg(speed / 1024.0, 0, 'f', 2);
        }
        else {
            speedText = QString::fromLocal8Bit("速度: %1 KB/s").arg(speed, 0, 'f', 2);
        }
        ui.speedLabel->setText(speedText);
    }

    // 更新总传输字节数
    QString sizeText;
    if (transferred >= 1024 * 1024) {
        sizeText = QString::fromLocal8Bit("总计: %1 MB").arg(transferred / (1024.0 * 1024.0), 0, 'f', 2);
    }
    else if (transferred >= 1024) {
        sizeText = QString::fromLocal8Bit("总计: %1 KB").arg(transferred / 1024.0, 0, 'f', 2);
    }
    else {
        sizeText = QString::fromLocal8Bit("总计: %1 B").arg(transferred);
    }
    ui.totalBytesLabel->setText(sizeText);
}
