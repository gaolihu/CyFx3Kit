// Source/File/FileManager.h
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>
#include <QElapsedTimer>
#include <QMutex>
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QThread>
#include <QCoreApplication>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <functional>

#include "DataPacket.h"

// 文件格式定义
enum class FileFormat {
    RAW,        // 原始二进制数据
    BMP,        // BMP图像格式
    TIFF,       // TIFF图像格式
    PNG,        // PNG图像格式
    CSV,        // CSV文本格式（用于元数据）
    CUSTOM,     // 自定义格式
    TEXT        // 文本格式
};

// 保存参数接口
struct SaveParameters {
    QString basePath;              // 保存基础路径
    FileFormat format;             // 文件格式
    bool autoNaming;               // 是否自动命名
    QString filePrefix;            // 文件名前缀
    bool createSubfolder;          // 是否创建子文件夹
    bool appendTimestamp;          // 是否附加时间戳
    int compressionLevel;          // 压缩级别 (0-9, 0表示不压缩)
    bool saveMetadata;             // 是否保存元数据
    QMap<QString, QVariant> options; // 其他选项
};

// 文件保存状态接口
enum class SaveStatus {
    FS_IDLE,           // 空闲
    FS_SAVING,         // 保存中
    FS_PAUSED,         // 暂停
    FS_COMPLETED,      // 完成
    FS_ERROR           // 错误
};

struct SaveStatistics {
    uint64_t totalBytes;           // 已保存总字节数
    uint64_t estimatedTotalBytes;  // 预估总字节数
    uint64_t packetCount;          // 包计数
    uint64_t currentFileBytes;     // 当前文件字节数
    uint64_t fileCount;            // 已保存文件数
    double saveRate;               // 保存速率 (MB/s)
    double progress;               // 保存进度 (%)
    QDateTime startTime;           // 开始时间
    QDateTime lastUpdateTime;      // 上次更新stamp时间
    QDateTime currentFileStartTime;// 当前文件开始保存时间
    QString currentFileName;       // 当前文件名
    QString savePath;              // 保存路径
    SaveStatus status;             // 保存状态
    QString lastError;             // 最后错误信息
};

// 数据转换器接口 (前向声明，详细定义在DataConverters.h)
class IDataConverter;

// 文件写入接口
class IFileWriter {
public:
    virtual ~IFileWriter() = default;

    // 打开文件
    virtual bool open(const QString& filename) = 0;

    // 写入数据
    virtual bool write(const QByteArray& data) = 0;

    // 关闭文件
    virtual bool close() = 0;

    // 获取最后错误
    virtual QString getLastError() const = 0;

    // 检查文件是否打开
    virtual bool isOpen() const = 0;
};

// 标准文件写入器
class WriterFileStandard : public IFileWriter {
public:
    WriterFileStandard() : m_isOpen(false) {}

    virtual ~WriterFileStandard() {
        close();
    }

    bool open(const QString& filename) override;
    bool write(const QByteArray& data) override;
    bool close() override;
    QString getLastError() const override;
    bool isOpen() const override {
        return m_isOpen;
    }

private:
    QFile m_file;
    bool m_isOpen;
    QString m_lastError;
};

// 异步文件写入器
class WriterFileAsync : public IFileWriter {
public:
    WriterFileAsync();
    ~WriterFileAsync() override;

    bool open(const QString& filename) override;
    bool write(const QByteArray& data) override;
    bool close() override;
    QString getLastError() const override;
    bool isOpen() const override;

private:
    void writerThreadFunc();

    QFile m_file;
    bool m_isOpen;
    std::atomic<bool> m_running;
    std::thread m_writerThread;
    std::queue<QByteArray> m_writeQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    QString m_lastError;
    static constexpr size_t MAX_QUEUE_SIZE = 100; // 最大队列大小，防止内存溢出
};

// 文件缓存管理器
class DataCacheManager {
public:
    DataCacheManager(size_t maxCacheSize = DEFAULT_CACHE_SIZE);
    ~DataCacheManager();

    // 添加数据到缓存
    void addToCache(const QByteArray& data);

    // 获取缓存数据
    QByteArray getCache();

    // 清空缓存
    void clearCache();

    // 当前缓存大小
    size_t getCurrentCacheSize();

    // 设置最大缓存大小
    void setMaxCacheSize(size_t maxSize);

private:
    QByteArray m_cache;
    size_t m_maxCacheSize;
    std::mutex m_cacheMutex;

    static constexpr size_t DEFAULT_CACHE_SIZE = 16 * 1024 * 1024; // 16MB
};

// 文件保存管理器 - 核心类
class FileManager : public QObject {
    Q_OBJECT

public:
    static FileManager& instance();

    // 设置保存参数
    void setSaveParameters(const SaveParameters& params);

    // 获取当前保存参数
    const SaveParameters& getSaveParameters() const;

    // 开始保存数据
    bool startSaving();

    // 停止保存数据
    bool stopSaving();

    // 暂停/恢复保存数据
    bool pauseSaving(bool pause);

    // 获取保存统计信息
    SaveStatistics getStatistics();

    // 注册数据转换器
    void registerConverter(FileFormat format, std::shared_ptr<IDataConverter> converter);

