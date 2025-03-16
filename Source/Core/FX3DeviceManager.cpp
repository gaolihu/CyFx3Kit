// Source/Core/FX3DeviceManager.cpp

#include "FX3DeviceManager.h"
#include "Logger.h"
#include <QThread>
#include <QCoreApplication>
#include <algorithm>

FX3DeviceManager::FX3DeviceManager(QObject* parent)
    : QObject(parent)
    , m_debounceTimer(this)
    , m_lastTransferred(0)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3DeviceManager构造函数"));

    // 初始化防抖定时器
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(DEBOUNCE_DELAY);
    m_lastDeviceEventTime.start();

    // 初始化传输统计
    m_transferStats.bytesTransferred = 0;
    m_transferStats.transferRate = 0.0;
    m_transferStats.errorCount = 0;
    m_transferStats.startTime = std::chrono::steady_clock::now();
}

FX3DeviceManager::~FX3DeviceManager()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3DeviceManager析构函数入口"));

    try {
        // 确保安全停止和释放资源
        prepareForShutdown();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3DeviceManager析构函数退出 - 成功"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("析构函数异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("析构函数未知异常"));
    }
}

void FX3DeviceManager::prepareForShutdown()
{
    // 设置关闭标志，防止处理新的事件
    {
        std::lock_guard<std::mutex> lock(m_shutdownMutex);
        if (m_shuttingDown) {
            // 避免重复执行关闭流程
            return;
        }
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设置关闭标志"));
        m_shuttingDown = true;
    }

    // 停止所有定时器
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止定时器"));
    m_debounceTimer.stop();

    // 断开所有信号连接
    LOG_INFO(LocalQTCompat::fromLocal8Bit("断开所有信号连接"));
    disconnect(this, nullptr, nullptr, nullptr);

    // 停止所有数据传输
    stopAllTransfers();

    // 释放资源
    releaseResources();
}

bool FX3DeviceManager::initializeDeviceAndManager(HWND windowHandle)
{
    try {
        std::lock_guard<std::mutex> lock(m_shutdownMutex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化USB设备和管理器"));

        if (m_deviceInitialized) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("设备管理器已初始化"));
            return true;
        }

        // 创建USB设备
        m_usbDevice = std::make_shared<USBDevice>(windowHandle);
        if (!m_usbDevice) {
            handleCriticalError(
                LocalQTCompat::fromLocal8Bit("初始化错误"),
                LocalQTCompat::fromLocal8Bit("创建USB设备失败"),
                StateEvent::ERROR_OCCURRED,
                LocalQTCompat::fromLocal8Bit("创建USB设备失败")
            );
            return false;
        }

        // 创建采集管理器
        try {
            m_acquisitionManager = DataAcquisitionManager::create(m_usbDevice);
        }
        catch (const std::exception& e) {
            handleCriticalError(
                LocalQTCompat::fromLocal8Bit("初始化错误"),
                LocalQTCompat::fromLocal8Bit("创建采集管理器失败: ") + e.what(),
                StateEvent::ERROR_OCCURRED,
                LocalQTCompat::fromLocal8Bit("创建采集管理器失败: ") + e.what()
            );
            m_usbDevice.reset();
            return false;
        }

        // 初始化信号连接
        initConnections();
        m_deviceInitialized = true;

        return true;
    }
    catch (const std::exception& e) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("初始化错误"),
            LocalQTCompat::fromLocal8Bit("设备初始化异常: ") + e.what(),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("设备初始化异常: ") + e.what()
        );
        return false;
    }
    catch (...) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("初始化错误"),
            LocalQTCompat::fromLocal8Bit("设备初始化未知异常"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("设备初始化未知异常")
        );
        return false;
    }
}

