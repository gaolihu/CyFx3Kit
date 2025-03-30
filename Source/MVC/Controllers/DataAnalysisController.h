// Source/MVC/Controllers/DataAnalysisController.h
#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QVector>
#include <QMutex>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <memory>

class DataAnalysisView;
class DataAnalysisModel;
namespace Ui { class DataAnalysisClass; }
struct DataPacket;
struct PacketIndexEntry;

/**
 * @brief 数据分析控制器
 *
 * 处理USB数据索引界面的业务逻辑，专注于：
 * - 高速USB数据的捕获和解析
 * - 特定头部模式的识别和索引
 * - 索引数据的展示和导出
 */
class DataAnalysisController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param view 关联的视图对象
     */
    explicit DataAnalysisController(DataAnalysisView* view);

    /**
     * @brief 析构函数
     */
    ~DataAnalysisController();

    /**
     * @brief 初始化控制器
     * 连接信号和槽，设置初始状态
     */
    void initialize();

    /**
     * @brief 初始化表格
     * 设置表格列和格式
     */
    void initializeTable();

    /**
     * @brief 加载索引数据
     * 从IndexGenerator获取索引并更新UI
     */
    void loadData();

    /**
     * @brief 导入数据文件
     * @param filePath 文件路径，若为空则弹出文件选择对话框
     * @return 是否导入成功
     */
    bool importData(const QString& filePath = QString());

    /**
     * @brief 导出索引数据
     * @param filePath 文件路径，若为空则弹出文件选择对话框
     * @param selectedRowsOnly 是否只导出选中行
     * @return 是否导出成功
     */
    bool exportData(const QString& filePath = QString(), bool selectedRowsOnly = false);

    /**
     * @brief 导出索引到CSV文件
     * @param filePath 文件路径
     * @param entries 索引条目列表
     * @return 是否成功
     */
    bool exportToCsv(const QString& filePath, const QVector<PacketIndexEntry>& entries);

    /**
     * @brief 导出索引到JSON文件
     * @param filePath 文件路径
     * @param entries 索引条目列表
     * @return 是否成功
     */
    bool exportToJson(const QString& filePath, const QVector<PacketIndexEntry>& entries);

    /**
     * @brief 清除索引数据
     */
    void clearData();

    /**
     * @brief 从文件加载索引数据
     * @param filePath 文件路径
     * @return 是否成功
     */
    bool loadDataFromFile(const QString& filePath);

    /**
     * @brief 设置当前数据源
     * @param filePath 文件路径
     */
    void setDataSource(const QString& filePath);

    /**
     * @brief 获取当前数据源
     * @return 当前数据源路径
     */
    QString getDataSource() const {
        return m_currentDataSource;
    }

    /**
     * @brief 处理原始数据流
     * @param data 原始数据缓冲区
     * @param size 缓冲区大小（字节）
     * @param fileOffset 此缓冲区在文件中的起始偏移
     * @param fileName 被处理的文件名
     */
    void processRawData(const uint8_t* data, size_t size, uint64_t fileOffset = 0, const QString& fileName = QString());

    /**
     * @brief 处理数据包集合
     * @param packets 数据包列表
     */
    void processDataPackets(const std::vector<DataPacket>& packets);

    /**
     * @brief 更新索引表格
     * @param entries 索引条目列表
     */
    void updateIndexTable(const QVector<PacketIndexEntry>& entries);

    /**
     * @brief 配置索引会话
     * @param sessionId 会话标识符
     * @param basePath 基本路径
     */
    void configureIndexing(const QString& sessionId, const QString& basePath);

    /**
     * @brief 设置最大批处理大小
     * @param maxBatchSize 最大处理包数量
     */
    void setMaxBatchSize(int maxBatchSize);

    /**
     * @brief 是否正在处理数据
     * @return 是否处理中
     */
    bool isProcessing() const;

public slots:
    /**
     * @brief 处理数据变化
     */
    void slot_DA_C_onDataChanged();

    /**
     * @brief 处理导入完成
     * @param success 是否成功
     * @param message 消息
     */
    void slot_DA_C_onImportCompleted(bool success, const QString& message);

    /**
     * @brief 处理导入数据按钮点击
     */
    void slot_DA_C_onImportDataClicked();

    /**
     * @brief 处理导出数据按钮点击
     */
    void slot_DA_C_onExportDataClicked();

    /**
     * @brief 处理清除数据按钮点击
     */
    void slot_DA_C_onClearDataClicked();

    /**
     * @brief 处理表格选择变化
     * @param selectedRows 选中的行索引列表
     */
    void slot_DA_C_onSelectionChanged(const QList<int>& selectedRows);

    /**
     * @brief 处理从文件加载数据请求
     * @param filePath 文件路径
     */
    void slot_DA_C_onLoadDataFromFileRequested(const QString& filePath);

    /**
     * @brief 处理索引条目添加
     * @param entry 添加的索引条目
     */
    void slot_DA_C_onIndexEntryAdded(const PacketIndexEntry& entry);

    /**
     * @brief 处理索引更新
     * @param count 索引条目数量
     */
    void slot_DA_C_onIndexUpdated(int count);

    /**
     * @brief 异步处理完成槽
     */
    void slot_DA_C_onProcessingFinished();

    /**
     * @brief 处理索引文件变化
     * @param path 变化的文件路径
     */
    void slot_DA_C_onIndexFileChanged(const QString& path);

    /**
     * @brief 处理批量数据加载完成
     */
    void slot_DA_C_onBatchLoadFinished();

private:
    /**
     * @brief 连接信号与槽
     */
    void connectSignals();

    /**
     * @brief 分批加载索引数据
     * @param entries 索引条目
     * @param batchSize 批量大小
     */
    void loadDataBatched(const QVector<PacketIndexEntry>& entries, int batchSize = 1000);

    /**
     * @brief 优化表格更新
     * @param entries 索引条目
     * @param startRow 起始行
     * @param count 数量
     */
    void optimizedTableUpdate(const QVector<PacketIndexEntry>& entries, int startRow, int count);

private:
    DataAnalysisView* m_view;                     ///< 视图对象
    Ui::DataAnalysisClass* m_ui;                  ///< UI对象
    DataAnalysisModel* m_model;                   ///< 模型对象（单例）

    size_t m_dataCounter;                         ///< 数据计数器
    QString m_currentDataSource;                  ///< 当前数据源
    QString m_sessionBasePath;                    ///< 会话基本路径
    QString m_sessionId;                          ///< 会话ID

    QList<int> m_selectedRows;                    ///< 选中的行列表
    bool m_isUpdatingTable;                       ///< 表格更新中标志
    bool m_isInitialized;                         ///< 初始化标志

    QFutureWatcher<int> m_processWatcher;         ///< 异步处理监视器
    QFutureWatcher<void> m_loadWatcher;           ///< 加载数据监视器
    QFileSystemWatcher m_fileWatcher;             ///< 文件监视器
    QElapsedTimer m_performanceTimer;             ///< 性能计时器
    QVector<PacketIndexEntry> m_bufferedEntries;  ///< 缓冲的条目
    int m_maxBatchSize;                           ///< 最大批处理大小
    bool m_processingData;                        ///< 数据处理中标志
    int m_batchLoadPosition;                      ///< 批量加载位置
    QVector<PacketIndexEntry> m_entriesToLoad;    ///< 待加载条目
    QMutex m_processMutex;                        ///< 处理互斥锁
};