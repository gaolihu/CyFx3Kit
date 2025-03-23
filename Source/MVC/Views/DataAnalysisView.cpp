// Source/MVC/Views/DataAnalysisView.cpp

#include "DataAnalysisView.h"
#include "ui_DataAnalysis.h"
#include "DataVisualization.h"
#include "DataAnalysisController.h"
#include "DataAnalysisModel.h"
#include "Logger.h"

#include <QMessageBox>
#include <QProgressBar>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QtCharts/QChartView>
#include <QVBoxLayout>
#include <QFileDialog>

DataAnalysisView::DataAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DataAnalysisClass)
    , m_updateTimer(nullptr)
    , m_realTimeUpdateEnabled(false)
    , m_updateInterval(1000)
    , m_realtimeVisualizationEnabled(false)
{
    ui->setupUi(this);

    initializeUI();

    // 创建自动更新定时器
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        if (m_controller && m_realTimeUpdateEnabled) {
            // 刷新表格显示
            m_controller->loadData();
        }
        });

    // 创建控制器
    m_controller = std::make_unique<DataAnalysisController>(this);

    // 初始化控制器
    m_controller->initialize();

    // 设置分割器初始比例
    QList<int> sizes;
    sizes << 45 << 55;  // 左侧表格占45%，右侧分析区域占55%
    ui->mainSplitter->setSizes(sizes);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析视图已创建"));
}


DataAnalysisView::~DataAnalysisView()
{
    delete ui;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析视图已销毁"));
}

void DataAnalysisView::initializeUI()
{
    // 设置窗口标题
    setWindowTitle(LocalQTCompat::fromLocal8Bit("数据分析"));

    // 初始化表格
    initializeTable();

    // 连接信号和槽
    connectSignals();

    // 设置初始状态
    slot_DA_V_updateUIState(false);

    // 更新状态栏
    slot_DA_V_updateStatusBar(LocalQTCompat::fromLocal8Bit("就绪"), 0);

    // 默认禁用实时更新
    ui->realTimeUpdateCheckBox->setChecked(false);
    ui->updateIntervalSpinBox->setValue(1000);

    // 设置统计信息表格内容
    slot_DA_V_updateStatisticsDisplay(StatisticsInfo());

    // 为图表类型添加图标
    ui->chartTypeComboBox->setIconSize(QSize(16, 16));
    ui->chartTypeComboBox->setItemIcon(0, QIcon(":/icons/line_chart.png"));
    ui->chartTypeComboBox->setItemIcon(1, QIcon(":/icons/bar_chart.png"));
    ui->chartTypeComboBox->setItemIcon(2, QIcon(":/icons/histogram.png"));
    ui->chartTypeComboBox->setItemIcon(3, QIcon(":/icons/scatter_plot.png"));

    // 美化生成可视化按钮
    ui->visualizeBtn->setIcon(QIcon(":/icons/chart.png"));
    ui->visualizeBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; border-radius: 4px; }"
        "QPushButton:hover { background-color: #45a049; }"
    );

    // 使用图标美化标签页
    ui->analysisTabWidget->setTabIcon(0, QIcon(":/icons/stats.png"));
    ui->analysisTabWidget->setTabIcon(1, QIcon(":/icons/chart.png"));

    // 优化标签页样式
    ui->analysisTabWidget->setStyleSheet(
        "QTabBar::tab { height: 28px; padding: 2px 8px; }"
        "QTabBar::tab:selected { background-color: #e7f0fa; }"
    );
}

