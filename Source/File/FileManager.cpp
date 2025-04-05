// Source/File/FileManager.cpp

#include "FileManager.h"
#include "Logger.h"
#include "DataConverters.h"
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <future>
#include <chrono>

FileManager& FileManager::instance()
{
    static FileManager instance;
    return instance;
}

FileManager::FileManager()
    : m_running(false)
    , m_paused(false)
    , m_useAsyncWriter(true)
    , m_lastSavedBytes(0)
{
    // 初始化默认保存参数
    m_saveParams.basePath = QDir::homePath() + "/FX3Data";
    m_saveParams.format = FileFormat::RAW;
    m_saveParams.autoNaming = true;
    m_saveParams.filePrefix = "capture";
    m_saveParams.createSubfolder = true;
    m_saveParams.appendTimestamp = true;
    m_saveParams.compressionLevel = 0;
    m_saveParams.saveMetadata = true;

    // 初始化文件写入器和缓存管理器
    resetFileWriter();
    m_cacheManager = std::make_unique<DataCacheManager>();

    // 初始化速度计时器
    m_speedTimer.start();

    // 初始化统计信息
    m_statistics.totalBytes = 0;
    m_statistics.fileCount = 0;
    m_statistics.saveRate = 0.0;
    m_statistics.status = SaveStatus::FS_IDLE;

    // 注册内置转换器
    registerConverter(FileFormat::RAW, DataConverterFactory::createConverter(FileFormat::RAW));
    registerConverter(FileFormat::BMP, DataConverterFactory::createConverter(FileFormat::BMP));
    registerConverter(FileFormat::TIFF, DataConverterFactory::createConverter(FileFormat::TIFF));
    registerConverter(FileFormat::PNG, DataConverterFactory::createConverter(FileFormat::PNG));
    registerConverter(FileFormat::CSV, DataConverterFactory::createConverter(FileFormat::CSV));

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存管理器已创建"));
}

FileManager::~FileManager()
{
    stopSaving();
}

void FileManager::setSaveParameters(const SaveParameters& params)
{
    std::lock_guard<std::mutex> lock(m_paramsMutex);

    // 如果正在保存中，仅部分参数可以更改
    if (m_running) {
        // 可以在运行时更改的参数
        m_saveParams.compressionLevel = params.compressionLevel;
        m_saveParams.appendTimestamp = params.appendTimestamp;
        m_saveParams.autoNaming = params.autoNaming;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("保存过程中更新参数 - 压缩级别: %1, 时间戳: %2, 自动命名: %3")
            .arg(params.compressionLevel)
            .arg(params.appendTimestamp ? "是" : "否")
            .arg(params.autoNaming ? "是" : "否"));
    }
    else {
        // 非保存状态，可以完全更改参数
        m_saveParams = params;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("完全更新保存参数 - 路径: %1, 格式: %2")
            .arg(params.basePath)
            .arg(static_cast<int>(params.format)));
    }
}

const SaveParameters& FileManager::getSaveParameters() const
{
    return m_saveParams;
}

void FileManager::setUseAsyncWriter(bool useAsync)
{
    // 如果保存正在进行中不允许切换
    if (m_running) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("保存进行中无法切换写入器模式"));
        return;
    }

    bool oldValue = m_useAsyncWriter.exchange(useAsync);
    if (oldValue != useAsync) {
        resetFileWriter();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件写入器已切换为%1模式")
            .arg(useAsync ? "异步" : "同步"));
    }
}

bool FileManager::createNewFile(const DataPacket& packet) {
    // 创建文件名
    QString filename = createFileName(packet);
    QString fullPath = m_statistics.savePath + "/" + filename;

    // 打开新文件
    if (!m_fileWriter->open(fullPath)) {
        QString error = LocalQTCompat::fromLocal8Bit("无法打开文件: %1 - %2")
            .arg(fullPath)
            .arg(m_fileWriter->getLastError());
        LOG_ERROR(error);

        // 设置错误状态
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.status = SaveStatus::FS_ERROR;
        m_statistics.lastError = error;

        // 发送错误信号
        emit signal_FSM_saveError(error);
        return false;
    }

    // 更新当前文件名和计数
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.currentFileName = filename;
        m_statistics.fileCount++;
        m_statistics.currentFileBytes = 0;  // 重置当前文件字节计数
        m_statistics.currentFileStartTime = QDateTime::currentDateTime();  // 记录文件开始时间
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建新文件: %1").arg(fullPath));
    return true;
}

