// Source/Analysis/DataAccessService.h
#pragma once

#include <QObject>
#include <QCache>
#include <QMutex>
#include <QFuture>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <QPair>
#include <memory>
#include "IIndexAccess.h"

class FileOperationController;

/**
 * @brief 数据访问服务，提供统一的文件和索引访问接口
 */
class DataAccessService : public QObject {
    Q_OBJECT

public:
    static DataAccessService& getInstance();

    /**
     * @brief 设置索引访问实现
     */
    void setIndexAccess(std::shared_ptr<IIndexAccess> indexAccess) {
        m_indexAccess = indexAccess;
    }

    /**
     * @brief 获取索引访问接口
     * @return 索引访问接口实例
     */
    std::shared_ptr<IIndexAccess> getIndexAccess() const {
        return m_indexAccess;
    }

    /**
     * @brief 根据索引条目读取原始数据
     * @param entry 索引条目
     * @return 读取的数据
     */
    QByteArray readPacketData(const PacketIndexEntry& entry);

    /**
     * @brief 根据时间戳读取数据
     * @param timestamp 时间戳
     * @return 读取的数据
     */
    QByteArray readPacketByTimestamp(uint64_t timestamp);

    /**
     * @brief 读取特定时间范围内的所有数据
     * @param startTime 开始时间戳
     * @param endTime 结束时间戳
     * @param callback 每个数据包的回调函数
     * @return 操作是否成功
     */
    bool readPacketsInRange(uint64_t startTime, uint64_t endTime,
        std::function<void(const QByteArray&, const PacketIndexEntry&)> callback);

    /**
     * @brief 读取指定命令类型的数据包
     * @param commandType 命令类型
     * @param limit 最大结果数量，-1表示不限制
     * @return 数据包内容列表
     */
    QVector<QByteArray> readPacketsByCommandType(uint8_t commandType, int limit = -1);

    /**
     * @brief 按查询条件读取数据包
     * @param query 查询条件
     * @param callback 每个数据包的回调函数
     * @return 操作是否成功
     */
    bool queryAndReadPackets(const IndexQuery& query,
        std::function<void(const QByteArray&, const PacketIndexEntry&)> callback);

    /**
     * @brief 异步读取单个数据包
     */
    QFuture<QByteArray> readPacketDataAsync(const PacketIndexEntry& entry);

    /**
     * @brief 异步读取时间范围内数据
     * @param startTime 开始时间戳
     * @param endTime 结束时间戳
     * @return 异步操作的Future
     */
    QFuture<QVector<QByteArray>> readPacketsInRangeAsync(uint64_t startTime, uint64_t endTime);

    /**
     * @brief 异步按查询条件读取数据包
     * @param query 查询条件
     * @return 异步操作的Future，返回数据和索引条目的对
     */
    QFuture<QVector<QPair<QByteArray, PacketIndexEntry>>> queryAndReadPacketsAsync(const IndexQuery& query);

    /**
     * @brief 设置缓存大小
     * @param sizeInMB 缓存大小(MB)
     */
    void setCacheSize(int sizeInMB);

    /**
     * @brief 清除缓存
     */
    void clearCache();

    /**
     * @brief 设置读取操作超时
     * @param milliseconds 超时时间(毫秒)
     */
    void setReadTimeout(int milliseconds);

    /**
     * @brief 检查文件是否可读
     */
    bool isFileReadable(const QString& filePath);

    /**
     * @brief 性能统计信息
     */
    struct PerformanceStats {
        int cacheHits = 0;
        int cacheMisses = 0;
        int readErrors = 0;
        int totalReads = 0;
        qint64 totalReadTime = 0;
    };

    /**
     * @brief 获取性能统计信息
     */
    PerformanceStats getPerformanceStats() const;

    /**
     * @brief 重置性能统计信息
     */
    void resetPerformanceStats();

    /**
     * @brief 波形数据结构
     */
    struct WaveformData {
        QVector<double> indexData;
        QVector<QVector<double>> channelData;
        uint64_t timestamp;
        bool isValid;
    };

    /**
     * @brief 读取指定通道的数据
     * @param channel 通道索引(0-3)
     * @param startIndex 起始索引
     * @param length 数据长度
     * @return 通道数据向量
     */
    QVector<double> getChannelData(int channel, int startIndex, int length);

    /**
     * @brief 从数据包提取通道数据
     * @param data 数据包
     * @param channel 通道索引(0-3)
     * @return 通道数据向量
     */
    QVector<double> extractChannelData(const QByteArray& data, int channel);

    /**
     * @brief 读取指定数据包的波形数据
     * @param packetIndex 数据包索引
     * @return 波形数据结构
     */
    WaveformData readWaveformData(uint64_t packetIndex);

    void setFileOperationController(FileOperationController* controller) {
        m_fileOperationController = controller;
    }

signals:
    /**
     * @brief 数据读取完成信号
     * @param timestamp 时间戳
     * @param data 读取的数据
     */
    void signal_DT_ACC_dataReadComplete(uint64_t timestamp, const QByteArray& data);

    /**
     * @brief 数据读取错误信号
     * @param errorMessage 错误信息
     */
    void signal_DT_ACC_dataReadError(const QString& errorMessage);

private slots:
    /**
     * @brief 检查并清理未使用的文件
     */
    void slot_DT_ACC_checkAndCleanupUnusedFiles();

private:
    DataAccessService(QObject* parent = nullptr);
    ~DataAccessService();
    DataAccessService(const DataAccessService&) = delete;
    DataAccessService& operator=(const DataAccessService&) = delete;

    // 索引访问接口
    std::shared_ptr<IIndexAccess> m_indexAccess;

    // 文件缓存，避免频繁打开关闭文件
    struct FileCache {
        QFile* file = nullptr;
        QString path;
        QDateTime lastAccess;
    };

    static constexpr int MAX_OPEN_FILES = 20;  // 最大同时打开文件数

    QMap<QString, FileCache> m_openFiles;
    QMutex m_fileMutex;

    // 数据缓存
    QCache<QString, QByteArray> m_dataCache;  // 键格式: "filename:offset:size"
    QMutex m_cacheMutex;

    // 性能统计
    PerformanceStats m_stats;

    // 读取超时（毫秒）
    int m_readTimeout;

    // 文件清理定时器
    QTimer* m_fileCleanupTimer = nullptr;

    // 获取或打开文件
    QFile* getOrOpenFile(const QString& filePath);

    // 生成缓存键
    QString generateCacheKey(const QString& filename, uint64_t offset, uint32_t size);

    // 文件操作控制器
    FileOperationController* m_fileOperationController{ nullptr };
};