void DataAnalysisView::initializeTable()
{
    // 配置表格
    QTableWidget* table = ui->tableWidget;

    // 设置列标题
    QStringList headers = {
        LocalQTCompat::fromLocal8Bit("序号"),
        LocalQTCompat::fromLocal8Bit("时间戳"),
        LocalQTCompat::fromLocal8Bit("数值"),
        LocalQTCompat::fromLocal8Bit("描述"),
        LocalQTCompat::fromLocal8Bit("数据点1"),
        LocalQTCompat::fromLocal8Bit("数据点2"),
        LocalQTCompat::fromLocal8Bit("数据点3"),
        LocalQTCompat::fromLocal8Bit("数据点4"),
        LocalQTCompat::fromLocal8Bit("数据点5"),
        LocalQTCompat::fromLocal8Bit("数据点6"),
        LocalQTCompat::fromLocal8Bit("数据点7"),
        LocalQTCompat::fromLocal8Bit("数据点8")
    };
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);

    // 设置列宽
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table->horizontalHeader()->setStretchLastSection(true);

    // 设置行高
    table->verticalHeader()->setDefaultSectionSize(25);

    // 设置选择模式
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 设置行数
    table->setRowCount(0);

    // 设置交替行颜色
    table->setAlternatingRowColors(true);

    // 设置交替行颜色使用更柔和的配色
    QPalette palette = ui->tableWidget->palette();
    palette.setColor(QPalette::AlternateBase, QColor(245, 249, 252));
    ui->tableWidget->setPalette(palette);

    // 调整行高以显示更多数据
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(24);

    // 设置空表格的友好提示
    ui->tableWidget->setRowCount(0);
    if (ui->tableWidget->rowCount() == 0) {
        ui->tableWidget->setColumnCount(1);
        ui->tableWidget->setRowCount(1);
        QTableWidgetItem* item = new QTableWidgetItem(
            LocalQTCompat::fromLocal8Bit("请导入数据或从文件加载数据"));
        item->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(0, 0, item);
        ui->tableWidget->setSpan(0, 0, 1, 1);
    }
}

void DataAnalysisView::connectSignals()
{
    // 导入/导出/清除按钮
    connect(ui->importDataBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_importDataClicked);
    connect(ui->exportDataBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_exportDataClicked);
    connect(ui->clearDataBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_clearDataClicked);

    // 视频预览按钮
    connect(ui->videoShowBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_videoPreviewClicked);

    // 实时图表按钮
    connect(ui->realTimeChartBtn, &QPushButton::toggled, this, &DataAnalysisView::slot_DA_V_onRealTimeChartToggled);

    // 实时更新控件
    connect(ui->realTimeUpdateCheckBox, &QCheckBox::toggled, this, &DataAnalysisView::slot_DA_V_onRealTimeUpdateToggled);
    connect(ui->updateIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &DataAnalysisView::slot_DA_V_onUpdateIntervalChanged);

    // 表格选择变化
    connect(ui->tableWidget, &QTableWidget::itemSelectionChanged, this, &DataAnalysisView::slot_DA_V_onTableSelectionChanged);

    // 分析按钮
    connect(ui->analyzeBtn, &QPushButton::clicked, this, &DataAnalysisView::slot_DA_V_onAnalyzeButtonClicked);

    // 可视化按钮
    connect(ui->visualizeBtn, &QPushButton::clicked, this, &DataAnalysisView::slot_DA_V_onVisualizeButtonClicked);

    // 导出图表按钮
    connect(ui->exportChartBtn, &QPushButton::clicked, this, [this]() {
        emit signal_DA_V_exportChartClicked();
        });

    // 筛选按钮
    connect(ui->applyFilterBtn, &QPushButton::clicked, this, &DataAnalysisView::slot_DA_V_onApplyFilterClicked);

    // 连接按钮信号
    //connect(ui->loadFromFileBtn, &QPushButton::clicked, this, &DataAnalysisView::slot_DA_V_onLoadFromFileClicked);
}

