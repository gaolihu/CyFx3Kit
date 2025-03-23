// Source/MVC/Views/DataVisualization.cpp

#include "DataVisualization.h"
#include "DataAnalysisModel.h"
#include "Logger.h"
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QScatterSeries>
#include <QFileInfo>
#include <QPainter>
#include <algorithm>
#include <numeric>

DataVisualization::DataVisualization(QWidget* parent)
    : QWidget(parent)
    , m_chartView(nullptr)
    , m_chart(nullptr)
{
    // 创建布局
    m_layout = new QGridLayout(this);
    this->setLayout(m_layout);

    // 初始化图表
    setupChart();

    LOG_INFO("数据可视化组件已创建");
}

DataVisualization::~DataVisualization()
{
    LOG_INFO("数据可视化组件已销毁");
}

void DataVisualization::setupChart()
{
    // 创建图表
    m_chart = new QChart();

    // 创建图表视图
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 添加到布局
    m_layout->addWidget(m_chartView, 0, 0);

    // 设置默认主题
    updateChartTheme(m_currentOptions);
}

void DataVisualization::setLineData(const QVector<double>& xValues, const QVector<double>& yValues,
    const DataVisualizationOptions& options)
{
    if (xValues.size() != yValues.size() || xValues.isEmpty()) {
        LOG_ERROR("设置线图数据失败：数据为空或长度不匹配");
        return;
    }

    // 保存选项
    m_currentOptions = options;
    m_currentOptions.type = DataVisualizationOptions::LINE_CHART;

    // 清除现有系列
    m_chart->removeAllSeries();

    // 创建线系列
    QLineSeries* series = new QLineSeries();
    series->setName(options.title);

    // 添加数据点
    for (int i = 0; i < xValues.size(); ++i) {
        series->append(xValues[i], yValues[i]);
    }

    // 添加到图表
    m_chart->addSeries(series);

    // 创建坐标轴
    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText(options.xAxisTitle);
    axisX->setLabelFormat("%.1f");
    axisX->setTickCount(5);
    axisX->setGridLineVisible(options.gridLines);

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText(options.yAxisTitle);
    axisY->setLabelFormat("%.1f");
    axisY->setTickCount(5);
    axisY->setGridLineVisible(options.gridLines);

    // 自动计算坐标轴范围
    double xMin = *std::min_element(xValues.begin(), xValues.end());
    double xMax = *std::max_element(xValues.begin(), xValues.end());
    double yMin = *std::min_element(yValues.begin(), yValues.end());
    double yMax = *std::max_element(yValues.begin(), yValues.end());

    // 添加一些边距
    double xMargin = (xMax - xMin) * 0.05;
    double yMargin = (yMax - yMin) * 0.05;

    axisX->setRange(xMin - xMargin, xMax + xMargin);
    axisY->setRange(yMin - yMargin, yMax + yMargin);

    // 设置坐标轴
    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    // 更新图表主题
    updateChartTheme(options);

    // 连接点击信号
    connect(series, &QLineSeries::clicked, this, [this](const QPointF& point) {
        emit pointClicked(point.x(), point.y());
        });

    // 发送更新完成信号
    emit chartUpdated();

    LOG_INFO(QString("线图已更新，数据点数量: %1").arg(xValues.size()));
}