void FX3DeviceManager::initConnections()
{
    // USB设备信号连接
    if (m_usbDevice) {
        // 使用新的处理方法名称
        connect(m_usbDevice.get(), &USBDevice::signal_USB_statusChanged,
            this, &FX3DeviceManager::slot_FX3_DevM_handleUsbStatusChanged);
        connect(m_usbDevice.get(), &USBDevice::signal_USB_transferProgress,
            this, &FX3DeviceManager::slot_FX3_DevM_handleTransferProgress, Qt::QueuedConnection);
        connect(m_usbDevice.get(), &USBDevice::signal_USB_deviceError,
            this, &FX3DeviceManager::slot_FX3_DevM_handleDeviceError, Qt::QueuedConnection);
    }

    // 采集管理器信号连接
    if (m_acquisitionManager) {
        // 使用新的处理方法名称
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::signal_AQ_batchDataReceived,
            this, &FX3DeviceManager::slot_FX3_DevM_handleDataReceived, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::signal_AQ_errorOccurred,
            this, &FX3DeviceManager::slot_FX3_DevM_handleAcquisitionError, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::signal_AQ_statsUpdated,
            this, &FX3DeviceManager::slot_FX3_DevM_handleStatsUpdated, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::signal_AQ_acquisitionStateChanged,
            this, &FX3DeviceManager::slot_FX3_DevM_handleAcquisitionStateChanged, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::signal_AQ_acquisitionStarted,
            this, &FX3DeviceManager::slot_FX3_DevM_handleAcquisitionStarted, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::signal_AQ_acquisitionStopped,
            this, &FX3DeviceManager::slot_FX3_DevM_handleAcquisitionStopped, Qt::QueuedConnection);
    }
}

bool FX3DeviceManager::checkAndOpenDevice()
{
    if (m_shuttingDown) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略设备检查"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("检查设备连接状态..."));

    if (!m_usbDevice) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("设备错误"),
            LocalQTCompat::fromLocal8Bit("USB设备对象未初始化"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("USB设备对象未初始化")
        );
        return false;
    }

    // 检查设备连接状态
    if (!m_usbDevice->isConnected()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("未检测到设备连接"));
        AppStateMachine::instance().processEvent(
            StateEvent::DEVICE_DISCONNECTED,
            LocalQTCompat::fromLocal8Bit("未检测到设备连接")
        );
        emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), false, false);
        return false;
    }

    // 获取并记录设备信息
    QString deviceInfo = m_usbDevice->getDeviceInfo();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("发现设备: %1").arg(deviceInfo));

    // 尝试打开设备
    if (!m_usbDevice->open()) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("设备错误"),
            LocalQTCompat::fromLocal8Bit("打开设备失败"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("打开设备失败")
        );
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备: {%1}检查打开OK").arg(deviceInfo));

    // 发送设备连接事件
    AppStateMachine::instance().processEvent(
        StateEvent::DEVICE_CONNECTED,
        LocalQTCompat::fromLocal8Bit("设备已成功连接和打开")
    );

    // 通知UI更新
    LOG_INFO("发送设备连接信息");
    emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), isUSB3(), true);

    return true;
}

bool FX3DeviceManager::resetDevice()
{
    if (m_shuttingDown) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略重置请求"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置设备"));

    if (!m_usbDevice) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("设备错误"),
            LocalQTCompat::fromLocal8Bit("USB设备对象未初始化"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("USB设备对象未初始化")
        );
        return false;
    }

    // 如果正在传输，先停止
    if (m_isTransferring) {
        stopTransfer();

        // 等待传输完全停止
        QElapsedTimer timer;
        timer.start();
        while (m_isTransferring && timer.elapsed() < 1000) {
            QThread::msleep(10);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        if (m_isTransferring) {
            handleCriticalError(
                LocalQTCompat::fromLocal8Bit("设备错误"),
                LocalQTCompat::fromLocal8Bit("无法停止当前传输，重置失败"),
                StateEvent::ERROR_OCCURRED,
                LocalQTCompat::fromLocal8Bit("无法停止当前传输，重置失败")
            );
            return false;
        }
    }

    // 发送设备重置开始事件
    AppStateMachine::instance().processEvent(
        StateEvent::DEVICE_DISCONNECTED,
        LocalQTCompat::fromLocal8Bit("正在重置设备")
    );

    if (m_usbDevice->reset()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设备重置成功"));

        // 发送设备连接事件，因为重置相当于重新连接
        AppStateMachine::instance().processEvent(
            StateEvent::DEVICE_CONNECTED,
            LocalQTCompat::fromLocal8Bit("设备重置成功")
        );
        // 通知UI更新USB速度信息
        emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), isUSB3(), true);
        return true;
    }
    else {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("设备错误"),
            LocalQTCompat::fromLocal8Bit("设备重置失败"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("设备重置失败")
        );
        emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), false, false);
        return false;
    }
}