void DataAnalysisView::slot_DA_V_updateStatisticsDisplay(const StatisticsInfo& info)
{
    // 统计数值使用更醒目的显示方式
    QString valueTemplate = "<span style='color:#2c6fbd; font-weight:bold;'>%1</span>";

    ui->countValueLabel->setText(QString::number(info.count));
    ui->minValueLabel->setText(valueTemplate.arg(QString::number(info.min, 'f', 2)));
    ui->maxValueLabel->setText(valueTemplate.arg(QString::number(info.max, 'f', 2)));
    ui->avgValueLabel->setText(valueTemplate.arg(QString::number(info.average, 'f', 2)));
    ui->medianValueLabel->setText(valueTemplate.arg(QString::number(info.median, 'f', 2)));
    ui->stdDevValueLabel->setText(valueTemplate.arg(QString::number(info.stdDeviation, 'f', 2)));

    // 添加简单的进度条可视化最大/最小/平均值的相对位置
    if (info.count > 0 && (info.max - info.min) > 0.001) {
        // 如果数据有效，显示可视化进度条
        QProgressBar* avgPositionBar = new QProgressBar(ui->basicStatsGroupBox);
        avgPositionBar->setTextVisible(false);
        avgPositionBar->setRange(0, 100);

        // 计算平均值的相对位置
        int position = int(((info.average - info.min) / (info.max - info.min)) * 100);
        avgPositionBar->setValue(position);

        // 添加到布局
        // 假设在ui中已经有一个用于此目的的布局
        if (ui->basicStatsLayout->count() > 6) {  // 移除旧的进度条
            QLayoutItem* item = ui->basicStatsLayout->takeAt(6);
            if (item) {
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
        }
        ui->basicStatsLayout->addWidget(avgPositionBar, 6, 0, 1, 2);
    }
}

void DataAnalysisView::slot_DA_V_showMessageDialog(const QString& title, const QString& message, bool isError)
{
    if (isError) {
        QMessageBox::critical(this, title, message);
    }
    else {
        QMessageBox::information(this, title, message);
    }
}

void DataAnalysisView::slot_DA_V_clearDataTable()
{
    ui->tableWidget->setRowCount(0);
    slot_DA_V_updateStatisticsDisplay(StatisticsInfo());
    slot_DA_V_updateStatusBar(LocalQTCompat::fromLocal8Bit("数据已清除"), 0);
}

void DataAnalysisView::slot_DA_V_updateUIState(bool hasData)
{
    // 导出、清除、视频预览按钮在有数据时才启用
    ui->exportDataBtn->setEnabled(hasData);
    ui->clearDataBtn->setEnabled(hasData);
    ui->videoShowBtn->setEnabled(hasData);

    // 分析按钮需要有数据和选中行
    ui->analyzeBtn->setEnabled(hasData && !m_selectedRows.isEmpty());

    // 可视化按钮和导出图表按钮在有数据时才启用
    ui->visualizeBtn->setEnabled(hasData);
    ui->exportChartBtn->setEnabled(hasData);
}

void DataAnalysisView::slot_DA_V_updateStatusBar(const QString& statusText, int dataCount)
{
    ui->statusLabel->setText(statusText);
    ui->dataCountLabel->setText(QString(LocalQTCompat::fromLocal8Bit("%1 项数据")).arg(dataCount));
}

void DataAnalysisView::slot_DA_V_enableRealTimeUpdate(bool enable)
{
    m_realTimeUpdateEnabled = enable;
    ui->realTimeUpdateCheckBox->setChecked(enable);

    if (m_realTimeUpdateEnabled && m_updateInterval > 0) {
        m_updateTimer->start(m_updateInterval);
    }
    else {
        m_updateTimer->stop();
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("实时数据更新: %1"))
        .arg(enable ? LocalQTCompat::fromLocal8Bit("启用") : LocalQTCompat::fromLocal8Bit("禁用")));
}

void DataAnalysisView::slot_DA_V_setUpdateInterval(int msec)
{
    if (msec <= 0) {
        return;
    }

    m_updateInterval = msec;
    ui->updateIntervalSpinBox->setValue(msec);

    if (m_realTimeUpdateEnabled) {
        m_updateTimer->start(m_updateInterval);
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("数据更新间隔设置为: %1 ms")).arg(msec));
}

