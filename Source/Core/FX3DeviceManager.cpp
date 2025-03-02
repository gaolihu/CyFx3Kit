#include "FX3DeviceManager.h"
#include "Logger.h"
#include <QThread>
#include <QCoreApplication>

FX3DeviceManager::FX3DeviceManager(QObject* parent)
    : QObject(parent)
    , m_debounceTimer(this)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3DeviceManager构造函数"));

    // 初始化防抖定时器
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(DEBOUNCE_DELAY);
    m_lastDeviceEventTime.start();

    // 初始化成员变量
    m_isTransferring.store(false);
    m_running.store(false);
    m_errorOccurred.store(false);
    m_stoppingInProgress.store(false);
    m_commandsLoaded.store(false);
}

FX3DeviceManager::~FX3DeviceManager() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3DeviceManager析构函数入口"));

    // 使用互斥量防止析构过程中的竞态条件
    std::unique_lock<std::mutex> shutdownLock(m_shutdownMutex);

    try {
        // 设置关闭标志，防止在关闭过程中处理新事件
        m_shuttingDown = true;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设置关闭标志"));

        // 确保停止所有定时器
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止定时器"));
        m_debounceTimer.stop();

        // 断开所有信号连接
        LOG_INFO(LocalQTCompat::fromLocal8Bit("断开所有信号连接"));
        disconnect(this, nullptr, nullptr, nullptr);

        // 停止所有数据传输
        stopAllTransfers();

        // 按照依赖顺序释放资源
        releaseResources();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3DeviceManager析构函数退出 - 成功"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("析构函数异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("析构函数未知异常"));
    }
}

bool FX3DeviceManager::initializeDeviceAndManager(HWND windowHandle) {
    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化USB设备和管理器，数据采集管理器"));

        // 首先创建 USB 设备
        m_usbDevice = std::make_shared<USBDevice>(windowHandle);
        if (!m_usbDevice) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建USB设备失败"));
            AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("创建USB设备失败"));
            return false;
        }

        // 然后创建采集管理器
        try {
            m_acquisitionManager = DataAcquisitionManager::create(m_usbDevice);
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建采集管理器异常: %1").arg(e.what()));
            m_usbDevice.reset();  // 清理已创建的资源
            AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("创建采集管理器失败: %1").arg(e.what()));
            return false;
        }

        // 初始化所有信号连接
        initConnections();

        // 检查设备状态
        if (checkAndOpenDevice()) {
            return true;
        }
        else {
            // 设备未连接或打开失败，但初始化成功
            AppStateMachine::instance().processEvent(StateEvent::APP_INIT, LocalQTCompat::fromLocal8Bit("初始化完成但设备未连接"));
            return true;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备初始化异常: %1").arg(e.what()));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("设备初始化异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备初始化未知异常"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("设备初始化未知异常"));
        return false;
    }
}

void FX3DeviceManager::initConnections() {
    // USB设备信号连接
    if (m_usbDevice) {
        connect(m_usbDevice.get(), &USBDevice::statusChanged,
            this, &FX3DeviceManager::onUsbStatusChanged, Qt::QueuedConnection);
        connect(m_usbDevice.get(), &USBDevice::transferProgress,
            this, &FX3DeviceManager::onTransferProgress, Qt::QueuedConnection);
        connect(m_usbDevice.get(), &USBDevice::deviceError,
            this, &FX3DeviceManager::onDeviceError, Qt::QueuedConnection);
    }

    // 采集管理器信号连接
    if (m_acquisitionManager) {
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::dataReceived,
            this, &FX3DeviceManager::onDataReceived, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::errorOccurred,
            this, &FX3DeviceManager::onAcquisitionError, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::statsUpdated,
            this, &FX3DeviceManager::onStatsUpdated, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::acquisitionStateChanged,
            this, &FX3DeviceManager::onAcquisitionStateChanged, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::acquisitionStarted,
            this, &FX3DeviceManager::onAcquisitionStarted, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::acquisitionStopped,
            this, &FX3DeviceManager::onAcquisitionStopped, Qt::QueuedConnection);
    }
}