bool FileManager::shouldSplitFile() {
    if (!m_fileWriter->isOpen()) {
        return true;  // 如果文件未打开，需要创建新文件
    }

    std::lock_guard<std::mutex> lock(m_paramsMutex);

    // 检查是否超过最大文件大小
    uint64_t maxFileSize = m_saveParams.options.value("max_file_size", 1024 * 1024 * 1024).toULongLong();
    if (m_statistics.currentFileBytes >= maxFileSize) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件达到最大大小 (%1 字节)，创建新文件").arg(maxFileSize));
        return true;
    }

    // 检查是否超过最大时间
    int maxFileDuration = m_saveParams.options.value("auto_split_time", 300).toInt();
    if (maxFileDuration > 0) {
        QDateTime now = QDateTime::currentDateTime();
        int secsElapsed = m_statistics.currentFileStartTime.secsTo(now);
        if (secsElapsed >= maxFileDuration) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("文件时长达到上限 (%1 秒)，创建新文件").arg(maxFileDuration));
            return true;
        }
    }

    return false;
}

bool FileManager::startLoading(const QString& filePath) {
    std::lock_guard<std::mutex> lock(m_loadMutex);

    if (m_loading) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("已经有文件正在加载中，请先停止当前加载"));
        return false;
    }

    // 打开文件
    m_currentFilePath = filePath;
    m_currentFile.setFileName(filePath);
    if (!m_currentFile.open(QIODevice::ReadOnly)) {
        QString error = LocalQTCompat::fromLocal8Bit("无法打开文件: ") + filePath + " - " + m_currentFile.errorString();
        LOG_ERROR(error);
        emit signal_FSM_loadError(error);
        return false;
    }

    // 初始化加载状态
    m_loadPosition = 0;
    m_loadFileSize = m_currentFile.size();
    m_loading = true;

    // 清空加载队列
    std::queue<DataPacket> empty;
    std::swap(m_loadQueue, empty);

    // 分配初始缓冲区
    const size_t INITIAL_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB
    m_loadBuffer.resize(static_cast<int>(std::min(INITIAL_BUFFER_SIZE, static_cast<size_t>(m_loadFileSize))));

    // 启动加载线程
    try {
        m_loadThread = std::thread(&FileManager::loadThreadFunction, this);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件加载线程已启动"));

        // 发送开始加载信号
        emit signal_FSM_loadStarted(filePath, m_loadFileSize);
        return true;
    }
    catch (const std::exception& e) {
        m_loading = false;
        m_currentFile.close();
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动加载线程失败: ") + QString(e.what()));
        emit signal_FSM_loadError(LocalQTCompat::fromLocal8Bit("启动加载线程失败: ") + QString(e.what()));
        return false;
    }
}

bool FileManager::stopLoading() {
    std::lock_guard<std::mutex> lock(m_loadMutex);

    if (!m_loading) {
        return true;
    }

    m_loading = false;

    // 关闭文件
    if (m_currentFile.isOpen()) {
        m_currentFile.close();
    }

    // 等待加载线程结束
    if (m_loadThread.joinable()) {
        m_loadThread.join();
    }

    // 清空加载队列
    std::queue<DataPacket> empty;
    std::swap(m_loadQueue, empty);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件加载已停止"));
    return true;
}

bool FileManager::isLoading() const {
    return m_loading;
}

QString FileManager::getCurrentFileName() const {
    return m_currentFilePath;
}

DataPacket FileManager::getNextPacket() {
    std::lock_guard<std::mutex> lock(m_loadMutex);

    if (m_loadQueue.empty()) {
        // 返回空数据包
        return DataPacket();
    }

    DataPacket packet = std::move(m_loadQueue.front());
    m_loadQueue.pop();
    return packet;
}

bool FileManager::hasMorePackets() {
    std::lock_guard<std::mutex> lock(m_loadMutex);
    return !m_loadQueue.empty() || m_loadPosition < m_loadFileSize;
}

void FileManager::seekTo(uint64_t position) {
    std::lock_guard<std::mutex> lock(m_loadMutex);

    if (!m_loading || !m_currentFile.isOpen()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位：文件未加载"));
        return;
    }

    // 确保位置有效
    position = std::min(position, m_loadFileSize);

    // 设置加载位置
    m_loadPosition = position;

    // 清空加载队列
    std::queue<DataPacket> empty;
    std::swap(m_loadQueue, empty);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件定位到: %1").arg(position));
}

uint64_t FileManager::getTotalFileSize() {
    std::lock_guard<std::mutex> lock(m_loadMutex);
    return m_loadFileSize;
}