void DataAnalysisView::slot_DA_V_enableRealtimeVisualization(bool enable)
{
    m_realtimeVisualizationEnabled = enable;
    ui->realTimeChartBtn->setChecked(enable);

    if (enable) {
        createRealtimeVisualization();

        if (m_realtimeVisualization) {
            // 启用实时更新
            slot_DA_V_enableRealTimeUpdate(true);

            // 显示可视化窗口
            m_realtimeVisualization->show();
            m_realtimeVisualization->raise();

            // 设置初始数据
            const std::vector<DataAnalysisItem>& items = DataAnalysisModel::getInstance()->getDataItems();

            if (!items.empty()) {
                // 设置图表选项
                DataVisualizationOptions options;
                options.type = DataVisualizationOptions::LINE_CHART;
                options.title = LocalQTCompat::fromLocal8Bit("实时数据趋势");
                options.xAxisTitle = LocalQTCompat::fromLocal8Bit("时间");
                options.yAxisTitle = LocalQTCompat::fromLocal8Bit("数值");

                m_realtimeVisualization->visualizeFromItems(items, options);
            }
        }
    }
    else {
        // 如果取消实时可视化，隐藏窗口
        if (m_realtimeVisualization) {
            m_realtimeVisualization->hide();
        }
    }

    emit signal_DA_V_realtimeVisualizationChanged(enable);
}

void DataAnalysisView::slot_DA_V_updateRealtimeChart()
{
    if (m_realtimeVisualizationEnabled && m_realtimeVisualization) {
        const std::vector<DataAnalysisItem>& items = DataAnalysisModel::getInstance()->getDataItems();

        if (!items.empty()) {
            // 设置图表选项
            DataVisualizationOptions options;
            options.type = DataVisualizationOptions::LINE_CHART;
            options.title = LocalQTCompat::fromLocal8Bit("实时数据趋势");
            options.xAxisTitle = LocalQTCompat::fromLocal8Bit("时间");
            options.yAxisTitle = LocalQTCompat::fromLocal8Bit("数值");

            m_realtimeVisualization->visualizeFromItems(items, options);
        }
    }
}

void DataAnalysisView::slot_DA_V_showChartInTab(QChartView* chartView)
{
    if (!chartView) {
        return;
    }

    // 清除当前图表布局中的所有项目
    QLayoutItem* item;
    while ((item = ui->chartLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->setParent(nullptr);
        }
        delete item;
    }

    // 优化图表设置
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->chart()->setBackgroundVisible(false);
    chartView->chart()->legend()->setAlignment(Qt::AlignBottom);
    chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 添加图表视图到布局
    ui->chartLayout->addWidget(chartView);

    // 切换到可视化标签页
    ui->analysisTabWidget->setCurrentIndex(1);
}

void DataAnalysisView::slot_DA_V_showAnalysisResult(const QString& resultText)
{
    ui->analysisResultLabel->setText(resultText);
}

void DataAnalysisView::createRealtimeVisualization()
{
    if (!m_realtimeVisualization) {
        m_realtimeVisualization = std::make_unique<DataVisualization>(nullptr);
        m_realtimeVisualization->setAttribute(Qt::WA_DeleteOnClose, false);
        m_realtimeVisualization->setWindowTitle(LocalQTCompat::fromLocal8Bit("实时数据可视化"));

        // 使用更大的默认窗口尺寸
        m_realtimeVisualization->resize(900, 500);

        // 应用图表样式选项
        // 注意：这些函数需要在DataVisualization类中实现
        /*
        if (ui->antialiasCheckBox && ui->antialiasCheckBox->isChecked()) {
            m_realtimeVisualization->enableAntialiasing(true);
        }

        if (ui->legendCheckBox && !ui->legendCheckBox->isChecked()) {
            m_realtimeVisualization->showLegend(false);
        }

        if (ui->gridCheckBox && !ui->gridCheckBox->isChecked()) {
            m_realtimeVisualization->showGrid(false);
        }

        if (ui->chartStyleComboBox) {
            m_realtimeVisualization->setChartStyle(ui->chartStyleComboBox->currentIndex());
        }
        */

        // 连接关闭事件
        connect(m_realtimeVisualization.get(), &QWidget::destroyed, this, [this]() {
            m_realtimeVisualizationEnabled = false;
            ui->realTimeChartBtn->setChecked(false);
            });
    }
}

