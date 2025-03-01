#include <QDateTime>
#include <QCoreApplication>
#include <QApplication>
#include "DataAcquisition.h"
#include "Logger.h"
#include "ThreadHelper.h"

// ��̬�������������ڴ���DataAcquisitionManagerʵ��
std::shared_ptr<DataAcquisitionManager> DataAcquisitionManager::create(std::shared_ptr<USBDevice> device) {
    // ʹ��std::shared_ptr�Ĺ��캯������ʹ��make_shared���Ա�֤enable_shared_from_this��ȷ����
    std::shared_ptr<DataAcquisitionManager> manager(new DataAcquisitionManager(device));
    return manager;
}

DataAcquisitionManager::DataAcquisitionManager(std::shared_ptr<USBDevice> device)
    : m_deviceWeak(device)
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

    // ��¼��ʼʱ��
    m_startTime = std::chrono::steady_clock::now();
}

DataAcquisitionManager::~DataAcquisitionManager()
{
    LOG_INFO("DataAcquisitionManager destructor START");

    try {
        // ȷ�������߳���ֹͣ
        stopAcquisition();

        // ��ֹѭ������
        m_processor.reset();

        LOG_INFO("DataAcquisitionManager destructor END");
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in DataAcquisitionManager destructor: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR("Unknown exception in DataAcquisitionManager destructor");
    }
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

    // ��ȡ�豸����
    auto device = m_deviceWeak.lock();
    if (!device) {
        LOG_ERROR("Device no longer available");
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
    LOG_INFO("Acquisition started OK");
    return true;
}

void DataAcquisitionManager::stopAcquisition() {
    std::unique_lock<std::mutex> stopLock(m_stopMutex);

    if (!m_running) {
        LOG_INFO("Acquisition already stopped");
        return;
    }

    LOG_INFO("Stopping acquisition...");

    // ��������ֹͣ��־
    m_running = false;
    m_dataReady.notify_all();

    // �ڹرչ����в�����UI�����ź�
    bool shouldUpdateUI = !m_isShuttingDown && !QApplication::closingDown();

    stopLock.unlock(); // �����������߳���ֹ

    try {
        // ��ȫ���ղɼ��߳�
        if (m_acquisitionThread.joinable()) {
            LOG_INFO("Joining acquisition thread...");

            auto joinFuture = std::async(std::launch::async, [this]() {
                if (m_acquisitionThread.joinable()) {
                    m_acquisitionThread.join();
                }
                });

            // ���ȴ�300ms
            if (joinFuture.wait_for(std::chrono::milliseconds(300)) != std::future_status::ready) {
                LOG_WARN("Acquisition thread join timed out, detaching");
                if (m_acquisitionThread.joinable()) {
                    m_acquisitionThread.detach();
                }
            }
        }

        // ��ȫ���մ����߳�
        if (m_processingThread.joinable()) {
            LOG_INFO("Joining processing thread...");

            auto joinFuture = std::async(std::launch::async, [this]() {
                if (m_processingThread.joinable()) {
                    m_processingThread.join();
                }
                });

            // ���ȴ�300ms
            if (joinFuture.wait_for(std::chrono::milliseconds(300)) != std::future_status::ready) {
                LOG_WARN("Processing thread join timed out, detaching");
                if (m_processingThread.joinable()) {
                    m_processingThread.detach();
                }
            }
        }

        // ֻ�ڷǹر�״̬����UI
        if (shouldUpdateUI) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [this]() {
                if (!m_isShuttingDown && !QApplication::closingDown()) {
                    emit statsUpdated(0, 0.0, 0);
                    m_buffer->reset();
                    updateAcquisitionState(AcquisitionState::AC_IDLE);
                    emit acquisitionStopped();
                }
                }, Qt::QueuedConnection);
        }

        LOG_INFO("Acquisition stopped successfully");
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception during stop: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR("Unknown exception during stop");
    }
}