bool FX3DeviceManager::loadCommandFiles(const QString& directoryPath)
{
    if (m_shuttingDown) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略命令加载请求"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("从目录加载命令文件: %1").arg(directoryPath));

    // 设置命令目录
    if (!CommandManager::instance().setCommandDirectory(directoryPath)) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("命令错误"),
            LocalQTCompat::fromLocal8Bit("设置命令目录失败: ") + directoryPath,
            StateEvent::COMMANDS_UNLOADED,
            LocalQTCompat::fromLocal8Bit("设置命令目录失败")
        );
        m_commandsLoaded = false;
        return false;
    }

    // 验证所有必需的命令文件
    if (!CommandManager::instance().validateCommands()) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("命令错误"),
            LocalQTCompat::fromLocal8Bit("命令验证失败，请确保所有必需文件存在"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("命令验证失败")
        );
        m_commandsLoaded = false;
        return false;
    }

    m_commandsLoaded = true;

    // 发送命令加载事件
    LOG_INFO(LocalQTCompat::fromLocal8Bit("命令文件加载成功"));

    // 确保在主线程中执行状态更新
    QMetaObject::invokeMethod(QCoreApplication::instance(), [this]() {
        AppStateMachine::instance().processEvent(
            StateEvent::COMMANDS_LOADED,
            LocalQTCompat::fromLocal8Bit("命令文件加载成功")
        );
        }, Qt::QueuedConnection);

    return true;
}

bool FX3DeviceManager::startTransfer(uint16_t width, uint16_t height, uint8_t captureType)
{
    if (m_shuttingDown) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略启动请求"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("启动数据传输"));

    if (!m_usbDevice || !m_acquisitionManager) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("设备错误"),
            LocalQTCompat::fromLocal8Bit("设备或采集管理器未初始化"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("设备或采集管理器未初始化")
        );
        return false;
    }

    // 检查设备连接状态
    if (!isDeviceConnected()) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("设备错误"),
            LocalQTCompat::fromLocal8Bit("设备未连接，无法启动传输"),
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("设备未连接，无法启动传输")
        );
        return false;
    }

    // 如果已经在传输，先停止
    if (m_isTransferring) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("已有传输正在进行，先停止"));
        stopTransfer();

        // 等待传输完全停止
        QElapsedTimer timer;
        timer.start();
        while (m_isTransferring && timer.elapsed() < 1000) {
            QThread::msleep(10);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        if (m_isTransferring) {
            handleCriticalError(
                LocalQTCompat::fromLocal8Bit("传输错误"),
                LocalQTCompat::fromLocal8Bit("无法停止当前传输"),
                StateEvent::ERROR_OCCURRED,
                LocalQTCompat::fromLocal8Bit("无法停止当前传输")
            );
            return false;
        }
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("启动采集的参数 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(captureType, 2, 16, QChar('0')));

    // 发送开始传输请求事件
    AppStateMachine::instance().processEvent(
        StateEvent::START_REQUESTED,
        LocalQTCompat::fromLocal8Bit("请求开始传输")
    );

    // 重置传输统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_transferStats.bytesTransferred = 0;
        m_transferStats.transferRate = 0.0;
        m_transferStats.errorCount = 0;
        m_transferStats.startTime = std::chrono::steady_clock::now();
    }
    m_lastTransferred = 0;
    m_transferStartTime = std::chrono::steady_clock::now();

    // 设置USB设备的图像参数
    m_usbDevice->setImageParams(width, height, captureType);

    // 先启动采集管理器
    if (!m_acquisitionManager->startAcquisition(width, height, captureType)) {
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("传输错误"),
            LocalQTCompat::fromLocal8Bit("启动采集管理器失败"),
            StateEvent::START_FAILED,
            LocalQTCompat::fromLocal8Bit("启动采集管理器失败")
        );
        return false;
    }

    // 再启动USB传输
    if (!m_usbDevice->startTransfer()) {
        // 启动失败，停止采集管理器
        m_acquisitionManager->stopAcquisition();
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("传输错误"),
            LocalQTCompat::fromLocal8Bit("启动USB传输失败"),
            StateEvent::START_FAILED,
            LocalQTCompat::fromLocal8Bit("启动USB传输失败")
        );
        return false;
    }

    // 标记为传输中
    m_isTransferring = true;
    emit signal_FX3_DevM_transferStateChanged(true);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据采集启动成功"));
    return true;
}

