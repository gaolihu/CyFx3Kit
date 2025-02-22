#include <Windows.h>
#include <thread>
#include <chrono>
#include "DataAcquisition.h"
#include "Logger.h"

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
}

DataAcquisitionManager::CircularBuffer::CircularBuffer(size_t bufferCount, size_t bufferSize)
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

std::pair<uint8_t*, size_t> DataAcquisitionManager::CircularBuffer::getWriteBuffer()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buffers.empty()) {
        LOG_ERROR("Buffer array is empty");
        return { nullptr, 0 };
    }

    size_t bufferSize = m_buffers[m_currentWriteBuffer].size();
    uint8_t* bufferPtr = m_buffers[m_currentWriteBuffer].data();

    LOG_DEBUG(QString("Providing write buffer - Index: %1, Size: %2 bytes, Address: 0x%3")
        .arg(m_currentWriteBuffer)
        .arg(bufferSize)
        .arg(reinterpret_cast<quint64>(bufferPtr), 0, 16));

    return { bufferPtr, bufferSize };
}

void DataAcquisitionManager::CircularBuffer::commitBuffer(size_t bytesWritten)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG_DEBUG(QString("Committing buffer - Index: %1, Written bytes: %2")
        .arg(m_currentWriteBuffer)
        .arg(bytesWritten));

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

    size_t oldIndex = m_currentWriteBuffer;
    m_currentWriteBuffer = (m_currentWriteBuffer + 1) % m_buffers.size();

    LOG_DEBUG(QString("Buffer committed - Old index: %1, New index: %2, Queue size: %3")
        .arg(oldIndex)
        .arg(m_currentWriteBuffer)
        .arg(m_readyBuffers.size()));
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

bool DataAcquisitionManager::startAcquisition(uint16_t width, uint16_t height, uint8_t capType)
{
    LOG_INFO("Try start acquisition");

    // 确保线程未在运行
    stopAcquisition();

    // 设置采集参数
    m_params.width = width;
    m_params.height = height;
    m_params.format = capType;

    // 重置状态和统计信息
    m_totalBytes = 0;
    m_startTime = std::chrono::steady_clock::now();

    // 这里明确设置运行标志
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

    LOG_INFO("Acquisition started");
    emit acquisitionStarted();
    return true;
}

void DataAcquisitionManager::stopAcquisition()
{
    if (!m_running) {
        return;
    }

    LOG_INFO("Stopping data acquisition");

    m_running = false;

    // 等待采集线程结束
    if (m_acquisitionThread.joinable()) {
        m_acquisitionThread.join();
    }

    // 等待处理线程结束
    if (m_processingThread.joinable()) {
        m_processingThread.join();
    }

    // 停止USB传输
    if (m_device) {
        m_device->stopTransfer();
    }

    // 重置统计信息
    m_totalBytes = 0;

    emit acquisitionStopped();
    LOG_INFO("Acquisition stoped");
}

void DataAcquisitionManager::updateAcquisitionState(AcquisitionState newState)
{
    LOG_INFO("Update acquisition state");

    m_acquisitionState = newState;

    QString stateStr;
    switch (newState) {
    case AcquisitionState::AC_IDLE: stateStr = "空闲"; break;
    case AcquisitionState::AC_CONFIGURING: stateStr = "配置中"; break;
    case AcquisitionState::AC_RUNNING: stateStr = "采集中"; break;
    case AcquisitionState::AC_PAUSED: stateStr = "已暂停"; break;
    case AcquisitionState::AC_ERROR: stateStr = "错误"; break;
    }

    acquisitionStateChanged(stateStr);
}

bool DataAcquisitionManager::validateAcquisitionParams() {
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

// 错误处理
void DataAcquisitionManager::handleAcquisitionError(const std::string& error) {
    LOG_INFO("Handle acquisition error");
    LOG_ERROR(QString::fromStdString(error));
    updateAcquisitionState(AcquisitionState::AC_ERROR);
    emit errorOccurred(QString::fromStdString(error));

    // 停止所有采集相关活动
    if (m_running) {
        m_running = false;
        if (m_acquisitionThread.joinable()) {
            m_acquisitionThread.join();
        }
        if (m_processingThread.joinable()) {
            m_processingThread.join();
        }
    }

    // 确保USB设备停止传输
    if (m_device) {
        m_device->stopTransfer();
    }
}

void DataAcquisitionManager::acquisitionThread() {
    LOG_INFO("Data acquisition thread started");

    const size_t BUFFER_SIZE = 16384;  // 16KB
    std::vector<uint8_t> readBuffer(BUFFER_SIZE);

    LOG_INFO(QString("Acquisition buffer configured - Size: %1 bytes").arg(BUFFER_SIZE));

    while (m_running) {
        auto [writeBuffer, size] = m_buffer->getWriteBuffer();
        if (!writeBuffer) {
            LOG_WARN("Failed to get write buffer, retrying after delay");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        LOG_DEBUG(QString("Starting USB read - Buffer size: %1 bytes").arg(size));

        LONG actualLength = BUFFER_SIZE;
        bool readSuccess = m_device->readData(writeBuffer, actualLength);

        LOG_DEBUG(QString("USB read completed - Success: %1, Requested: %2, Actual: %3")
            .arg(readSuccess)
            .arg(BUFFER_SIZE)
            .arg(actualLength));

        if (readSuccess && actualLength > 0) {
            m_buffer->commitBuffer(actualLength);
            m_totalBytes += actualLength;
            m_dataReady.notify_one();

            LOG_DEBUG(QString("Data processed - Total bytes: %1, This read: %2")
                .arg(m_totalBytes)
                .arg(actualLength));
        }
        else {
            LOG_ERROR(QString("Failed to read data from device, read len: %1")
                .arg(actualLength));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    LOG_WARN("Data acquisition thread exiting");
}

void DataAcquisitionManager::processingThread()
{
    LOG_INFO("Processing thread started");

    while (m_running) {
        // 获取待处理的数据包
        if (auto packet = m_buffer->getReadBuffer()) {
            if (m_processor) {
                try {
                    // 处理数据
                    m_processor->processData(*packet);
                    emit dataReceived(*packet);
                }
                catch (const std::exception& e) {
                    emit errorOccurred(QString("数据处理错误: %1").arg(e.what()));
                }
            }
        }
        else {
            // 如果没有数据可处理，等待新数据到达
            std::unique_lock<std::mutex> lock(m_mutex);
            m_dataReady.wait_for(lock, std::chrono::milliseconds(100));
        }
    }

    LOG_WARN("Processing thread stopped");
}

void DataAcquisitionManager::updateStats()
{
    static auto lastUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate).count() >= 1) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
        if (duration > 0) {
            double dataRate = static_cast<double>(m_totalBytes) / duration / (1024 * 1024); // MB/s
            LOG_INFO("Update status");
            emit statsUpdated(m_totalBytes, dataRate);
        }
        lastUpdate = now;
    }
}
