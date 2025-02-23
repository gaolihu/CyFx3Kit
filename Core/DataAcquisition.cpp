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

    // ����ѭ��������
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

    // ���òɼ�����
    m_params.width = width;
    m_params.height = height;
    m_params.format = capType;

    if (!validateAcquisitionParams()) {
        LOG_ERROR("Invalid acquisition parameters");
        return false;
    }

    // ����״̬��ͳ����Ϣ
    m_transferStats.reset();
    m_buffer->reset();

    // �������б�־
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running = true;
    }

    try {
        LOG_INFO("Start acquisition / process thread");

        // �����ɼ��߳�
        m_acquisitionThread = std::thread([this]() {
            LOG_INFO("Acquisition thread started with ID: " +
                Logger::instance().getThreadIdAsString(m_acquisitionThread));
            this->acquisitionThread();
            });

        // ���������߳�
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

    // ��������ͳ������
    uint64_t finalBytes = m_transferStats.getTotalBytes();
    double finalSpeed = m_transferStats.getCurrentSpeed();

    // ����ֹͣ��־
    m_running = false;
    m_dataReady.notify_all();

    // �ȴ��߳̽���
    if (m_acquisitionThread.joinable() &&
        std::this_thread::get_id() != m_acquisitionThread.get_id()) {
        m_acquisitionThread.join();
    }

    if (m_processingThread.joinable() &&
        std::this_thread::get_id() != m_processingThread.get_id()) {
        m_processingThread.join();
    }

    // ��������ͳ������
    emit statsUpdated(finalBytes, 0.0);  // �ٶ���Ϊ0��ʾ��ֹͣ

    // ������Դ
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
        emit errorOccurred(QString::fromLocal8Bit("���ݶ�ȡ���󣬲ɼ���ֹͣ"));
        break;

    case StopReason::DEVICE_ERROR:
        LOG_ERROR("Stopping acquisition due to device error");
        m_errorOccurred = true;
        emit errorOccurred(QString::fromLocal8Bit("�豸���󣬲ɼ���ֹͣ"));
        break;

    case StopReason::BUFFER_OVERFLOW:
        LOG_ERROR("Stopping acquisition due to buffer overflow");
        m_errorOccurred = true;
        emit errorOccurred(QString::fromLocal8Bit("������������ɼ���ֹͣ"));
        break;

    case StopReason::USER_REQUEST:
        LOG_INFO("Stopping acquisition by user request");
        break;
    }

    m_running = false;
    m_dataReady.notify_all();

    // ֪ͨUI����״̬
    emit acquisitionStateChanged(QString::fromLocal8Bit("��ֹͣ"));
    emit statsUpdated(m_totalBytes, 0.0);  // ��������
}

void DataAcquisitionManager::acquisitionThread()
{
    LOG_INFO("Data acquisition thread started");

    // ���������ȡʧ�ܼ���
    int consecutiveFailures = 0;

    try {
        while (m_running) {
            if (!m_running) break;  // ���ټ��ֹͣ��־

            // ��黺����״̬
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
                // ��ȡ�ɹ�������ʧ�ܼ���
                consecutiveFailures = 0;

                m_buffer->commitBuffer(actualLength);
                m_totalBytes += actualLength;
                m_dataReady.notify_one();

                // ����ͳ�Ƹ���Ƶ��
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
                // ��ȡʧ�ܴ���
                consecutiveFailures++;

                if (consecutiveFailures >= MAX_CONSECUTIVE_FAILURES) {
                    LOG_ERROR("Too many consecutive read failures, stopping acquisition");
                    signalStop(StopReason::READ_ERROR);
                    break;
                }

                LOG_WARN(QString("Failed to read data (attempt %1/%2)")
                    .arg(consecutiveFailures)
                    .arg(MAX_CONSECUTIVE_FAILURES));

                // ������ʱ���������
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // �ó�ʱ��Ƭ
            std::this_thread::yield();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in acquisition thread: %1").arg(e.what()));
        signalStop(StopReason::DEVICE_ERROR);
    }

    // ��¼���մ���ͳ��
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

            // �ȴ����ݻ�ֹͣ�ź�
            m_dataReady.wait_for(lock, std::chrono::milliseconds(STOP_CHECK_INTERVAL_MS),
                [this]() { return !m_running || m_buffer->getReadBuffer().has_value(); });

            if (!m_running) break;

            // ��ȡ����������ݰ�
            if (auto packet = m_buffer->getReadBuffer()) {
                lock.unlock();  // �����������д���

                if (m_processor) {
                    try {
                        m_processor->processData(*packet);
                        emit dataReceived(*packet);
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR(QString("Data processing error: %1").arg(e.what()));
                        emit errorOccurred(QString("���ݴ������: %1").arg(e.what()));
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
        // ������������
        m_dataRate = static_cast<double>(m_totalBytes.load()) / duration / (1024 * 1024);

        // �ڷ����ź�ǰ��ȡatomic������ֵ
        uint64_t currentBytes = m_totalBytes.load();
        double currentRate = m_dataRate;

        // ʹ��Qt���źŲۻ��ư�ȫ�ظ���UI
        QMetaObject::invokeMethod(this, [this, currentBytes, currentRate]() {
            emit statsUpdated(currentBytes, currentRate);
            }, Qt::QueuedConnection);
    }
}

bool DataAcquisitionManager::validateAcquisitionParams() const
{
    // ͼ������֤
    if (m_params.width == 0 || m_params.width > 4096) {
        LOG_ERROR(QString("Invalid image width: %1").arg(m_params.width));
        return false;
    }

    // ͼ��߶���֤
    if (m_params.height == 0 || m_params.height > 4096) {
        LOG_ERROR(QString("Invalid image height: %1").arg(m_params.height));
        return false;
    }

    // �ɼ���ʽ��֤
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
        stateStr = "����";
        break;
    case AcquisitionState::AC_CONFIGURING:
        stateStr = "������";
        break;
    case AcquisitionState::AC_RUNNING:
        stateStr = "�ɼ���";
        break;
    case AcquisitionState::AC_STOPPING:
        stateStr = "����ֹͣ";
        break;
    case AcquisitionState::AC_ERROR:
        stateStr = "����";
        break;
    }

    emit acquisitionStateChanged(stateStr);
}