// 槽函数实现
void DataAnalysisView::slot_DA_V_onRealTimeUpdateToggled(bool checked)
{
    // 更新内部状态
    m_realTimeUpdateEnabled = checked;

    // 更新计时器状态
    if (checked && m_updateInterval > 0) {
        m_updateTimer->start(m_updateInterval);
    }
    else {
        m_updateTimer->stop();
    }

    // 发送信号
    emit signal_DA_V_realTimeUpdateToggled(checked);

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("实时数据更新: %1"))
        .arg(checked ? LocalQTCompat::fromLocal8Bit("启用") : LocalQTCompat::fromLocal8Bit("禁用")));
}

void DataAnalysisView::slot_DA_V_onUpdateIntervalChanged(int interval)
{
    // 更新内部状态
    m_updateInterval = interval;

    // 如果正在实时更新，重启计时器
    if (m_realTimeUpdateEnabled) {
        m_updateTimer->start(interval);
    }

    // 发送信号
    emit signal_DA_V_updateIntervalChanged(interval);

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("数据更新间隔设置为: %1 ms")).arg(interval));
}

void DataAnalysisView::slot_DA_V_onRealTimeChartToggled(bool checked)
{
    slot_DA_V_enableRealtimeVisualization(checked);
}

void DataAnalysisView::slot_DA_V_onTableSelectionChanged()
{
    // 获取选中的行索引
    QList<int> selectedRows;
    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();

    for (QTableWidgetItem* item : selectedItems) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    // 更新内部状态
    m_selectedRows = selectedRows;

    // 更新UI状态（分析按钮需要有选中行）
    ui->analyzeBtn->setEnabled(!selectedRows.isEmpty());

    // 发送信号
    emit signal_DA_V_selectionChanged(selectedRows);
}

void DataAnalysisView::slot_DA_V_onAnalyzeButtonClicked()
{
    if (m_selectedRows.isEmpty()) {
        slot_DA_V_showMessageDialog(
            LocalQTCompat::fromLocal8Bit("提示"),
            LocalQTCompat::fromLocal8Bit("请先选择要分析的数据行"),
            true
        );
        return;
    }

    // 获取当前选择的分析器类型
    int analyzerType = ui->analyzerComboBox->currentIndex();

    // 发送信号
    emit signal_DA_V_analyzeButtonClicked(analyzerType);
}

void DataAnalysisView::slot_DA_V_onVisualizeButtonClicked()
{
    // 获取当前选择的图表类型
    int chartType = ui->chartTypeComboBox->currentIndex();

    // 发送信号
    emit signal_DA_V_visualizeButtonClicked(chartType);
}

void DataAnalysisView::slot_DA_V_onApplyFilterClicked()
{
    // 获取筛选文本
    QString filterText = ui->filterLineEdit->text().trimmed();

    // 发送信号
    emit signal_DA_V_applyFilterClicked(filterText);
}

void DataAnalysisView::slot_DA_V_onLoadFromFileClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        LocalQTCompat::fromLocal8Bit("选择数据文件"),
        QString(),
        LocalQTCompat::fromLocal8Bit("RAW文件 (*.raw);;CSV文件 (*.csv);;所有文件 (*.*)")
    );

    if (!fileName.isEmpty()) {
        emit signal_DA_V_loadDataFromFileRequested(fileName);
    }
}