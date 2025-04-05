// Source/Analysis/DataAccessService.cpp
#include "DataAccessService.h"
#include "Logger.h"
#include <QtConcurrent/QtConcurrent>
#include <QElapsedTimer>

DataAccessService& DataAccessService::getInstance()
{
    static DataAccessService instance;
    return instance;
}

DataAccessService::DataAccessService(QObject* parent)
    : QObject(parent)
    , m_dataCache(10 * 1024 * 1024) // 默认10MB缓存
    , m_readTimeout(5000) // 5秒超时
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据访问服务已初始化，缓存大小: 10MB"));

    // 默认使用 IndexGenerator 实现
    m_indexAccess = std::make_shared<IndexGeneratorAccess>();

    // 初始化文件检查定时器
    m_fileCleanupTimer = new QTimer(this);
    connect(m_fileCleanupTimer, &QTimer::timeout, this, &DataAccessService::slot_DT_ACC_checkAndCleanupUnusedFiles);
    m_fileCleanupTimer->start(60000); // 每分钟检查一次
}

DataAccessService::~DataAccessService()
{
    // 关闭所有打开的文件
    QMutexLocker locker(&m_fileMutex);
    for (auto& fileCache : m_openFiles) {
        if (fileCache.file && fileCache.file->isOpen()) {
            fileCache.file->close();
            delete fileCache.file;
        }
    }
    m_openFiles.clear();

    if (m_fileCleanupTimer) {
        m_fileCleanupTimer->stop();
        delete m_fileCleanupTimer;
    }
}

QByteArray DataAccessService::readPacketData(const PacketIndexEntry& entry)
{
    QElapsedTimer timer;
    timer.start();
    m_stats.totalReads++;

    try {
        // 生成缓存键
        QString cacheKey = generateCacheKey(entry.fileName, entry.fileOffset, entry.size);

        // 检查缓存
        {
            QMutexLocker locker(&m_cacheMutex);
            QByteArray* cachedData = m_dataCache.object(cacheKey);
            if (cachedData) {
                m_stats.cacheHits++;
                return *cachedData;
            }
            m_stats.cacheMisses++;
        }

        // 检查文件是否可读
        if (!isFileReadable(entry.fileName)) {
            m_stats.readErrors++;
            emit signal_DT_ACC_dataReadError(LocalQTCompat::fromLocal8Bit("文件不可读: %1").arg(entry.fileName));
            return QByteArray();
        }

        // 打开文件
        QFile* file = getOrOpenFile(entry.fileName);
        if (!file) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(entry.fileName));
            m_stats.readErrors++;
            emit signal_DT_ACC_dataReadError(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(entry.fileName));
            return QByteArray();
        }

        // 增加超时和重试机制
        int retryCount = 0;
        const int MAX_RETRIES = 3;
        QByteArray data;

        while (retryCount < MAX_RETRIES) {
            // 定位到偏移位置并读取数据
            QMutexLocker locker(&m_fileMutex);

            if (!file->seek(entry.fileOffset)) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位到文件偏移位置: %1 在 %2")
                    .arg(entry.fileOffset).arg(entry.fileName));

                // 检查是否超时
                if (timer.elapsed() > m_readTimeout) {
                    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件操作超时"));
                    break;
                }

                retryCount++;
                LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试重试 (%1/%2)").arg(retryCount).arg(MAX_RETRIES));
                QThread::msleep(100); // 短暂延迟后重试
                continue;
            }

            // 读取数据
            data = file->read(entry.size);

            // 检查是否超时或数据不完整
            if (timer.elapsed() > m_readTimeout || data.size() != static_cast<int>(entry.size)) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据失败或超时: 应为 %1 字节，实际读取 %2 字节")
                    .arg(entry.size).arg(data.size()));

                retryCount++;
                LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试重试 (%1/%2)").arg(retryCount).arg(MAX_RETRIES));
                QThread::msleep(100); // 短暂延迟后重试
                continue;
            }

            // 成功读取数据，添加到缓存并返回
            {
                QMutexLocker cacheLocker(&m_cacheMutex);
                m_dataCache.insert(cacheKey, new QByteArray(data));
            }

            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("从文件读取数据: %1 偏移 %2, 大小 %3 字节")
                .arg(entry.fileName).arg(entry.fileOffset).arg(entry.size));

            // 更新统计
            m_stats.totalReadTime += timer.elapsed();

            // 发送信号
            emit signal_DT_ACC_dataReadComplete(entry.timestamp, data);
            return data;
        }

        // 所有重试失败
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据失败，已达最大重试次数"));
        m_stats.readErrors++;
        emit signal_DT_ACC_dataReadError(LocalQTCompat::fromLocal8Bit("读取数据重试失败"));
        return QByteArray();
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据异常: %1").arg(e.what()));
        m_stats.readErrors++;
        emit signal_DT_ACC_dataReadError(LocalQTCompat::fromLocal8Bit("读取数据异常: %1").arg(e.what()));
        return QByteArray();
    }
}

