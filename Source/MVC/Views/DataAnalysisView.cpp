// Source/MVC/Views/DataAnalysisView.cpp

#include "DataAnalysisView.h"
#include "ui_DataAnalysis.h"
#include "Logger.h"

#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QFileDialog>

DataAnalysisView::DataAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::DataAnalysisClass)
    , m_progressDialog(nullptr)
{
    ui->setupUi(this);

    // 隐藏不需要的UI元素
    simplifyUI();

    // 设置窗口标题
    setWindowTitle(LocalQTCompat::fromLocal8Bit("USB数据索引工具"));

    // 连接信号和槽
    connectSignals();

    // 初始化状态
    slot_DA_V_updateUIState(false);
    slot_DA_V_updateStatusBar(LocalQTCompat::fromLocal8Bit("就绪"), 0);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据索引视图已创建"));
}


DataAnalysisView::~DataAnalysisView()
{
    delete ui;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据索引视图已销毁"));
}

void DataAnalysisView::simplifyUI()
{
    // 简化工具栏，只保留必要按钮
    QList<QPushButton*> buttons = ui->toolbar->findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
        if (button != ui->importDataBtn &&
            button != ui->loadFromFileBtn &&
            button != ui->exportDataBtn &&
            button != ui->clearDataBtn) {
            button->hide();
        }
    }

    // 设置表格区域的标题为"USB数据索引"
    if (ui->dataTableGroupBox) {
        ui->dataTableGroupBox->setTitle(LocalQTCompat::fromLocal8Bit("USB数据索引"));
    }

    // 添加过滤区（由于显示空间增加可以保留）
    if (ui->filterLineEdit) {
        ui->filterLineEdit->setPlaceholderText(LocalQTCompat::fromLocal8Bit("输入文件名或偏移地址进行筛选..."));
    }
}

void DataAnalysisView::connectSignals()
{
    // 导入/导出/清除按钮
    connect(ui->importDataBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_importDataClicked);
    connect(ui->exportDataBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_exportDataClicked);
    connect(ui->clearDataBtn, &QPushButton::clicked, this, &DataAnalysisView::signal_DA_V_clearDataClicked);
    connect(ui->loadFromFileBtn, &QPushButton::clicked, this, &DataAnalysisView::slot_DA_V_onLoadFromFileClicked);

    // 表格选择变化
    connect(ui->tableWidget, &QTableWidget::itemSelectionChanged, this, &DataAnalysisView::slot_DA_V_onTableSelectionChanged);

    // 筛选按钮
    connect(ui->applyFilterBtn, &QPushButton::clicked, this, &DataAnalysisView::slot_DA_V_onApplyFilterClicked);
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

void DataAnalysisView::slot_DA_V_showProgressDialog(const QString& title, const QString& text, int min, int max)
{
    // 如果已有进度对话框，先关闭
    if (m_progressDialog) {
        m_progressDialog->close();
        delete m_progressDialog;
    }

    // 创建新的进度对话框
    m_progressDialog = new QProgressDialog(title, LocalQTCompat::fromLocal8Bit("取消"), min, max, this);
    m_progressDialog->setWindowTitle(title);
    m_progressDialog->setLabelText(text);
    m_progressDialog->setWindowModality(Qt::WindowModal);
    m_progressDialog->setMinimumDuration(500); // 0.5秒后显示
    m_progressDialog->setValue(min);
    m_progressDialog->show();
}

void DataAnalysisView::slot_DA_V_updateProgressDialog(int value)
{
    if (m_progressDialog) {
        m_progressDialog->setValue(value);

        // 处理Qt事件，保持UI响应
        QApplication::processEvents();
    }
}

void DataAnalysisView::slot_DA_V_hideProgressDialog()
{
    if (m_progressDialog) {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
}

void DataAnalysisView::slot_DA_V_updateUIState(bool hasData)
{
    // 导出、清除按钮在有数据时才启用
    ui->exportDataBtn->setEnabled(hasData);
    ui->clearDataBtn->setEnabled(hasData);
    ui->applyFilterBtn->setEnabled(hasData);
    ui->filterLineEdit->setEnabled(hasData);
}

void DataAnalysisView::slot_DA_V_updateStatusBar(const QString& statusText, int dataCount)
{
    ui->statusLabel->setText(statusText);
    ui->dataCountLabel->setText(QString(LocalQTCompat::fromLocal8Bit("%1 条索引")).arg(dataCount));
}

QList<int> DataAnalysisView::getSelectedRows() const
{
    QList<int> selectedRows;

    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    for (QTableWidgetItem* item : selectedItems) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    return selectedRows;
}

void DataAnalysisView::slot_DA_V_onTableSelectionChanged()
{
    QList<int> selectedRows = getSelectedRows();

    // 发送信号
    emit signal_DA_V_selectionChanged(selectedRows);
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
        LocalQTCompat::fromLocal8Bit("FX3数据文件 (*.raw *.bin);;索引文件 (*.idx *.json);;所有文件 (*.*)")
    );

    if (!fileName.isEmpty()) {
        emit signal_DA_V_loadDataFromFileRequested(fileName);
    }
}