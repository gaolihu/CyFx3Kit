// Source/Analysis/IndexGenerator.cpp
#include "IndexGenerator.h"
#include "Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QRegularExpression>

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
    , m_foundPartialHeader(false)
    , m_persistentMode(true)
{
}

IndexGenerator::~IndexGenerator() {
    close();
}

bool IndexGenerator::open(const QString& path)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("打开索引文件：%1").arg(path));

    if (m_isOpen) {
        return true;
    }

    // 确保目录存在
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建索引目录: %1").arg(dir.path()));
            return false;
        }
    }

    // 直接加载已有的JSON索引或创建新的内存索引
    if (QFile::exists(path + ".json")) {
        return loadIndex(path);
    }

    // 初始化空索引
    m_indexEntries.clear();
    m_timestampToIndex.clear();
    m_entryCount = 0;
    m_lastSavedCount = 0;
    m_isOpen = true;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引文件已创建: %1").arg(path));
    return true;
}

void IndexGenerator::close()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭索引文件"));

    if (m_isOpen) {
        // 保存索引到JSON
        saveIndex(true);
        m_isOpen = false;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("索引文件已关闭，总条目数: %1").arg(m_entryCount));
    }
}

bool IndexGenerator::saveIndex(bool forceSave)
{
    if (!m_isOpen) {
        return false;
    }

    // 增加保存阈值，避免频繁IO操作
    if (!forceSave && m_entryCount - m_lastSavedCount < 10000) {
        return true; // 没有足够的新条目，跳过保存
    }

    // 创建JSON文档
    QJsonArray entriesArray;

    for (const PacketIndexEntry& entry : m_indexEntries) {
        QJsonObject entryObj;
        entryObj["timestamp"] = QString::number(entry.timestamp);
        entryObj["fileOffset"] = QString::number(entry.fileOffset);
        entryObj["size"] = static_cast<int>(entry.size);
        entryObj["fileName"] = entry.fileName;
        entryObj["batchId"] = static_cast<int>(entry.batchId);
        entryObj["packetIndex"] = static_cast<int>(entry.packetIndex);
        entryObj["commandType"] = static_cast<int>(entry.commandType);
        entryObj["sequence"] = static_cast<int>(entry.sequence);
        entryObj["isValidHeader"] = entry.isValidHeader;
        entryObj["commandDesc"] = entry.commandDesc;

        entriesArray.append(entryObj);
    }

    QJsonObject rootObj;
    rootObj["version"] = "2.1"; // 更新版本号表示索引格式变更
    rootObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["entries"] = entriesArray;

    QJsonDocument doc(rootObj);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存到索引文件：%1").arg(m_indexFileName));
    // 保存到JSON文件
    QFile jsonFile(m_basePath + m_indexFileName);

    if (!jsonFile.open(QIODevice::WriteOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开JSON索引文件: %1").arg(jsonFile.errorString()));
        return false;
    }

    // 使用紧凑格式减小文件大小
    jsonFile.write(doc.toJson(QJsonDocument::Compact));
    jsonFile.close();

    m_lastSavedCount = m_entryCount;
    return true;
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
    entry.packetIndex = packet.packetIndex;

    // 新增字段
    entry.commandType = packet.commandType;
    entry.sequence = packet.sequence;
    entry.isValidHeader = packet.isValidHeader;
    entry.commandDesc = getCommandDescription(packet.commandType);

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
        << static_cast<int>(entry.commandType) << ","
        << entry.sequence << ","
        << (entry.isValidHeader ? "1" : "0") << ","
        << entry.commandDesc;

    m_textStream << "\n";
    m_entryCount++;

    // 每500条记录刷新一次，平衡性能和安全
    if (m_entryCount % 500 == 0) {
        m_textStream.flush();
        emit indexUpdated(m_entryCount);
    }

    return indexId;
}

int IndexGenerator::addPacketIndexBatch(const std::vector<DataPacket>& packets, uint64_t startFileOffset)
{
    if (!m_isOpen || packets.empty()) {
        return 0;
    }
#if 0
    LOG_INFO(LocalQTCompat::fromLocal8Bit("批次保存索引文件，偏移：%2，大小：%3")
        .arg(startFileOffset)
        .arg(packets.size()));
#endif
    int totalAdded = 0;
    uint64_t currentOffset = startFileOffset;

    // 预先计算缓冲区的大小，减少多次分配内存
    QString entriesBuffer;
    entriesBuffer.reserve(packets.size() * 120); // 每条记录预估120个字符

    // 批量处理所有数据包
    for (const auto& packet : packets) {
        // 创建新索引条目
        PacketIndexEntry entry;
        entry.timestamp = packet.timestamp;
        entry.fileOffset = packet.offsetInFile;
        entry.size = static_cast<uint32_t>(packet.getSize());
        // entry.fileName = m_indexFileName;
        entry.batchId = packet.batchId;
        entry.packetIndex = packet.packetIndex;

        // 新增字段
        entry.commandType = packet.commandType;
        entry.sequence = packet.sequence;
        entry.isValidHeader = packet.isValidHeader;
        entry.commandDesc = getCommandDescription(packet.commandType);

        // 添加到内存索引
        int indexId = m_indexEntries.size();
        m_indexEntries.append(entry);

        // 更新快速查找映射
        m_timestampToIndex.insert(entry.timestamp, indexId);

        // 构建索引记录
        entriesBuffer.append(QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11\n")
            .arg(indexId)
            .arg(entry.timestamp)
            .arg(entry.size)
            .arg(entry.fileOffset)
            .arg(entry.fileName)
            .arg(entry.batchId)
            .arg(entry.packetIndex)
            .arg(static_cast<int>(entry.commandType))
            .arg(entry.sequence)
            .arg(entry.isValidHeader ? "1" : "0")
            .arg(entry.commandDesc));

        totalAdded++;
        currentOffset += packet.getSize();
    }

    // 一次性写入所有记录
    m_textStream << entriesBuffer;
    m_entryCount += totalAdded;

    // 检查是否需要保存索引
    if (m_entryCount - m_lastSavedCount >= 5000) {
        saveIndex(false);
    }

    // 每处理完一批数据发送一次信号
    if (totalAdded > 0) {
        emit indexUpdated(m_entryCount);
    }

    return totalAdded;
}

// #define IDXG_DBG
int IndexGenerator::parseDataStream(const uint8_t* data, size_t size, uint64_t fileOffset)
{
#ifdef IDXG_DBG
    LOG_INFO(LocalQTCompat::fromLocal8Bit("解析数据流，大小：%1字节, 偏移：%2")
        .arg(size)
        .arg(fileOffset));
#endif // IDXG_DBG

    // 检查输入有效性
    if (!data || size == 0) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("解析数据流失败：无效数据"));
        return 0;
    }

    // 确保索引文件已打开
    if (!m_isOpen && m_persistentMode) {
#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("%1打开文件，%2持久化模式").arg(m_isOpen ? "已" : "未").arg(m_persistentMode ? "" : "非"));
#endif // IDXG_DBG

        // 如果处于持久化模式但索引文件未打开，尝试使用当前会话ID打开
        if (!m_sessionId.isEmpty() && !m_basePath.isEmpty()) {
            QString indexPath = QString("%1/%2.idx").arg(m_basePath).arg(m_sessionId);
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("indexPath: %1").arg(indexPath));
#endif // IDXG_DBG
            if (!open(indexPath)) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开索引文件: %1").arg(indexPath));
                return 0;
            }
        }
        else {
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("置基本路径: %1打开文件，会话ID：%2").arg(m_sessionId).arg(m_basePath));
#endif // IDXG_DBG
            return 0;
        }
    }

    int packetsFound = 0;
    std::vector<uint8_t> buffer;

    // 处理可能的跨缓冲区头部
    if (m_foundPartialHeader && !m_lastBuffer.empty()) {
        // 合并上一个缓冲区的末尾与当前缓冲区
#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("合并上一个缓冲区数据，大小：%1字节").arg(m_lastBuffer.size()));
#endif // IDXG_DBG

        buffer.reserve(m_lastBuffer.size() + size);
        buffer.insert(buffer.end(), m_lastBuffer.begin(), m_lastBuffer.end());
        buffer.insert(buffer.end(), data, data + size);

#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("合并后缓冲区大小：%1字节").arg(buffer.size()));
#endif // IDXG_DBG

        m_lastBuffer.clear();
        m_foundPartialHeader = false;
    }
    else {
        // 只使用当前缓冲区
#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用当前缓冲区数据"));
#endif // IDXG_DBG
        buffer.assign(data, data + size);
    }

    size_t offset = 0;
    const size_t bufferSize = buffer.size();
    const size_t minPacketSize = 32; // 最小的有效包大小（头部+元数据）

    // 批处理数据包以提高性能
    std::vector<DataPacket> packetBatch;
    packetBatch.reserve(1000);

    // 打印前64字节用于调试
    if (bufferSize >= 64) {
        QString hexDump;
        for (size_t i = 0; i < 64; i++) {
            hexDump += QString("%1 ").arg(buffer[i], 2, 16, QChar('0'));
            if ((i + 1) % 16 == 0 && i < 63) hexDump += "\n";
        }
#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("缓冲区前64字节: \n%1").arg(hexDump));
#endif // IDXG_DBG
    }

    // 记录上次找到的有效包的偏移量，用于模式识别
    size_t lastValidOffset = 0;
    size_t patternDistance = 0;
    int patternMatches = 0;
    bool usingPatternMatching = false;

    // 安全计数器，防止无限循环
    size_t iterationCount = 0;
    // 最多只允许扫描一次缓冲区
    const size_t MAX_ITERATIONS = bufferSize / 4;

    // 快速终止条件
    const int EARLY_TERMINATE_PACKET_COUNT = 32; // 足够识别流数据模式

    // 在整个缓冲区查找数据包
    while (offset + minPacketSize <= bufferSize && iterationCount < MAX_ITERATIONS) {
        iterationCount++;

        // 输出迭代进度的频率
        if (iterationCount % 10000 == 0 || iterationCount == 1 || iterationCount == 100 || iterationCount == 1000) {
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("迭代进度：已进行%1次迭代，当前偏移量：%2，找到%3个数据包").
                arg(iterationCount).arg(offset).arg(packetsFound));
#endif // IDXG_DBG
        }

        // 快速终止条件：如果找到足够多的包并且模式稳定，提前结束扫描
        if (packetsFound >= EARLY_TERMINATE_PACKET_COUNT && usingPatternMatching && patternMatches >= 3) {
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("已找到%1个数据包且模式稳定，提前结束扫描").arg(packetsFound));
#endif // IDXG_DBG
            break;
        }

        // 尝试使用识别到的包模式进行跳跃式搜索
        if (usingPatternMatching && patternDistance > 0 && packetsFound > 3) {
            // 优先使用下一个预测位置
            size_t nextPredictedPosition = lastValidOffset + patternDistance;
            if (nextPredictedPosition + minPacketSize <= bufferSize) {
                offset = nextPredictedPosition;
#ifdef IDXG_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("使用模式匹配直接跳转到预测位置: %1").arg(offset));
#endif // IDXG_DBG
            }
        }

        // 查找标识序列 "00 00 00 00 99 99 99 99 00 00 00 00"
        bool foundPotentialHeader = false;

        // 检查是否有4个连续的0字节（可能的头部开始）
        if (offset + 4 <= bufferSize &&
            buffer[offset] == 0x00 && buffer[offset + 1] == 0x00 &&
            buffer[offset + 2] == 0x00 && buffer[offset + 3] == 0x00) {

            // 在接下来的20字节内搜索 "99 99 99 99" 序列
            for (size_t i = offset + 4; i <= offset + 20 && i + 4 <= bufferSize; i++) {
                if (buffer[i] == 0x99 && buffer[i + 1] == 0x99 &&
                    buffer[i + 2] == 0x99 && buffer[i + 3] == 0x99) {

                    // 找到了 "99 99 99 99"，现在检查它后面是否有 "00 00 00 00"
                    if (i + 8 <= bufferSize &&
                        buffer[i + 4] == 0x00 && buffer[i + 5] == 0x00 &&
                        buffer[i + 6] == 0x00 && buffer[i + 7] == 0x00) {

                        // 找到完整头部标记
                        foundPotentialHeader = true;

                        // 计算头部的大小（从开始的"00 00 00 00"到第二个"00 00 00 00"的结束）
                        size_t headerSize = (i + 8) - offset;

                        // 打印找到的头部信息
#ifdef IDXG_DBG
                        QString headerHex;
                        for (size_t j = 0; j < headerSize && offset + j < bufferSize; j++) {
                            headerHex += QString("%1 ").arg(buffer[offset + j], 2, 16, QChar('0'));
                        }
                        LOG_INFO(LocalQTCompat::fromLocal8Bit("偏移%1处找到潜在头，i: %2，大小: %3~[%4]")
                            .arg(offset)
                            .arg(i)
                            .arg(headerSize)
                            .arg(headerHex));
#endif // IDXG_DBG

                        // 确保有足够的空间读取元数据（至少24字节来包含XX和SC1-SC3）
                        if (offset + headerSize + 24 <= bufferSize) {
                            // 提取类型和大小信息
                            uint8_t type1 = buffer[offset + headerSize];
                            uint32_t repeat = (((uint32_t)buffer[offset + headerSize + 1]) << 16) |
                                (((uint32_t)buffer[offset + headerSize + 2]) << 8) |
                                (uint32_t)buffer[offset + headerSize + 3];

                            uint8_t type2 = buffer[offset + headerSize + 4];
                            uint32_t repeat_inv = ((uint32_t)0xFF000000) |
                                (((uint32_t)buffer[offset + headerSize + 5]) << 16) |
                                (((uint32_t)buffer[offset + headerSize + 6]) << 8) |
                                (uint32_t)buffer[offset + headerSize + 7];

                            // 提取命令类型XX和序列号SC1-SC3
                            uint8_t commandType = 0x00; // 默认值
                            uint32_t sequence = 0;
#ifdef IDXG_DBG
                            // 打印元数据字节
                            QString metaHex;
                            for (size_t j = 0; j < 8 && offset + headerSize + j < bufferSize; j++) {
                                metaHex += QString("%1 ").arg(buffer[offset + headerSize + j], 2, 16, QChar('0'));
                            }
                            LOG_INFO(LocalQTCompat::fromLocal8Bit("元数据字节: %1").arg(metaHex));
#endif // IDXG_DBG
#ifdef IDXG_DBG
                            LOG_INFO(LocalQTCompat::fromLocal8Bit("解析元数据: type1=%1, repeat=%2(0x%3), type2=%4, repeat_inv=0x%5")
                                .arg(type1)
                                .arg(repeat)
                                .arg(repeat, 8, 16, QChar('0'))
                                .arg(type2)
                                .arg(repeat_inv, 8, 16, QChar('0')));
#endif // IDXG_DBG

                            // 验证元数据
                            bool isValidMetadata = false;

                            // 检查类型匹配和重复计数有效
                            if (type1 == type2) {
                                // 检查repeat_inv是否是repeat的按位取反（允许一些变化）
                                uint32_t expected_inv = ~repeat & 0xFFFFFFFF;

#ifdef IDXG_DBG
                                LOG_INFO(LocalQTCompat::fromLocal8Bit("repeat=0x%1, ~repeat=0x%2, repeat_inv=0x%3").
                                    arg(repeat, 8, 16, QChar('0'))
                                    .arg(expected_inv, 8, 16, QChar('0'))
                                    .arg(repeat_inv, 8, 16, QChar('0')));
#endif // IDXG_DBG

                                // 允许repeat_inv与~repeat之间有小的差异（有时位操作可能不精确）
                                if (repeat_inv == expected_inv ||
                                    (repeat ^ repeat_inv) == 0xFFFFFFFF) {
                                    isValidMetadata = true;
                                }
                            }

                            if (isValidMetadata) {
                                // 一次性写入文件可提升性能
                                uint32_t dataSize = repeat * 4;

#ifdef IDXG_DBG
                                LOG_INFO(LocalQTCompat::fromLocal8Bit("有效数据包，数据大小: %1字节 (repeat=%2 * 4)").
                                    arg(dataSize).arg(repeat));
#endif // IDXG_DBG

                                // 确保数据包大小合理（设置一个上限，例如10MB）
                                if (dataSize > 10 * 1024 * 1024) {
                                    LOG_WARN(LocalQTCompat::fromLocal8Bit("数据包大小异常 (%1字节)，可能无效，跳过").arg(dataSize));
                                    // 跳过这个位置，继续搜索
                                    offset += 4;  // 至少跳过头部的第一部分
                                    break;
                                }

                                // 确保完整的数据包在缓冲区内
                                if (offset + headerSize + 8 + dataSize <= bufferSize) {
                                    // 创建数据包
                                    DataPacket packet;

                                    // 计算文件偏移量
                                    uint64_t packetOffset;
                                    if (m_lastBuffer.empty()) {
                                        packetOffset = fileOffset + offset;
                                    }
                                    else {
                                        if (offset >= m_lastBuffer.size()) {
                                            packetOffset = fileOffset + (offset - m_lastBuffer.size());
                                        }
                                        else {
                                            packetOffset = fileOffset - (m_lastBuffer.size() - offset);
                                        }
                                    }

                                    // 设置包属性
                                    packet.timestamp = QDateTime::currentMSecsSinceEpoch() * 1000000;
                                    packet.batchId = type1;
                                    packet.packetIndex = packetsFound;
                                    packet.offsetInFile = i - 4;

                                    // 设置新增字段
                                    packet.commandType = commandType;
                                    packet.sequence = sequence;
                                    packet.isValidHeader = true;  // 已验证的有效头部

#ifdef IDXG_DBG
                                    LOG_INFO(LocalQTCompat::fromLocal8Bit("创建数据包：偏移=%1，文件偏移：%2，命令类型=0x%2，序列号=0x%3").
                                        arg(packetOffset).
                                        arg(i - 4).
                                        arg(commandType, 2, 16, QChar('0')).
                                        arg(sequence, 6, 16, QChar('0')));
#endif // IDXG_DBG

                                    // 创建数据向量
                                    std::shared_ptr<std::vector<uint8_t>> dataVector = std::make_shared<std::vector<uint8_t>>();
                                    dataVector->reserve(dataSize);
                                    dataVector->insert(dataVector->begin(),
                                        &buffer[offset + headerSize + 8],
                                        &buffer[offset + headerSize + 8 + dataSize]);
                                    packet.data = dataVector;

                                    // 添加到批处理队列
                                    packetBatch.push_back(packet);
                                    packetsFound++;

#ifdef IDXG_DBG
                                    if (packetsFound % 100 == 0) {
                                        LOG_INFO(LocalQTCompat::fromLocal8Bit("已处理 %1 个数据包").arg(packetsFound));
                                    }
#endif // IDXG_DBG

                                    // 计算模式信息
                                    if (lastValidOffset > 0) {
                                        size_t currentDistance = offset - lastValidOffset;

                                        // 如果发现规律的包距离
                                        if (patternDistance == 0) {
                                            patternDistance = currentDistance;
                                            patternMatches = 1;
#ifdef IDXG_DBG
                                            LOG_INFO(LocalQTCompat::fromLocal8Bit("检测到可能的包间距模式: %1字节").arg(patternDistance));
#endif // IDXG_DBG
                                        }
                                        else if (currentDistance == patternDistance) {
                                            patternMatches++;

                                            // 如果连续找到3次相同距离，启用模式匹配跳跃
                                            if (patternMatches >= 3 && !usingPatternMatching) {
                                                usingPatternMatching = true;
#ifdef IDXG_DBG
                                                LOG_INFO(LocalQTCompat::fromLocal8Bit("启用模式匹配跳跃，包间距: %1字节").arg(patternDistance));
#endif // IDXG_DBG
                                            }
                                        }
                                        else if (usingPatternMatching) {
                                            // 模式中断，重新学习
#ifdef IDXG_DBG
                                            LOG_INFO(LocalQTCompat::fromLocal8Bit("包间距模式中断: 预期%1，实际%2").arg(patternDistance).arg(currentDistance));
#endif // IDXG_DBG
                                            patternDistance = currentDistance;
                                            patternMatches = 1;
                                        }
                                    }

                                    // 批量添加索引
                                    if (packetBatch.size() >= 1000) {
                                        addPacketIndexBatch(packetBatch, fileOffset);
#ifdef IDXG_DBG
                                        LOG_INFO(LocalQTCompat::fromLocal8Bit("批量添加了 %1 个数据包索引").arg(packetBatch.size()));
#endif // IDXG_DBG
                                        packetBatch.clear();
                                        packetBatch.reserve(1000);
                                    }

                                    lastValidOffset = offset;

                                    // 跳过已处理的数据
                                    size_t totalPacketSize = headerSize + 8 + dataSize;
                                    offset += totalPacketSize;
#ifdef IDXG_DBG
                                    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据包处理完成，新偏移量: %1").arg(offset));
#endif // IDXG_DBG

                                    // 继续处理下一个包
                                    break;
                                }
                                else {
                                    // 数据包跨越缓冲区边界，保存到下次处理
#ifdef IDXG_DBG
                                    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据包跨缓冲区边界，保存%1字节到下次处理").
                                        arg(bufferSize - offset));
#endif // IDXG_DBG
                                    m_lastBuffer.assign(buffer.begin() + offset, buffer.end());
                                    m_foundPartialHeader = true;

                                    // 直接退出循环，等待下一批数据
                                    offset = bufferSize;
                                    break;
                                }
                            }
                            else {
                                LOG_WARN(LocalQTCompat::fromLocal8Bit("元数据验证失败"));
                                // 移动到下一个可能的头部位置
                                offset += 4;
                            }
                        }
                        else {
                            // 元数据跨越缓冲区边界
#ifdef IDXG_DBG
                            LOG_INFO(LocalQTCompat::fromLocal8Bit("元数据跨缓冲区边界，保存%1字节到下次处理").
                                arg(bufferSize - offset));
#endif // IDXG_DBG
                            m_lastBuffer.assign(buffer.begin() + offset, buffer.end());
                            m_foundPartialHeader = true;
                            offset = bufferSize; // 设置offset等于bufferSize以退出循环
                            break;
                        }

                        // 如果到这里，说明头部验证失败，继续寻找下一个可能的头部
                        break;
                    }
                }
            }
        }

        // 如果没有找到有效头部，移动到下一个位置
        if (!foundPotentialHeader) {
            // 模式匹配条件下，如果验证失败则直接跳到下一个预测位置
            if (usingPatternMatching && patternMatches >= 3 && lastValidOffset > 0) {
                // 已建立稳定模式但当前预测位置未找到头部，跳到下一个预测位置
                offset = lastValidOffset + patternDistance;

                // 安全检查：确保不超出缓冲区
                if (offset >= bufferSize - minPacketSize) {
#ifdef IDXG_DBG
                    LOG_INFO(LocalQTCompat::fromLocal8Bit("预测位置 %1 接近缓冲区末尾，终止搜索").arg(offset));
#endif // IDXG_DBG
                    break;
                }

#ifdef IDXG_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("头部验证失败，跳转到下一个预测位置: %1").arg(offset));
#endif // IDXG_DBG
            }
            else if (usingPatternMatching) {
                // 如果正在使用模式匹配但模式不够稳定，跳过当前4字节
                offset += 4;
            }
            else {
                // 未使用模式匹配，使用最小步进
                offset++;
            }

            // 每处理1MB数据输出一次进度
            if (offset % (1024 * 1024) == 0) {
#ifdef IDXG_DBG
                LOG_INFO(LocalQTCompat::fromLocal8Bit("已扫描 %1 MB数据，找到 %2 个数据包").
                    arg(offset / (1024 * 1024)).arg(packetsFound));
#endif // IDXG_DBG
            }
        }

        // 检查是否已处理完整个缓冲区
        if (offset + minPacketSize > bufferSize) {
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("已接近缓冲区末尾，当前偏移量: %1，缓冲区大小: %2").
                arg(offset).arg(bufferSize));