void FileManager::loadThreadFunction() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("加载线程已启动"));

    // 设置较低线程优先级，避免影响主线程响应
    QThread::currentThread()->setPriority(QThread::LowPriority);

    // 每次读取的大小
    const size_t READ_CHUNK_SIZE = 1024 * 1024; // 1MB

    // 每个数据包的最大大小
    const size_t MAX_PACKET_SIZE = 64 * 1024; // 64KB

    // 上次进度通知时的位置
    uint64_t lastProgressPosition = 0;

    try {
        while (m_loading && m_loadPosition < m_loadFileSize) {
            // 确定当前需要读取的大小
            size_t bytesToRead = std::min(READ_CHUNK_SIZE, static_cast<size_t>(m_loadFileSize - m_loadPosition));

            // 调整缓冲区大小（如果需要）
            if (m_loadBuffer.size() < static_cast<int>(bytesToRead)) {
                m_loadBuffer.resize(static_cast<int>(bytesToRead));
            }

            // 读取数据
            {
                std::lock_guard<std::mutex> lock(m_loadMutex);
                if (!m_currentFile.seek(m_loadPosition)) {
                    throw std::runtime_error(LocalQTCompat::fromLocal8Bit("无法定位到文件位置: ").toStdString() +
                        std::to_string(m_loadPosition) + " - " +
                        m_currentFile.errorString().toStdString());
                }

                qint64 bytesRead = m_currentFile.read(m_loadBuffer.data(), bytesToRead);
                if (bytesRead <= 0) {
                    throw std::runtime_error(LocalQTCompat::fromLocal8Bit("读取文件数据失败: ").toStdString() +
                        m_currentFile.errorString().toStdString());
                }

                // 更新位置
                m_loadPosition += bytesRead;

                // 将数据分割成数据包并添加到队列
                for (qint64 offset = 0; offset < bytesRead; offset += MAX_PACKET_SIZE) {
                    qint64 packetSize = std::min(static_cast<qint64>(MAX_PACKET_SIZE),
                        bytesRead - offset);

                    // 创建数据包
                    DataPacket packet;

                    // 为数据包分配内存并复制数据
                    uint8_t* packetData = new uint8_t[packetSize];
                    std::memcpy(packetData, m_loadBuffer.data() + offset, packetSize);

                    // 设置数据包属性
                    // packet.setData(packetData, packetSize);
                    packet.timestamp = QDateTime::currentMSecsSinceEpoch() * 1000000; // 当前时间（纳秒）

                    // 添加到队列
                    m_loadQueue.push(packet);
                }

                // 通知有新数据可用
                emit signal_FSM_newDataAvailable(m_loadPosition - bytesRead, bytesRead);
            }

            // 发送进度通知（每5%发送一次）
            double progressPercentage = static_cast<double>(m_loadPosition) / m_loadFileSize;
            double lastProgressPercentage = static_cast<double>(lastProgressPosition) / m_loadFileSize;
            if (progressPercentage - lastProgressPercentage >= 0.05 || m_loadPosition == m_loadFileSize) {
                emit signal_FSM_loadProgress(m_loadPosition, m_loadFileSize);
                lastProgressPosition = m_loadPosition;
            }

            // 避免CPU占用过高
            if (m_loadQueue.size() > 1000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // 加载完成
        if (m_loadPosition >= m_loadFileSize) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("文件加载完成"));
            emit signal_FSM_loadCompleted(m_currentFile.fileName(), m_loadFileSize);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件加载异常: ") + QString(e.what()));
        emit signal_FSM_loadError(LocalQTCompat::fromLocal8Bit("文件加载异常: ") + QString(e.what()));
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("加载线程已退出"));
}

void FileManager::processBatchData(const QByteArray& batchData, uint64_t offset, uint32_t batchId) {
    if (!m_running) {
        return;
    }

    // 创建批次数据包
    DataPacketBatch batch;

    // 计算每个包的大小
    const size_t MAX_PACKET_SIZE = 64 * 1024; // 64KB
    size_t remainingSize = batchData.size();
    size_t currentOffset = 0;

    // 将大块数据拆分为多个适当大小的数据包
    while (remainingSize > 0) {
        size_t packetSize = std::min(remainingSize, MAX_PACKET_SIZE);

        // 创建数据包
        DataPacket packet;
        std::vector<uint8_t> packetData(packetSize);

        // 复制数据
        memcpy(packetData.data(), batchData.constData() + currentOffset, packetSize);
        packet.data = std::make_shared<std::vector<uint8_t>>(std::move(packetData));

        // 设置包元数据
        packet.timestamp = QDateTime::currentMSecsSinceEpoch() * 1000000; // 当前时间（纳秒）
        packet.offsetInFile = offset + currentOffset;
        packet.batchId = batchId;
        packet.packetIndex = batch.size();

        // 添加到批次
        batch.push_back(std::move(packet));

        // 更新进度
        currentOffset += packetSize;
        remainingSize -= packetSize;
    }

    // 设置批次完成标志
    if (!batch.empty()) {
        batch.back().isBatchComplete = true;
    }

    // 处理批次
    slot_FSM_processDataBatch(batch);
}

