// Source/File/DataAccessService.h
#pragma once

#include <QObject>
#include <QCache>
#include <QMutex>
#include <QFuture>
#include <QDateTime>
#include <QElapsedTimer>
#include <memory>
#include "IIndexAccess.h"

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
     */
    void setReadTimeout(int milliseconds) {
        m_readTimeout = milliseconds;
    }

    /**
     * @brief 检查文件是否可读
     */
    bool isFileReadable(const QString& filePath);

    /**
     * @brief 获取性能统计信息
     */
    struct PerformanceStats {
        int cacheHits = 0;
        int cacheMisses = 0;
        int readErrors = 0;
        int totalReads = 0;
        qint64 totalReadTime = 0;
    };

    PerformanceStats getPerformanceStats() const {
        return m_stats;
    }

    void resetPerformanceStats() {
        m_stats = PerformanceStats();
    }

signals:
    /**
     * @brief 数据读取完成信号
     * @param timestamp 时间戳
     * @param data 读取的数据
     */
    void dataReadComplete(uint64_t timestamp, const QByteArray& data);

    /**
     * @brief 数据读取错误信号
     * @param errorMessage 错误信息
     */
    void dataReadError(const QString& errorMessage);

private:
    DataAccessService(QObject* parent = nullptr);
    ~DataAccessService();

    // 索引访问接口
    std::shared_ptr<IIndexAccess> m_indexAccess;

    // 文件缓存，避免频繁打开关闭文件
    struct FileCache {
        QFile* file;
        QString path;
        QDateTime lastAccess;
    };

    QMap<QString, FileCache> m_openFiles;
    QMutex m_fileMutex;

    // 数据缓存
    QCache<QString, QByteArray> m_dataCache;  // 键格式: "filename:offset:size"
    QMutex m_cacheMutex;

    // 性能统计
    PerformanceStats m_stats;

    // 读取超时（毫秒）
    int m_readTimeout = 5000;

    // 获取或打开文件
    QFile* getOrOpenFile(const QString& filePath);

    // 生成缓存键
    QString generateCacheKey(const QString& filename, uint64_t offset, uint32_t size);

    // 检查并清理未使用的文件
    void checkAndCleanupUnusedFiles();
};