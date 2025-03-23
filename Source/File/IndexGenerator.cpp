// Source/File/IndexGenerator.cpp
#include "IndexGenerator.h"
#include "Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>

IndexGenerator& IndexGenerator::getInstance()
{
    static IndexGenerator instance;
    return instance;
}

IndexGenerator::IndexGenerator(QObject* parent)
    : QObject(parent)
    , m_isOpen(false)
    , m_entryCount(0)
    , m_lastSavedCount(0)
{
}

IndexGenerator::~IndexGenerator() {
    saveIndex(true);
    close();
}

bool IndexGenerator::open(const QString& path)
{
    QMutexLocker locker(&m_mutex);

    if (m_isOpen) {
        close();
    }

    m_indexPath = path;

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建索引目录: %1").arg(dir.path()));
            return false;
        }
    }

    m_indexFile.setFileName(path);
    if (!m_indexFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建索引文件: %1 - %2").arg(path).arg(m_indexFile.errorString()));
        return false;
    }

    m_textStream.setDevice(&m_indexFile);

    // 写入索引文件头
    m_textStream << "# FX3 Data Index File v2.0\n";
    m_textStream << "# Format: ID,TimestampNs,PacketSize,FileOffset,FileName,BatchId,PacketIndex,Features\n";
    m_textStream << "# Created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    m_textStream.flush();

    m_isOpen = true;
    m_entryCount = 0;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引文件已创建: %1").arg(path));
    return true;
}

void IndexGenerator::close()
{
    QMutexLocker locker(&m_mutex);

    if (m_isOpen) {
        // 保存索引
        saveIndex(true);

        // 写入摘要信息
        m_textStream << "# Total Entries: " << m_entryCount << "\n";
        m_textStream << "# Closed: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";

        m_textStream.flush();
        m_indexFile.close();
        m_isOpen = false;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("索引文件已关闭，总条目数: %1").arg(m_entryCount));
    }
}

int IndexGenerator::addPacketIndex(const DataPacket& packet, uint64_t fileOffset, const QString& fileName)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isOpen) {
        return -1;
    }

    // 创建新索引条目
    PacketIndexEntry entry;
    entry.timestamp = packet.timestamp;
    entry.fileOffset = fileOffset;
    entry.size = static_cast<uint32_t>(packet.getSize());
    entry.fileName = fileName;
    entry.batchId = packet.batchId;
    entry.packetIndex = static_cast<uint32_t>(packet.getSize());

    // 添加到内存索引
    int indexId = m_indexEntries.size();
    m_indexEntries.append(entry);

    // 更新快速查找映射
    m_timestampToIndex.insert(entry.timestamp, indexId);

    // 写入索引记录
    m_textStream << indexId << ","
        << entry.timestamp << ","
        << entry.size << ","
        << entry.fileOffset << ","
        << entry.fileName << ","
        << entry.batchId << ","
        << entry.packetIndex << ","
        << "{}";  // 空特征JSON对象

    m_textStream << "\n";
    m_entryCount++;

    // 每100条记录刷新一次，平衡性能和安全
    if (m_entryCount % 100 == 0) {
        m_textStream.flush();

        // 自动保存索引
        if (m_entryCount - m_lastSavedCount >= 1000) {
            saveIndex();
        }

        // 发送更新信号
        emit indexUpdated(m_entryCount);
    }

    // 发送条目添加信号
    emit indexEntryAdded(entry);

    return indexId;
}

QVector<int> IndexGenerator::addBatchIndex(const DataPacketBatch& batch, uint64_t fileOffset, const QString& fileName)
{
    QVector<int> indexIds;

    if (batch.empty() || !m_isOpen) {
        return indexIds;
    }

    uint64_t currentOffset = fileOffset;

    for (const auto& packet : batch) {
        int id = addPacketIndex(packet, currentOffset, fileName);
        if (id >= 0) {
            indexIds.append(id);
        }
        currentOffset += packet.getSize();
    }

    return indexIds;
}

