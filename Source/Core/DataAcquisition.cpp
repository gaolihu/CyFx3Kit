// Source/Core/DataAcquisitionManager.cpp

#include <QDateTime>
#include <QCoreApplication>
#include <QApplication>
#include "DataAcquisition.h"
#include "Logger.h"
#include "ThreadHelper.h"

std::shared_ptr<DataAcquisitionManager> DataAcquisitionManager::create(std::shared_ptr<USBDevice> device) {
    // 使用std::shared_ptr的构造函数，不使用make_shared，以保证enable_shared_from_this正确工作
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

    // 创建循环缓冲区
    m_buffer = std::make_unique<CircularBuffer>(BUFFER_COUNT, BUFFER_SIZE);
    LOG_INFO(QString("Created buffer pool - Count: %1, Size per buffer: %2 bytes")
        .arg(BUFFER_COUNT)
        .arg(BUFFER_SIZE));

    // 设置批处理参数
    m_buffer->setBatchingParams(8, 50); // 每批最多8个包，最大间隔50ms

    // 记录开始时间
    m_startTime = std::chrono::steady_clock::now();
}

DataAcquisitionManager::~DataAcquisitionManager()
{
    LOG_INFO("DataAcquisitionManager destructor START");

    try {
        // 确保所有线程已停止
        stopAcquisition();

        // 防止循环引用
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

    // 创建数据包
    DataPacket packet;

    // 使用共享指针管理数据
    packet.data = std::make_shared<std::vector<uint8_t>>(
        m_buffers[m_currentWriteBuffer].begin(),
        m_buffers[m_currentWriteBuffer].begin() + bytesWritten
        );

    packet.timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    // 批处理逻辑
    auto now = std::chrono::steady_clock::now();

    // 如果是新批次的开始
    if (m_packetsInCurrentBatch == 0) {
        m_currentBatchId++;
        m_batchStartTime = now;
        m_currentBatch.clear();
#ifdef AQ_DBG
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("开始新批次，ID: %1").arg(m_currentBatchId));
#endif // AQ_DBG
    }

    m_packetsInCurrentBatch++;

    // 设置批处理标识
    packet.batchId = m_currentBatchId;
    packet.packetsInBatch = m_packetsInCurrentBatch;

    // 添加到当前批次
    m_currentBatch.push_back(packet);
#ifdef AQ_DBG
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("添加数据包到批次 %1，当前包含 %2 个数据包").arg(m_currentBatchId).arg(m_packetsInCurrentBatch));
#endif // AQ_DBG
    // 判断是否完成当前批次
    bool completeBatch = false;

    // 达到最大包数量或最大时间间隔
    std::chrono::milliseconds elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_batchStartTime);
    if (m_packetsInCurrentBatch >= m_maxPacketsPerBatch ||
        elapsed.count() >= static_cast<int64_t>(m_maxBatchIntervalMs)) {
        completeBatch = true;
#ifdef AQ_DBG
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("批次 %1 已完成: 数据包=%2, 经过时间=%3ms")
            .arg(m_currentBatchId)
            .arg(m_packetsInCurrentBatch)
            .arg(elapsed.count()));
#endif // AQ_DBG

    }

    if (completeBatch) {
        // 设置最后一个包的批次完成标志
        if (!m_currentBatch.empty()) {
            m_currentBatch.back().isBatchComplete = true;

            // 确保批次不为空，再将完整批次放入队列
            if (!m_currentBatch.empty()) {
                DataPacketBatch batchCopy = m_currentBatch;  // 创建副本
                m_readyBatches.push(std::move(batchCopy));

#ifdef AQ_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("批次 %1 已入队，包含 %2 个数据包")
                    .arg(m_currentBatchId)
                    .arg(m_packetsInCurrentBatch));
#endif // AQ_DBG
            }
            else {
                LOG_WARN(QString("批次 %1 被标记为完成但为空").arg(m_currentBatchId));
            }
        }

        // 重置批次计数
        m_packetsInCurrentBatch = 0;
        m_currentBatch.clear();  // 清空当前批次
    }
    else {
        // 不是批次的最后一个包
        if (!m_currentBatch.empty()) {
            m_currentBatch.back().isBatchComplete = false;
        }
    }

    // 同时将数据包添加到单独的队列，用于单包处理
    m_readyBuffers.push(packet);

    // 更新下一个写缓冲区
    m_currentWriteBuffer = (m_currentWriteBuffer + 1) % m_buffers.size();
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

    // 获取设备引用
    auto device = m_deviceWeak.lock();
    if (!device) {
        LOG_ERROR("Device no longer available");
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
    m_buffer->reset();
    m_rateStats.reset();
    m_totalBytes.store(0);
    m_dataRate.store(0.0);

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

    // 只在状态变化时发送状态变更信号
    updateAcquisitionState(AcquisitionState::AC_RUNNING);
    emit signal_AQ_acquisitionStarted();
    LOG_INFO("Acquisition started OK");
    return true;
}