QByteArray FileManager::readFileRange(const QString& filePath, uint64_t startOffset, uint64_t size) {
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件进行读取: %1 - %2")
            .arg(filePath)
            .arg(file.errorString()));
        return QByteArray();
    }

    // 获取文件大小
    uint64_t fileSize = file.size();

    // 验证偏移范围
    if (startOffset >= fileSize) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取偏移超出文件大小: %1 >= %2")
            .arg(startOffset)
            .arg(fileSize));
        file.close();
        return QByteArray();
    }

    // 调整读取大小
    uint64_t actualSize = std::min(size, fileSize - startOffset);

    // 定位到指定位置
    if (!file.seek(startOffset)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位到文件位置 %1: %2")
            .arg(startOffset)
            .arg(file.errorString()));
        file.close();
        return QByteArray();
    }

    // 读取数据
    QByteArray data = file.read(actualSize);
    file.close();

    if (static_cast<uint64_t>(data.size()) != actualSize) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("读取数据不完整: 请求 %1 字节，实际读取 %2 字节")
            .arg(actualSize)
            .arg(data.size()));
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("成功从文件 %1 读取 %2 字节数据, 偏移: %3")
        .arg(filePath)
        .arg(data.size())
        .arg(startOffset));

    return data;
}

QByteArray FileManager::readLoadedFileRange(uint64_t startOffset, uint64_t size) {
    std::lock_guard<std::mutex> lock(m_loadMutex);

    if (!m_loading || !m_currentFile.isOpen()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法读取: 文件未加载"));
        return QByteArray();
    }

    // 验证偏移范围
    if (startOffset >= m_loadFileSize) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取偏移超出文件大小: %1 >= %2")
            .arg(startOffset)
            .arg(m_loadFileSize));
        return QByteArray();
    }

    // 调整读取大小
    uint64_t actualSize = std::min(size, m_loadFileSize - startOffset);

    // 定位到指定位置
    if (!m_currentFile.seek(startOffset)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位到文件位置 %1: %2")
            .arg(startOffset)
            .arg(m_currentFile.errorString()));
        return QByteArray();
    }

    // 读取数据
    QByteArray data = m_currentFile.read(actualSize);

    if (static_cast<uint64_t>(data.size()) != actualSize) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("读取数据不完整: 请求 %1 字节，实际读取 %2 字节")
            .arg(actualSize)
            .arg(data.size()));
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("成功从当前加载文件读取 %1 字节数据, 偏移: %2")
        .arg(data.size())
        .arg(startOffset));

    return data;
}

bool FileManager::readFileRangeAsync(const QString& filePath, uint64_t startOffset, uint64_t size, uint32_t requestId) {
    // 检查是否已有异步读取任务在运行
    if (m_asyncReadRunning) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("已有异步读取任务在运行，请等待完成"));
        emit signal_FSM_dataReadError(LocalQTCompat::fromLocal8Bit("已有异步读取任务在运行，请等待完成"), requestId);
        return false;
    }

    // 检查文件存在性
    if (!QFile::exists(filePath)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件不存在: %1").arg(filePath));
        emit signal_FSM_dataReadError(LocalQTCompat::fromLocal8Bit("文件不存在"), requestId);
        return false;
    }

    // 启动异步读取线程
    try {
        m_asyncReadRunning = true;
        m_asyncReadThread = std::thread(&FileManager::dataReadThreadFunction, this, filePath, startOffset, size, requestId);

        // 分离线程，使其独立运行
        m_asyncReadThread.detach();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("已启动异步读取任务，请求ID: %1").arg(requestId));
        return true;
    }
    catch (const std::exception& e) {
        m_asyncReadRunning = false;
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动异步读取线程失败: %1").arg(e.what()));
        emit signal_FSM_dataReadError(LocalQTCompat::fromLocal8Bit("启动异步读取线程失败"), requestId);
        return false;
    }
}

void FileManager::dataReadThreadFunction(const QString& filePath, uint64_t startOffset, uint64_t size, uint32_t requestId) {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("异步读取线程已启动，请求ID: %1").arg(requestId));

    try {
        // 读取数据
        QByteArray data = readFileRange(filePath, startOffset, size);

        // 发送读取完成信号
        emit signal_FSM_dataReadCompleted(data, startOffset, requestId);
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("异步读取异常: %1").arg(e.what()));
        emit signal_FSM_dataReadError(LocalQTCompat::fromLocal8Bit("异步读取异常: ") + e.what(), requestId);
    }

    // 重置运行标志
    m_asyncReadRunning = false;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("异步读取线程已完成，请求ID: %1").arg(requestId));
}

void FileManager::resetFileWriter()
{
    if (m_useAsyncWriter) {
        m_fileWriter = std::make_unique<WriterFileAsync>();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用异步文件写入器"));
    }
    else {
        m_fileWriter = std::make_unique<WriterFileStandard>();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用标准文件写入器"));
    }
}