QByteArray DataAccessService::readPacketByTimestamp(uint64_t timestamp)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("开始读取数据包，时间戳: %1").arg(timestamp));

    if (!m_indexAccess) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("未设置索引访问接口"));
        return QByteArray();
    }

    // 查找最接近的数据包
    PacketIndexEntry entry = m_indexAccess->findClosestPacket(timestamp);

    // 如果找到有效条目，读取数据
    if (!entry.fileName.isEmpty() && entry.size > 0) {
        return readPacketData(entry);
    }

    LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法找到时间戳 %1 对应的数据包").arg(timestamp));
    return QByteArray();
}

bool DataAccessService::readPacketsInRange(uint64_t startTime, uint64_t endTime,
    std::function<void(const QByteArray&, const PacketIndexEntry&)> callback)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("读取时间范围内数据包: %1 - %2").arg(startTime).arg(endTime));

    if (!m_indexAccess) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("未设置索引访问接口"));
        return false;
    }

    // 直接使用增强的接口方法获取时间范围内的索引条目
    QVector<PacketIndexEntry> entries = m_indexAccess->getPacketsInRange(startTime, endTime);

    if (entries.isEmpty()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("指定时间范围内没有数据包: %1 - %2")
            .arg(startTime)
            .arg(endTime));
        return false;
    }

    // 按照文件名分组，减少文件切换
    QMap<QString, QVector<PacketIndexEntry>> fileGroups;
    for (const PacketIndexEntry& entry : entries) {
        fileGroups[entry.fileName].append(entry);
    }

    // 处理每个文件的数据包
    for (auto it = fileGroups.begin(); it != fileGroups.end(); ++it) {
        const QString& fileName = it.key();
        QVector<PacketIndexEntry>& fileEntries = it.value();

        // 打开文件
        QFile* file = getOrOpenFile(fileName);
        if (!file) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(fileName));
            continue;
        }

        // 按照偏移排序，优化读取性能
        std::sort(fileEntries.begin(), fileEntries.end(),
            [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
                return a.fileOffset < b.fileOffset;
            });

        // 读取每个数据包
        {
            QMutexLocker locker(&m_fileMutex);
            for (const PacketIndexEntry& entry : fileEntries) {
                // 定位到偏移位置
                if (!file->seek(entry.fileOffset)) {
                    LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位到文件偏移位置: %1 在 %2")
                        .arg(entry.fileOffset)
                        .arg(fileName));
                    continue;
                }

                // 读取数据
                QByteArray data = file->read(entry.size);
                if (data.size() != static_cast<int>(entry.size)) {
                    LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据大小不匹配: 应为 %1 字节，实际读取 %2 字节")
                        .arg(entry.size)
                        .arg(data.size()));
                    continue;
                }

                // 如果需要，更新缓存
                {
                    QMutexLocker cacheLocker(&m_cacheMutex);
                    QString cacheKey = generateCacheKey(entry.fileName, entry.fileOffset, entry.size);
                    if (!m_dataCache.contains(cacheKey)) {
                        m_dataCache.insert(cacheKey, new QByteArray(data));
                    }
                }

                // 调用回调函数
                callback(data, entry);
            }
        }
    }

    return true;
}

QFuture<QVector<QByteArray>> DataAccessService::readPacketsInRangeAsync(uint64_t startTime, uint64_t endTime)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("异步读取时间范围内数据包: %1 - %2").arg(startTime).arg(endTime));

    return QtConcurrent::run([this, startTime, endTime]() {
        QVector<QByteArray> results;

        // 直接使用增强的接口方法获取范围内的索引条目
        QVector<PacketIndexEntry> entries = m_indexAccess->getPacketsInRange(startTime, endTime);

        // 读取每个数据包
        for (const PacketIndexEntry& entry : entries) {
            QByteArray data = readPacketData(entry);
            if (!data.isEmpty()) {
                results.append(data);
            }
        }

        return results;
        });
}