#endif // IDXG_DBG
            break;
        }
    }

    // 安全检查 - 如果迭代次数达到上限，记录警告
    if (iterationCount >= MAX_ITERATIONS) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("达到最大迭代次数(%1)，可能存在无限循环，强制退出").arg(MAX_ITERATIONS));
    }

#ifdef IDXG_DBG
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主循环结束，总迭代次数: %1, 最终偏移量: %2/%3, 找到%4个数据包").
        arg(iterationCount).arg(offset).arg(bufferSize).arg(packetsFound));
#endif // IDXG_DBG

    // 性能统计日志
    if (packetsFound > 0 && iterationCount > 0) {
        double avgIterationsPerPacket = static_cast<double>(iterationCount) / packetsFound;
#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("性能统计：平均每个数据包需要%1次迭代").arg(avgIterationsPerPacket, 0, 'f', 2));
#endif // IDXG_DBG

        if (usingPatternMatching) {
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("模式匹配统计：模式间距=%1字节，匹配次数=%2").arg(patternDistance).arg(patternMatches));
#endif // IDXG_DBG
        }
    }

    // 处理缓冲区末尾可能的不完整头部
    if (!m_foundPartialHeader && offset < bufferSize) {
        size_t remainBytes = bufferSize - offset;
        // 如果剩余不到32字节（最小完整包大小），保存到下次处理
        if (remainBytes < 32) {
            m_lastBuffer.assign(buffer.begin() + offset, buffer.end());
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("保存缓冲区末尾%1字节到下次处理").arg(remainBytes));
#endif // IDXG_DBG
        }
        else {
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("丢弃缓冲区末尾%1字节").arg(remainBytes));
#endif // IDXG_DBG
        }
    }

    // 处理批处理队列剩余的数据包
    if (!packetBatch.empty()) {
        addPacketIndexBatch(packetBatch, fileOffset);
#ifdef IDXG_DBG
        LOG_INFO(LocalQTCompat::fromLocal8Bit("批量添加最后%1个数据包索引").arg(packetBatch.size()));
#endif // IDXG_DBG
    }

    // 保存索引
    if (packetsFound > 0) {
        if (packetsFound > 4000) {
            saveIndex(true);
            emit indexUpdated(m_entryCount);
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("找到大量数据包(%1)，已保存索引").arg(packetsFound));
#endif // IDXG_DBG
        }
        else if (m_entryCount - m_lastSavedCount >= 5000) {
            saveIndex(false);
            emit indexUpdated(m_entryCount);
#ifdef IDXG_DBG
            LOG_INFO(LocalQTCompat::fromLocal8Bit("索引条目累积到阈值，已保存索引"));
#endif // IDXG_DBG
        }
    }

