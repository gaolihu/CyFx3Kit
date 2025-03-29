// Source/File/DataAccessService.cpp
#include "DataAccessService.h"
#include "Logger.h"
#include <QtConcurrent/QtConcurrent>

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
                LOG_DEBUG(LocalQTCompat::fromLocal8Bit("从缓存读取数据: %1").arg(cacheKey));
                m_stats.cacheHits++;
                return *cachedData;
            }
            m_stats.cacheMisses++;
        }

        // 检查文件是否可读
        if (!isFileReadable(entry.fileName)) {
            m_stats.readErrors++;
            emit dataReadError(LocalQTCompat::fromLocal8Bit("文件不可读: %1").arg(entry.fileName));
            return QByteArray();
        }

        // 打开文件
        QFile* file = getOrOpenFile(entry.fileName);
        if (!file) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(entry.fileName));
            m_stats.readErrors++;
            emit dataReadError(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(entry.fileName));
            return QByteArray();
        }

        // 增加超时和重试机制
        int retryCount = 0;
        const int MAX_RETRIES = 3;

        while (retryCount < MAX_RETRIES) {
            // 定位到偏移位置
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
            QByteArray data = file->read(entry.size);

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
            emit dataReadComplete(entry.timestamp, data);
            return data;
        }

        // 所有重试失败
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据失败，已达最大重试次数"));
        m_stats.readErrors++;
        emit dataReadError(LocalQTCompat::fromLocal8Bit("读取数据重试失败"));
        return QByteArray();
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("读取数据异常: %1").arg(e.what()));
        m_stats.readErrors++;
        emit dataReadError(LocalQTCompat::fromLocal8Bit("读取数据异常: %1").arg(e.what()));
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

    // 获取所有索引条目
    QVector<PacketIndexEntry> allEntries = m_indexAccess->getAllIndexEntries();

    // 过滤时间范围内的条目
    QVector<PacketIndexEntry> entries;
    for (const auto& entry : allEntries) {
        if (entry.timestamp >= startTime && entry.timestamp <= endTime) {
            entries.append(entry);
        }
    }

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
        const QVector<PacketIndexEntry>& fileEntries = it.value();

        // 打开文件
        QFile* file = getOrOpenFile(fileName);
        if (!file) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(fileName));
            continue;
        }

        // 按照偏移排序
        QVector<PacketIndexEntry> sortedEntries = fileEntries;
        std::sort(sortedEntries.begin(), sortedEntries.end(),
            [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
                return a.fileOffset < b.fileOffset;
            });

        // 读取每个数据包
        {
            QMutexLocker locker(&m_fileMutex);
            for (const PacketIndexEntry& entry : sortedEntries) {
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

QFuture<QVector<QByteArray>> DataAccessService::readPacketsInRangeAsync(uint64_t startTime, uint64_t endTime)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("异步读取时间范围内数据包: %1 - %2").arg(startTime).arg(endTime));

    return QtConcurrent::run([this, startTime, endTime]() {
        QVector<QByteArray> results;

        // 获取时间范围内的所有数据包
        QVector<PacketIndexEntry> allEntries = m_indexAccess->getAllIndexEntries();

        // 过滤时间范围内的条目
        QVector<PacketIndexEntry> entries;
        for (const auto& entry : allEntries) {
            if (entry.timestamp >= startTime && entry.timestamp <= endTime) {
                entries.append(entry);
            }
        }

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

QFuture<QByteArray> DataAccessService::readPacketDataAsync(const PacketIndexEntry& entry)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("异步读取数据包: %1, 偏移 %2").arg(entry.fileName).arg(entry.fileOffset));

    return QtConcurrent::run([this, entry]() {
        return readPacketData(entry);
        });
}

void DataAccessService::checkAndCleanupUnusedFiles()
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
    if (m_openFiles.size() >= 10) {
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