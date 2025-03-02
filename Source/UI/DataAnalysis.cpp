#include "DataAnalysis.h"
#include "Logger.h"

#include <QLabel>

// Qt图表库的条件编译
#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarCategoryAxis>
#endif

DataAnalysis::DataAnalysis(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    initializeUI();
    connectSignals();

    LOG_INFO("数据分析窗口已创建");
}

DataAnalysis::~DataAnalysis()
{
    LOG_INFO("数据分析窗口被销毁");
}

void DataAnalysis::initializeUI()
{
    // 设置窗口标题
    setWindowTitle("数据分析");

    // 初始化表格
    ui.tableWidget_Sel->clear();
    ui.tableWidget_Sel->setRowCount(100);
    ui.tableWidget_Sel->setColumnCount(12);

    // 设置表格标题
    QStringList headers;
    headers << "帧号" << "时间戳" << "长度" << "类型";
    for (int i = 0; i < 8; i++) {
        headers << QString("数据%1").arg(i + 1);
    }
    ui.tableWidget_Sel->setHorizontalHeaderLabels(headers);

    // 设置表格样式
    ui.tableWidget_Sel->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableWidget_Sel->setSelectionMode(QAbstractItemView::SingleSelection);
    ui.tableWidget_Sel->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tableWidget_Sel->setAlternatingRowColors(true);

    // 设置表格列宽
    ui.tableWidget_Sel->setColumnWidth(0, 60);  // 帧号
    ui.tableWidget_Sel->setColumnWidth(1, 120); // 时间戳
    ui.tableWidget_Sel->setColumnWidth(2, 60);  // 长度
    ui.tableWidget_Sel->setColumnWidth(3, 80);  // 类型
    for (int i = 4; i < 12; i++) {
        ui.tableWidget_Sel->setColumnWidth(i, 100); // 数据列
    }

    // 创建统计图表
    createStatisticsChart();

    // 初始状态下禁用按钮
    ui.saveDatabtn->setEnabled(false);
    ui.videoShowbtn->setEnabled(false);
}

void DataAnalysis::connectSignals()
{
    // 连接按钮信号
    connect(ui.saveDatabtn, &QPushButton::clicked, this, &DataAnalysis::onSaveDataButtonClicked);
    connect(ui.videoShowbtn, &QPushButton::clicked, this, &DataAnalysis::onVideoShowButtonClicked);

    // 连接表格选择信号
    connect(ui.tableWidget_Sel, &QTableWidget::itemSelectionChanged, this, &DataAnalysis::onTableItemSelected);
}

void DataAnalysis::createStatisticsChart()
{
#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 创建图表布局容器
    QWidget* container = ui.horizontalLayoutWidget_2;
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // 创建图表视图
    QChartView* chartView = new QChartView(container);
    chartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView);

    // 创建空图表
    QChart* chart = new QChart();
    chart->setTitle("数据分析图表");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chartView->setChart(chart);

    // 设置默认显示提示
    QLabel* placeholderLabel = new QLabel("数据为空，请先获取数据");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    chart->setPlotAreaBackgroundVisible(false);
    layout->addWidget(placeholderLabel);

    // 保存布局
    container->setLayout(layout);
#else
    // 创建简单布局
    QWidget* container = ui.horizontalLayoutWidget_2;
    QVBoxLayout* layout = new QVBoxLayout(container);

    // 添加提示标签
    QLabel* placeholderLabel = new QLabel("数据为空，请先获取数据");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholderLabel);

    // 设置布局
    container->setLayout(layout);
#endif
}

void DataAnalysis::onSaveDataButtonClicked()
{
    // 发出保存数据请求信号
    emit saveDataRequested();
}

void DataAnalysis::onVideoShowButtonClicked()
{
    // 发出视频显示请求信号
    emit videoDisplayRequested();
}

void DataAnalysis::onTableItemSelected()
{
    // 表格选择事件的处理
    // 此处可以增加选中行的数据处理逻辑
}

void DataAnalysis::setAnalysisData(const QVector<QByteArray>& data)
{
    LOG_INFO(QString("设置分析数据，数据包数量: %1").arg(data.size()));

    // 保存数据
    m_analysisData = data;

    // 更新表格数据
    updateTableData();

    // 更新统计信息
    updateStatistics();

    // 启用按钮
    ui.saveDatabtn->setEnabled(!m_analysisData.isEmpty());
    ui.videoShowbtn->setEnabled(!m_analysisData.isEmpty());
}

