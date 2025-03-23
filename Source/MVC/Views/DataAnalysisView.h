// Source/MVC/Views/DataAnalysisView.h
#pragma once

#include <QWidget>
#include <QTimer>
#include <memory>

namespace Ui {
    class DataAnalysisClass;
}

class DataAnalysisController;
class DataVisualization;
class QChartView;
struct StatisticsInfo;

/**
 * @brief 数据分析视图类
 *
 * 负责显示数据分析界面，仅处理UI交互，具体业务逻辑由控制器实现
 */
class DataAnalysisView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit DataAnalysisView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DataAnalysisView();

    /**
     * @brief 获取UI对象
     * @return UI对象指针
     */
    Ui::DataAnalysisClass* getUi() const { return ui; }

public slots:
    /**
     * @brief 更新统计信息显示
     * @param info 统计信息
     */
    void slot_DA_V_updateStatisticsDisplay(const StatisticsInfo& info);

    /**
     * @brief 显示消息对话框
     * @param title 标题
     * @param message 消息内容
     * @param isError 是否为错误消息
     */
    void slot_DA_V_showMessageDialog(const QString& title, const QString& message, bool isError = false);

    /**
     * @brief 清除数据表格
     */
    void slot_DA_V_clearDataTable();

    /**
     * @brief 更新UI状态
     * @param hasData 是否有数据
     */
    void slot_DA_V_updateUIState(bool hasData);

    /**
     * @brief 更新状态栏
     * @param statusText 状态文本
     * @param dataCount 数据项数量
     */
    void slot_DA_V_updateStatusBar(const QString& statusText, int dataCount);

    /**
     * @brief 启用/禁用实时数据更新
     * @param enable 是否启用
     */
    void slot_DA_V_enableRealTimeUpdate(bool enable);

    /**
     * @brief 设置自动刷新间隔
     * @param msec 刷新间隔（毫秒）
     */
    void slot_DA_V_setUpdateInterval(int msec);

    /**
     * @brief 启用/禁用实时数据可视化
     * @param enable 是否启用
     */
    void slot_DA_V_enableRealtimeVisualization(bool enable);

    /**
     * @brief 更新实时图表
     */
    void slot_DA_V_updateRealtimeChart();

    /**
     * @brief 在可视化标签页中显示图表
     * @param chartView 图表视图
     */
    void slot_DA_V_showChartInTab(QChartView* chartView);

    /**
     * @brief 显示分析结果
     * @param resultText 结果文本
     */
    void slot_DA_V_showAnalysisResult(const QString& resultText);

signals:
    /**
     * @brief 导入数据按钮点击信号
     */
    void signal_DA_V_importDataClicked();

    /**
     * @brief 导出数据按钮点击信号
     */
    void signal_DA_V_exportDataClicked();

    /**
     * @brief 清除数据按钮点击信号
     */
    void signal_DA_V_clearDataClicked();

    /**
     * @brief 视频预览按钮点击信号
     */
    void signal_DA_V_videoPreviewClicked();

    /**
     * @brief 保存数据按钮点击信号
     */
    void signal_DA_V_saveDataClicked();

    /**
     * @brief 实时更新状态变更信号
     * @param enabled 是否启用
     */
    void signal_DA_V_realTimeUpdateToggled(bool enabled);

    /**
     * @brief 更新间隔变更信号
     * @param interval 更新间隔（毫秒）
     */
    void signal_DA_V_updateIntervalChanged(int interval);

    /**
     * @brief 表格行选择变化信号
     * @param rows 选中的行索引列表
     */
    void signal_DA_V_selectionChanged(const QList<int>& rows);

    /**
     * @brief 实时可视化状态变更信号
     * @param enabled 是否启用
     */
    void signal_DA_V_realtimeVisualizationChanged(bool enabled);

    /**
     * @brief 分析按钮点击信号
     * @param analyzerType 分析器类型索引
     */
    void signal_DA_V_analyzeButtonClicked(int analyzerType);

    /**
     * @brief 可视化按钮点击信号
     * @param chartType 图表类型索引
     */
    void signal_DA_V_visualizeButtonClicked(int chartType);

    /**
     * @brief 导出图表按钮点击信号
     * @param filePath 文件路径，如为空则由控制器负责显示文件选择对话框
     */
    void signal_DA_V_exportChartClicked(const QString& filePath = QString());

    /**
     * @brief 应用筛选按钮点击信号
     * @param filterText 筛选文本
     */
    void signal_DA_V_applyFilterClicked(const QString& filterText);

    /**
     * @brief 从文件加载数据请求信号
     * @param filePath 文件路径
     */
    void signal_DA_V_loadDataFromFileRequested(const QString& filePath);

private slots:
    /**
     * @brief 处理实时更新复选框状态变化
     * @param checked 是否选中
     */
    void slot_DA_V_onRealTimeUpdateToggled(bool checked);

    /**
     * @brief 处理更新间隔变化
     * @param interval 间隔（毫秒）
     */
    void slot_DA_V_onUpdateIntervalChanged(int interval);

    /**
     * @brief 处理实时图表按钮点击
     * @param checked 是否选中
     */
    void slot_DA_V_onRealTimeChartToggled(bool checked);

    /**
     * @brief 处理表格选择变化
     */
    void slot_DA_V_onTableSelectionChanged();

    /**
     * @brief 处理分析按钮点击
     */
    void slot_DA_V_onAnalyzeButtonClicked();

    /**
     * @brief 处理可视化按钮点击
     */
    void slot_DA_V_onVisualizeButtonClicked();

    /**
     * @brief 处理应用筛选按钮点击
     */
    void slot_DA_V_onApplyFilterClicked();

    /**
     * @brief 处理从文件加载按钮点击
     */
    void slot_DA_V_onLoadFromFileClicked();

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

    /**
     * @brief 优化布局
     */
    void optimizeLayout();

    /**
     * @brief 初始化表格
     */
    void initializeTable();

    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 创建实时可视化窗口
     */
    void createRealtimeVisualization();

private:
    Ui::DataAnalysisClass* ui;                              ///< UI对象
    QTimer* m_updateTimer;                                  ///< 更新定时器
    bool m_realTimeUpdateEnabled;                           ///< 实时更新标志
    int m_updateInterval;                                   ///< 更新间隔（毫秒）
    std::unique_ptr<DataAnalysisController> m_controller;   ///< 控制器对象
    std::unique_ptr<DataVisualization> m_realtimeVisualization; ///< 实时可视化窗口
    bool m_realtimeVisualizationEnabled;                    ///< 实时可视化启用标志
    QList<int> m_selectedRows;                              ///< 选中的行索引列表
};