void FileManager::saveDataBatch(const DataPacketBatch& packets)
{
    if (packets.empty()) {
        return;
    }

    try {
        // Get batch reference packet (first in batch)
        const DataPacket& refPacket = packets.front();

        // Get converter
        std::shared_ptr<IDataConverter> converter;
        {
            std::lock_guard<std::mutex> lock(m_paramsMutex);
            auto it = m_converters.find(m_saveParams.format);
            if (it != m_converters.end()) {
                converter = it->second;
            }
        }

        if (!converter) {
            throw std::runtime_error("No suitable converter found for the selected format");
        }

        // Create new file if needed
        if (!m_fileWriter->isOpen()) {
            QString filename = createFileName(refPacket);
            QString fullPath = m_statistics.savePath + "/" + filename;

            if (!m_fileWriter->open(fullPath)) {
                throw std::runtime_error(LocalQTCompat::fromLocal8Bit("无法打开文件: %1 - %2")
                    .arg(fullPath)
                    .arg(m_fileWriter->getLastError()).toStdString());
            }

            // Update current filename
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_statistics.currentFileName = filename;
                m_statistics.fileCount++;
            }

            LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建新文件 (批处理): %1").arg(fullPath));
        }

        // Process batch using batch optimized conversion if available
        QByteArray formattedData;
        {
            std::lock_guard<std::mutex> lock(m_paramsMutex);
            formattedData = converter->convertBatch(packets, m_saveParams);
        }

        if (formattedData.isEmpty()) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("批量转换返回空数据"));
            return;
        }

        // Write batch data
        if (!m_fileWriter->write(formattedData)) {
            throw std::runtime_error(LocalQTCompat::fromLocal8Bit("写入批次数据失败: %1")
                .arg(m_fileWriter->getLastError()).toStdString());
        }

        // Calculate total batch size for statistics
        uint64_t totalBatchSize = formattedData.size();

        // Update statistics
        updateStatistics(totalBatchSize);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("已保存数据批次 (%1 个包, %2 字节)")
            .arg(packets.size())
            .arg(totalBatchSize));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("批量保存数据异常: %1").arg(e.what()));
        throw; // Let the main thread function handle the exception
    }
}

bool FileManager::startSaving()
{
    if (m_running) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("保存已在进行中"));
        return false;
    }

    // 检查基本参数
    {
        std::lock_guard<std::mutex> lock(m_paramsMutex);
        if (m_saveParams.basePath.isEmpty()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("未设置保存路径"));
            emit signal_FSM_saveError(LocalQTCompat::fromLocal8Bit("未设置保存路径"));
            return false;
        }
    }

    // 创建保存目录
    if (!createSaveDirectory()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建保存目录失败"));
        emit signal_FSM_saveError(LocalQTCompat::fromLocal8Bit("创建保存目录失败"));
        return false;
    }

    // 重置统计信息
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.totalBytes = 0;
        m_statistics.fileCount = 0;
        m_statistics.saveRate = 0.0;
        m_statistics.status = SaveStatus::FS_SAVING;
        m_lastSavedBytes = 0;
        m_speedTimer.restart();
    }

    // 设置运行标志
    m_running = true;
    m_paused = false;

    // 清空队列
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<DataPacket> empty;
        std::swap(m_dataQueue, empty);
    }

    // 启动保存线程
    try {
        m_saveThread = std::thread(&FileManager::saveThreadFunction, this);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存线程已启动"));

        // 发送状态更新信号
        emit signal_FSM_saveStatusChanged(SaveStatus::FS_SAVING);
        return true;
    }
    catch (const std::exception& e) {
        m_running = false;
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动保存线程失败: %1").arg(e.what()));
        emit signal_FSM_saveError(LocalQTCompat::fromLocal8Bit("启动保存线程失败: %1").arg(e.what()));
        return false;
    }
}

bool FileManager::stopSaving()
{
    if (!m_running) {
        return true;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));

    // 设置停止标志
    m_running = false;
    m_paused = false;

    // 通知数据就绪条件变量，以便线程可以退出等待
    m_dataReady.notify_all();

    // 使用超时等待确保线程正常退出
    if (m_saveThread.joinable()) {
        try {
            // 在另一线程中尝试join，以便可以实现超时
            auto joinResult = std::async(std::launch::async, [this]() {
                if (m_saveThread.joinable()) {
                    m_saveThread.join();
                    return true;
                }
                return false;
                });

            // 等待线程结束，最多等待3秒
            if (joinResult.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止保存线程超时，可能存在资源泄漏"));
                // 此处我们不调用detach()，因为这可能导致更严重的资源泄漏
                // 但我们也不能强制终止线程，因为这可能导致资源损坏
                return false;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止保存线程异常: %1").arg(e.what()));
            return false;
        }
    }

    // 确保文件已关闭
    m_fileWriter->close();

    // 更新统计信息
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.status = SaveStatus::FS_COMPLETED;
        m_statistics.saveRate = 0.0;
    }

    // 发送状态变更和完成信号
    emit signal_FSM_saveStatusChanged(SaveStatus::FS_COMPLETED);
    emit signal_FSM_saveCompleted(m_statistics.savePath, m_statistics.totalBytes);

    return true;
}

