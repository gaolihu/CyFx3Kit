// Source/File/DataConverters.h
#pragma once

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QThread>
#include <atomic>
#include <memory>
#include "FileSaveModel.h"
#include "DataPacket.h"
#include "DataConverters.h"

/**
 * @brief 文件保存工作线程类
 *
 * 负责异步处理文件保存操作，支持批量处理，避免在主线程中执行I/O操作阻塞UI
 */
class FileSaveWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit FileSaveWorker(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FileSaveWorker();

    /**
     * @brief 设置保存参数
     * @param params 保存参数
     */
    void setParameters(const SaveParameters& params);

    /**
     * @brief 停止保存
     */
    void stop();

public slots:
    /**
     * @brief 开始保存
     */
    void startSaving();

    /**
     * @brief 处理单个数据包
     * @param packet 数据包
     */
    void processDataPacket(const DataPacket& packet);

    /**
     * @brief 处理批量数据包
     * @param packets 数据包批次
     */
    void processDataBatch(const DataPacketBatch& packets);

signals:
    /**
     * @brief 保存进度信号
     * @param bytesWritten 已写入字节数
     * @param fileCount 文件数量
     */
    void saveProgress(uint64_t bytesWritten, int fileCount);

    /**
     * @brief 保存完成信号
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void saveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 保存错误信号
     * @param error 错误消息
     */
    void saveError(const QString& error);

private:
    /**
     * @brief 保存数据包到文件
     * @param packet 数据包
     * @return 是否保存成功
     */
    bool saveDataPacket(const DataPacket& packet);

    /**
     * @brief 保存批量数据包到文件
     * @param packets 数据包批次
     * @return 是否保存成功
     */
    bool saveDataBatch(const DataPacketBatch& packets);

    /**
     * @brief 生成文件名
     * @return 文件名（不含扩展名）
     */
    QString generateFileName();

    /**
     * @brief 创建保存路径
     * @return 完整的保存路径
     */
    QString createSavePath();

    /**
     * @brief 添加扩展名
     * @param baseName 基本文件名
     * @param format 文件格式
     * @return 带扩展名的文件名
     */
    QString addFileExtension(const QString& baseName, FileFormat format);

    /**
     * @brief 检查磁盘空间
     * @param path 路径
     * @param requiredBytes 所需字节数
     * @return 是否有足够空间
     */
    bool checkDiskSpace(const QString& path, uint64_t requiredBytes);

    /**
     * @brief 写入数据到文件
     * @param fullPath 完整文件路径
     * @param data 数据字节数组
     * @return 是否成功写入
     */
    bool writeToFile(const QString& fullPath, const QByteArray& data);

private:
    SaveParameters m_parameters;           // 保存参数
    QString m_savePath;                    // 保存路径
    std::atomic<bool> m_isStopping;        // 停止标志
    QMutex m_mutex;                        // 互斥锁
    uint64_t m_totalBytes;                 // 总字节数
    int m_fileCount;                       // 文件数量
    int m_fileIndex;                       // 文件索引（用于自动命名）
    std::shared_ptr<IDataConverter> m_converter; // 当前使用的数据转换器
};