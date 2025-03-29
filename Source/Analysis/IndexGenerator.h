// Source/Analysis/IndexGenerator.h
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
#include <memory>
#include "DataPacket.h"

struct IndexQuery {
    uint64_t timestampStart = 0;
    uint64_t timestampEnd = UINT64_MAX;
    QStringList featureFilters;  // 特征过滤条件，如"amplitude>50"
    int limit = -1;              // 结果限制数量，-1表示无限制
    bool descending = false;     // 是否降序排序
};

// 数据包索引结构
struct PacketIndexEntry {
    uint64_t timestamp;        // 数据包时间戳
    uint64_t fileOffset;       // 文件中的偏移位置
    uint32_t size;             // 数据包大小
    QString fileName;          // 所在文件名
    uint32_t batchId;          // 批次ID
    uint32_t packetIndex;      // 在批次中的索引
};

/**
 * @brief 数据索引生成与查询服务
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
     * @brief 索引文件是否打开
     * @return 是否打开
     */
    bool isOpen() const { return m_isOpen; }

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
     * @brief 批量添加数据包索引
     * @param packets 数据包列表
     * @param startFileOffset 起始文件偏移位置
     * @param fileName 文件名
     * @return 添加成功的条目数量
     */
    int addPacketIndexBatch(const std::vector<DataPacket>& packets,
        uint64_t startFileOffset,
        const QString& fileName);

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

    /**
     * @brief 解析数据流识别数据包并添加到索引
     * @param data 原始数据缓冲区
     * @param size 缓冲区大小
     * @param fileOffset 文件偏移位置
     * @return 识别到的数据包数量
     */
    int parseDataStream(const uint8_t* data, size_t size, uint64_t fileOffset);

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
     * @brief 根据查询条件检索数据包索引
     * @param query 查询条件
     * @return 匹配的索引条目列表
     */
    QVector<PacketIndexEntry> queryIndex(const IndexQuery& query);

    /**
     * @brief 设置当前会话ID
     * @param sessionId 会话标识符
     */
    void setSessionId(const QString& sessionId);

    /**
     * @brief 获取当前会话ID
     * @return 当前会话ID
     */
    QString getSessionId();

    /**
     * @brief 设置索引文件基本路径
     * @param basePath 文件存储基本路径
     */
    void setBasePath(const QString& basePath);

    /**
     * @brief 获取索引文件基本路径
     * @return 索引文件基本路径
     */
    QString getBasePath();

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

    /**
     * @brief 二分查找指定时间戳
     * @param timestamp 目标时间戳
     * @param lowerBound 是否查找下边界
     * @return 找到的索引位置，-1表示未找到
     */
    int binarySearchTimestamp(uint64_t timestamp, bool lowerBound) const;

    // 快速内存映射查找
    QMap<uint64_t, int> m_timestampToIndex;     // 时间戳到索引的映射

    QVector<PacketIndexEntry> m_indexEntries;   // 所有索引条目
    QFile m_indexFile;
    QTextStream m_textStream;
    QMutex m_mutex;
    bool m_isOpen;
    uint64_t m_entryCount;
    uint64_t m_lastSavedCount;          ///< 最后一次保存时的条目数

    // 解析数据流的状态变量
    std::vector<uint8_t> m_lastBuffer;  ///< 上一次缓冲区末尾数据
    bool m_foundPartialHeader;          ///< 是否发现不完整头部

    QString m_sessionId;                ///< 会话标识符
    QString m_basePath;                 ///< 索引文件基本路径
    QString m_indexFileName;            ///< 索引文件名称
    bool m_persistentMode;              ///< 持久化模式
};