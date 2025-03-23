// Source/MVC/Controllers/DataAnalysisController.h
#pragma once

#include <QObject>
#include <memory>

class DataAnalysisView;
class DataAnalysisModel;
namespace Ui { class DataAnalysisClass; }
struct DataAnalysisItem;
struct StatisticsInfo;
class DataVisualization;
struct DataVisualizationOptions;
struct DataPacket;
struct PacketIndexEntry;

/**
 * @brief 数据分析控制器
 *
 * 处理数据分析界面的业务逻辑，包括：
 * - 表格数据展示和操作
 * - 数据导入导出
 * - 统计信息计算和显示
 * - 数据筛选和排序
 * - 可视化和分析
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
     * @brief 加载数据
     * 从模型加载数据并更新UI
     */
    void loadData();

    /**
     * @brief 导入数据
     * @param filePath 文件路径，若为空则弹出文件选择对话框
     * @return 是否导入成功
     */
    bool importData(const QString& filePath = QString());

    /**
     * @brief 导出数据
     * @param filePath 文件路径，若为空则弹出文件选择对话框
     * @param selectedRowsOnly 是否只导出选中行
     * @return 是否导出成功
     */
    bool exportData(const QString& filePath = QString(), bool selectedRowsOnly = false);

    /**
     * @brief 清除数据
     */
    void clearData();

    /**
     * @brief 筛选数据
     * @param filterExpression 筛选表达式
     */
    void filterData(const QString& filterExpression);

    /**
     * @brief 分析选中的数据
     * 提取特征并显示分析结果
     */
    void analyzeSelectedData();

    /**
     * @brief 可视化数据
     * @param chartType 图表类型
     */
    void visualizeData(int chartType);

    /**
     * @brief 导出可视化图表
     * @param filePath 文件保存路径，为空则弹出文件选择对话框
     */
    void exportVisualization(const QString& filePath = QString());

    /**
     * @brief 获取选中的行索引列表
     * @return 选中的行索引列表
     */
    QList<int> getSelectedRows() const;

    /**
     * @brief 处理数据包
     * @param packets 数据包列表
     */
    void processDataPackets(const std::vector<DataPacket>& packets);

    /**
     * @brief 转换数据包为分析项
     * @param packet 数据包
     * @return 分析项
     */
    DataAnalysisItem convertPacketToAnalysisItem(const DataPacket& packet);

    /**
     * @brief 启用/禁用自动特征提取
     * @param enable 是否启用
     * @param interval 提取间隔（数据项数量）
     */
    void enableAutoFeatureExtraction(bool enable, int interval = 10);

    /**
     * @brief 处理应用筛选
     * @param filterText 筛选表达式
     */
    void handleFilter(const QString& filterText);

    /**
     * @brief 处理分析请求
     * @param analyzerType 分析器类型索引
     */
    void handleAnalyzeRequest(int analyzerType);

    /**
     * @brief 处理可视化请求
     * @param chartType 图表类型索引
     */
    void handleVisualizeRequest(int chartType);

    /**
     * @brief 提取特征并更新索引
     * @param item 数据分析项
     * @param fileName 文件名
     */
    void extractFeaturesAndIndex(const DataAnalysisItem& item, const QString& fileName);

    /**
     * @brief 从索引文件加载数据
     * @param indexPath 索引文件路径
     * @return 是否成功
     */
    bool loadDataFromIndex(const QString& indexPath);

    /**
     * @brief 将索引条目转换为数据分析项
     * @param entry 索引条目
     * @return 数据分析项
     */
    DataAnalysisItem convertIndexEntryToAnalysisItem(const PacketIndexEntry& entry);

    /**
     * @brief 导出分析结果
     * @param filePath 文件路径
     * @return 是否成功
     */
    bool exportAnalysisResults(const QString& filePath);

    /**
     * @brief 从文件加载数据
     * @param filePath 文件路径
     * @return 是否成功
     */
    bool loadDataFromFile(const QString& filePath);

    /**
     * @brief 设置当前数据源
     * @param filePath 文件路径
     */
    void setDataSource(const QString& filePath) {
        m_currentDataSource = filePath;
    }

    /**
     * @brief 获取当前数据源
     * @return 当前数据源路径
     */
    QString getDataSource() const {
        return m_currentDataSource;
    }

