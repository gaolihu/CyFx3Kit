// Source/File/IndexGenerator.h
#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include "DataPacket.h"

/**
 * @brief 数据索引生成器
 * 为保存的原始数据生成索引文件，方便后续分析
 */
class IndexGenerator : public QObject {
    Q_OBJECT

public:
    explicit IndexGenerator(QObject* parent = nullptr);
    ~IndexGenerator();

    /**
     * @brief 打开索引文件
     * @param path 索引文件路径
     * @return 是否成功
     */
    bool open(const QString& path);

    /**
     * @brief 关闭索引文件
     */
    void close();

    /**
     * @brief 添加数据包索引
     * @param packet 数据包
     * @param fileOffset 文件中的偏移位置
     * @param fileName 文件名
     */
    void addPacketIndex(const DataPacket& packet, uint64_t fileOffset, const QString& fileName);

    /**
     * @brief 添加批次索引
     * @param batch 数据包批次
     * @param fileOffset 文件中的偏移位置
     * @param fileName 文件名
     */
    void addBatchIndex(const DataPacketBatch& batch, uint64_t fileOffset, const QString& fileName);

    /**
     * @brief 刷新索引到磁盘
     */
    void flush();

private:
    QFile m_indexFile;
    QTextStream m_textStream;
    QMutex m_mutex;
    bool m_isOpen;
    uint64_t m_entryCount;
};