#ifdef IDXG_DBG
    LOG_INFO(LocalQTCompat::fromLocal8Bit("解析数据流完成：共找到%1个数据包").arg(packetsFound));
#endif // IDXG_DBG
    return packetsFound;
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

QVector<PacketIndexEntry> IndexGenerator::queryIndex(const IndexQuery& query)
{
    QMutexLocker locker(&m_mutex);
    QVector<PacketIndexEntry> results;

    if (m_indexEntries.isEmpty()) {
        return results;
    }

    // 使用二分查找快速定位开始和结束范围
    int startIdx = binarySearchTimestamp(query.timestampStart, true);   // 下边界(首个大于等于开始时间戳的索引)
    int endIdx = binarySearchTimestamp(query.timestampEnd, false);      // 上边界(最后一个小于等于结束时间戳的索引)

    if (startIdx == -1 || endIdx == -1 || startIdx > endIdx) {
        return results;
    }

    // 基于时间戳范围过滤
    for (int i = startIdx; i <= endIdx; ++i) {
        const PacketIndexEntry& entry = m_indexEntries[i];

        if (entry.timestamp >= query.timestampStart && entry.timestamp <= query.timestampEnd) {
            // 检查特征过滤条件
            bool passesFilter = true;

            // 处理特征过滤
            if (!query.featureFilters.isEmpty()) {
                for (const QString& filter : query.featureFilters) {
                    // 支持简单的筛选条件如 "batchId=5" 或 "fileName=data.bin"
                    if (filter.contains('=')) {
                        QString field = filter.section('=', 0, 0).trimmed();
                        QString value = filter.section('=', 1).trimmed();

                        // 检查匹配条件
                        if (field == "batchId" && QString::number(entry.batchId) != value) {
                            passesFilter = false;
                            break;
                        }
                        else if (field == "fileName" && !entry.fileName.contains(value)) {
                            passesFilter = false;
                            break;
                        }
                        else if (field == "packetIndex" && QString::number(entry.packetIndex) != value) {
                            passesFilter = false;
                            break;
                        }
                        else if (field == "size" && QString::number(entry.size) != value) {
                            passesFilter = false;
                            break;
                        }
                    }
                }
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

    // 获取版本信息，用于处理不同版本的索引格式
    QString version = rootObj["version"].toString("1.0");
    bool isNewVersion = (version >= "2.1");

    LOG_INFO(LocalQTCompat::fromLocal8Bit("加载索引文件，版本: %1").arg(version));

    for (int i = 0; i < entriesArray.size(); ++i) {
        QJsonObject entryObj = entriesArray[i].toObject();

        PacketIndexEntry entry;
        entry.timestamp = entryObj["timestamp"].toString().toULongLong();
        entry.fileOffset = entryObj["fileOffset"].toString().toULongLong();
        entry.size = entryObj["size"].toInt();
        entry.fileName = entryObj["fileName"].toString();
        entry.batchId = entryObj["batchId"].toInt();
        entry.packetIndex = entryObj["packetIndex"].toInt();

        // 处理新版本字段
        if (isNewVersion) {
            entry.commandType = entryObj["commandType"].toInt();
            entry.sequence = entryObj["sequence"].toInt();
            entry.isValidHeader = entryObj["isValidHeader"].toBool();
            entry.commandDesc = entryObj["commandDesc"].toString();
        }
        else {
            // 旧版本默认值
            entry.commandType = 0;
            entry.sequence = 0;
            entry.isValidHeader = false;
            entry.commandDesc = getCommandDescription(0);
        }

        // 添加到内存索引
        int indexId = m_indexEntries.size();
        m_indexEntries.append(entry);

        // 更新快速查找映射
        m_timestampToIndex.insert(entry.timestamp, indexId);
    }

    m_entryCount = m_indexEntries.size();
    m_lastSavedCount = m_entryCount;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("成功加载索引从: %1，共 %2 条记录").arg(jsonPath).arg(m_entryCount));

    // 打开索引文件用于附加
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
    QVector<PacketIndexEntry> entriesCopy;
    {
        QMutexLocker locker(&m_mutex);
        entriesCopy = m_indexEntries;
    }
    return entriesCopy;
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

void IndexGenerator::setSessionId(const QString& sessionId)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置会话ID: %1").arg(sessionId));
    QMutexLocker locker(&m_mutex);

    if (m_sessionId == sessionId) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("会话相同: %1").arg(m_sessionId));
        return;  // 相同的会话ID，不需要改变
    }

    // 如果已有打开的索引文件，先保存并关闭
    if (m_isOpen) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("已经打开: %1").arg(m_sessionId));
        saveIndex(true);
        close();
    }

    m_sessionId = sessionId;
    m_indexFileName = m_sessionId + ".json";
    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引文件会话ID已设置: %1").arg(m_sessionId));
}