void DataAcquisitionManager::signalStop(StopReason reason) {
    // �ȼ���Ƿ��Ѿ�ֹͣ�������ε���
    {
        std::lock_guard<std::mutex> lock(m_stopMutex);
        if (!m_running) {
            return;
        }
        m_running = false; // ��������ֹͣ��־
    }

    LOG_INFO(QString::fromLocal8Bit("����ֹͣ�ɼ���ԭ��: %1").arg(static_cast<int>(reason)));

    // ֪ͨ�ȴ��߳�
    m_dataReady.notify_all();

    // ʹ�������ý���UI���º��źŷ���
    std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();

    QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, reason]() {
        // ���Դ������û�ȡ����ָ��
        auto self = weakSelf.lock();
        if (!self) return; // �����ѱ����٣���ȫ�˳�

        // ���Ӧ�ó����Ƿ����ڹر�
        if (QApplication::closingDown() || self->isShuttingDown()) {
            LOG_INFO(QString::fromLocal8Bit("Ӧ�ó������ڹرգ������������UI����"));
            return;
        }

        // ����ֹͣԭ����
        switch (reason) {
        case StopReason::READ_ERROR:
            LOG_ERROR("Stopping acquisition due to read errors");
            self->m_errorOccurred = true;
            emit self->errorOccurred(QString::fromLocal8Bit("���ݶ�ȡ���󣬲ɼ���ֹͣ"));
            break;

        case StopReason::DEVICE_ERROR:
            LOG_ERROR("Stopping acquisition due to device error");
            self->m_errorOccurred = true;
            emit self->errorOccurred(QString::fromLocal8Bit("�豸���󣬲ɼ���ֹͣ"));
            break;

        case StopReason::BUFFER_OVERFLOW:
            LOG_ERROR("Stopping acquisition due to buffer overflow");
            self->m_errorOccurred = true;
            emit self->errorOccurred(QString::fromLocal8Bit("������������ɼ���ֹͣ"));
            break;

        case StopReason::USER_REQUEST:
            LOG_INFO("Stopping acquisition by user request");
            break;
        }

        // ȷ���豸����״̬Ҳ������
        auto device = self->m_deviceWeak.lock();
        if (device) {
            device->stopTransfer();
        }

        // ֪ͨUI����״̬
        emit self->acquisitionStateChanged(QString::fromLocal8Bit("��ֹͣ"));
        emit self->statsUpdated(self->m_totalBytes.load(), 0.0, 0);  // ��������

        // ���ʹ��Qt���źŶ��л��Ƹ�����ֹͣ״̬
        QTimer::singleShot(0, [weakSelf]() {
            if (auto self = weakSelf.lock()) {
                if (!self->isShuttingDown()) {
                    self->updateAcquisitionState(AcquisitionState::AC_IDLE);
                    emit self->acquisitionStopped();
                }
            }
            });
        }, Qt::QueuedConnection);
}