    // 获取支持的文件格式列表
    QStringList getSupportedFormats() const;

    // 设置使用异步写入
    void setUseAsyncWriter(bool useAsync);

    // 创建新文件
    bool createNewFile(const DataPacket& packet);

    // 判断是否应该分割文件
    bool shouldSplitFile();

    // 离线操作
    bool startLoading(const QString& filePath);
    bool stopLoading();
    bool isLoading() const;
    QString getCurrentFileName() const;
    DataPacket getNextPacket();
    bool hasMorePackets();
    void seekTo(uint64_t position);
    uint64_t getTotalFileSize();

    /**
     * @brief 处理批量数据
     * @param batchData 批量数据
     * @param offset 文件偏移
     * @param batchId 批次ID
     */
    void processBatchData(const QByteArray& batchData, uint64_t offset, uint32_t batchId);

    /**
     * @brief 读取指定范围的文件数据
     * @param filePath 文件路径
     * @param startOffset 起始偏移
     * @param size 数据大小
     * @return 读取的数据
     */
    QByteArray readFileRange(const QString& filePath, uint64_t startOffset, uint64_t size);

    /**
     * @brief 从当前加载的文件中读取指定范围的数据
     * @param startOffset 起始偏移
     * @param size 数据大小
     * @return 读取的数据
     */
    QByteArray readLoadedFileRange(uint64_t startOffset, uint64_t size);

    /**
     * @brief 异步读取指定范围的文件数据
     * @param filePath 文件路径
     * @param startOffset 起始偏移
     * @param size 数据大小
     * @param requestId 请求ID，用于关联响应
     * @return 是否成功启动异步读取
     */
    bool readFileRangeAsync(const QString& filePath, uint64_t startOffset, uint64_t size, uint32_t requestId);

    std::unique_ptr<IFileWriter> m_fileWriter;

public slots:
    // 处理数据包
    void slot_FSM_processDataPacket(const DataPacket& packet);

    void slot_FSM_processDataBatch(const DataPacketBatch& packets);

signals:
    // 保存状态变更信号
    void signal_FSM_saveStatusChanged(SaveStatus status);

    // 保存进度更新信号
    void signal_FSM_saveProgressUpdated(const SaveStatistics& stats);

    // 保存完成信号
    void signal_FSM_saveCompleted(const QString& path, uint64_t totalBytes);

    // 保存错误信号
    void signal_FSM_saveError(const QString& error);

    // 离线文件加载相关信号
    void signal_FSM_loadStarted(const QString& filePath, uint64_t fileSize);
    void signal_FSM_loadProgress(uint64_t bytesRead, uint64_t totalBytes);
    void signal_FSM_loadCompleted(const QString& filePath, uint64_t totalBytes);
    void signal_FSM_loadError(const QString& error);
    void signal_FSM_newDataAvailable(uint64_t offset, uint64_t size);

    /**
     * @brief 异步数据读取完成信号
     * @param data 读取的数据
     * @param startOffset 起始偏移
     * @param requestId 请求ID
     */
    void signal_FSM_dataReadCompleted(const QByteArray& data, uint64_t startOffset, uint32_t requestId);

    /**
     * @brief 异步数据读取错误信号
     * @param error 错误信息
     * @param requestId 请求ID
     */
    void signal_FSM_dataReadError(const QString& error, uint32_t requestId);

private:
    FileManager();
    ~FileManager();
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    // 创建文件名
    QString createFileName(const DataPacket& packet);

    // 创建保存目录
    bool createSaveDirectory();

    // 更新统计信息
    void updateStatistics(uint64_t bytesWritten);

    // 保存元数据
    bool saveMetadata();

    // 保存线程函数
    void saveThreadFunction();

    // 重置文件写入器
    void resetFileWriter();

    void saveDataBatch(const DataPacketBatch& packets);

    // 文件加载线程函数
    void loadThreadFunction();

    // 异步读取线程函数
    void dataReadThreadFunction(const QString& filePath, uint64_t startOffset, uint64_t size, uint32_t requestId);

private:
    SaveParameters m_saveParams;
    SaveStatistics m_statistics;
    std::mutex m_statsMutex;
    std::mutex m_paramsMutex;

    std::atomic<bool> m_running;
    std::atomic<bool> m_paused;
    std::atomic<bool> m_useAsyncWriter;

    std::map<FileFormat, std::shared_ptr<IDataConverter>> m_converters;
    std::unique_ptr<DataCacheManager> m_cacheManager;

    std::thread m_saveThread;
    std::queue<DataPacket> m_dataQueue;
    std::queue<DataPacketBatch> m_batchQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_dataReady;

    QElapsedTimer m_speedTimer;
    uint64_t m_lastSavedBytes;

    std::mutex m_readMutex;
    std::atomic<bool> m_asyncReadRunning{ false };
    std::thread m_asyncReadThread;

    // 离线文件加载相关成员
    std::atomic<bool> m_loading{ false };
    QFile m_currentFile;
    QString m_currentFilePath;
    QByteArray m_loadBuffer;
    uint64_t m_loadPosition{ 0 };
    uint64_t m_loadFileSize{ 0 };
    std::mutex m_loadMutex;
    std::thread m_loadThread;
    std::queue<DataPacket> m_loadQueue;
};