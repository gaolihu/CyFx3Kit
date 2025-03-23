// Source/File/IndexGenerator.h
#pragma once

#include <QObject>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QVector>
#include <QMap>
#include <QHash>
#include <QList>
#include <QSharedPointer>
#include <QVariant>
#include <map>
#include "DataPacket.h"

// 数据包索引结构
struct PacketIndexEntry {
    uint64_t timestamp;        // 数据包时间戳
    uint64_t fileOffset;       // 文件中的偏移位置
    uint32_t size;             // 数据包大小
    QString fileName;          // 所在文件名
    uint32_t batchId;          // 批次ID
    uint32_t packetIndex;      // 在批次中的索引
    QHash<QString, QVariant> features;  // 提取的特征信息
};

// 查询条件结构
struct IndexQuery {
    uint64_t timestampStart = 0;
    uint64_t timestampEnd = UINT64_MAX;
    QStringList featureFilters;  // 特征过滤条件，如"amplitude>50"
    int limit = -1;              // 结果限制数量，-1表示无限制
    bool descending = false;     // 是否降序排序
};

/**
 * @brief 增强型数据索引生成与查询服务
 */
class IndexGenerator : public QObject {
    Q_OBJECT

public:
    static IndexGenerator& getInstance();

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
     * @return 生成的索引条目ID
     */
    int addPacketIndex(const DataPacket& packet, uint64_t fileOffset, const QString& fileName);

    /**
     * @brief 添加批次索引
     * @param batch 数据包批次
     * @param fileOffset 文件中的偏移位置
     * @param fileName 文件名
     * @return 生成的索引条目ID集合
     */
    QVector<int> addBatchIndex(const DataPacketBatch& batch, uint64_t fileOffset, const QString& fileName);

    /**
     * @brief 添加特征到指定索引条目
     * @param indexId 索引条目ID
     * @param featureName 特征名称
     * @param featureValue 特征值
     * @return 是否成功
     */
    bool addFeature(int indexId, const QString& featureName, const QVariant& featureValue);

    /**
     * @brief 查询符合条件的索引条目
     * @param query 查询条件
     * @return 匹配的索引条目列表
     */
    QVector<PacketIndexEntry> queryIndex(const IndexQuery& query);

    /**
     * @brief 根据时间戳查找最接近的数据包
     * @param timestamp 目标时间戳
     * @return 最接近的索引条目
     */
    PacketIndexEntry findClosestPacket(uint64_t timestamp);

    /**
     * @brief 获取时间范围内的所有数据包
     * @param startTime 开始时间戳
     * @param endTime 结束时间戳
     * @return 范围内的索引条目列表
     */
    QVector<PacketIndexEntry> getPacketsInRange(uint64_t startTime, uint64_t endTime);

    /**
     * @brief 保存索引到磁盘
     * @param forceSave 是否强制保存（忽略缓存条件）
     * @return 是否成功
     */
    bool saveIndex(bool forceSave = false);

    /**
     * @brief 从磁盘加载索引
     * @param path 索引文件路径
     * @return 是否成功
     */
    bool loadIndex(const QString& path);

    /**
     * @brief 获取所有索引条目
     * @return 所有索引条目的列表
     */
    QVector<PacketIndexEntry> getAllIndexEntries();

    /**
     * @brief 获取总索引条目数
     * @return 条目数量
     */
    int getIndexCount();

    /**
     * @brief 清空索引
     */
    void clearIndex();

    /**
     * @brief 刷新索引到磁盘
     */
    void flush();

signals:
    /**
     * @brief 新索引条目添加信号
     * @param entry 新添加的索引条目
     */
    void indexEntryAdded(const PacketIndexEntry& entry);

    /**
     * @brief 索引更新信号
     * @param count 当前索引总数
     */
    void indexUpdated(int count);

private:
    IndexGenerator(QObject* parent = nullptr);
    ~IndexGenerator();
    IndexGenerator(const IndexGenerator&) = delete;
    IndexGenerator& operator=(const IndexGenerator&) = delete;

    int binarySearchTimestamp(uint64_t timestamp, bool findFirst) const;

    // 快速内存映射查找
    QMap<uint64_t, int> m_timestampToIndex;  // 时间戳到索引的映射
    QMap<QString, QVector<int>> m_featureToIndices;  // 特征到索引的映射

    QVector<PacketIndexEntry> m_indexEntries;  // 所有索引条目
    QFile m_indexFile;
    QTextStream m_textStream;
    QMutex m_mutex;
    bool m_isOpen;
    uint64_t m_entryCount;
    uint64_t m_lastSavedCount;  // 最后一次保存时的条目数
    QString m_indexPath;
};