bool FX3DeviceManager::stopTransfer()
{
    if (m_shuttingDown) {
        // 如果应用正在关闭，简化处理流程
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，执行简化停止"));
        stopAllTransfers();
        return true;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("Fx3设备管理器停止数据传输"));

    // 避免多次调用停止
    if (m_stoppingInProgress) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("停止操作已在进行中"));
        return true;
    }

    // 如果没有传输在进行，直接返回
    if (!m_isTransferring) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("没有传输在进行，忽略停止请求"));
        return true;
    }

    m_stoppingInProgress = true;
    AppStateMachine::instance().processEvent(
        StateEvent::STOP_REQUESTED,
        LocalQTCompat::fromLocal8Bit("请求停止传输")
    );

    try {
        // 首先停止USB传输
        if (m_usbDevice && m_usbDevice->isTransferring()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("停止USB设备传输"));
            if (!m_usbDevice->stopTransfer()) {
                LOG_WARN(LocalQTCompat::fromLocal8Bit("停止USB传输返回失败"));
            }
        }

        // 然后停止采集管理器
        if (m_acquisitionManager && m_acquisitionManager->isRunning()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("停止采集管理器"));
            m_acquisitionManager->stopAcquisition();
        }

        // 在这里不调用成功事件，等待来自采集管理器的停止通知
        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3设备管理器停止请求已发送"));
        return true;
    }
    catch (const std::exception& e) {
        m_stoppingInProgress = false;
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("传输错误"),
            LocalQTCompat::fromLocal8Bit("停止传输异常: ") + e.what(),
            StateEvent::STOP_FAILED,
            LocalQTCompat::fromLocal8Bit("停止传输异常: ") + e.what()
        );
        return false;
    }
    catch (...) {
        m_stoppingInProgress = false;
        handleCriticalError(
            LocalQTCompat::fromLocal8Bit("传输错误"),
            LocalQTCompat::fromLocal8Bit("停止传输未知异常"),
            StateEvent::STOP_FAILED,
            LocalQTCompat::fromLocal8Bit("停止传输未知异常")
        );
        return false;
    }
}

void FX3DeviceManager::stopAllTransfers()
{
    std::lock_guard<std::mutex> lock(m_shutdownMutex);

    // 如果采集处于活动状态，强制停止
    if (m_isTransferring || m_stoppingInProgress) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("强制停止所有传输"));

        try {
            // 停止采集管理器
            if (m_acquisitionManager && m_acquisitionManager->isRunning()) {
                LOG_INFO(LocalQTCompat::fromLocal8Bit("停止采集管理器"));
                m_acquisitionManager->stopAcquisition();
            }

            // 停止USB传输
            if (m_usbDevice && m_usbDevice->isTransferring()) {
                LOG_INFO(LocalQTCompat::fromLocal8Bit("停止USB设备传输"));
                m_usbDevice->stopTransfer();
            }

            // 等待一小段时间让停止操作完成
            QElapsedTimer timer;
            timer.start();
            const int MAX_WAIT_MS = 200;

            while ((m_acquisitionManager && m_acquisitionManager->isRunning() ||
                m_usbDevice && m_usbDevice->isTransferring()) &&
                timer.elapsed() < MAX_WAIT_MS) {
                QThread::msleep(10);
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }

            // 强制重置状态
            m_isTransferring = false;
            m_stoppingInProgress = false;
            emit signal_FX3_DevM_transferStateChanged(false);
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止传输异常: %1").arg(e.what()));
        }
        catch (...) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止传输未知异常"));
        }
    }
}