QVector<QByteArray> DataAccessService::readPacketsByCommandType(uint8_t commandType, int limit)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("读取指定命令类型的数据包: 0x%1").arg(commandType, 2, 16, QChar('0')));

    QVector<QByteArray> results;

    if (!m_indexAccess) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("未设置索引访问接口"));
        return results;
    }

    // 获取特定命令类型的索引条目
    QVector<PacketIndexEntry> entries = m_indexAccess->findPacketsByCommandType(commandType, limit);

    if (entries.isEmpty()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("未找到指定命令类型的数据包: 0x%1").arg(commandType, 2, 16, QChar('0')));
        return results;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("找到 %1 个命令类型为 0x%2 的数据包").arg(entries.size()).arg(commandType, 2, 16, QChar('0')));

    // 按文件分组并排序
    QMap<QString, QVector<PacketIndexEntry>> fileGroups;
    for (const PacketIndexEntry& entry : entries) {
        fileGroups[entry.fileName].append(entry);
    }

    // 处理每个文件
    for (auto it = fileGroups.begin(); it != fileGroups.end(); ++it) {
        const QString& fileName = it.key();
        QVector<PacketIndexEntry>& fileEntries = it.value();

        // 排序以优化读取性能
        std::sort(fileEntries.begin(), fileEntries.end(),
            [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
                return a.fileOffset < b.fileOffset;
            });

        // 打开文件并读取数据
        QFile* file = getOrOpenFile(fileName);
        if (!file) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(fileName));
            continue;
        }

        // 读取数据
        QMutexLocker locker(&m_fileMutex);
        for (const PacketIndexEntry& entry : fileEntries) {
            if (!file->seek(entry.fileOffset)) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位到文件偏移位置: %1 在 %2")
                    .arg(entry.fileOffset).arg(fileName));
                continue;
            }

            QByteArray data = file->read(entry.size);
            if (data.size() != static_cast<int>(entry.size)) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据大小不匹配: 应为 %1 字节，实际读取 %2 字节")
                    .arg(entry.size).arg(data.size()));
                continue;
            }

            results.append(data);
        }
    }

    return results;
}

QFuture<QByteArray> DataAccessService::readPacketDataAsync(const PacketIndexEntry& entry)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("异步读取数据包: %1, 偏移 %2").arg(entry.fileName).arg(entry.fileOffset));

    return QtConcurrent::run([this, entry]() {
        return readPacketData(entry);
        });
}

bool DataAccessService::queryAndReadPackets(const IndexQuery& query,
    std::function<void(const QByteArray&, const PacketIndexEntry&)> callback)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("按条件查询读取数据包: %1 - %2").arg(query.timestampStart).arg(query.timestampEnd));

    if (!m_indexAccess) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("未设置索引访问接口"));
        return false;
    }

    // 使用增强的查询接口
    QVector<PacketIndexEntry> entries = m_indexAccess->queryIndex(query);

    if (entries.isEmpty()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("没有符合条件的数据包"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("查询到 %1 个符合条件的数据包").arg(entries.size()));

    // 执行与readPacketsInRange类似的数据读取流程
    QMap<QString, QVector<PacketIndexEntry>> fileGroups;
    for (const PacketIndexEntry& entry : entries) {
        fileGroups[entry.fileName].append(entry);
    }

    // 处理每个文件的数据包
    for (auto it = fileGroups.begin(); it != fileGroups.end(); ++it) {
        const QString& fileName = it.key();
        QVector<PacketIndexEntry>& fileEntries = it.value();

        // 按照偏移排序，优化读取性能
        std::sort(fileEntries.begin(), fileEntries.end(),
            [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
                return a.fileOffset < b.fileOffset;
            });

        // 打开文件
        QFile* file = getOrOpenFile(fileName);
        if (!file) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(fileName));
            continue;
        }

        // 读取每个数据包
        {
            QMutexLocker locker(&m_fileMutex);
            for (const PacketIndexEntry& entry : fileEntries) {
                // 定位到偏移位置
                if (!file->seek(entry.fileOffset)) {
                    LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法定位到文件偏移位置: %1 在 %2")
                        .arg(entry.fileOffset)
                        .arg(fileName));
                    continue;
                }

                // 读取数据
                QByteArray data = file->read(entry.size);
                if (data.size() != static_cast<int>(entry.size)) {
                    LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据大小不匹配: 应为 %1 字节，实际读取 %2 字节")
                        .arg(entry.size)
                        .arg(data.size()));
                    continue;
                }

                // 调用回调函数
                callback(data, entry);
            }
        }
    }

    return true;
}