QString IndexGenerator::getSessionId()
{
    QMutexLocker locker(&m_mutex);
    return m_sessionId;
}

void IndexGenerator::setBasePath(const QString& basePath)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置基础路径: %1").arg(basePath));
    QMutexLocker locker(&m_mutex);

    // 确保路径存在
    QDir dir(basePath);
    if (!dir.exists() && !dir.mkpath(".")) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建索引存储目录: %1").arg(basePath));
        return;
    }

    m_basePath = basePath;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引文件基本路径已设置: %1").arg(m_basePath));
}

QString IndexGenerator::getBasePath()
{
    QMutexLocker locker(&m_mutex);
    return m_basePath;
}

int IndexGenerator::binarySearchTimestamp(uint64_t timestamp, bool lowerBound) const
{
    if (m_indexEntries.isEmpty()) {
        return -1;
    }

    int left = 0;
    int right = m_indexEntries.size() - 1;
    int result = -1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (m_indexEntries[mid].timestamp == timestamp) {
            return mid;  // 精确匹配
        }

        if (m_indexEntries[mid].timestamp < timestamp) {
            left = mid + 1;
            if (lowerBound) {
                result = mid;  // 更新下边界
            }
        }
        else {
            right = mid - 1;
            if (!lowerBound) {
                result = mid;  // 更新上边界
            }
        }
    }

    // 处理边界情况
    if (lowerBound) {
        // 找下边界，如果所有元素都大于目标时间戳
        if (result == -1 && left == 0) {
            return 0;
        }
    }
    else {
        // 找上边界，如果所有元素都小于目标时间戳
        if (result == -1 && right == m_indexEntries.size() - 1) {
            return m_indexEntries.size() - 1;
        }
    }

    return result;
}

QString IndexGenerator::getCommandDescription(uint8_t commandType)
{
    switch (commandType) {
    case 0x00: return LocalQTCompat::fromLocal8Bit("默认，显示到2345行");
    case 0x11: return LocalQTCompat::fromLocal8Bit("CMD行指令数据");
    case 0x22: return LocalQTCompat::fromLocal8Bit("CMD行BTA标志");
    case 0x33: return LocalQTCompat::fromLocal8Bit("CMD行ULPS标志");
    case 0x44: return LocalQTCompat::fromLocal8Bit("视频预览有效行");
    case 0x55: return LocalQTCompat::fromLocal8Bit("此笔数据含有复制标识的行");
    case 0x66: return LocalQTCompat::fromLocal8Bit("命令行指令");
    case 0x77: return LocalQTCompat::fromLocal8Bit("FRAME一帧的开始");
    case 0x88: return LocalQTCompat::fromLocal8Bit("监流设备");
    default: return LocalQTCompat::fromLocal8Bit("未知指令类型");
    }
}