void DataAcquisitionManager::acquisitionThread()
{
    LOG_INFO("Data acquisition thread started");

    // ��ȡ�豸����
    auto device = m_deviceWeak.lock();
    if (!device) {
        LOG_ERROR("Device no longer available");

        // ʹ�������ð�ȫ�ص���signalStop
        std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
        QMetaObject::invokeMethod(QApplication::instance(), [weakSelf]() {
            if (auto self = weakSelf.lock()) {
                if (self->isRunning() && !self->isShuttingDown()) {
                    self->signalStop(StopReason::DEVICE_ERROR);
                }
            }
            }, Qt::QueuedConnection);
        return;
    }

    // ���������ȡʧ�ܼ���
    int consecutiveFailures = 0;

    try {
        while (m_running) {
            if (!m_running) break;  // ���ټ��ֹͣ��־

            // ��黺����״̬
            if (m_buffer->checkBufferStatus() == CircularBuffer::WarningLevel::C_CRITICAL) {
                LOG_ERROR("Buffer overflow detected");

                // ʹ�������ð�ȫ�ص���signalStop
                std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
                QMetaObject::invokeMethod(QApplication::instance(), [weakSelf]() {
                    if (auto self = weakSelf.lock()) {
                        if (self->isRunning() && !self->isShuttingDown()) {
                            self->signalStop(StopReason::BUFFER_OVERFLOW);
                        }
                    }
                    }, Qt::QueuedConnection);
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

            // ѭ�������»�ȡ�豸���ã���ֹ�豸�ڲ����ڼ䱻����
            device = m_deviceWeak.lock();
            if (!device) {
                LOG_ERROR("Device disconnected during acquisition");

                // ʹ�������ð�ȫ�ص���signalStop
                std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
                QMetaObject::invokeMethod(QApplication::instance(), [weakSelf]() {
                    if (auto self = weakSelf.lock()) {
                        if (self->isRunning() && !self->isShuttingDown()) {
                            self->signalStop(StopReason::DEVICE_ERROR);
                        }
                    }
                    }, Qt::QueuedConnection);
                break;
            }

            bool readSuccess = device->readData(writeBuffer, actualLength);

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

                    // ʹ�������ð�ȫ�ص���signalStop
                    std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
                    QMetaObject::invokeMethod(QApplication::instance(), [weakSelf]() {
                        if (auto self = weakSelf.lock()) {
                            if (self->isRunning() && !self->isShuttingDown()) {
                                self->signalStop(StopReason::READ_ERROR);
                            }
                        }
                        }, Qt::QueuedConnection);
                    break;
                }

                LOG_WARN(QString("Failed to read data (attempt %1/%2)")
                    .arg(consecutiveFailures)
                    .arg(MAX_CONSECUTIVE_FAILURES));

                // ����Э�̺��������
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // �ó�ʱ��Ƭ
            std::this_thread::yield();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in acquisition thread: %1").arg(e.what()));

        // ʹ�������ð�ȫ�ص���signalStop
        std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
        QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, errorMsg = std::string(e.what())]() {
            if (auto self = weakSelf.lock()) {
                if (self->isRunning() && !self->isShuttingDown()) {
                    // ���������Ϣ
                    QString qErrorMsg = QString::fromStdString(errorMsg);
                    self->signalStop(StopReason::DEVICE_ERROR);
                    if (!self->isShuttingDown()) {
                        emit self->errorOccurred(QString("�ɼ��߳��쳣: %1").arg(qErrorMsg));
                    }
                }
            }
        }, Qt::QueuedConnection);
    }

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

        // ʹ�������ð�ȫ�ص���signalStop
        std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
        QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, errorMsg = std::string(e.what())]() {
            if (auto self = weakSelf.lock()) {
                if (self->isRunning() && !self->isShuttingDown()) {
                    self->signalStop(StopReason::DEVICE_ERROR);
                    if (!self->isShuttingDown()) {
                        emit self->errorOccurred(QString("�����߳��쳣: %1").arg(QString::fromStdString(errorMsg)));
                    }
                }
            }
        }, Qt::QueuedConnection);
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
        // Calculate data rate
        m_dataRate = static_cast<double>(m_totalBytes.load()) / duration / (1024 * 1024);

        // Get values before sending signal
        uint64_t currentBytes = m_totalBytes.load();
        double currentRate = m_dataRate;
        uint64_t elapsedTime = duration;

        // Use weak reference for safe UI update
        std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
        QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, currentBytes, currentRate, elapsedTime]() {
            if (auto self = weakSelf.lock()) {
                if (!self->isShuttingDown()) {
                    // Add elapsed time to the stats update signal
                    emit self->statsUpdated(currentBytes, currentRate, elapsedTime);
                }
            }
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
        stateStr = QString::fromLocal8Bit("����");
        break;
    case AcquisitionState::AC_CONFIGURING:
        stateStr = QString::fromLocal8Bit("������");
        break;
    case AcquisitionState::AC_RUNNING:
        stateStr = QString::fromLocal8Bit("�ɼ���");
        break;
    case AcquisitionState::AC_STOPPING:
        stateStr = QString::fromLocal8Bit("����ֹͣ");
        break;
    case AcquisitionState::AC_ERROR:
        stateStr = QString::fromLocal8Bit("����");
        break;
    }

    // ʹ�������ð�ȫ�ط����ź�
    std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
    QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, stateStr]() {
        if (auto self = weakSelf.lock()) {
            if (!self->isShuttingDown()) {
                emit self->acquisitionStateChanged(stateStr);
            }
        }
        }, Qt::QueuedConnection);
}