QFuture<QVector<QPair<QByteArray, PacketIndexEntry>>> DataAccessService::queryAndReadPacketsAsync(const IndexQuery& query)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("异步按条件查询读取数据包"));

    return QtConcurrent::run([this, query]() {
        QVector<QPair<QByteArray, PacketIndexEntry>> results;

        // 获取查询结果
        QVector<PacketIndexEntry> entries = m_indexAccess->queryIndex(query);
        if (entries.isEmpty()) {
            return results;
        }

        // 按文件分组
        QMap<QString, QVector<PacketIndexEntry>> fileGroups;
        for (const PacketIndexEntry& entry : entries) {
            fileGroups[entry.fileName].append(entry);
        }

        // 处理每个文件
        for (auto it = fileGroups.begin(); it != fileGroups.end(); ++it) {
            const QString& fileName = it.key();
            QVector<PacketIndexEntry>& fileEntries = it.value();

            // 排序优化
            std::sort(fileEntries.begin(), fileEntries.end(),
                [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
                    return a.fileOffset < b.fileOffset;
                });

            // 打开文件
            QFile* file = getOrOpenFile(fileName);
            if (!file) continue;

            // 读取数据
            QMutexLocker locker(&m_fileMutex);
            for (const PacketIndexEntry& entry : fileEntries) {
                if (!file->seek(entry.fileOffset)) continue;

                QByteArray data = file->read(entry.size);
                if (data.size() != static_cast<int>(entry.size)) continue;

                results.append(qMakePair(data, entry));
            }
        }

        return results;
        });
}

void DataAccessService::slot_DT_ACC_checkAndCleanupUnusedFiles()
{
    QMutexLocker locker(&m_fileMutex);
    QDateTime currentTime = QDateTime::currentDateTime();

    // 关闭超过5分钟未使用的文件
    QStringList filesToRemove;
    for (auto it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
        if (it.value().lastAccess.secsTo(currentTime) > 300) { // 5分钟
            if (it.value().file) {
                it.value().file->close();
                delete it.value().file;
            }
            filesToRemove.append(it.key());
        }
    }

    // 从映射中移除
    for (const QString& file : filesToRemove) {
        m_openFiles.remove(file);
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("关闭长时间未访问的文件: %1").arg(file));
    }
}

bool DataAccessService::isFileReadable(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件不存在: %1").arg(filePath));
        return false;
    }

    if (!fileInfo.isReadable()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件不可读: %1").arg(filePath));
        return false;
    }

    return true;
}

void DataAccessService::setCacheSize(int sizeInMB)
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.setMaxCost(sizeInMB * 1024 * 1024);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据缓存大小设置为 %1 MB").arg(sizeInMB));
}

void DataAccessService::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.clear();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据缓存已清除"));
}

DataAccessService::PerformanceStats DataAccessService::getPerformanceStats() const
{
    return m_stats;
}

void DataAccessService::resetPerformanceStats()
{
    m_stats = PerformanceStats();
}

QVector<double> DataAccessService::getChannelData(int channel, int startIndex, int length)
{
    QVector<double> result;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取通道数据: 通道=%2, 起始=%3, 长度=%4")
        .arg(channel).arg(startIndex).arg(length));

    if (channel < 0 || channel > 3) {
        LOG_ERROR(QString("无效的通道索引: %1").arg(channel));
        return result;
    }

    try {
        // 文件应该从文件缓存获取, TODO
        QFile* file = nullptr;
        LOG_ERROR(QString("需要从文件缓存管理器获取文件描述符"));
        return {};
        QMutexLocker locker(&m_fileMutex);

        // 定位到文件起始位置
        if (!file->seek(startIndex)) {
            LOG_ERROR(QString("无法定位到文件偏移位置: %1")
                .arg(startIndex));
            return result;
        }

        // 读取指定长度的数据
        QByteArray data = file->read(length);

        if (data.size() < length) {
            LOG_WARN(QString("读取的数据长度小于请求长度: %1 < %2")
                .arg(data.size())
                .arg(length));
        }

        // 提取指定通道的数据
        result = extractChannelData(data, channel);

        return result;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("读取通道数据异常: %1").arg(e.what()));
        return result;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道数据获取完成: 大小=%1").arg(result.size()));
}