void FX3DeviceManager::releaseResources()
{
    std::lock_guard<std::mutex> lock(m_shutdownMutex);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放设备资源 - 开始"));

    // 先确保传输停止
    stopAllTransfers();

    // 1. 首先释放采集管理器
    if (m_acquisitionManager) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("释放采集管理器"));
        try {
            // 断开所有信号连接
            disconnect(m_acquisitionManager.get(), nullptr, nullptr, nullptr);

            // 在关闭前准备
            m_acquisitionManager->prepareForShutdown();

            // 释放智能指针
            m_acquisitionManager.reset();
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("释放采集管理器异常: %1").arg(e.what()));
        }
    }

    // 短暂延时确保采集管理器已完全释放
    QThread::msleep(20);

    // 2. 然后释放USB设备
    if (m_usbDevice) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("释放USB设备"));
        try {
            // 断开所有信号连接
            disconnect(m_usbDevice.get(), nullptr, nullptr, nullptr);

            // 确保设备已关闭
            if (m_usbDevice->isConnected()) {
                m_usbDevice->close();
            }

            // 释放智能指针
            m_usbDevice.reset();
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("释放USB设备异常: %1").arg(e.what()));
        }
    }

    // 重置标志
    m_deviceInitialized = false;
    m_isTransferring = false;
    m_stoppingInProgress = false;
    m_commandsLoaded = false;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放设备资源 - 完成"));
}

void FX3DeviceManager::debounceDeviceEvent(std::function<void()> action)
{
    if (m_shuttingDown) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略设备事件"));
        return;
    }

    static QElapsedTimer lastEventTime;
    const int MIN_EVENT_INTERVAL = 300; // 最小事件间隔(ms)

    // 防止事件重复触发
    if (lastEventTime.isValid() &&
        lastEventTime.elapsed() < MIN_EVENT_INTERVAL) {
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("忽略重复的设备事件"));
        return;
    }
    lastEventTime.restart();

    // 使用定时器延迟处理，等待设备枚举完成
    QTimer::singleShot(DEBOUNCE_DELAY, this, action);
}

void FX3DeviceManager::handleCriticalError(const QString& title, const QString& message,
    StateEvent eventType, const QString& eventReason)
{
    LOG_ERROR(title + ": " + message);

    m_errorOccurred = true;

    // 发送事件到状态机
    if (eventType != StateEvent::APP_NONE) {
        AppStateMachine::instance().processEvent(
            eventType,
            eventReason.isEmpty() ? message : eventReason
        );
    }

    // 发送错误信号
    emit signal_FX3_DevM_deviceError(title, message);
}

void FX3DeviceManager::updateTransferStats(uint64_t transferred, double speed, uint32_t elapseMs)
{
    // 更新内部统计
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_transferStats.bytesTransferred = transferred;
        m_transferStats.transferRate = speed;
        m_transferStats.elapseMs = elapseMs;
    }

    // 发送更新信号
    emit signal_FX3_DevM_transferStatsUpdated(transferred, speed, elapseMs);
}

DeviceTransferStats FX3DeviceManager::getTransferStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_transferStats;
}

bool FX3DeviceManager::isDeviceConnected() const
{
    return m_usbDevice && m_usbDevice->isConnected();
}

