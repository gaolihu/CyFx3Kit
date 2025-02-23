#include "DataAcquisition.h"
#include "Logger.h"
#include <QDateTime>
#include <QCoreApplication>

DataAcquisitionManager::DataAcquisitionManager(std::shared_ptr<USBDevice> device)
    : m_device(device)
{
    if (!device) {
        LOG_ERROR("Invalid USB device pointer");
        throw std::invalid_argument("Device pointer cannot be null");
    }

    // 创建循环缓冲区
    m_buffer = std::make_unique<CircularBuffer>(BUFFER_COUNT, BUFFER_SIZE);
    LOG_INFO(QString("Created buffer pool - Count: %1, Size per buffer: %2 bytes")
        .arg(BUFFER_COUNT)
        .arg(BUFFER_SIZE));
}

DataAcquisitionManager::~DataAcquisitionManager()
{
    stopAcquisition();
}

DataAcquisitionManager::CircularBuffer::CircularBuffer(size_t bufferCount, size_t bufferSize)
    : m_warningThreshold(bufferCount * 0.75)
    , m_criticalThreshold(bufferCount * 0.90)
{
    LOG_INFO(QString("Initializing circular buffer - Count: %1, Size per buffer: %2 bytes")
        .arg(bufferCount).arg(bufferSize));

    m_buffers.resize(bufferCount);
    for (auto& buffer : m_buffers) {
        buffer.resize(bufferSize);
    }
    m_currentWriteBuffer = 0;

    LOG_INFO(QString("Circular buffer initialized - Total capacity: %1 bytes")
        .arg(bufferCount * bufferSize));
}

auto DataAcquisitionManager::CircularBuffer::checkBufferStatus() -> WarningLevel
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t queueSize = m_readyBuffers.size();

    if (queueSize >= m_criticalThreshold) {
        return WarningLevel::C_CRITICAL;
    }
    else if (queueSize >= m_warningThreshold) {
        return WarningLevel::C_WARNING;
    }
    return WarningLevel::C_NORMAL;
}

std::pair<uint8_t*, size_t> DataAcquisitionManager::CircularBuffer::getWriteBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffers.empty()) {
        LOG_ERROR("Buffer array is empty");
        return { nullptr, 0 };
    }

    size_t bufferSize = m_buffers[m_currentWriteBuffer].size();
    uint8_t* bufferPtr = m_buffers[m_currentWriteBuffer].data();

#if 0
    LOG_DEBUG(QString("Providing write buffer - Index: %1, Size: %2 bytes, Address: 0x%3")
        .arg(m_currentWriteBuffer)
        .arg(bufferSize)
        .arg(reinterpret_cast<quint64>(bufferPtr), 0, 16));
#endif

    return { bufferPtr, bufferSize };
}

void DataAcquisitionManager::CircularBuffer::commitBuffer(size_t bytesWritten)
{
    std::lock_guard<std::mutex> lock(m_mutex);

#if 0
    LOG_DEBUG(QString("Committing buffer - Index: %1, Written bytes: %2")
        .arg(m_currentWriteBuffer)
        .arg(bytesWritten));
#endif

    if (bytesWritten == 0) {
        LOG_WARN("Attempting to commit empty buffer");
        return;
    }

    if (bytesWritten > m_buffers[m_currentWriteBuffer].size()) {
        LOG_ERROR(QString("Buffer overflow - Written: %1, Capacity: %2")
            .arg(bytesWritten)
            .arg(m_buffers[m_currentWriteBuffer].size()));
        return;
    }

    DataPacket packet;
    packet.data = std::vector<uint8_t>(
        m_buffers[m_currentWriteBuffer].begin(),
        m_buffers[m_currentWriteBuffer].begin() + bytesWritten
        );
    packet.size = bytesWritten;
    packet.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    m_readyBuffers.push(std::move(packet));

    m_currentWriteBuffer = (m_currentWriteBuffer + 1) % m_buffers.size();

#if 0
    LOG_DEBUG(QString("Buffer committed - Size: %1, Queue size: %2")
        .arg(bytesWritten)
        .arg(m_readyBuffers.size()));
#endif
}

std::optional<DataPacket> DataAcquisitionManager::CircularBuffer::getReadBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_readyBuffers.empty()) {
        return std::nullopt;
    }

    DataPacket packet = std::move(m_readyBuffers.front());
    m_readyBuffers.pop();
    return packet;
}

void DataAcquisitionManager::CircularBuffer::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_readyBuffers.empty()) {
        m_readyBuffers.pop();
    }
    m_currentWriteBuffer = 0;
}