bool IndexGenerator::addFeature(int indexId, const QString& featureName, const QVariant& featureValue)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isOpen || indexId < 0 || indexId >= m_indexEntries.size()) {
        return false;
    }

    // 更新内存中的特征 - 使用std::map的方式
    m_indexEntries[indexId].features[featureName] = featureValue;

    // 更新特征到索引的映射
    if (!m_featureToIndices.contains(featureName)) {
        // 使用insert而不是[]操作符
        m_featureToIndices.insert(featureName, QVector<int>());
    }

    // 使用引用访问避免不必要的拷贝和查找
    QVector<int>& indices = m_featureToIndices[featureName];
    if (!indices.contains(indexId)) {
        indices.append(indexId);
    }

    return true;
}

QVector<PacketIndexEntry> IndexGenerator::queryIndex(const IndexQuery& query)
{
    QMutexLocker locker(&m_mutex);
    QVector<PacketIndexEntry> results;

    // 使用二分查找快速定位开始和结束范围
    int startIdx = binarySearchTimestamp(query.timestampStart, true);
    int endIdx = binarySearchTimestamp(query.timestampEnd, false);

    if (startIdx == -1 || endIdx == -1 || startIdx > endIdx) {
        return results;
    }

    // 基于时间戳范围过滤
    for (int i = startIdx; i <= endIdx; ++i) {
        const PacketIndexEntry& entry = m_indexEntries[i];

        if (entry.timestamp >= query.timestampStart && entry.timestamp <= query.timestampEnd) {
            // 检查特征过滤条件
            bool passesFilter = true;

            for (const QString& filter : query.featureFilters) {
                // 解析过滤条件，支持简单的比较操作：">", ">=", "<", "<=", "="
                QString featureName;
                QString op;
                QVariant value;

                if (filter.contains(">")) {
                    if (filter.contains(">=")) {
                        featureName = filter.section(">=", 0, 0).trimmed();
                        op = ">=";
                        value = filter.section(">=", 1).trimmed();
                    }
                    else {
                        featureName = filter.section(">", 0, 0).trimmed();
                        op = ">";
                        value = filter.section(">", 1).trimmed();
                    }
                }
                else if (filter.contains("<")) {
                    if (filter.contains("<=")) {
                        featureName = filter.section("<=", 0, 0).trimmed();
                        op = "<=";
                        value = filter.section("<=", 1).trimmed();
                    }
                    else {
                        featureName = filter.section("<", 0, 0).trimmed();
                        op = "<";
                        value = filter.section("<", 1).trimmed();
                    }
                }
                else if (filter.contains("=")) {
                    featureName = filter.section("=", 0, 0).trimmed();
                    op = "=";
                    value = filter.section("=", 1).trimmed();
                }
                else {
                    continue; // 无效过滤器
                }

                // 检查特征是否存在 - 使用QMap的contains
                if (!entry.features.contains(featureName)) {
                    passesFilter = false;
                    break;
                }

                // 比较特征值
                QVariant featureValue = entry.features.value(featureName);

                if (op == ">") {
                    if (featureValue.metaType().id() == QMetaType::Double) {
                        if (featureValue.toDouble() <= value.toDouble()) {
                            passesFilter = false;
                            break;
                        }
                    }
                    else if (featureValue.metaType().id() == QMetaType::Int) {
                        if (featureValue.toInt() <= value.toInt()) {
                            passesFilter = false;
                            break;
                        }
                    }
                }
                else if (op == ">=") {
                    // 类似逻辑...
                }
                // 其他操作符的比较逻辑...
            }

            if (passesFilter) {
                results.append(entry);
            }
        }
    }

    // 排序结果
    if (query.descending) {
        std::sort(results.begin(), results.end(), [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
            return a.timestamp > b.timestamp;
            });
    }
    else {
        std::sort(results.begin(), results.end(), [](const PacketIndexEntry& a, const PacketIndexEntry& b) {
            return a.timestamp < b.timestamp;
            });
    }

    // 限制结果数量
    if (query.limit > 0 && results.size() > query.limit) {
        results.resize(query.limit);
    }

    return results;
}