QVector<double> DataAccessService::extractChannelData(const QByteArray& data, int channel)
{
    QVector<double> result;

    if (data.isEmpty() || channel < 0 || channel > 3) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("提取通道数据失败：无效的数据或通道索引%1").arg(channel));
        return result;
    }

    // 为结果分配空间
    result.reserve(data.size());

    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始提取通道%1数据，数据大小：%2字节")
        .arg(channel).arg(data.size()));

    // 解析数据，提取指定通道的位
    for (int i = 0; i < data.size(); ++i) {
        uint8_t byteValue = static_cast<uint8_t>(data.at(i));

        // 根据通道索引提取不同的位
        double value = 0.0;

        switch (channel) {
        case 0: // BYTE0：提取最低2位
            value = byteValue & 0x03;
            break;
        case 1: // BYTE1：提取中间2位
            value = (byteValue >> 2) & 0x03;
            break;
        case 2: // BYTE2：提取高2位
            value = (byteValue >> 4) & 0x03;
            break;
        case 3: // BYTE3：提取最高2位
            value = (byteValue >> 6) & 0x03;
            break;
        }

        // 归一化到0-1范围，大于0的值都视为高电平
        value = value > 0 ? 1.0 : 0.0;

        result.append(value);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1数据提取完成，提取了%2个数据点")
        .arg(channel).arg(result.size()));

    return result;
}

DataAccessService::WaveformData DataAccessService::readWaveformData(uint64_t packetIndex)
{
    WaveformData result;
    result.isValid = false;

    try {
        // 查找最接近的数据包
        PacketIndexEntry entry;

        // 这里需要实现一个根据索引查找数据包的方法
        // 例如：entry = findPacketByIndex(packetIndex);

        // 或者使用时间戳查找（如果有时间戳到索引的映射）
        // uint64_t timestamp = indexToTimestamp(packetIndex);
        // entry = findClosestPacket(timestamp);

        if (entry.fileName.isEmpty() || entry.size == 0) {
            LOG_ERROR(QString("未找到索引 %1 对应的数据包").arg(packetIndex));
            return result;
        }

        // 读取数据包
        QByteArray data = readPacketData(entry);

        if (data.isEmpty()) {
            LOG_ERROR(QString("读取数据包失败, 索引: %1").arg(packetIndex));
            return result;
        }

        // 设置索引数据
        result.indexData.reserve(data.size());
        for (int i = 0; i < data.size(); ++i) {
            result.indexData.append(i);
        }

        // 设置通道数据
        result.channelData.resize(4);
        for (int ch = 0; ch < 4; ++ch) {
            result.channelData[ch] = extractChannelData(data, ch);
        }

        result.timestamp = entry.timestamp;
        result.isValid = true;

        return result;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("读取波形数据异常: %1").arg(e.what()));
        return result;
    }
}
 
void DataAccessService::setReadTimeout(int milliseconds)
{
    m_readTimeout = milliseconds;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("读取超时设置为 %1 毫秒").arg(milliseconds));
}

QFile* DataAccessService::getOrOpenFile(const QString& filePath)
{
    QMutexLocker locker(&m_fileMutex);

    // 检查文件是否已打开
    if (m_openFiles.contains(filePath)) {
        FileCache& fileCache = m_openFiles[filePath];
        fileCache.lastAccess = QDateTime::currentDateTime();
        return fileCache.file;
    }

    // 打开新文件
    QFile* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1 - %2")
            .arg(filePath)
            .arg(file->errorString()));
        delete file;
        return nullptr;
    }

    // 如果打开的文件太多，关闭最早访问的文件
    if (m_openFiles.size() >= MAX_OPEN_FILES) {
        QString oldestFile;
        QDateTime oldestTime = QDateTime::currentDateTime();

        for (auto it = m_openFiles.begin(); it != m_openFiles.end(); ++it) {
            if (it.value().lastAccess < oldestTime) {
                oldestTime = it.value().lastAccess;
                oldestFile = it.key();
            }
        }

        if (!oldestFile.isEmpty()) {
            if (m_openFiles[oldestFile].file) {
                m_openFiles[oldestFile].file->close();
                delete m_openFiles[oldestFile].file;
            }
            m_openFiles.remove(oldestFile);
            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("关闭最早访问的文件: %1").arg(oldestFile));
        }
    }

    // 添加到打开文件缓存
    FileCache fileCache;
    fileCache.file = file;
    fileCache.path = filePath;
    fileCache.lastAccess = QDateTime::currentDateTime();
    m_openFiles[filePath] = fileCache;

    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("打开文件: %1").arg(filePath));

    return file;
}

QString DataAccessService::generateCacheKey(const QString& filename, uint64_t offset, uint32_t size)
{
    return QString("%1:%2:%3").arg(filename).arg(offset).arg(size);
}