bool DataAcquisitionManager::startAcquisition(uint16_t width, uint16_t height, uint8_t capType)
{
    LOG_INFO("Try start acquisition");

    if (m_running) {
        LOG_WARN("Acquisition already running");
        return false;
    }

    // 配置采集参数
    m_params.width = width;
    m_params.height = height;
    m_params.format = capType;

    if (!validateAcquisitionParams()) {
        LOG_ERROR("Invalid acquisition parameters");
        return false;
    }

    // 重置状态和统计信息
    m_transferStats.reset();
    m_buffer->reset();

    // 设置运行标志
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = true;
    }

    try {
        LOG_INFO("Start acquisition / process thread");

        // 启动采集线程
        m_acquisitionThread = std::thread([this]() {
            LOG_INFO("Acquisition thread started with ID: " +
                Logger::instance().getThreadIdAsString(m_acquisitionThread));
            this->acquisitionThread();
            });

        // 启动处理线程
        m_processingThread = std::thread([this]() {
            LOG_INFO("Processing thread started with ID: " +
                Logger::instance().getThreadIdAsString(m_processingThread));
            this->processingThread();
            });

        LOG_INFO("Threads created successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Failed to start threads: %1").arg(e.what()));
        m_running = false;
        return false;
    }

    updateAcquisitionState(AcquisitionState::AC_RUNNING);
    emit acquisitionStarted();
    LOG_INFO("Acquisition started");
    return true;
}

void DataAcquisitionManager::stopAcquisition() {
    std::unique_lock<std::mutex> stopLock(m_stopMutex);

    if (!m_running) {
        return;
    }

    // 保存最终统计数据
    uint64_t finalBytes = m_transferStats.getTotalBytes();
    double finalSpeed = m_transferStats.getCurrentSpeed();

    // 设置停止标志
    m_running = false;
    m_dataReady.notify_all();

    // 等待线程结束
    if (m_acquisitionThread.joinable() &&
        std::this_thread::get_id() != m_acquisitionThread.get_id()) {
        m_acquisitionThread.join();
    }

    if (m_processingThread.joinable() &&
        std::this_thread::get_id() != m_processingThread.get_id()) {
        m_processingThread.join();
    }

    // 发送最终统计数据
    emit statsUpdated(finalBytes, 0.0);  // 速度设为0表示已停止

    // 清理资源
    m_buffer->reset();
    updateAcquisitionState(AcquisitionState::AC_IDLE);
    emit acquisitionStopped();
}

void DataAcquisitionManager::signalStop(StopReason reason)
{
    std::lock_guard<std::mutex> lock(m_stopMutex);

    if (!m_running) {
        return;
    }

    switch (reason) {
    case StopReason::READ_ERROR:
        LOG_ERROR("Stopping acquisition due to read errors");
        m_errorOccurred = true;
        emit errorOccurred(QString::fromLocal8Bit("数据读取错误，采集已停止"));
        break;

    case StopReason::DEVICE_ERROR:
        LOG_ERROR("Stopping acquisition due to device error");
        m_errorOccurred = true;
        emit errorOccurred(QString::fromLocal8Bit("设备错误，采集已停止"));
        break;

    case StopReason::BUFFER_OVERFLOW:
        LOG_ERROR("Stopping acquisition due to buffer overflow");
        m_errorOccurred = true;
        emit errorOccurred(QString::fromLocal8Bit("缓冲区溢出，采集已停止"));
        break;

    case StopReason::USER_REQUEST:
        LOG_INFO("Stopping acquisition by user request");
        break;
    }

    m_running = false;
    m_dataReady.notify_all();

    // 通知UI更新状态
    emit acquisitionStateChanged(QString::fromLocal8Bit("已停止"));
    emit statsUpdated(m_totalBytes, 0.0);  // 速率清零
}

void DataAcquisitionManager::acquisitionThread()
{
    LOG_INFO("Data acquisition thread started");

    // 添加连续读取失败计数
    int consecutiveFailures = 0;

    try {
        while (m_running) {
            if (!m_running) break;  // 快速检查停止标志

            // 检查缓冲区状态
            if (m_buffer->checkBufferStatus() == CircularBuffer::WarningLevel::C_CRITICAL) {
                LOG_ERROR("Buffer overflow detected");
                signalStop(StopReason::BUFFER_OVERFLOW);
                break;
            }

            auto [writeBuffer, size] = m_buffer->getWriteBuffer();
            if (!writeBuffer) {
                std::this_thread::sleep_for(std::chrono::milliseconds(STOP_CHECK_INTERVAL_MS));
                continue;
            }

            LONG actualLength = static_cast<LONG>(size);
            if (static_cast<size_t>(actualLength) > MAX_PACKET_SIZE) {
                actualLength = static_cast<LONG>(MAX_PACKET_SIZE);
            }

            bool readSuccess = m_device->readData(writeBuffer, actualLength);

            if (readSuccess && actualLength > 0) {
                // 读取成功，重置失败计数
                consecutiveFailures = 0;

                m_buffer->commitBuffer(actualLength);
                m_totalBytes += actualLength;
                m_dataReady.notify_one();

                // 降低统计更新频率
                static auto lastUpdate = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastUpdate).count();

                if (duration >= STATS_UPDATE_INTERVAL_MS) {
                    updateStats();
                    lastUpdate = now;
                }
            }
            else {
                // 读取失败处理
                consecutiveFailures++;

                if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                    LOG_ERROR("Too many consecutive read failures, stopping acquisition");
                    signalStop(StopReason::READ_ERROR);
                    break;
                }

                LOG_WARN(QString("Failed to read data (attempt %1/%2)")
                    .arg(consecutiveFailures)
                    .arg(MAX_CONSECUTIVE_FAILURES));

                // 短暂延时后继续尝试
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 让出时间片
            std::this_thread::yield();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in acquisition thread: %1").arg(e.what()));
        signalStop(StopReason::DEVICE_ERROR);
    }

    // 记录最终传输统计
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - m_transferStats.getStartTime()).count();