void DataVisualization::setBarData(const QStringList& categories, const QVector<double>& values,
    const DataVisualizationOptions& options)
{
    if (categories.size() != values.size() || categories.isEmpty()) {
        LOG_ERROR("设置柱状图数据失败：数据为空或长度不匹配");
        return;
    }

    // 保存选项
    m_currentOptions = options;
    m_currentOptions.type = DataVisualizationOptions::BAR_CHART;

    // 清除现有系列
    m_chart->removeAllSeries();

    // 创建柱状图系列
    QBarSeries* series = new QBarSeries();
    QBarSet* barSet = new QBarSet(options.title);

    // 设置颜色
    barSet->setColor(options.themeColor);

    // 添加数据
    for (const double& value : values) {
        *barSet << value;
    }

    series->append(barSet);
    m_chart->addSeries(series);

    // 创建类别轴
    QBarCategoryAxis* axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setTitleText(options.xAxisTitle);
    axisX->setGridLineVisible(options.gridLines);

    // 创建值轴
    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText(options.yAxisTitle);
    axisY->setLabelFormat("%.1f");
    axisY->setTickCount(5);
    axisY->setGridLineVisible(options.gridLines);

    // 自动计算Y轴范围
    double yMin = 0; // 柱状图通常从0开始
    double yMax = *std::max_element(values.begin(), values.end());

    // 添加一些边距
    double yMargin = yMax * 0.1;
    axisY->setRange(yMin, yMax + yMargin);

    // 设置坐标轴
    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    // 更新图表主题
    updateChartTheme(options);

    // 连接点击信号
    connect(series, &QBarSeries::clicked, this, [this, values, categories](int index, QBarSet*) {
        if (index >= 0 && index < values.size()) {
            emit pointClicked(index, values[index]);
        }
        });

    // 发送更新完成信号
    emit chartUpdated();

    LOG_INFO(QString("柱状图已更新，类别数量: %1").arg(categories.size()));
}

void DataVisualization::setHistogramData(const QVector<double>& values, int binCount,
    const DataVisualizationOptions& options)
{
    if (values.isEmpty() || binCount <= 0) {
        LOG_ERROR("设置直方图数据失败：数据为空或分箱数量无效");
        return;
    }

    // 保存选项
    m_currentOptions = options;
    m_currentOptions.type = DataVisualizationOptions::HISTOGRAM;

    // 计算直方图分箱
    double minValue = *std::min_element(values.begin(), values.end());
    double maxValue = *std::max_element(values.begin(), values.end());

    // 处理边界情况
    if (qFuzzyCompare(minValue, maxValue)) {
        minValue -= 1.0;
        maxValue += 1.0;
    }

    // 计算箱宽度
    double binWidth = (maxValue - minValue) / binCount;

    // 初始化箱计数
    QVector<double> binCounts(binCount, 0);

    // 统计每个箱的数量
    for (double value : values) {
        int binIndex = std::min(static_cast<int>((value - minValue) / binWidth), binCount - 1);
        if (binIndex >= 0 && binIndex < binCount) {
            binCounts[binIndex]++;
        }
    }

    // 生成箱标签
    QStringList binLabels;
    for (int i = 0; i < binCount; ++i) {
        double binStart = minValue + i * binWidth;
        double binEnd = binStart + binWidth;
        binLabels << QString("[%1, %2]").arg(binStart, 0, 'f', 1).arg(binEnd, 0, 'f', 1);
    }

    // 使用柱状图展示直方图
    setBarData(binLabels, binCounts, options);

    LOG_INFO(QString("直方图已更新，数据点数量: %1，分箱数量: %2").arg(values.size()).arg(binCount));
}

