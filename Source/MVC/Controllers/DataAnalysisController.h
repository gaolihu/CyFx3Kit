// Source/MVC/Controllers/DataAnalysisController.h
#pragma once

#include <QObject>
#include <memory>

class DataAnalysisView;
class DataAnalysisModel;
namespace Ui { class DataAnalysisClass; }
struct DataAnalysisItem;
struct StatisticsInfo;

/**
 * @brief 数据分析控制器
 *
 * 处理数据分析界面的业务逻辑，包括：
 * - 表格数据展示和操作
 * - 数据导入导出
 * - 统计信息计算和显示
 * - 数据筛选和排序
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

public slots:
    /**
     * @brief 处理视频预览按钮点击
     */
    void onVideoPreviewClicked();

    /**
     * @brief 处理保存数据按钮点击
     */
    void onSaveDataClicked();

    /**
     * @brief 处理导入数据按钮点击
     */
    void onImportDataClicked();

    /**
     * @brief 处理导出数据按钮点击
     */
    void onExportDataClicked();

    /**
     * @brief 处理表格选择变化
     * @param selectedRows 选中的行索引列表
     */
    void onSelectionChanged(const QList<int>& selectedRows);

    /**
     * @brief 处理统计信息变化
     * @param stats 新的统计信息
     */
    void onStatisticsChanged(const StatisticsInfo& stats);

    /**
     * @brief 处理数据变化
     */
    void onDataChanged();

    /**
     * @brief 处理导入完成
     * @param success 是否成功
     * @param message 消息
     */
    void onImportCompleted(bool success, const QString& message);

    /**
     * @brief 处理导出完成
     * @param success 是否成功
     * @param message 消息
     */
    void onExportCompleted(bool success, const QString& message);

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
     * @brief 获取选中的行索引列表
     * @return 选中的行索引列表
     */
    QList<int> getSelectedRows() const;

private:
    DataAnalysisView* m_view;                  ///< 视图对象
    Ui::DataAnalysisClass* m_ui;               ///< UI对象
    DataAnalysisModel* m_model;                ///< 模型对象

    QList<int> m_selectedRows;                 ///< 选中的行列表
    bool m_isUpdatingTable;                    ///< 表格更新中标志
    bool m_isInitialized;                      ///< 初始化标志
};