bool FileManager::pauseSaving(bool pause)
{
    if (!m_running) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("未进行保存，无法暂停/恢复"));
        return false;
    }

    if (m_paused == pause) {
        return true; // 已经是请求的状态
    }

    m_paused = pause;

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.status = pause ? SaveStatus::FS_PAUSED : SaveStatus::FS_SAVING;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存状态变更为: %1")
        .arg(pause ? "暂停" : "保存中"));

    // 如果从暂停恢复，通知线程继续处理
    if (!pause) {
        m_dataReady.notify_one();
    }

    // 发送状态变更信号
    emit signal_FSM_saveStatusChanged(pause ? SaveStatus::FS_PAUSED : SaveStatus::FS_SAVING);

    return true;
}

SaveStatistics FileManager::getStatistics()
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_statistics;
}

void FileManager::registerConverter(FileFormat format, std::shared_ptr<IDataConverter> converter)
{
    if (!converter) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("尝试注册空转换器"));
        return;
    }

    m_converters[format] = converter;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("已注册格式转换器: %1").arg(static_cast<int>(format)));
}

QStringList FileManager::getSupportedFormats() const
{
    QStringList formats;

    for (const auto& [format, converter] : m_converters) {
        QString extension = converter->getFileExtension();
        formats.append(extension.toUpper() + " (*." + extension.toLower() + ")");
    }

    return formats;
}

void FileManager::slot_FSM_processDataPacket(const DataPacket& packet)
{
    if (!m_running) {
        LOG_ERROR("Not running");
        return;
    }

    // Add data packet to queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_dataQueue.push(packet);

        // Warning if queue gets too large
        if (m_dataQueue.size() > 100) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("数据队列积累过多: %1 个包").arg(m_dataQueue.size()));
        }
    }

    // Notify saving thread data is ready
    m_dataReady.notify_one();
}

void FileManager::slot_FSM_processDataBatch(const DataPacketBatch& packets)
{
    if (!m_running || packets.empty()) {
        return;
    }

    // Calculate total size for statistics
    uint64_t totalBatchSize = 0;
    for (const auto& packet : packets) {
        totalBatchSize += packet.getSize();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("收到数据批次: %1 个包, 总大小: %2 字节")
        .arg(packets.size())
        .arg(totalBatchSize));

    // Add batch to queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_batchQueue.push(packets);

        // Warning if queue gets too large
        if (m_batchQueue.size() > 20) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("批次队列积累过多: %1 批次").arg(m_batchQueue.size()));
        }
    }

    // Notify saving thread data is ready
    m_dataReady.notify_one();
}

QString FileManager::createFileName(const DataPacket& packet)
{
    std::lock_guard<std::mutex> lock(m_paramsMutex);

    QString filename = m_saveParams.filePrefix;

    // Append frame number or other identifier
    if (m_statistics.fileCount > 0) {
        filename += QString("_%1").arg(m_statistics.fileCount, 6, 10, QChar('0'));
    }

    // Append timestamp
    if (m_saveParams.appendTimestamp) {
        QDateTime now = QDateTime::currentDateTime();
        filename += "_" + now.toString("yyyyMMdd_HHmmss_zzz");
    }

    // Get corresponding format converter and append extension
    auto it = m_converters.find(m_saveParams.format);
    if (it != m_converters.end()) {
        filename += "." + it->second->getFileExtension();
    }
    else {
        // Default extension
        switch (m_saveParams.format) {
        case FileFormat::RAW:
            filename += ".raw";
            break;
        case FileFormat::BMP:
            filename += ".bmp";
            break;
        case FileFormat::TIFF:
            filename += ".tiff";
            break;
        case FileFormat::PNG:
            filename += ".png";
            break;
        case FileFormat::CSV:
            filename += ".csv";
            break;
        case FileFormat::CUSTOM:
            filename += ".dat";
            break;
        default:
            filename += ".bin";
            break;
        }
    }

    return filename;
}

