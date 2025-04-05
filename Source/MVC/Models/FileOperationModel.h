// Source/MVC/Models/FileOperationModel.h
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QDateTime>
#include <QMutex>
#include <atomic>
#include <memory>
#include <FileManager.h>

/**
 * @brief 文件保存模型类
 *
 * 文件保存模型管理所有与文件保存相关的数据和状态，
 * 包括保存参数、状态和统计信息。
 */
class FileOperationModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的引用
     */
    static FileOperationModel* getInstance();

    /**
     * @brief 获取保存参数
     * @return 当前保存参数
     */
    SaveParameters getSaveParameters() const;

    /**
     * @brief 设置保存参数
     * @param parameters 新的保存参数
     */
    void setSaveParameters(const SaveParameters& parameters);

    /**
     * @brief 获取当前状态
     * @return 当前保存状态
     */
    SaveStatus getStatus() const;

    /**
     * @brief 设置状态
     * @param status 新的保存状态
     */
    void setStatus(SaveStatus status);

    /**
     * @brief 获取保存统计信息
     * @return 当前保存统计信息
     */
    SaveStatistics getStatistics() const;

    /**
     * @brief 更新保存统计信息
     * @param statistics 新的统计信息
     */
    void updateStatistics(const SaveStatistics& statistics);

    /**
     * @brief 重置保存统计信息
     */
    void resetStatistics();

    /**
     * @brief 获取保存目标完整路径
     * @return 完整的保存路径字符串
     */
    QString getFullSavePath() const;

    /**
     * @brief 获取保存选项值
     * @param key 选项键
     * @param defaultValue 默认值
     * @return 选项值，如不存在则返回默认值
     */
    QVariant getOption(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 设置保存选项
     * @param key 选项键
     * @param value 选项值
     */
    void setOption(const QString& key, const QVariant& value);

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 使用异步文件写入器
     * @param use 是否使用异步写入
     */
    void setUseAsyncWriter(bool use);

    /**
     * @brief 是否使用异步文件写入器
     * @return 是否使用异步写入
     */
    bool isUsingAsyncWriter() const;

    /**
     * @brief 保存当前配置到系统设置
     * @return 保存结果，true表示成功
     */
    bool saveConfigToSettings();

    /**
     * @brief 从系统设置加载配置
     * @return 加载结果，true表示成功
     */
    bool loadConfigFromSettings();

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

    bool startSaving();
    bool stopSaving();
    void processDataPacket(const DataPacket& packet);
    void processDataBatch(const DataPacketBatch& packets);

    /**
     * @brief 开始加载文件
     * @param filePath 文件路径
     * @return 加载结果，true表示成功
     */
    bool startLoading(const QString& filePath);

    /**
     * @brief 停止加载文件
     * @return 停止结果，true表示成功
     */
    bool stopLoading();

    /**
     * @brief 是否正在加载文件
     * @return 是否正在加载
     */
    bool isLoading() const;

    /**
     * @brief 获取下一个数据包
     * @return 数据包
     */
    DataPacket getNextPacket();

    /**
     * @brief 是否还有更多数据包
     * @return 是否还有数据包
     */
    bool hasMorePackets() const;

    /**
     * @brief 定位到指定位置
     * @param position 文件位置
     */
    void seekTo(uint64_t position);

    /**
     * @brief 获取文件总大小
     * @return 文件大小
     */
    uint64_t getTotalFileSize() const;

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

    /**
     * @brief 获取当前加载文件的名称
     * @return 文件名
     */
    QString getCurrentFileName() const;

signals:
    /**
     * @brief 参数变更信号
     * @param parameters 新的保存参数
     */
    void signal_FS_M_parametersChanged(const SaveParameters& parameters);

    /**
     * @brief 状态变更信号
     * @param status 新的保存状态
     */
    void signal_FS_M_statusChanged(SaveStatus status);

    /**
     * @brief 统计信息更新信号
     * @param statistics 新的统计信息
     */
    void signal_FS_M_statisticsUpdated(const SaveStatistics& statistics);

    /**
     * @brief 保存完成信号
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void signal_FS_M_saveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 保存错误信号
     * @param error 错误消息
     */
    void signal_FS_M_saveError(const QString& error);

    /**
     * @brief 加载开始信号
     * @param filePath 文件路径
     * @param fileSize 文件大小
     */
    void signal_FS_M_loadStarted(const QString& filePath, uint64_t fileSize);

    /**
     * @brief 加载进度信号
     * @param bytesRead 已读取字节数
     * @param totalBytes 总字节数
     */
    void signal_FS_M_loadProgress(uint64_t bytesRead, uint64_t totalBytes);

    /**
     * @brief 加载完成信号
     * @param filePath 文件路径
     * @param totalBytes 总字节数
     */
    void signal_FS_M_loadCompleted(const QString& filePath, uint64_t totalBytes);

    /**
     * @brief 加载错误信号
     * @param error 错误消息
     */
    void signal_FS_M_loadError(const QString& error);

    /**
     * @brief 新数据可用信号
     * @param offset 文件偏移
     * @param size 数据大小
     */
    void signal_FS_M_newDataAvailable(uint64_t offset, uint64_t size);

    /**
     * @brief 异步数据读取完成信号
     * @param data 读取的数据
     * @param startOffset 起始偏移
     * @param requestId 请求ID
     */
    void signal_FS_M_dataReadCompleted(const QByteArray& data, uint64_t startOffset, uint32_t requestId);

    /**
     * @brief 异步数据读取错误信号
     * @param error 错误信息
     * @param requestId 请求ID
     */
    void signal_FS_M_dataReadError(const QString& error, uint32_t requestId);

private:
    /**
     * @brief 构造函数 (私有)
     */
    FileOperationModel();

    /**
     * @brief 析构函数 (私有)
     */
    ~FileOperationModel();

    /**
     * @brief 禁用拷贝构造函数
     */
    FileOperationModel(const FileOperationModel&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    FileOperationModel& operator=(const FileOperationModel&) = delete;

    void syncFromManager();

    // 加FileManager事件处理方法
    void onSaveManagerStatusChanged(SaveStatus status);
    void onSaveManagerProgressUpdated(const SaveStatistics& stats);
    void onSaveManagerCompleted(const QString& path, uint64_t totalBytes);
    void onSaveManagerError(const QString& error);

private:
    FileManager& m_fileManager;             // 引用FileManager单例
    QString m_loadedFilePath;               // 当前加载的文件路径
    SaveParameters m_parameters;            // 保存参数
    std::atomic<SaveStatus> m_status;       // 当前状态
    SaveStatistics m_statistics;            // 统计信息
    mutable QMutex m_dataMutex;             // 数据互斥锁
    bool m_useAsyncWriter;                  // 使用异步写入器
};