void DataAnalysis::updateTableData()
{
    // 清空表格
    ui.tableWidget_Sel->clearContents();

    // 设置行数
    int rowCount = qMin(m_analysisData.size(), 100); // 最多显示100行
    ui.tableWidget_Sel->setRowCount(rowCount);

    // 填充数据
    for (int row = 0; row < rowCount; row++) {
        const QByteArray& packet = m_analysisData[row];

        // 帧号
        QTableWidgetItem* frameItem = new QTableWidgetItem(QString::number(row + 1));
        ui.tableWidget_Sel->setItem(row, 0, frameItem);

        // 时间戳 (假设前8字节是时间戳)
        if (packet.size() >= 8) {
            quint64 timestamp = 0;
            for (int i = 0; i < 8; i++) {
                timestamp |= (static_cast<quint64>(static_cast<quint8>(packet[i])) << (i * 8));
            }
            QTableWidgetItem* timeItem = new QTableWidgetItem(QString::number(timestamp));
            ui.tableWidget_Sel->setItem(row, 1, timeItem);
        }

        // 长度
        QTableWidgetItem* lenItem = new QTableWidgetItem(QString::number(packet.size()));
        ui.tableWidget_Sel->setItem(row, 2, lenItem);

        // 类型 (假设第9字节是类型)
        if (packet.size() > 8) {
            QTableWidgetItem* typeItem = new QTableWidgetItem(QString::number(static_cast<quint8>(packet[8])));
            ui.tableWidget_Sel->setItem(row, 3, typeItem);
        }

        // 数据字段 (显示接下来的8个字节)
        for (int col = 0; col < 8 && col + 9 < packet.size(); col++) {
            QTableWidgetItem* dataItem = new QTableWidgetItem(QString::number(static_cast<quint8>(packet[col + 9]), 16).toUpper());
            ui.tableWidget_Sel->setItem(row, col + 4, dataItem);
        }
    }
}

void DataAnalysis::updateStatistics()
{
    // 如果没有数据，清空统计信息
    if (m_analysisData.isEmpty()) {
        clearStatistics();
        return;
    }

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 获取布局容器
    QWidget* container = ui.horizontalLayoutWidget_2;
    QLayout* oldLayout = container->layout();

    // 清空原有布局
    if (oldLayout) {
        while (QLayoutItem* item = oldLayout->takeAt(0)) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
        delete oldLayout;
    }

    // 创建新布局
    QVBoxLayout* layout = new QVBoxLayout(container);

    // 创建图表
    QChartView* chartView = new QChartView(container);
    chartView->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView);

    // 创建图表对象
    QChart* chart = new QChart();
    chart->setTitle("数据包长度分布");

    // 创建数据系列
    QBarSet* lengthSet = new QBarSet("数据包长度");
    QMap<int, int> lengthDistribution;

    // 统计数据包长度分布
    for (const QByteArray& packet : m_analysisData) {
        int length = packet.size();
        lengthDistribution[length]++;
    }

    // 添加数据到系列
    QStringList categories;
    for (auto it = lengthDistribution.begin(); it != lengthDistribution.end(); ++it) {
        *lengthSet << it.value();
        categories << QString::number(it.key());
    }

    // 创建条形图系列
    QBarSeries* series = new QBarSeries();
    series->append(lengthSet);

    // 添加系列到图表
    chart->addSeries(series);

    // 创建坐标轴
    QBarCategoryAxis* axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis* axisY = new QValueAxis();
    axisY->setRange(0, lengthSet->at(lengthSet->count() - 1) * 1.1);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // 显示图例
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // 设置图表到视图
    chartView->setChart(chart);

    // 更新布局
    container->setLayout(layout);
#else
    // 显示备用统计信息（无图表模式）
    QWidget* container = ui.horizontalLayoutWidget_2;
    QLayout* oldLayout = container->layout();

    // 清空原有布局
    if (oldLayout) {
        while (QLayoutItem* item = oldLayout->takeAt(0)) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
        delete oldLayout;
    }

    // 创建新布局
    QVBoxLayout* layout = new QVBoxLayout(container);

    // 添加统计文本标签
    QLabel* statsLabel = new QLabel(container);
    statsLabel->setAlignment(Qt::AlignCenter);

    // 计算基本统计信息
    int totalPackets = m_analysisData.size();
    int minSize = INT_MAX;
    int maxSize = 0;
    qint64 totalSize = 0;

    for (const QByteArray& packet : m_analysisData) {
        int size = packet.size();
        minSize = qMin(minSize, size);
        maxSize = qMax(maxSize, size);
        totalSize += size;
    }

    double avgSize = totalSize / (double)totalPackets;

    QString statsText = QString(
        "数据包统计信息:\n"
        "总数据包: %1\n"
        "最小长度: %2 字节\n"
        "最大长度: %3 字节\n"
        "平均长度: %4 字节\n"
        "总数据量: %5 字节"
    ).arg(totalPackets).arg(minSize).arg(maxSize).arg(avgSize, 0, 'f', 2).arg(totalSize);

    statsLabel->setText(statsText);
    layout->addWidget(statsLabel);

    container->setLayout(layout);
#endif
}

void DataAnalysis::clearStatistics()
{
    // 获取布局容器
    QWidget* container = ui.horizontalLayoutWidget_2;
    QLayout* oldLayout = container->layout();

    // 清空原有布局
    if (oldLayout) {
        while (QLayoutItem* item = oldLayout->takeAt(0)) {
            if (item->widget()) {
                delete item->widget();
            }
            delete item;
        }
        delete oldLayout;
    }

    // 创建新布局
    QVBoxLayout* layout = new QVBoxLayout(container);

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 创建图表视图
    QChartView* chartView = new QChartView(container);
    chartView->setRenderHint(QPainter::Antialiasing);

    // 创建空图表
    QChart* chart = new QChart();
    chart->setTitle("数据分析图表");
    chartView->setChart(chart);

    layout->addWidget(chartView);
#endif

    // 添加提示标签
    QLabel* placeholderLabel = new QLabel("数据为空，请先获取数据");
    placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholderLabel);

    // 设置布局
    container->setLayout(layout);
}