bool FileManager::createSaveDirectory()
{
    std::lock_guard<std::mutex> lock(m_paramsMutex);

    // 规范化路径
    QString normalizedBasePath = QDir::cleanPath(m_saveParams.basePath);
    QDir baseDir(normalizedBasePath);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("检查基本目录: %1").arg(normalizedBasePath));

    // 检查基本目录
    if (!baseDir.exists()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("尝试创建基本目录: %1").arg(normalizedBasePath));

        // 使用mkpath创建完整路径，包括所有父目录
        if (!QDir().mkpath(normalizedBasePath)) {
            QString errorMsg = LocalQTCompat::fromLocal8Bit("无法创建基本目录: %1").arg(normalizedBasePath);
            LOG_ERROR(errorMsg);

            // 检查权限
            QFileInfo parentDir(QFileInfo(normalizedBasePath).path());
            if (!parentDir.isWritable()) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("父目录不可写: %1").arg(parentDir.filePath()));
            }

            return false;
        }

        // 验证目录创建成功
        if (!baseDir.exists()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("目录创建失败: %1").arg(normalizedBasePath));
            return false;
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("基本目录创建成功: %1").arg(normalizedBasePath));
    }

    QString savePath = normalizedBasePath;

    // 创建子文件夹
    if (m_saveParams.createSubfolder) {
        QString subfolder = QDate::currentDate().toString("yyyy-MM-dd");
        savePath = QDir::cleanPath(baseDir.filePath(subfolder));
        QDir subDir(savePath);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("检查子文件夹: %1").arg(savePath));
        if (!subDir.exists()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("尝试创建子文件夹: %1").arg(savePath));

            if (!QDir().mkpath(savePath)) {
                QString errorMsg = LocalQTCompat::fromLocal8Bit("无法创建子文件夹: %1").arg(savePath);
                LOG_ERROR(errorMsg);
                return false;
            }

            // 验证目录创建成功
            if (!subDir.exists()) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("子目录创建失败: %1").arg(savePath));
                return false;
            }

            LOG_INFO(LocalQTCompat::fromLocal8Bit("子文件夹创建成功: %1").arg(savePath));
        }
    }

    // 确保目录可写
    QFileInfo savePathInfo(savePath);
    if (!savePathInfo.isWritable()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("保存目录不可写: %1").arg(savePath));
        return false;
    }

    // 更新保存路径
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_statistics.savePath = savePath;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存目录最终确定为: %1").arg(savePath));
    return true;
}

void FileManager::updateStatistics(uint64_t bytesWritten)
{
    std::lock_guard<std::mutex> lock(m_statsMutex);

    // 更新总字节数
    m_statistics.totalBytes += bytesWritten;

    // 更新保存速率
    qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed > 200) { // 每200ms更新一次速率
        uint64_t bytesDelta = m_statistics.totalBytes - m_lastSavedBytes;
        double mbPerSecond = (bytesDelta * 1000.0) / (elapsed * 1024.0 * 1024.0);

        // 使用简单的移动平均进行平滑处理
        const double alpha = 0.3;
        m_statistics.saveRate = alpha * mbPerSecond + (1.0 - alpha) * m_statistics.saveRate;

        // 重置基准值
        m_lastSavedBytes = m_statistics.totalBytes;
        m_speedTimer.restart();

        // 发送进度更新信号
        emit signal_FSM_saveProgressUpdated(m_statistics);
    }
}

bool FileManager::saveMetadata()
{
    if (!m_saveParams.saveMetadata) {
        return true;
    }

    // 创建元数据文件名
    QString metadataPath = m_statistics.savePath + "/metadata.json";

    // 创建元数据对象
    QJsonObject metadata;
    metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["totalBytes"] = static_cast<double>(m_statistics.totalBytes);
    metadata["fileCount"] = static_cast<int>(m_statistics.fileCount);
    metadata["format"] = static_cast<int>(m_saveParams.format);

    // 添加其他选项
    QJsonObject options;
    for (auto it = m_saveParams.options.begin(); it != m_saveParams.options.end(); ++it) {
        options[it.key()] = QJsonValue::fromVariant(it.value());
    }
    metadata["options"] = options;

    // 写入文件
    QJsonDocument doc(metadata);
    QFile file(metadataPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开元数据文件: %1").arg(metadataPath));
        return false;
    }

    file.write(doc.toJson());
    file.close();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("元数据已保存: %1").arg(metadataPath));
    return true;
}

