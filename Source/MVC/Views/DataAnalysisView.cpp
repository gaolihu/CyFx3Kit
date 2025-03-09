// Source/MVC/Views/DataAnalysisView.cpp
#include "DataAnalysisView.h"
#include "ui_DataAnalysis.h"
#include "DataAnalysisController.h"
#include "DataAnalysisModel.h"
#include "Logger.h"

#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>

DataAnalysisView::DataAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DataAnalysisClass)
    , m_statisticsPanel(nullptr)
{
    ui->setupUi(this);

    initializeUI();

    // 创建控制器
    m_controller = std::make_unique<DataAnalysisController>(this);

    // 初始化控制器
    m_controller->initialize();

    LOG_INFO("数据分析视图已创建");
}

DataAnalysisView::~DataAnalysisView()
{
    delete ui;
    LOG_INFO("数据分析视图已销毁");
}

void DataAnalysisView::initializeUI()
{
    // 设置窗口标题
    setWindowTitle("数据分析");

    // 初始化表格
    initializeTable();

    // 创建统计信息面板
    createStatisticsPanel();

    // 连接信号和槽
    connectSignals();

    // 设置初始状态
    updateUIState(false);
}

void DataAnalysisView::initializeTable()
{
    // 配置表格
    QTableWidget* table = ui->tableWidget_Sel;

    // 设置列标题
    QStringList headers = { "序号", "时间戳", "数值", "描述",
                           "数据点1", "数据点2", "数据点3", "数据点4",
                           "数据点5", "数据点6", "数据点7", "数据点8" };
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
}

void DataAnalysisView::createStatisticsPanel()
{
    // 获取统计信息分组框
    QGroupBox* statsGroupBox = ui->statisticsGroupBox;
    if (!statsGroupBox) {
        LOG_ERROR("统计信息分组框不存在");
        return;
    }

    // 创建统计面板
    m_statisticsPanel = new QWidget(statsGroupBox);
    QVBoxLayout* statsLayout = new QVBoxLayout(m_statisticsPanel);

    // 创建统计项目
    QStringList statsLabels = { "数据项数量:", "最小值:", "最大值:",
                               "平均值:", "中位数:", "标准差:" };

    for (const QString& label : statsLabels) {
        QHBoxLayout* itemLayout = new QHBoxLayout();

        QLabel* nameLabel = new QLabel(label);
        nameLabel->setMinimumWidth(100);

        QLabel* valueLabel = new QLabel("--");
        valueLabel->setObjectName(label.trimmed().replace(":", "Label"));

        itemLayout->addWidget(nameLabel);
        itemLayout->addWidget(valueLabel);
        itemLayout->addStretch(1);

        statsLayout->addLayout(itemLayout);
    }

    // 添加按钮和操作区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* importBtn = new QPushButton("导入数据");
    importBtn->setObjectName("importDataBtn");

    QPushButton* exportBtn = new QPushButton("导出数据");
    exportBtn->setObjectName("exportDataBtn");

    QPushButton* clearBtn = new QPushButton("清除数据");
    clearBtn->setObjectName("clearDataBtn");

    buttonLayout->addWidget(importBtn);
    buttonLayout->addWidget(exportBtn);
    buttonLayout->addWidget(clearBtn);

    statsLayout->addStretch(1);
    statsLayout->addLayout(buttonLayout);

    // 将统计面板添加到统计信息分组框
    QVBoxLayout* groupBoxLayout = new QVBoxLayout(statsGroupBox);
    groupBoxLayout->addWidget(m_statisticsPanel);
    statsGroupBox->setLayout(groupBoxLayout);

    // 连接信号
    connect(importBtn, &QPushButton::clicked, this, &DataAnalysisView::importDataClicked);
    connect(exportBtn, &QPushButton::clicked, this, &DataAnalysisView::exportDataClicked);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_controller) {
            m_controller->clearData();
        }
        });
}

void DataAnalysisView::connectSignals()
{
    // 连接视频预览按钮
    connect(ui->videoShowbtn, &QPushButton::clicked,
        this, &DataAnalysisView::videoPreviewClicked);

    // 连接保存数据按钮
    connect(ui->saveDatabtn, &QPushButton::clicked,
        this, &DataAnalysisView::saveDataClicked);

    // 表格选择变化
    connect(ui->tableWidget_Sel, &QTableWidget::itemSelectionChanged, this, [this]() {
        QList<int> selectedRows;
        QList<QTableWidgetItem*> selectedItems = ui->tableWidget_Sel->selectedItems();

        for (QTableWidgetItem* item : selectedItems) {
            int row = item->row();
            if (!selectedRows.contains(row)) {
                selectedRows.append(row);
            }
        }

        emit selectionChanged(selectedRows);
        });
}

void DataAnalysisView::updateStatisticsDisplay(const StatisticsInfo& info)
{
    // 查找并更新统计信息标签
    QLabel* countLabel = m_statisticsPanel->findChild<QLabel*>("数据项数量Label");
    QLabel* minLabel = m_statisticsPanel->findChild<QLabel*>("最小值Label");
    QLabel* maxLabel = m_statisticsPanel->findChild<QLabel*>("最大值Label");
    QLabel* avgLabel = m_statisticsPanel->findChild<QLabel*>("平均值Label");
    QLabel* medianLabel = m_statisticsPanel->findChild<QLabel*>("中位数Label");
    QLabel* stddevLabel = m_statisticsPanel->findChild<QLabel*>("标准差Label");

    if (countLabel) {
        countLabel->setText(QString::number(info.count));
    }

    if (minLabel) {
        minLabel->setText(QString::number(info.min, 'f', 2));
    }

    if (maxLabel) {
        maxLabel->setText(QString::number(info.max, 'f', 2));
    }

    if (avgLabel) {
        avgLabel->setText(QString::number(info.average, 'f', 2));
    }

    if (medianLabel) {
        medianLabel->setText(QString::number(info.median, 'f', 2));
    }

    if (stddevLabel) {
        stddevLabel->setText(QString::number(info.stdDeviation, 'f', 2));
    }
}

void DataAnalysisView::showMessageDialog(const QString& title, const QString& message, bool isError)
{
    if (isError) {
        QMessageBox::critical(this, title, message);
    }
    else {
        QMessageBox::information(this, title, message);
    }
}

void DataAnalysisView::clearDataTable()
{
    ui->tableWidget_Sel->setRowCount(0);
}

void DataAnalysisView::updateUIState(bool hasData)
{
    // 视频预览和保存数据按钮应该在有数据时才启用
    ui->videoShowbtn->setEnabled(hasData);
    ui->saveDatabtn->setEnabled(hasData);

    // 查找导出按钮，也应该在有数据时才启用
    QPushButton* exportBtn = m_statisticsPanel->findChild<QPushButton*>("exportDataBtn");
    if (exportBtn) {
        exportBtn->setEnabled(hasData);
    }

    // 查找清除按钮，也应该在有数据时才启用
    QPushButton* clearBtn = m_statisticsPanel->findChild<QPushButton*>("clearDataBtn");
    if (clearBtn) {
        clearBtn->setEnabled(hasData);
    }
}