public slots:
    /**
     * @brief 处理视频预览按钮点击
     */
    void slot_DA_C_onVideoPreviewClicked();

    /**
     * @brief 处理保存数据按钮点击
     */
    void slot_DA_C_onSaveDataClicked();

    /**
     * @brief 处理导入数据按钮点击
     */
    void slot_DA_C_onImportDataClicked();

    /**
     * @brief 处理导出数据按钮点击
     */
    void slot_DA_C_onExportDataClicked();

    /**
     * @brief 处理表格选择变化
     * @param selectedRows 选中的行索引列表
     */
    void slot_DA_C_onSelectionChanged(const QList<int>& selectedRows);

    /**
     * @brief 处理统计信息变化
     * @param stats 新的统计信息
     */
    void slot_DA_C_onStatisticsChanged(const StatisticsInfo& stats);

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
     * @brief 处理导出完成
     * @param success 是否成功
     * @param message 消息
     */
    void slot_DA_C_onExportCompleted(bool success, const QString& message);

    /**
     * @brief 处理清除数据按钮点击
     */
    void slot_DA_C_onClearDataClicked();

    /**
     * @brief 处理从文件加载数据请求
     * @param filePath 文件路径
     */
    void slot_DA_C_onLoadDataFromFileRequested(const QString& filePath);

    /**
     * @brief 处理特征提取完成
     * @param index 数据项索引
     * @param features 提取的特征
     */
    void slot_DA_C_onFeaturesExtracted(int index, const QMap<QString, QVariant>& features);

    /**
     * @brief 处理实时更新状态变化
     * @param enabled 是否启用
     */
    void slot_DA_C_onRealTimeUpdateToggled(bool enabled);

    /**
     * @brief 处理更新间隔变化
     * @param interval 更新间隔（毫秒）
     */
    void slot_DA_C_onUpdateIntervalChanged(int interval);

    /**
     * @brief 处理分析按钮点击
     * @param analyzerType 分析器类型索引
     */
    void slot_DA_C_onAnalyzeButtonClicked(int analyzerType);

    /**
     * @brief 处理可视化按钮点击
     * @param chartType 图表类型索引
     */
    void slot_DA_C_onVisualizeButtonClicked(int chartType);

    /**
     * @brief 处理应用筛选按钮点击
     * @param filterText 筛选文本
     */
    void slot_DA_C_onApplyFilterClicked(const QString& filterText);

private:
    /**
     * @brief 连接信号与槽
     */
    void connectSignals();

    /**
     * @brief 更新表格
     * @param items 数据项列表
     */
    void updateTable(const std::vector<DataAnalysisItem>& items);

    /**
     * @brief 更新表格行
     * @param row 行索引
     * @param item 数据项
     */
    void updateTableRow(int row, const DataAnalysisItem& item);

    /**
     * @brief 从数据分析项创建数据包
     * @param item 数据分析项
     * @return 数据包
     */
    DataPacket createDataPacketFromItem(const DataAnalysisItem& item) const;

    /**
     * @brief 序列化特征映射为字符串
     * @param features 特征映射
     * @return 序列化后的字符串
     */
    QString serializeFeatures(const QMap<QString, QVariant>& features) const;

private:
    DataAnalysisView* m_view;                           ///< 视图对象
    Ui::DataAnalysisClass* m_ui;                        ///< UI对象
    DataAnalysisModel* m_model;                         ///< 模型对象（单例）

    bool m_autoExtractFeatures;                         ///< 是否自动提取特征
    int m_featureExtractInterval;                       ///< 特征提取间隔（数据项数量）
    size_t m_dataCounter;                               ///< 数据计数器
    QString m_currentDataSource;                        ///< 当前数据源

    QList<int> m_selectedRows;                          ///< 选中的行列表
    bool m_isUpdatingTable;                             ///< 表格更新中标志
    bool m_isInitialized;                               ///< 初始化标志
    std::unique_ptr<DataVisualization> m_visualization; ///< 可视化组件
};