bool FX3DeviceManager::checkAndOpenDevice() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("检查设备连接状态..."));

    if (!m_usbDevice) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("USB设备对象未初始化"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("USB设备对象未初始化"));
        return false;
    }

    // 检查设备连接状态
    if (!m_usbDevice->isConnected()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("未检测到设备连接"));
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, LocalQTCompat::fromLocal8Bit("未检测到设备连接"));
        return false;
    }

    // 获取并记录设备信息
    QString deviceInfo = m_usbDevice->getDeviceInfo();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("发现设备: %1").arg(deviceInfo));

    // 尝试打开设备
    if (!m_usbDevice->open()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("打开设备失败"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("打开设备失败"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备检查和打开成功"));

    // 发送设备连接事件
    AppStateMachine::instance().processEvent(StateEvent::DEVICE_CONNECTED, LocalQTCompat::fromLocal8Bit("设备已成功连接和打开"));

    // 通知UI更新USB速度信息
    emit usbSpeedUpdated(getUsbSpeedDescription(), isUSB3());

    return true;
}

bool FX3DeviceManager::resetDevice() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置设备"));

    if (!m_usbDevice) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("USB设备对象未初始化"));
        return false;
    }

    // 发送设备重置开始事件
    AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, LocalQTCompat::fromLocal8Bit("正在重置设备"));

    if (m_usbDevice->reset()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设备重置成功"));

        // 发送设备连接事件，因为重置相当于重新连接
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_CONNECTED, LocalQTCompat::fromLocal8Bit("设备重置成功"));

        // 通知UI更新USB速度信息
        emit usbSpeedUpdated(getUsbSpeedDescription(), isUSB3());

        return true;
    }
    else {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备重置失败"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("设备重置失败"));
        return false;
    }
}

bool FX3DeviceManager::loadCommandFiles(const QString& directoryPath) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("从目录加载命令文件: %1").arg(directoryPath));

    // 设置命令目录
    if (!CommandManager::instance().setCommandDirectory(directoryPath)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设置命令目录失败"));
        m_commandsLoaded = false;
        return false;
    }

    // 验证所有必需的命令文件
    if (!CommandManager::instance().validateCommands()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("命令验证失败"));
        m_commandsLoaded = false;
        return false;
    }

    m_commandsLoaded = true;

    // 发送命令加载事件
    LOG_INFO(LocalQTCompat::fromLocal8Bit("命令文件加载成功，发送COMMANDS_LOADED事件"));

    // 确保在主线程中执行状态更新
    QMetaObject::invokeMethod(QCoreApplication::instance(), [this]() {
        AppStateMachine::instance().processEvent(StateEvent::COMMANDS_LOADED,
            LocalQTCompat::fromLocal8Bit("命令文件加载成功"));
        }, Qt::QueuedConnection);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("命令文件加载完成"));
    return true;
}

bool FX3DeviceManager::startTransfer(uint16_t width, uint16_t height, uint8_t capType) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("启动数据传输"));

    if (m_shuttingDown) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略启动请求"));
        return false;
    }

    if (!m_usbDevice || !m_acquisitionManager) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("设备或采集管理器未初始化"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("设备或采集管理器未初始化"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("启动采集的参数 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    // 发送开始传输请求事件
    AppStateMachine::instance().processEvent(StateEvent::START_REQUESTED, LocalQTCompat::fromLocal8Bit("请求开始传输"));

    // 设置USB设备的图像参数
    m_usbDevice->setImageParams(width, height, capType);

    // 记录传输开始时间
    m_transferStartTime = std::chrono::steady_clock::now();
    m_lastTransferred = 0;

    // 先启动采集管理器
    if (!m_acquisitionManager->startAcquisition(width, height, capType)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动采集管理器失败"));
        AppStateMachine::instance().processEvent(StateEvent::START_FAILED, LocalQTCompat::fromLocal8Bit("启动采集管理器失败"));
        return false;
    }

    // 再启动USB传输
    if (!m_usbDevice->startTransfer()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动USB传输失败"));
        m_acquisitionManager->stopAcquisition();
        AppStateMachine::instance().processEvent(StateEvent::START_FAILED, LocalQTCompat::fromLocal8Bit("启动USB传输失败"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据采集启动成功"));
    return true;
}

bool FX3DeviceManager::stopTransfer() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止数据传输"));

    // 如果应用程序正在关闭，简化停止流程
    if (m_shuttingDown) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，执行简化停止"));
        try {
            if (m_acquisitionManager && m_acquisitionManager->isRunning()) {
                m_acquisitionManager->stopAcquisition();
            }
            if (m_usbDevice && m_usbDevice->isTransferring()) {
                m_usbDevice->stopTransfer();
            }
        }
        catch (...) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("关闭过程中停止传输异常"));
        }
        return true;
    }

    // 避免多次调用停止
    if (m_stoppingInProgress) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("停止操作已在进行中"));
        return true;
    }

    m_stoppingInProgress = true;
    AppStateMachine::instance().processEvent(StateEvent::STOP_REQUESTED, LocalQTCompat::fromLocal8Bit("请求停止传输"));

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
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止请求已发送"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止传输异常: %1").arg(e.what()));
        m_stoppingInProgress = false;
        AppStateMachine::instance().processEvent(StateEvent::STOP_FAILED, LocalQTCompat::fromLocal8Bit("停止传输异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止传输未知异常"));
        m_stoppingInProgress = false;
        AppStateMachine::instance().processEvent(StateEvent::STOP_FAILED, LocalQTCompat::fromLocal8Bit("停止传输未知异常"));
        return false;
    }
}