bool FX3DeviceManager::isTransferring() const
{
    return m_isTransferring;
}

QString FX3DeviceManager::getDeviceInfo() const
{
    if (!m_usbDevice) {
        return LocalQTCompat::fromLocal8Bit("无设备信息");
    }
    return m_usbDevice->getDeviceInfo();
}

QString FX3DeviceManager::getUsbSpeedDescription() const
{
    if (!m_usbDevice) {
        LOG_INFO("未连接");
        return LocalQTCompat::fromLocal8Bit("未连接");
    }
    return m_usbDevice->getUsbSpeedDescription();
}

bool FX3DeviceManager::isUSB3() const
{
    return m_usbDevice && m_usbDevice->isUSB3();
}

// 事件处理方法
void FX3DeviceManager::slot_FX3_DevM_onDeviceArrival()
{
    debounceDeviceEvent([this]() {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("检测到USB设备接入"));

        if (m_shuttingDown) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略设备接入事件"));
            return;
        }

        if (!m_usbDevice) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("USB设备对象未初始化"));
            return;
        }

        // 检查并打开设备
        checkAndOpenDevice();
        });
}

void FX3DeviceManager::slot_FX3_DevM_onDeviceRemoval()
{
    debounceDeviceEvent([this]() {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("检测到USB设备移除"));

        if (m_shuttingDown) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略设备移除事件"));
            return;
        }

        // 如果正在传输，标记停止
        if (m_isTransferring) {
            m_isTransferring = false;
            m_stoppingInProgress = false;
            emit signal_FX3_DevM_transferStateChanged(false);
        }

        // 确保设备正确关闭
        if (m_usbDevice) {
            m_usbDevice->close();
        }

        // 发送设备断开事件
        AppStateMachine::instance().processEvent(
            StateEvent::DEVICE_DISCONNECTED,
            LocalQTCompat::fromLocal8Bit("设备已断开连接")
        );
        emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), false, false);
        });
}

// 信号处理方法
void FX3DeviceManager::slot_FX3_DevM_handleUsbStatusChanged(const std::string& status)
{
    if (m_shuttingDown) {
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3 USB设备状态变更: ") + QString::fromStdString(status));

    if (status == "ready") {
        emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), isUSB3(), true);

        if (m_commandsLoaded) {
            AppStateMachine::instance().processEvent(
                StateEvent::COMMANDS_LOADED,
                LocalQTCompat::fromLocal8Bit("设备就绪且命令已加载")
            );
        }
        else {
            AppStateMachine::instance().processEvent(
                StateEvent::DEVICE_CONNECTED,
                LocalQTCompat::fromLocal8Bit("设备就绪但命令未加载")
            );
        }
    }
    else if (status == "transferring") {
        m_isTransferring = true;
        emit signal_FX3_DevM_transferStateChanged(true);

        AppStateMachine::instance().processEvent(
            StateEvent::START_SUCCEEDED,
            LocalQTCompat::fromLocal8Bit("USB状态变为传输中")
        );
    }
    else if (status == "disconnected") {
        m_isTransferring = false;
        emit signal_FX3_DevM_transferStateChanged(false);
        emit signal_FX3_DevM_usbSpeedUpdated(getUsbSpeedDescription(), false, false);

        AppStateMachine::instance().processEvent(
            StateEvent::DEVICE_DISCONNECTED,
            LocalQTCompat::fromLocal8Bit("USB状态变为已断开")
        );
    }
    else if (status == "error") {
        m_errorOccurred = true;

        AppStateMachine::instance().processEvent(
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("USB设备错误")
        );
    }
}

void FX3DeviceManager::slot_FX3_DevM_handleTransferProgress(uint64_t transferred, int length, int success, int failed)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("传输进度, 已传输: %1 bytes, buf大小: %2, 成功: %3, 失败: %4")
        .arg(transferred).arg(length).arg(success).arg(failed));
}