void DataAcquisitionManager::stopAcquisition()
{
    std::unique_lock<std::mutex> stopLock(m_stopMutex);

    if (!m_running) {
        LOG_INFO("Acquisition already stopped");
        return;
    }

    LOG_INFO("Stopping acquisition...");

    // 立即设置停止标志
    m_running = false;
    m_dataReady.notify_all();

    // 在关闭过程中不发送UI更新信号
    bool shouldUpdateUI = !m_isShuttingDown && !QApplication::closingDown();

    stopLock.unlock(); // 解锁以允许线程终止

    try {
        // 安全回收采集线程
        if (m_acquisitionThread.joinable()) {
            LOG_INFO("Joining acquisition thread...");

            auto joinFuture = std::async(std::launch::async, [this]() {
                if (m_acquisitionThread.joinable()) {
                    m_acquisitionThread.join();
                }
                });

            // 最多等待300ms
            if (joinFuture.wait_for(std::chrono::milliseconds(300)) != std::future_status::ready) {
                LOG_WARN("Acquisition thread join timed out, detaching");
                if (m_acquisitionThread.joinable()) {
                    m_acquisitionThread.detach();
                }
            }
        }

        // 安全回收处理线程
        if (m_processingThread.joinable()) {
            LOG_INFO("Joining processing thread...");

            auto joinFuture = std::async(std::launch::async, [this]() {
                if (m_processingThread.joinable()) {
                    m_processingThread.join();
                }
                });

            // 最多等待300ms
            if (joinFuture.wait_for(std::chrono::milliseconds(300)) != std::future_status::ready) {
                LOG_WARN("Processing thread join timed out, detaching");
                if (m_processingThread.joinable()) {
                    m_processingThread.detach();
                }
            }
        }

        // 只在非关闭状态更新UI
        if (shouldUpdateUI) {
            QMetaObject::invokeMethod(QCoreApplication::instance(), [this]() {
                if (!m_isShuttingDown && !QApplication::closingDown()) {
                    m_buffer->reset();
                    // 只在状态变化时发送状态变更信号
                    updateAcquisitionState(AcquisitionState::AC_IDLE);
                    emit signal_AQ_acquisitionStopped();
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

void DataAcquisitionManager::signalStop(StopReason reason)
{
    // 先检查是否已经停止，避免多次调用
    {
        std::lock_guard<std::mutex> lock(m_stopMutex);
        if (!m_running) {
            return;
        }
        m_running = false; // 立即设置停止标志
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("正在停止采集，原因: %1").arg(static_cast<int>(reason)));

    // 通知等待线程
    m_dataReady.notify_all();

    // 使用弱引用进行UI更新和信号发送
    std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();

    QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, reason]() {
        // 尝试从弱引用获取共享指针
        auto self = weakSelf.lock();
        if (!self) return; // 对象已被销毁，安全退出

        // 检查应用程序是否正在关闭
        if (QApplication::closingDown() || self->isShuttingDown()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序正在关闭，跳过错误处理和UI更新"));
            return;
        }

        // 根据停止原因处理
        switch (reason) {
        case StopReason::READ_ERROR:
            LOG_ERROR("Stopping acquisition due to read errors");
            self->m_errorOccurred = true;
            emit self->signal_AQ_errorOccurred(LocalQTCompat::fromLocal8Bit("数据读取错误，采集已停止"));
            break;

        case StopReason::DEVICE_ERROR:
            LOG_ERROR("Stopping acquisition due to device error");
            self->m_errorOccurred = true;
            emit self->signal_AQ_errorOccurred(LocalQTCompat::fromLocal8Bit("设备错误，采集已停止"));
            break;

        case StopReason::BUFFER_OVERFLOW:
            LOG_ERROR("Stopping acquisition due to buffer overflow");
            self->m_errorOccurred = true;
            emit self->signal_AQ_errorOccurred(LocalQTCompat::fromLocal8Bit("缓冲区溢出，采集已停止"));
            break;

        case StopReason::USER_REQUEST:
            LOG_INFO("Stopping acquisition by user request");
            break;
        }

        // 确保设备传输状态也被更新
        auto device = self->m_deviceWeak.lock();
        if (device) {
            device->stopTransfer();
        }

        // 发送状态变更和统计更新信号
        // 只在状态变化时发送状态变更信号
        emit self->signal_AQ_acquisitionStateChanged(LocalQTCompat::fromLocal8Bit("已停止"));
        emit self->signal_AQ_statsUpdated(self->m_totalBytes.load(), self->m_dataRate.load(),
            self->m_rateStats.getElapsedTimeMs());

        // 最后使用Qt的信号队列机制更新已停止状态
        QTimer::singleShot(0, [weakSelf]() {
            if (auto self = weakSelf.lock()) {
                if (!self->isShuttingDown()) {
                    self->updateAcquisitionState(AcquisitionState::AC_IDLE);
                    emit self->signal_AQ_acquisitionStopped();
                }
            }
            });
        }, Qt::QueuedConnection);
}

void DataAcquisitionManager::acquisitionThread()
{
    LOG_INFO("Data acquisition thread started");

    // 获取设备引用
    auto device = m_deviceWeak.lock();
    if (!device) {
        LOG_ERROR("Device no longer available");

        // 使用弱引用安全地调用signalStop
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

    // 添加连续读取失败计数
    int consecutiveFailures = 0;

    try {
        while (m_running) {
            if (!m_running) break;  // 快速检查停止标志

            // 检查缓冲区状态
            if (m_buffer->checkBufferStatus() == CircularBuffer::WarningLevel::C_CRITICAL) {
                LOG_ERROR("Buffer overflow detected");

                // 使用弱引用安全地调用signalStop
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

            // 循环中重新获取设备引用，防止设备在操作期间被销毁
            device = m_deviceWeak.lock();
            if (!device) {
                LOG_ERROR("Device disconnected during acquisition");

                // 使用弱引用安全地调用signalStop
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
                // 读取成功，重置失败计数
                consecutiveFailures = 0;

                m_buffer->commitBuffer(actualLength);

                // 更新统计数据
                m_rateStats.addBytes(actualLength);

                // 更新总字节数原子变量
                m_totalBytes.fetch_add(actualLength);

                // 通知处理线程有新数据
                m_dataReady.notify_one();

                // 定期更新统计信息
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

                    // 使用弱引用安全地调用signalStop
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

                // 短暂协程后继续尝试
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 让出时间片
            std::this_thread::yield();
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in acquisition thread: %1").arg(e.what()));

        // 使用弱引用安全地调用signalStop
        std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
        QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, errorMsg = std::string(e.what())]() {
            if (auto self = weakSelf.lock()) {
                if (self->isRunning() && !self->isShuttingDown()) {
                    // 保存错误信息
                    QString qErrorMsg = QString::fromStdString(errorMsg);
                    self->signalStop(StopReason::DEVICE_ERROR);
                    if (!self->isShuttingDown()) {
                        emit self->signal_AQ_errorOccurred(QString("采集线程异常: %1").arg(qErrorMsg));
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

            // 等待数据或停止信号
            m_dataReady.wait_for(lock, std::chrono::milliseconds(STOP_CHECK_INTERVAL_MS),
                [this]() {
                    return !m_running ||
                        m_buffer->getReadBuffer().has_value() ||
                        m_buffer->getReadyBatch().has_value();
                });

            if (!m_running) break;

            // 优先处理批次数据
            auto batchOpt = m_buffer->getReadyBatch();
#ifdef AQ_DBG
            LOG_DEBUG("Checking for ready batches...");
#endif // AQ_DBG

            if (batchOpt) {
                // 获取到一个完整批次
                DataPacketBatch batch = std::move(*batchOpt);
#ifdef AQ_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("获取到批次数据，包含 %1 个数据包").arg(batch.size()));
#endif
                lock.unlock();  // 解锁以允许并行处理

                if (!batch.empty()) {
                    if (m_processor) {
                        try {
                            // 调用处理器处理批量数据
#ifdef AQ_DBG
                            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("正在处理 %1 个数据包的批次").arg(batch.size()));
#endif // AQ_DBG
                            // 处理整个批次数据
                            m_processor->processBatchData(batch);

                            // 对于向后兼容，如果批次只有一个数据包，也发送单包信号
                            if (batch.size() == 1) {
                                emit signal_AQ_dataReceived(batch[0]);
                            }
                        }
                        catch (const std::exception& e) {
                            LOG_ERROR(QString("Data batch processing error: %1").arg(e.what()));
                            emit signal_AQ_errorOccurred(QString("批量数据处理错误: %1").arg(e.what()));
                        }
                    }

                    // 发送批量数据接收信号
                    emit signal_AQ_batchDataReceived(batch);
                }
                else {
                    LOG_WARN("获取到空批次 - 这不应该发生");
                }
                continue; // 继续检查下一个批次
            }

            // 向后兼容：处理单个数据包
            auto packetOpt = m_buffer->getReadBuffer();
            if (packetOpt) {
                DataPacket packet = std::move(*packetOpt);
                LOG_DEBUG("获取到单个数据包");
                lock.unlock();  // 解锁以允许并行处理

                if (m_processor) {
                    try {
                        m_processor->processData(packet);
                        emit signal_AQ_dataReceived(packet);
                    }
                    catch (const std::exception& e) {
                        LOG_ERROR(QString("Data processing error: %1").arg(e.what()));
                        emit signal_AQ_errorOccurred(QString("数据处理错误: %1").arg(e.what()));
                    }
                }
            }
            else {
                // 没有可用数据，释放锁并短暂让出时间片
                lock.unlock();
                std::this_thread::yield();
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("Exception in processing thread: %1").arg(e.what()));

        // 使用弱引用安全地调用signalStop
        std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
        QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, errorMsg = std::string(e.what())]() {
            if (auto self = weakSelf.lock()) {
                if (self->isRunning() && !self->isShuttingDown()) {
                    self->signalStop(StopReason::DEVICE_ERROR);
                    if (!self->isShuttingDown()) {
                        emit self->signal_AQ_errorOccurred(QString("处理线程异常: %1").arg(QString::fromStdString(errorMsg)));
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

    // Get current statistics
    uint64_t currentBytes = m_rateStats.getTotalBytes();
    double currentRate = m_rateStats.getDataRate();  // Get the actual rate
    uint64_t elapsedTime = m_rateStats.getElapsedTimeMs();

#if 0
    // Log the data rate for debugging
    LOG_DEBUG(QString("Current data rate: %1 MB/s, Total bytes: %2, Elapsed: %3 ms")
        .arg(currentRate, 0, 'f', 2)
        .arg(currentBytes)
        .arg(elapsedTime));
#endif

    // Update atomic variables for direct access
    m_totalBytes.store(currentBytes);
    m_dataRate.store(currentRate);

    // Use weak reference for safe UI update
    std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
    QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, currentBytes, currentRate, elapsedTime]() {
        if (auto self = weakSelf.lock()) {
            if (!self->isShuttingDown()) {
                // Send statistics update signal only
                emit self->signal_AQ_statsUpdated(currentBytes, currentRate, elapsedTime);
            }
        }
        }, Qt::QueuedConnection);
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
    AcquisitionState oldState = m_acquisitionState.exchange(newState);

    // 只有当状态发生变化时才发送信号
    if (oldState == newState) {
        return;
    }

    QString stateStr;
    switch (newState) {
    case AcquisitionState::AC_IDLE:
        stateStr = LocalQTCompat::fromLocal8Bit("空闲");
        break;
    case AcquisitionState::AC_CONFIGURING:
        stateStr = LocalQTCompat::fromLocal8Bit("配置中");
        break;
    case AcquisitionState::AC_RUNNING:
        stateStr = LocalQTCompat::fromLocal8Bit("采集中");
        break;
    case AcquisitionState::AC_STOPPING:
        stateStr = LocalQTCompat::fromLocal8Bit("正在停止");
        break;
    case AcquisitionState::AC_ERROR:
        stateStr = LocalQTCompat::fromLocal8Bit("错误");
        break;
    }

    // 使用弱引用安全地发送信号
    std::weak_ptr<DataAcquisitionManager> weakSelf = weak_from_this();
    QMetaObject::invokeMethod(QApplication::instance(), [weakSelf, stateStr]() {
        if (auto self = weakSelf.lock()) {
            if (!self->isShuttingDown()) {
                LOG_INFO(QString("Acquisition state changed to: %1").arg(stateStr));
                emit self->signal_AQ_acquisitionStateChanged(stateStr);
            }
        }
        }, Qt::QueuedConnection);
}