void FX3DeviceManager::stopAllTransfers() {
    // 如果采集处于活动状态，强制停止
    if (isTransferring()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("强制停止传输"));

        try {
            stopTransfer();

            // 等待一小段时间让停止操作完成
            QElapsedTimer timer;
            timer.start();
            const int MAX_WAIT_MS = 200;

            while (isTransferring() && timer.elapsed() < MAX_WAIT_MS) {
                QThread::msleep(10);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止传输异常: %1").arg(e.what()));
        }
        catch (...) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止传输未知异常"));
        }
    }
}

void FX3DeviceManager::releaseResources() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放资源 - 开始"));

    // 1. 首先释放采集管理器
    if (m_acquisitionManager) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("重置采集管理器"));
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
        LOG_INFO(LocalQTCompat::fromLocal8Bit("重置USB设备"));
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

    LOG_INFO(LocalQTCompat::fromLocal8Bit("释放资源 - 完成"));
}

void FX3DeviceManager::onDeviceArrival() {
    debounceDeviceEvent([this]() {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("检测到USB设备接入"));

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

void FX3DeviceManager::onDeviceRemoval() {
    debounceDeviceEvent([this]() {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("检测到USB设备移除"));

        if (m_shuttingDown) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("应用正在关闭，忽略设备移除事件"));
            return;
        }

        // 确保设备正确关闭
        if (m_usbDevice) {
            m_usbDevice->close();
        }

        // 发送设备断开事件
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, LocalQTCompat::fromLocal8Bit("设备已断开连接"));
        });
}

void FX3DeviceManager::debounceDeviceEvent(std::function<void()> action) {
    static QElapsedTimer lastEventTime;
    const int MIN_EVENT_INTERVAL = 500; // 最小事件间隔(ms)

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

void FX3DeviceManager::onUsbStatusChanged(const std::string& status) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3 USB设备状态变更: ") + QString::fromStdString(status));

    if (m_shuttingDown) {
        return;
    }

    if (status == "ready") {
        if (m_commandsLoaded) {
            AppStateMachine::instance().processEvent(StateEvent::COMMANDS_LOADED, LocalQTCompat::fromLocal8Bit("设备就绪且命令已加载"));
        }
        else {
            AppStateMachine::instance().processEvent(StateEvent::DEVICE_CONNECTED, LocalQTCompat::fromLocal8Bit("设备就绪但命令未加载"));
        }
    }
    else if (status == "transferring") {
        AppStateMachine::instance().processEvent(StateEvent::START_SUCCEEDED, LocalQTCompat::fromLocal8Bit("USB状态变为传输中"));
    }
    else if (status == "disconnected") {
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, LocalQTCompat::fromLocal8Bit("USB状态变为已断开"));
    }
    else if (status == "error") {
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("USB设备错误"));
    }
}

void FX3DeviceManager::onTransferProgress(uint64_t transferred, int length, int success, int failed) {
    if (m_shuttingDown) {
        return;
    }

    static QElapsedTimer speedTimer;
    static uint64_t lastTransferred = 0;

    // 初始化计时器
    if (!speedTimer.isValid()) {
        speedTimer.start();
        lastTransferred = transferred;
        return;
    }

    // 计算速度
    qint64 elapsed = speedTimer.elapsed();
    if (elapsed > 0) {
        double intervalBytes = static_cast<double>(transferred - lastTransferred);
        // 转换为MB/s
        double speed = (intervalBytes * 1000.0) / (elapsed * 1024.0 * 1024.0);

        // 更新统计数据
        emit transferStatsUpdated(transferred, speed, 0);

        // 更新基准值
        speedTimer.restart();
        lastTransferred = transferred;
    }
}

