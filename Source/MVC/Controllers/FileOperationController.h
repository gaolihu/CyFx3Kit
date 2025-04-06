// Source/MVC/Controllers/FileOperationController.h
#pragma once

#include <QObject>
#include <QTimer>
#include "FileOperationModel.h"
#include "FileOperationView.h"
#include "DataPacket.h"

class QWidget;
class QThread;

/**
 * @brief 文件保存控制器类
 *
 * 管理文件保存逻辑，协调模型和视图之间的交互。
 * 处理来自界面的用户操作，更新模型数据，控制保存流程。
 * 使用独立工作线程进行异步文件保存以提高响应性能。
 */
class FileOperationController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit FileOperationController(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FileOperationController();

    /**
     * @brief 初始化控制器
     * @return 初始化结果，true表示成功
     */
    bool initialize();

    /**
     * @brief 是否正在保存
     * @return 当前是否正在保存
     */
    bool isSaving() const;

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 创建文件保存视图
     * @param parent 父窗口指针
     * @return 文件保存视图指针
     */
    FileOperationView* createSaveView(QWidget* parent = nullptr);

    QString getCurrentFileName() const { return m_model->getFullSavePath(); }

public slots:
    /**
     * @brief 启动保存
     * @return 启动结果，true表示成功
     */
    bool slot_FO_C_startSaving();

    /**
     * @brief 停止保存
     * @return 停止结果，true表示成功
     */
    bool slot_FO_C_stopSaving();

    /**
     * @brief 显示设置视图
     * @param parent 父窗口指针
     */
    void slot_FO_C_showSettings(QWidget* parent = nullptr);

    /**
     * @brief 处理数据包
     * @param packet 数据包
     */
    void slot_FO_C_processDataPacket(const DataPacket& packet);

    /**
     * @brief 处理数据包批次
     * @param packets 数据包批次
     */
    void slot_FO_C_processDataBatch(const DataPacketBatch& packets);

    /**
     * @brief 是否启用自动保存
     * @return 是否启用自动保存
     */
    bool slot_FO_C_isAutoSaveEnabled() const;

    /**
     * @brief 设置是否启用自动保存
     * @param enabled 启用状态
     */
    void slot_FO_C_setAutoSaveEnabled(bool enabled);

    /**
     * @brief 处理视图保存参数变更
     * @param parameters 新的保存参数
     */
    void slot_FO_C_onViewParametersChanged(const SaveParameters& parameters);

    /**
     * @brief 开始加载文件
     * @param filePath 文件路径
     * @return 加载结果，true表示成功
     */
    bool slot_FO_C_startLoading(const QString& filePath);

    /**
     * @brief 停止加载文件
     * @return 停止结果，true表示成功
     */
    bool slot_FO_C_stopLoading();

    /**
     * @brief 是否正在加载文件
     * @return 是否正在加载
     */
    bool slot_FO_C_isLoading() const;

    /**
     * @brief 获取下一个数据包
     * @return 数据包
     */
    DataPacket slot_FO_C_getNextPacket();

    /**
     * @brief 是否还有更多数据包
     * @return 是否还有数据包
     */
    bool slot_FO_C_hasMorePackets() const;

    /**
     * @brief 定位到指定位置
     * @param position 文件位置
     */
    void slot_FO_C_seekTo(uint64_t position);

    /**
     * @brief 获取文件总大小
     * @return 文件大小
     */
    uint64_t slot_FO_C_getTotalFileSize() const;

    /**
     * @brief 获取指定范围的文件数据
     * @param startOffset 起始偏移
     * @param size 数据大小
     * @return 文件数据
     */
    QByteArray slot_FO_C_getFileData(uint64_t startOffset, uint64_t size);

    /**
 * @brief 读取指定范围的文件数据
 * @param filePath 文件路径
 * @param startOffset 起始偏移
 * @param size 数据大小
 * @return 读取的数据
 */
    QByteArray slot_FO_C_readFileRange(const QString& filePath, uint64_t startOffset, uint64_t size);

    /**
     * @brief 从当前加载的文件中读取指定范围的数据
     * @param startOffset 起始偏移
     * @param size 数据大小
     * @return 读取的数据
     */
    QByteArray slot_FO_C_readLoadedFileRange(uint64_t startOffset, uint64_t size);

    /**
     * @brief 异步读取指定范围的文件数据
     * @param filePath 文件路径
     * @param startOffset 起始偏移
     * @param size 数据大小
     * @param requestId 请求ID，用于关联响应，可选参数，默认生成
     * @return 请求ID，可用于匹配响应
     */
    uint32_t slot_FO_C_readFileRangeAsync(const QString& filePath, uint64_t startOffset, uint64_t size, uint32_t requestId = 0);

    /**
     * @brief 波形分析数据查询接口 - 获取指定范围的原始数据
     * @param startOffset 起始偏移
     * @param endOffset 结束偏移
     * @return 读取的数据
     */
    QByteArray slot_FO_C_getWaveformData(uint64_t startOffset, uint64_t endOffset);

signals:
    /**
     * @brief 保存开始信号
     */
    void signal_FO_C_saveStarted();

    /**
     * @brief 保存停止信号
     */
    void signal_FO_C_saveStopped();

    /**
     * @brief 保存完成信号
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void signal_FO_C_saveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 保存错误信号
     * @param error 错误消息
     */
    void signal_FO_C_saveError(const QString& error);

    /**
     * @brief 加载开始信号
     * @param filePath 文件路径
     * @param fileSize 文件大小
     */
    void signal_FO_C_loadStarted(const QString& filePath, uint64_t fileSize);

    /**
     * @brief 加载进度信号
     * @param bytesRead 已读取字节数
     * @param totalBytes 总字节数
     */
    void signal_FO_C_loadProgress(uint64_t bytesRead, uint64_t totalBytes);

    /**
     * @brief 加载完成信号
     * @param filePath 文件路径
     * @param totalBytes 总字节数
     */
    void signal_FO_C_loadCompleted(const QString& filePath, uint64_t totalBytes);

    /**
     * @brief 加载错误信号
     * @param error 错误消息
     */
    void signal_FO_C_loadError(const QString& error);

    /**
     * @brief 新数据可用信号
     * @param offset 文件偏移
     * @param size 数据大小
     */
    void signal_FO_C_newDataAvailable(uint64_t offset, uint64_t size);

    /**
     * @brief 数据查询结果信号
     * @param data 查询到的数据
     * @param startOffset 起始偏移
     * @param size 数据大小
     */
    void signal_FO_C_dataQueryResult(const QByteArray& data, uint64_t startOffset, uint64_t size);

    /**
     * @brief 异步数据读取完成信号
     * @param data 读取的数据
     * @param startOffset 起始偏移
     * @param requestId 请求ID
     */
    void signal_FO_C_dataReadCompleted(const QByteArray& data, uint64_t startOffset, uint32_t requestId);

    /**
     * @brief 异步数据读取错误信号
     * @param error 错误信息
     * @param requestId 请求ID
     */
    void signal_FO_C_dataReadError(const QString& error, uint32_t requestId);

    /**
     * @brief 波形数据准备完成信号
     * @param data 波形数据
     * @param startOffset 起始偏移
     * @param endOffset 结束偏移
     */
    void signal_FO_C_waveformDataReady(const QByteArray& data, uint64_t startOffset, uint64_t endOffset);

private slots:
    /**
     * @brief 处理模型状态变更
     * @param status 新的保存状态
     */
    void slot_FO_C_onModelStatusChanged(SaveStatus status);

    /**
     * @brief 处理模型统计信息更新
     * @param statistics 新的统计信息
     */
    void slot_FO_C_onModelStatisticsUpdated(const SaveStatistics& statistics);

    /**
     * @brief 处理模型保存完成
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void slot_FO_C_onModelSaveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 处理模型保存错误
     * @param error 错误消息
     */
    void slot_FO_C_onModelSaveError(const QString& error);

    /**
     * @brief 处理视图开始保存请求
     */
    void slot_FO_C_onViewStartSaveRequested();

    /**
     * @brief 处理视图停止保存请求
     */
    void slot_FO_C_onViewStopSaveRequested();

    /**
     * @brief 处理工作线程保存进度更新
     * @param bytesWritten 已写入字节数
     * @param fileCount 文件数量
     */
    void slot_FO_C_onWorkerSaveProgress(uint64_t bytesWritten, int fileCount);

    /**
     * @brief 处理工作线程保存完成
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void slot_FO_C_onWorkerSaveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 处理工作线程保存错误
     * @param error 错误消息
     */
    void slot_FO_C_onWorkerSaveError(const QString& error);

    /**
     * @brief 处理模型加载开始
     * @param filePath 文件路径
     * @param fileSize 文件大小
     */
    void slot_FO_C_onModelLoadStarted(const QString& filePath, uint64_t fileSize);

    /**
     * @brief 处理模型加载进度
     * @param bytesRead 已读取字节数
     * @param totalBytes 总字节数
     */
    void slot_FO_C_onModelLoadProgress(uint64_t bytesRead, uint64_t totalBytes);

    /**
     * @brief 处理模型加载完成
     * @param filePath 文件路径
     * @param totalBytes 总字节数
     */
    void slot_FO_C_onModelLoadCompleted(const QString& filePath, uint64_t totalBytes);

    /**
     * @brief 处理模型加载错误
     * @param error 错误消息
     */
    void slot_FO_C_onModelLoadError(const QString& error);

    /**
     * @brief 处理新数据可用信号
     * @param offset 文件偏移
     * @param size 数据大小
     */
    void slot_FO_C_onModelNewDataAvailable(uint64_t offset, uint64_t size);

    /**
     * @brief 处理模型数据读取完成
     * @param data 读取的数据
     * @param startOffset 起始偏移
     * @param requestId 请求ID
     */
    void slot_FO_C_onModelDataReadCompleted(const QByteArray& data, uint64_t startOffset, uint32_t requestId);

    /**
     * @brief 处理模型数据读取错误
     * @param error 错误信息
     * @param requestId 请求ID
     */
    void slot_FO_C_onModelDataReadError(const QString& error, uint32_t requestId);

private:
    /**
     * @brief 更新保存统计
     */
    void updateSaveStatistics();

    /**
     * @brief 连接模型信号
     */
    void connectModelSignals();

    /**
     * @brief 连接视图信号
     * @param view 视图对象
     */
    void connectViewSignals(FileOperationView* view);

    /**
     * @brief 连接工作线程信号
     */
    void connectWorkerSignals();

    // 生成唯一的请求ID
    uint32_t generateRequestId() {
        static uint32_t nextId = 1;
        return nextId++;
    }

private:
    FileOperationModel* m_model;          // 模型引用
    FileOperationView* m_currentView;     // 当前视图引用
    QTimer m_statsUpdateTimer;            // 统计更新定时器
    uint16_t m_currentWidth;              // 当前图像宽度
    uint16_t m_currentHeight;             // 当前图像高度
    uint8_t m_currentFormat;              // 当前图像格式
    bool m_initialized;                   // 初始化标志
};