#if 0
    LOG_INFO(QString("  Total Bytes: %1 bytes (%2 MB)")
        .arg(m_transferStats.getTotalBytes())
        .arg(m_transferStats.getTotalBytes() / (1024.0 * 1024.0), 0, 'f', 2));
#endif
    LOG_INFO(QString("  Duration: %1 seconds").arg(duration));
#if 0
    LOG_INFO(QString("  Average Speed: %1 MB/s")
        .arg(m_transferStats.getCurrentSpeed(), 0, 'f', 2));
    LOG_INFO(QString("  Successful Transfers: %1").arg(m_transferStats.getSuccessCount()));
    LOG_INFO(QString("  Failed Transfers: %1").arg(m_transferStats.getFailureCount()));
#endif

    LOG_WARN("Data acquisition thread stopped");
}

void DataAcquisitionManager::processingThread()
{
    LOG_INFO("Processing thread started");

    try {
        while (m_running) {
            std::unique_lock<std::mutex> lock(m_mutex);

            // 等待数据或停止信号
            m_dataReady.wait_for(lock, std::chrono::milliseconds(STOP_CHECK_INTERVAL_MS),
                [this]() { return !m_running || m_buffer->getReadBuffer().has_value(); });

            if (!m_running) break;

            // 获取待处理的数据包
            if (auto packet = m_buffer->getReadBuffer()) {
                lock.unlock();  // 解锁以允许并行处理

                if (m_processor) {
                    try {
                        m_processor->processData(*packet);
                        emit dataReceived(*packet);
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR(QString("Data processing error: %1").arg(e.what()));
                        emit errorOccurred(QString("数据处理错误: %1").arg(e.what()));
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in processing thread: %1").arg(e.what()));
        signalStop(StopReason::DEVICE_ERROR);
    }

    LOG_WARN("Processing thread stopped");
}

void DataAcquisitionManager::updateStats()
{
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();

    if (duration > 0) {
        // 计算数据速率
        m_dataRate = static_cast<double>(m_totalBytes.load()) / duration / (1024 * 1024);

        // 在发送信号前获取atomic变量的值
        uint64_t currentBytes = m_totalBytes.load();
        double currentRate = m_dataRate;

        // 使用Qt的信号槽机制安全地更新UI
        QMetaObject::invokeMethod(this, [this, currentBytes, currentRate]() {
            emit statsUpdated(currentBytes, currentRate);
            }, Qt::QueuedConnection);
    }
}

bool DataAcquisitionManager::validateAcquisitionParams() const
{
    // 图像宽度验证
    if (m_params.width == 0 || m_params.width > 4096) {
        LOG_ERROR(QString("Invalid image width: %1").arg(m_params.width));
        return false;
    }

    // 图像高度验证
    if (m_params.height == 0 || m_params.height > 4096) {
        LOG_ERROR(QString("Invalid image height: %1").arg(m_params.height));
        return false;
    }

    // 采集格式验证
    switch (m_params.format) {
    case 0x38: // RAW8
    case 0x39: // RAW10
    case 0x3A: // RAW12
        break;
    default:
        LOG_ERROR(QString("Unsupported capture format: 0x%1")
            .arg(m_params.format, 2, 16, QChar('0')));
        return false;
    }

    return true;
}

void DataAcquisitionManager::updateAcquisitionState(AcquisitionState newState)
{
    m_acquisitionState = newState;

    QString stateStr;
    switch (newState) {
    case AcquisitionState::AC_IDLE:
        stateStr = "空闲";
        break;
    case AcquisitionState::AC_CONFIGURING:
        stateStr = "配置中";
        break;
    case AcquisitionState::AC_RUNNING:
        stateStr = "采集中";
        break;
    case AcquisitionState::AC_STOPPING:
        stateStr = "正在停止";
        break;
    case AcquisitionState::AC_ERROR:
        stateStr = "错误";
        break;
    }

    emit acquisitionStateChanged(stateStr);
}