PacketIndexEntry IndexGenerator::findClosestPacket(uint64_t timestamp)
{
    QMutexLocker locker(&m_mutex);

    if (m_indexEntries.isEmpty()) {
        return PacketIndexEntry(); // 返回空条目
    }

    // 二分查找最接近的时间戳
    int left = 0;
    int right = m_indexEntries.size() - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (m_indexEntries[mid].timestamp == timestamp) {
            return m_indexEntries[mid];
        }

        if (m_indexEntries[mid].timestamp < timestamp) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    // 如果没有精确匹配，找出最接近的
    if (left >= m_indexEntries.size()) {
        return m_indexEntries[m_indexEntries.size() - 1];
    }

    if (left == 0) {
        return m_indexEntries[0];
    }

    // 比较哪个更接近
    uint64_t diffLeft = timestamp - m_indexEntries[left - 1].timestamp;
    uint64_t diffRight = m_indexEntries[left].timestamp - timestamp;

    return (diffLeft < diffRight) ? m_indexEntries[left - 1] : m_indexEntries[left];
}

QVector<PacketIndexEntry> IndexGenerator::getPacketsInRange(uint64_t startTime, uint64_t endTime)
{
    IndexQuery query;
    query.timestampStart = startTime;
    query.timestampEnd = endTime;

    return queryIndex(query);
}

bool IndexGenerator::saveIndex(bool forceSave)
{
    QMutexLocker locker(&m_mutex);

    if (!m_isOpen) {
        return false;
    }

    // 检查是否需要保存
    if (!forceSave && m_entryCount - m_lastSavedCount < 1000) {
        return true; // 没有足够的新条目，跳过保存
    }

    // 创建JSON文档
    QJsonArray entriesArray;

    for (const PacketIndexEntry& entry : m_indexEntries) {
        QJsonObject entryObj;
        entryObj["timestamp"] = QString::number(entry.timestamp); // 使用字符串避免大整数精度问题
        entryObj["fileOffset"] = QString::number(entry.fileOffset);
        entryObj["size"] = static_cast<int>(entry.size);
        entryObj["fileName"] = entry.fileName;
        entryObj["batchId"] = static_cast<int>(entry.batchId);
        entryObj["packetIndex"] = static_cast<int>(entry.packetIndex);

        // 添加特征
        QJsonObject featuresObj;
        QHashIterator<QString, QVariant> it(entry.features);
        while (it.hasNext()) {
            it.next();
            const QString& key = it.key();
            const QVariant& val = it.value();
            QJsonValue value;

            int typeId = val.metaType().id(); // 使用 metaType().id() 替代 type()

            switch (typeId) {
            case QMetaType::Double:
                value = val.toDouble();
                break;
            case QMetaType::Int:
                value = val.toInt();
                break;
            case QMetaType::Bool:
                value = val.toBool();
                break;
            case QMetaType::QString:
                value = val.toString();
                break;
            case QMetaType::QVariantList:
                value = QJsonArray::fromVariantList(val.toList());
                break;
            default:
                value = val.toString();
                break;
            }

            featuresObj.insert(key, value);
        }

        entryObj["features"] = featuresObj;
        entriesArray.append(entryObj);
    }

    QJsonObject rootObj;
    rootObj["version"] = "2.0";
    rootObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["entries"] = entriesArray;

    QJsonDocument doc(rootObj);

    // 保存到JSON文件
    QString jsonPath = m_indexPath + ".json";
    QFile jsonFile(jsonPath);

    if (!jsonFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开JSON索引文件: %1").arg(jsonFile.errorString()));
        return false;
    }

    jsonFile.write(doc.toJson());
    jsonFile.close();

    m_lastSavedCount = m_entryCount;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引已保存到: %1，共 %2 条记录").arg(jsonPath).arg(m_entryCount));

    return true;
}