void DataVisualization::setScatterData(const QVector<double>& xValues, const QVector<double>& yValues,
    const DataVisualizationOptions& options)
{
    if (xValues.size() != yValues.size() || xValues.isEmpty()) {
        LOG_ERROR("设置散点图数据失败：数据为空或长度不匹配");
        return;
    }

    // 保存选项
    m_currentOptions = options;
    m_currentOptions.type = DataVisualizationOptions::SCATTER_PLOT;

    // 清除现有系列
    m_chart->removeAllSeries();

    // 创建散点图系列
    QScatterSeries* series = new QScatterSeries();
    series->setName(options.title);
    series->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    series->setMarkerSize(10.0);
    series->setColor(options.themeColor);

    // 添加数据点
    for (int i = 0; i < xValues.size(); ++i) {
        series->append(xValues[i], yValues[i]);
    }

    // 添加到图表
    m_chart->addSeries(series);

    // 创建坐标轴
    QValueAxis* axisX = new QValueAxis();
    axisX->setTitleText(options.xAxisTitle);
    axisX->setLabelFormat("%.1f");
    axisX->setTickCount(5);
    axisX->setGridLineVisible(options.gridLines);

    QValueAxis* axisY = new QValueAxis();
    axisY->setTitleText(options.yAxisTitle);
    axisY->setLabelFormat("%.1f");
    axisY->setTickCount(5);
    axisY->setGridLineVisible(options.gridLines);

    // 自动计算坐标轴范围
    double xMin = *std::min_element(xValues.begin(), xValues.end());
    double xMax = *std::max_element(xValues.begin(), xValues.end());
    double yMin = *std::min_element(yValues.begin(), yValues.end());
    double yMax = *std::max_element(yValues.begin(), yValues.end());

    // 添加一些边距
    double xMargin = (xMax - xMin) * 0.05;
    double yMargin = (yMax - yMin) * 0.05;

    // 处理边界情况
    if (qFuzzyCompare(xMin, xMax)) {
        xMin -= 1.0;
        xMax += 1.0;
    }

    if (qFuzzyCompare(yMin, yMax)) {
        yMin -= 1.0;
        yMax += 1.0;
    }

    axisX->setRange(xMin - xMargin, xMax + xMargin);
    axisY->setRange(yMin - yMargin, yMax + yMargin);

    // 设置坐标轴
    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    // 更新图表主题
    updateChartTheme(options);

    // 连接点击信号
    connect(series, &QScatterSeries::clicked, this, [this](const QPointF& point) {
        emit pointClicked(point.x(), point.y());
        });

    // 发送更新完成信号
    emit chartUpdated();

    LOG_INFO(QString("散点图已更新，数据点数量: %1").arg(xValues.size()));
}

void DataVisualization::updateChartTheme(const DataVisualizationOptions& options)
{
    // 设置标题
    m_chart->setTitle(options.title);

    // 设置动画
    m_chart->setAnimationOptions(options.animation ?
        QChart::SeriesAnimations :
        QChart::NoAnimation);

    // 设置图例
    m_chart->legend()->setVisible(options.legend);

    // 设置主题颜色
    QPalette palette = m_chartView->palette();
    palette.setColor(QPalette::Window, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    m_chartView->setPalette(palette);

    // 设置字体
    QFont titleFont = m_chart->titleFont();
    titleFont.setBold(true);
    titleFont.setPointSize(12);
    m_chart->setTitleFont(titleFont);
}

bool DataVisualization::saveChart(const QString& filePath, int width, int height)
{
    if (!m_chartView) {
        LOG_ERROR("保存图表失败：图表视图未初始化");
        return false;
    }

    // 检查路径有效性
    QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            LOG_ERROR(QString("保存图表失败：无法创建目录 %1").arg(dir.path()));
            return false;
        }
    }

    // 创建QPixmap用于保存
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::white);

    // 在QPixmap上绘制图表
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // 计算绘制区域
    QRect rect(0, 0, width, height);

    // 绘制图表
    m_chartView->render(&painter, rect);

    // 保存为图片
    bool success = pixmap.save(filePath);

    if (success) {
        LOG_INFO(QString("图表已保存到：%1").arg(filePath));
    }
    else {
        LOG_ERROR(QString("保存图表失败：无法写入文件 %1").arg(filePath));
    }

    return success;
}

void DataVisualization::clearChart()
{
    if (m_chart) {
        m_chart->removeAllSeries();

        // 重置标题
        m_chart->setTitle("");

        LOG_INFO("图表已清除");
    }
}