void FX3DeviceManager::slot_FX3_DevM_handleDeviceError(const QString& error)
{
    if (m_shuttingDown) {
        return;
    }

    handleCriticalError(
        LocalQTCompat::fromLocal8Bit("设备错误"),
        error,
        StateEvent::ERROR_OCCURRED,
        LocalQTCompat::fromLocal8Bit("USB设备错误: ") + error
    );
}

void FX3DeviceManager::slot_FX3_DevM_handleAcquisitionStarted()
{
    if (m_shuttingDown) {
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("采集已开始"));

    m_isTransferring = true;
    emit signal_FX3_DevM_transferStateChanged(true);

    // 发送传输开始成功事件
    AppStateMachine::instance().processEvent(
        StateEvent::START_SUCCEEDED,
        LocalQTCompat::fromLocal8Bit("采集已成功开始")
    );
}

void FX3DeviceManager::slot_FX3_DevM_handleAcquisitionStopped()
{
    if (m_shuttingDown) {
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("采集已停止"));

    // 标记为非传输状态
    m_isTransferring = false;

    // 重置停止标志
    m_stoppingInProgress = false;

    // 通知UI
    emit signal_FX3_DevM_transferStateChanged(false);

    // 发送停止成功事件
    AppStateMachine::instance().processEvent(
        StateEvent::STOP_SUCCEEDED,
        LocalQTCompat::fromLocal8Bit("采集已成功停止")
    );
}

void FX3DeviceManager::slot_FX3_DevM_handleDataReceived(const std::vector<DataPacket>& packets)
{
    if (m_shuttingDown) {
        return;
    }

    // 转发数据包
    emit signal_FX3_DevM_dataPacketAvailable(packets);
}

void FX3DeviceManager::slot_FX3_DevM_handleAcquisitionError(const QString& error)
{
    if (m_shuttingDown) {
        return;
    }

    // 重置传输状态
    m_isTransferring = false;
    m_stoppingInProgress = false;
    emit signal_FX3_DevM_transferStateChanged(false);

    handleCriticalError(
        LocalQTCompat::fromLocal8Bit("采集错误"),
        error,
        StateEvent::ERROR_OCCURRED,
        LocalQTCompat::fromLocal8Bit("采集错误: ") + error
    );
}

void FX3DeviceManager::slot_FX3_DevM_handleStatsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapseMs)
{
    if (m_shuttingDown) {
        return;
    }

    // 更新内部统计
    updateTransferStats(receivedBytes, dataRate, elapseMs);
}

void FX3DeviceManager::slot_FX3_DevM_handleAcquisitionStateChanged(const QString& state)
{
    if (m_shuttingDown) {
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("采集状态变更为: ") + state);

    // 根据采集状态处理相应事件
    if (state == LocalQTCompat::fromLocal8Bit("空闲") ||
        state == LocalQTCompat::fromLocal8Bit("已停止")) {

        m_isTransferring = false;
        emit signal_FX3_DevM_transferStateChanged(false);

        if (m_stoppingInProgress) {
            // 如果停止操作正在进行中，这表示停止操作已完成
            m_stoppingInProgress = false;
            AppStateMachine::instance().processEvent(
                StateEvent::STOP_SUCCEEDED,
                LocalQTCompat::fromLocal8Bit("采集状态变为空闲/已停止")
            );
        }
    }
    else if (state == LocalQTCompat::fromLocal8Bit("采集中")) {
        m_isTransferring = true;
        emit signal_FX3_DevM_transferStateChanged(true);

        AppStateMachine::instance().processEvent(
            StateEvent::START_SUCCEEDED,
            LocalQTCompat::fromLocal8Bit("采集状态变为采集中")
        );
    }
    else if (state == LocalQTCompat::fromLocal8Bit("错误")) {
        m_isTransferring = false;
        m_stoppingInProgress = false;
        emit signal_FX3_DevM_transferStateChanged(false);

        AppStateMachine::instance().processEvent(
            StateEvent::ERROR_OCCURRED,
            LocalQTCompat::fromLocal8Bit("采集状态变为错误")
        );
    }
}