bool IndexGenerator::loadIndex(const QString& path)
{
    QMutexLocker locker(&m_mutex);

    // 清空当前索引
    clearIndex();

    // 尝试加载JSON索引
    QString jsonPath = path + ".json";
    QFile jsonFile(jsonPath);

    if (!jsonFile.exists()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("JSON索引文件不存在: %1").arg(jsonPath));
        return false;
    }

    if (!jsonFile.open(QIODevice::ReadOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开JSON索引文件: %1").arg(jsonFile.errorString()));
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();

    if (doc.isNull() || !doc.isObject()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的JSON索引文件"));
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray entriesArray = rootObj["entries"].toArray();

    for (int i = 0; i < entriesArray.size(); ++i) {
        QJsonObject entryObj = entriesArray[i].toObject();

        PacketIndexEntry entry;
        entry.timestamp = entryObj["timestamp"].toString().toULongLong();
        entry.fileOffset = entryObj["fileOffset"].toString().toULongLong();
        entry.size = entryObj["size"].toInt();
        entry.fileName = entryObj["fileName"].toString();
        entry.batchId = entryObj["batchId"].toInt();
        entry.packetIndex = entryObj["packetIndex"].toInt();

        // 加载特征
        QJsonObject featuresObj = entryObj["features"].toObject();
        for (auto it = featuresObj.begin(); it != featuresObj.end(); ++it) {
            QVariant value;

            if (it.value().isDouble()) {
                value = it.value().toDouble();
            }
            else if (it.value().isBool()) {
                value = it.value().toBool();
            }
            else if (it.value().isString()) {
                value = it.value().toString();
            }
            else if (it.value().isArray()) {
                value = it.value().toArray().toVariantList();
            }
            else {
                value = it.value().toString();
            }

            entry.features[it.key()] = value;
        }

        // 添加到内存索引
        int indexId = m_indexEntries.size();
        m_indexEntries.append(entry);

        // 更新快速查找映射
        m_timestampToIndex.insert(entry.timestamp, indexId);

        // 更新特征映射
        QHashIterator<QString, QVariant> featureIt(entry.features);
        while (featureIt.hasNext()) {
            featureIt.next();
            const QString& key = featureIt.key();
            if (!m_featureToIndices.contains(key)) {
                m_featureToIndices.insert(key, QVector<int>());
            }
            m_featureToIndices[key].append(indexId);
        }
    }

    m_entryCount = m_indexEntries.size();
    m_lastSavedCount = m_entryCount;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("成功加载索引从: %1，共 %2 条记录").arg(jsonPath).arg(m_entryCount));

    // 打开索引文件用于附加
    m_indexPath = path;
    m_indexFile.setFileName(path);

    if (!m_indexFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开索引文件用于附加: %1").arg(m_indexFile.errorString()));
        return false;
    }

    m_textStream.setDevice(&m_indexFile);
    m_isOpen = true;

    // 发送更新信号
    emit indexUpdated(m_entryCount);

    return true;
}

QVector<PacketIndexEntry> IndexGenerator::getAllIndexEntries()
{
    QMutexLocker locker(&m_mutex);
    return m_indexEntries;
}

int IndexGenerator::getIndexCount()
{
    QMutexLocker locker(&m_mutex);
    return m_entryCount;
}

void IndexGenerator::clearIndex()
{
    QMutexLocker locker(&m_mutex);

    m_indexEntries.clear();
    m_timestampToIndex.clear();
    m_featureToIndices.clear();
    m_entryCount = 0;
    m_lastSavedCount = 0;
}

void IndexGenerator::flush()
{
    QMutexLocker locker(&m_mutex);

    if (m_isOpen) {
        m_textStream.flush();
        saveIndex(true);
    }
}

int IndexGenerator::binarySearchTimestamp(uint64_t timestamp, bool findFirst) const
{
    if (m_indexEntries.isEmpty()) {
        return -1;
    }

    int left = 0;
    int right = m_indexEntries.size() - 1;
    int result = -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (m_indexEntries[mid].timestamp < timestamp) {
            left = mid + 1;
        }
        else if (m_indexEntries[mid].timestamp > timestamp) {
            right = mid - 1;
        }
        else {
            result = mid;
            if (findFirst) {
                right = mid - 1; // 继续查找第一个匹配的
            }
            else {
                left = mid + 1;  // 继续查找最后一个匹配的
            }
        }
    }

    if (result != -1) {
        return result;
    }

    // 如果没有精确匹配，返回合适的索引
    return findFirst ? left : right;
}