// #define FILE_SAVE_DBG
void FileManager::saveThreadFunction()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存线程已启动"));

    // Set thread priority
    QThread::currentThread()->setPriority(QThread::LowPriority);

    // 创建局部速度计算器
    QElapsedTimer speedCalculationTimer;
    uint64_t bytesWrittenSinceLastUpdate = 0;
    speedCalculationTimer.start();

    while (m_running) {
        bool hasBatch = false;
        bool hasPacket = false;
        DataPacket packet;
        DataPacketBatch batch;

        // Wait for data ready or stop signal
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_dataReady.wait(lock, [this]() {
                return !m_running ||
                    (!m_paused && (!m_dataQueue.empty() || !m_batchQueue.empty()));
                });

            // Check if thread should exit
            if (!m_running) {
                break;
            }

            // If paused, continue waiting
            if (m_paused) {
                continue;
            }

            // Prioritize batch processing over individual packets
            if (!m_batchQueue.empty()) {
                batch = std::move(m_batchQueue.front());
                m_batchQueue.pop();
                hasBatch = true;
                LOG_INFO(LocalQTCompat::fromLocal8Bit("从队列获取数据批次: %1 个包").arg(batch.size()));
            }
            else if (!m_dataQueue.empty()) {
                packet = std::move(m_dataQueue.front());
                m_dataQueue.pop();
                hasPacket = true;
#ifdef FILE_SAVE_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("从队列获取数据包: %1 字节").arg(packet.getSize()));
#endif // FILE_SAVE_DBG
            }
            else {
                continue;
            }
        }

        // Process data (batch or single packet)
        try {
            if (hasBatch) {
                saveDataBatch(batch);
            }
            else if (hasPacket) {
                // 直接使用原始数据，不进行转换
                QByteArray rawData = QByteArray(reinterpret_cast<const char*>(packet.getData()),
                    static_cast<int>(packet.getSize()));

#ifdef FILE_SAVE_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("使用原始数据，大小: %1 字节").arg(rawData.size()));
#endif // FILE_SAVE_DBG

                // Create new file (if necessary)
                if (!m_fileWriter->isOpen() || shouldSplitFile()) {
                    m_fileWriter->close(); // 确保关闭当前文件（如果有）

                    QString filename = createFileName(packet);
                    // 确保使用RAW扩展名
                    if (!filename.endsWith(".raw", Qt::CaseInsensitive)) {
                        filename = filename.replace(QRegularExpression("\\.[^.]+$"), ".raw");
                    }

                    m_currentFilePath = m_statistics.savePath + "/" + filename;

                    if (!m_fileWriter->open(m_currentFilePath)) {
                        throw std::runtime_error(LocalQTCompat::fromLocal8Bit("无法打开文件: %1 - %2")
                            .arg(m_currentFilePath)
                            .arg(m_fileWriter->getLastError()).toStdString());
                    }

                    // Update current filename
                    {
                        std::lock_guard<std::mutex> lock(m_statsMutex);
                        m_statistics.currentFileName = filename;
                        m_statistics.fileCount++;
                        m_statistics.currentFileBytes = 0; // 重置文件大小计数
                        m_statistics.currentFileStartTime = QDateTime::currentDateTime();
                    }

                    LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建新文件: %1").arg(m_currentFilePath));
                }

                // 直接写入原始数据
                if (!rawData.isEmpty()) {
                    if (!m_fileWriter->write(rawData)) {
                        throw std::runtime_error(LocalQTCompat::fromLocal8Bit("写入文件失败: %1")
                            .arg(m_fileWriter->getLastError()).toStdString());
                    }
#ifdef FILE_SAVE_DBG
                    LOG_INFO(LocalQTCompat::fromLocal8Bit("成功写入 %1 字节原始数据").arg(rawData.size()));
#endif // FILE_SAVE_DBG

                    // 更新文件大小计数
                    {
                        std::lock_guard<std::mutex> lock(m_statsMutex);
                        m_statistics.currentFileBytes += rawData.size();
                    }

                    // 累积写入字节数用于速率计算
                    bytesWrittenSinceLastUpdate += rawData.size();

                    // 每200ms更新一次保存速率
                    if (speedCalculationTimer.elapsed() > 200) {
                        double mbPerSecond = (bytesWrittenSinceLastUpdate * 1000.0) /
                            (speedCalculationTimer.elapsed() * 1024.0 * 1024.0);

                        // 使用平滑化系数更新速率
                        const double alpha = 0.3;

                        {
                            std::lock_guard<std::mutex> lock(m_statsMutex);
                            m_statistics.saveRate = alpha * mbPerSecond + (1.0 - alpha) * m_statistics.saveRate;
                            m_statistics.lastUpdateTime = QDateTime::currentDateTime();
                        }

                        // 发送进度更新信号
                        emit signal_FSM_saveProgressUpdated(m_statistics);

                        // 重置计数器
                        bytesWrittenSinceLastUpdate = 0;
                        speedCalculationTimer.restart();
                    }
                }
                // Update statistics
                updateStatistics(rawData.size());
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("保存数据异常: %1").arg(e.what()));

            // Update status to error
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_statistics.status = SaveStatus::FS_ERROR;
                m_statistics.lastError = LocalQTCompat::fromLocal8Bit("保存数据异常: %1").arg(e.what());
            }

            // Send error signal
            emit signal_FSM_saveError(LocalQTCompat::fromLocal8Bit("保存数据异常: %1").arg(e.what()));

            // Close file and reset
            m_fileWriter->close();

            // Short pause to avoid high load in error conditions
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // Close current file
    m_fileWriter->close();

    // Save metadata
    saveMetadata();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存线程已退出"));
}
