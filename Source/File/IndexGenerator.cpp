// Source/File/IndexGenerator.cpp

#include "IndexGenerator.h"
#include "Logger.h"

IndexGenerator::IndexGenerator(QObject* parent)
    : QObject(parent)
    , m_isOpen(false)
    , m_entryCount(0)
{
}

IndexGenerator::~IndexGenerator() {
    close();
}

bool IndexGenerator::open(const QString& path) {
    QMutexLocker locker(&m_mutex);

    if (m_isOpen) {
        close();
    }

    m_indexFile.setFileName(path);
    if (!m_indexFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString("无法创建索引文件: %1 - %2").arg(path).arg(m_indexFile.errorString()));
        return false;
    }

    m_textStream.setDevice(&m_indexFile);

    // 写入索引文件头
    m_textStream << "# FX3 Data Index File\n";
    m_textStream << "# Format: TimestampNs, PacketSize, FileOffset, FileName, [BatchId], [PacketInBatch]\n";
    m_textStream << "# Created: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    m_textStream.flush();

    m_isOpen = true;
    m_entryCount = 0;

    LOG_INFO(QString("索引文件已创建: %1").arg(path));
    return true;
}

void IndexGenerator::close() {
    QMutexLocker locker(&m_mutex);

    if (m_isOpen) {
        // 写入摘要信息
        m_textStream << "# Total Entries: " << m_entryCount << "\n";
        m_textStream << "# Closed: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";

        m_textStream.flush();
        m_indexFile.close();
        m_isOpen = false;

        LOG_INFO(QString("索引文件已关闭，总条目数: %1").arg(m_entryCount));
    }
}

void IndexGenerator::addPacketIndex(const DataPacket& packet, uint64_t fileOffset, const QString& fileName) {
    QMutexLocker locker(&m_mutex);

    if (!m_isOpen) {
        return;
    }

    // 写入索引记录: 时间戳(ns),大小,文件偏移,文件名,批次ID,批次中的包序号
    m_textStream << packet.timestamp << ","
        << packet.getSize() << ","
        << fileOffset << ","
        << fileName;

    // 如果有批次信息，则添加
    if (packet.batchId > 0 || packet.packetsInBatch > 0) {
        m_textStream << "," << packet.batchId << "," << packet.packetsInBatch;
    }

    m_textStream << "\n";

    m_entryCount++;

    // 每100条记录刷新一次，平衡性能和安全
    if (m_entryCount % 100 == 0) {
        m_textStream.flush();
    }
}

void IndexGenerator::addBatchIndex(const DataPacketBatch& batch, uint64_t fileOffset, const QString& fileName) {
    QMutexLocker locker(&m_mutex);

    if (!m_isOpen || batch.empty()) {
        return;
    }

    // 只记录批次的第一个包的信息，作为批次开始标记
    const DataPacket& firstPacket = batch.front();

    // 写入批次索引记录
    m_textStream << firstPacket.timestamp << ","
        << batch.size() << "," // 使用批次大小
        << fileOffset << ","
        << fileName << ","
        << firstPacket.batchId << ","
        << firstPacket.packetsInBatch << "\n";

    m_entryCount++;

    // 每写入10个批次记录就刷新一次
    if (m_entryCount % 10 == 0) {
        m_textStream.flush();
    }
}

void IndexGenerator::flush() {
    QMutexLocker locker(&m_mutex);

    if (m_isOpen) {
        m_textStream.flush();
    }
}