void DataVisualization::visualizeFromItems(const std::vector<DataAnalysisItem>& items,
    const DataVisualizationOptions& options)
{
    if (items.empty()) {
        LOG_ERROR("可视化数据项失败：数据为空");
        return;
    }

    // 根据图表类型不同，准备数据
    switch (options.type) {
    case DataVisualizationOptions::LINE_CHART:
    {
        // 准备时间序列数据
        QVector<double> xValues;
        QVector<double> yValues;

        // 将时间戳转换为相对时间（以第一个时间戳为基准）
        QDateTime baseTime = QDateTime::fromString(items.front().timeStamp, Qt::ISODate);

        for (const auto& item : items) {
            QDateTime timestamp = QDateTime::fromString(item.timeStamp, Qt::ISODate);
            double secondsElapsed = baseTime.secsTo(timestamp);

            xValues.append(secondsElapsed);
            yValues.append(item.value);
        }

        // 设置线图数据
        this->setLineData(xValues, yValues, options);
        break;
    }
    case DataVisualizationOptions::BAR_CHART:
    {
        // 准备柱状图数据
        QStringList categories;
        QVector<double> values;

        for (const auto& item : items) {
            categories.append(item.description.isEmpty() ?
                item.timeStamp :
                item.description);
            values.append(item.value);
        }

        // 限制显示数量，避免过多类别
        const int MAX_CATEGORIES = 20;
        if (categories.size() > MAX_CATEGORIES) {
            categories = categories.mid(0, MAX_CATEGORIES);
            values = values.mid(0, MAX_CATEGORIES);
        }

        // 设置柱状图数据
        setBarData(categories, values, options);
        break;
    }
    case DataVisualizationOptions::HISTOGRAM:
    {
        // 收集所有值
        QVector<double> allValues;

        for (const auto& item : items) {
            allValues.append(item.value);

            // 添加数据点
            for (const auto& point : item.dataPoints) {
                allValues.append(point);
            }
        }

        // 设置直方图数据
        setHistogramData(allValues, 10, options);
        break;
    }
    case DataVisualizationOptions::SCATTER_PLOT:
    {
        // 如果每个项目有数据点，可以将第一个数据点作为X，值作为Y
        QVector<double> xValues;
        QVector<double> yValues;

        for (const auto& item : items) {
            if (!item.dataPoints.isEmpty()) {
                xValues.append(item.dataPoints.first());
                yValues.append(item.value);
            }
        }

        if (xValues.isEmpty()) {
            // 如果没有数据点，使用索引作为X值
            for (size_t i = 0; i < items.size(); ++i) {
                xValues.append(static_cast<double>(i));
                yValues.append(items[i].value);
            }
        }

        // 设置散点图数据
        setScatterData(xValues, yValues, options);
        break;
    }
    default:
        LOG_ERROR("不支持的图表类型");
        break;
    }
}

void DataVisualization::visualizeFromFeatures(const QMap<QString, QVariant>& features,
    const DataVisualizationOptions& options)
{
    if (features.isEmpty()) {
        LOG_ERROR("可视化特征数据失败：数据为空");
        return;
    }

    // 直方图特殊处理
    if (features.contains("histogram") && features["histogram"].canConvert<QVariantList>()) {
        QVariantList histData = features["histogram"].toList();
        QVector<double> histValues;

        for (const QVariant& val : histData) {
            if (val.canConvert<double>()) {
                histValues.append(val.toDouble());
            }
        }

        if (!histValues.isEmpty()) {
            // 准备数据
            QStringList categories;
            for (int i = 0; i < histValues.size(); ++i) {
                categories << QString::number(i);
            }

            // 使用柱状图显示直方图数据
            DataVisualizationOptions histOptions = options;
            histOptions.title = "直方图";
            histOptions.xAxisTitle = "灰度值";
            histOptions.yAxisTitle = "频率";

            setBarData(categories, histValues, histOptions);
            return;
        }
    }

    // 一般特征可视化：将所有可数值化的特征显示为柱状图
    QStringList categories;
    QVector<double> values;

    for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
        if (it.value().canConvert<double>()) {
            categories.append(it.key());
            values.append(it.value().toDouble());
        }
    }

    if (!categories.isEmpty()) {
        // 设置柱状图数据
        DataVisualizationOptions featureOptions = options;
        featureOptions.title = "特征值";
        featureOptions.xAxisTitle = "特征名称";
        featureOptions.yAxisTitle = "特征值";

        setBarData(categories, values, featureOptions);
    }
    else {
        LOG_ERROR("可视化特征数据失败：没有可数值化的特征");
    }
}