void FX3DeviceManager::onDeviceError(const QString& error) {
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("FX3 USB设备错误: ") + error);

    if (m_shuttingDown) {
        return;
    }

    // 发送设备错误事件
    AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("USB设备错误: ") + error);

    // 通知UI显示错误
    emit deviceError(LocalQTCompat::fromLocal8Bit("设备错误"), error);
}

void FX3DeviceManager::onAcquisitionStarted() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("采集已开始"));

    if (m_shuttingDown) {
        return;
    }

    // 发送传输开始成功事件
    AppStateMachine::instance().processEvent(StateEvent::START_SUCCEEDED, LocalQTCompat::fromLocal8Bit("采集已成功开始"));
}

void FX3DeviceManager::onAcquisitionStopped() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("采集已停止"));

    if (m_shuttingDown) {
        return;
    }

    // 重置停止标志
    m_stoppingInProgress = false;

    // 发送停止成功事件
    AppStateMachine::instance().processEvent(StateEvent::STOP_SUCCEEDED, LocalQTCompat::fromLocal8Bit("采集已成功停止"));
}

void FX3DeviceManager::onDataReceived(const DataPacket& packet) {
    if (m_shuttingDown) {
        return;
    }

    // 转发数据包给UI或其他处理组件
    emit dataProcessed(packet);
}

void FX3DeviceManager::onAcquisitionError(const QString& error) {
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("采集错误: ") + error);

    if (m_shuttingDown) {
        return;
    }

    // 重置停止标志
    m_stoppingInProgress = false;

    // 发送错误事件
    AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("采集错误: ") + error);

    // 通知UI显示错误
    emit deviceError(LocalQTCompat::fromLocal8Bit("采集错误"), error);
}

void FX3DeviceManager::onStatsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeSeconds) {
    if (m_shuttingDown) {
        return;
    }

    // 转发统计数据给UI
    emit transferStatsUpdated(receivedBytes, dataRate, elapsedTimeSeconds);
}

void FX3DeviceManager::onAcquisitionStateChanged(const QString& state) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("采集状态变更为: ") + state);

    if (m_shuttingDown) {
        return;
    }

    // 根据采集状态处理相应事件
    if (state == LocalQTCompat::fromLocal8Bit("空闲") ||
        state == LocalQTCompat::fromLocal8Bit("已停止")) {
        if (m_stoppingInProgress) {
            // 如果停止操作正在进行中，这表示停止操作已完成
            m_stoppingInProgress = false;
            AppStateMachine::instance().processEvent(StateEvent::STOP_SUCCEEDED, LocalQTCompat::fromLocal8Bit("采集状态变为空闲/已停止"));
        }
    }
    else if (state == LocalQTCompat::fromLocal8Bit("采集中")) {
        AppStateMachine::instance().processEvent(StateEvent::START_SUCCEEDED, LocalQTCompat::fromLocal8Bit("采集状态变为采集中"));
    }
    else if (state == LocalQTCompat::fromLocal8Bit("错误")) {
        m_stoppingInProgress = false;
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, LocalQTCompat::fromLocal8Bit("采集状态变为错误"));
    }
}

bool FX3DeviceManager::isDeviceConnected() const {
    return m_usbDevice && m_usbDevice->isConnected();
}

bool FX3DeviceManager::isTransferring() const {
    return m_usbDevice && m_usbDevice->isTransferring();
}

QString FX3DeviceManager::getDeviceInfo() const {
    if (!m_usbDevice) {
        return LocalQTCompat::fromLocal8Bit("无设备信息");
    }
    return m_usbDevice->getDeviceInfo();
}

QString FX3DeviceManager::getUsbSpeedDescription() const {
    if (!m_usbDevice) {
        return LocalQTCompat::fromLocal8Bit("未连接");
    }
    return m_usbDevice->getUsbSpeedDescription();
}

bool FX3DeviceManager::isUSB3() const {
    return m_usbDevice && m_usbDevice->isUSB3();
}