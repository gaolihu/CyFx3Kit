// Source/MVC/Views/DataVisualization.h
#pragma once

#include <QWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QValueAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QScatterSeries>
#include <QGridLayout>
#include <QDir>
#include <QVector>
#include "DataAnalysisModel.h"

struct DataVisualizationOptions {
    enum ChartType {
        LINE_CHART,
        BAR_CHART,
        HISTOGRAM,
        SCATTER_PLOT,
        HEATMAP
    };

    ChartType type = LINE_CHART;
    QString title = "数据可视化";
    QString xAxisTitle = "X轴";
    QString yAxisTitle = "Y轴";
    bool legend = true;
    bool animation = true;
    bool gridLines = true;
    QColor themeColor = QColor(0, 120, 215);
};

/**
 * @brief 数据可视化组件
 *
 * 提供多种图表类型用于数据可视化，支持直接从DataAnalysisModel获取数据
 */
class DataVisualization : public QWidget {
    Q_OBJECT

public:
    explicit DataVisualization(QWidget* parent = nullptr);
    ~DataVisualization();

    /**
     * @brief 设置线图数据
     * @param xValues X轴数据
     * @param yValues Y轴数据
     * @param options 图表选项
     */
    void setLineData(const QVector<double>& xValues, const QVector<double>& yValues,
        const DataVisualizationOptions& options = DataVisualizationOptions());

    /**
     * @brief 设置柱状图数据
     * @param categories 类别标签
     * @param values 对应值
     * @param options 图表选项
     */
    void setBarData(const QStringList& categories, const QVector<double>& values,
        const DataVisualizationOptions& options = DataVisualizationOptions());

    /**
     * @brief 设置直方图数据
     * @param values 数据值
     * @param binCount 分箱数量
     * @param options 图表选项
     */
    void setHistogramData(const QVector<double>& values, int binCount = 10,
        const DataVisualizationOptions& options = DataVisualizationOptions());

    /**
     * @brief 设置散点图数据
     * @param xValues X坐标值
     * @param yValues Y坐标值
     * @param options 图表选项
     */
    void setScatterData(const QVector<double>& xValues, const QVector<double>& yValues,
        const DataVisualizationOptions& options = DataVisualizationOptions());

    /**
     * @brief 保存图表为图片
     * @param filePath 文件保存路径
     * @param width 图片宽度
     * @param height 图片高度
     * @return 是否保存成功
     */
    bool saveChart(const QString& filePath, int width = 800, int height = 600);

    /**
     * @brief 清除所有图表数据
     */
    void clearChart();

    /**
     * @brief 从DataAnalysisItem生成可视化
     * @param items 数据项列表
     * @param options 图表选项
     */
    void visualizeFromItems(const std::vector<DataAnalysisItem>& items,
        const DataVisualizationOptions& options = DataVisualizationOptions());

    /**
     * @brief 从特征数据生成可视化
     * @param features 特征数据映射
     * @param options 图表选项
     */
    void visualizeFromFeatures(const QMap<QString, QVariant>& features,
        const DataVisualizationOptions& options = DataVisualizationOptions());

signals:
    /**
     * @brief 点击图表上某个点时触发
     * @param x X坐标
     * @param y Y坐标
     */
    void pointClicked(double x, double y);

    /**
     * @brief 图表更新完成信号
     */
    void chartUpdated();

private:
    void setupChart();
    void updateChartTheme(const DataVisualizationOptions& options);

    QChartView* m_chartView;
    QChart* m_chart;
    QGridLayout* m_layout;
    DataVisualizationOptions m_currentOptions;
};