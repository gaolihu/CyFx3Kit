// Source/MVC/Controllers/DataAnalysisController.cpp
#include "DataAnalysisController.h"
#include "DataAnalysisView.h"
#include "ui_DataAnalysis.h"
#include "DataAnalysisModel.h"
#include "Logger.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QDateTime>

DataAnalysisController::DataAnalysisController(DataAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_isUpdatingTable(false)
    , m_isInitialized(false)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取或创建模型实例
    m_model = DataAnalysisModel::getInstance();
}

DataAnalysisController::~DataAnalysisController()
{
    // 不需要释放m_model，因为它是共享指针
    // 不需要释放m_ui和m_view，因为它们由外部管理
    LOG_INFO("数据分析控制器已销毁");
}

void DataAnalysisController::initialize()
{
    if (m_isInitialized || !m_ui) {
        return;
    }

    // 连接信号与槽
    connectSignals();

    // 加载数据
    loadData();

    m_isInitialized = true;
    LOG_INFO("数据分析控制器已初始化");
}

void DataAnalysisController::connectSignals()
{
    if (!m_model || !m_view) {
        return;
    }

    // 连接模型信号到控制器槽
    connect(m_model, &DataAnalysisModel::dataChanged,
        this, &DataAnalysisController::onDataChanged);
    connect(m_model, &DataAnalysisModel::statisticsChanged,
        this, &DataAnalysisController::onStatisticsChanged);
    connect(m_model, &DataAnalysisModel::importCompleted,
        this, &DataAnalysisController::onImportCompleted);
    connect(m_model, &DataAnalysisModel::exportCompleted,
        this, &DataAnalysisController::onExportCompleted);

    // 连接视图信号到控制器槽
    connect(m_view, &DataAnalysisView::videoPreviewClicked,
        this, &DataAnalysisController::onVideoPreviewClicked);
    connect(m_view, &DataAnalysisView::saveDataClicked,
        this, &DataAnalysisController::onSaveDataClicked);
    connect(m_view, &DataAnalysisView::importDataClicked,
        this, &DataAnalysisController::onImportDataClicked);
    connect(m_view, &DataAnalysisView::exportDataClicked,
        this, &DataAnalysisController::onExportDataClicked);
    connect(m_view, &DataAnalysisView::selectionChanged,
        this, &DataAnalysisController::onSelectionChanged);
}

void DataAnalysisController::loadData()
{
    if (!m_model || !m_view) {
        LOG_ERROR("加载数据失败：模型或视图对象为空");
        return;
    }

    // 获取模型数据
    const std::vector<DataAnalysisItem>& items = m_model->getDataItems();

    // 更新表格
    updateTable(items);

    // 更新统计信息
    onStatisticsChanged(m_model->getStatistics());

    // 更新UI状态
    m_view->updateUIState(!items.empty());

    LOG_INFO(QString("已加载 %1 条数据").arg(items.size()));
}

bool DataAnalysisController::importData(const QString& filePath)
{
    if (!m_model) {
        LOG_ERROR("导入数据失败：模型对象为空");
        return false;
    }

    QString selectedFilePath = filePath;

    // 如果未提供文件路径，弹出文件选择对话框
    if (selectedFilePath.isEmpty() && m_view) {
        selectedFilePath = QFileDialog::getOpenFileName(m_view,
            "选择数据文件", QString(), "CSV文件 (*.csv);;JSON文件 (*.json);;所有文件 (*.*)");

        if (selectedFilePath.isEmpty()) {
            return false; // 用户取消了选择
        }
    }

    // 导入数据
    return m_model->importData(selectedFilePath);
}

bool DataAnalysisController::exportData(const QString& filePath, bool selectedRowsOnly)
{
    if (!m_model) {
        LOG_ERROR("导出数据失败：模型对象为空");
        return false;
    }

    QString selectedFilePath = filePath;

    // 如果未提供文件路径，弹出文件保存对话框
    if (selectedFilePath.isEmpty() && m_view) {
        selectedFilePath = QFileDialog::getSaveFileName(m_view,
            "保存数据文件", QString(), "CSV文件 (*.csv);;JSON文件 (*.json);;所有文件 (*.*)");

        if (selectedFilePath.isEmpty()) {
            return false; // 用户取消了选择
        }
    }

    // 获取选中的行索引
    QVector<int> selectedIndices;
    if (selectedRowsOnly && !m_selectedRows.isEmpty()) {
        for (int row : m_selectedRows) {
            selectedIndices.append(row);
        }
    }

    // 导出数据
    return m_model->exportData(selectedFilePath, selectedIndices);
}

void DataAnalysisController::clearData()
{
    if (!m_model) {
        return;
    }

    // 显示确认对话框
    if (m_view) {
        QMessageBox::StandardButton result = QMessageBox::question(m_view,
            "确认清除", "确定要清除所有数据吗？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    // 清除模型数据
    m_model->clearDataItems();

    // 清除视图表格
    if (m_view) {
        m_view->clearDataTable();
        m_view->updateUIState(false);
    }

    LOG_INFO("数据已清除");
}

void DataAnalysisController::filterData(const QString& filterExpression)
{
    if (!m_model) {
        return;
    }

    // 应用过滤
    QVector<int> filteredIndices = m_model->filterData(filterExpression);

    if (filteredIndices.isEmpty()) {
        if (m_view) {
            m_view->showMessageDialog("过滤结果", "没有找到匹配的数据");
        }
        return;
    }

    // 获取所有数据项
    const std::vector<DataAnalysisItem>& allItems = m_model->getDataItems();

    // 创建过滤后的项目列表
    std::vector<DataAnalysisItem> filteredItems;
    for (int index : filteredIndices) {
        if (index >= 0 && index < static_cast<int>(allItems.size())) {
            filteredItems.push_back(allItems[index]);
        }
    }

    // 更新表格显示
    updateTable(filteredItems);

    LOG_INFO(QString("过滤得到 %1 条匹配数据").arg(filteredItems.size()));
}

void DataAnalysisController::onVideoPreviewClicked()
{
    LOG_INFO("视频预览按钮点击");

    // 检查是否有数据
    if (!m_model || m_model->getDataItemCount() == 0) {
        if (m_view) {
            m_view->showMessageDialog("提示", "没有可用的数据进行视频预览", true);
        }
        return;
    }

    // 实际应用中，这里可能会触发一个信号给主窗口，让主窗口打开视频预览窗口
    // 例如：emit videoPreviewRequested();

    if (m_view) {
        m_view->showMessageDialog("视频预览", "视频预览功能正在开发中");
    }
}

void DataAnalysisController::onSaveDataClicked()
{
    LOG_INFO("保存数据按钮点击");

    // 检查是否有数据
    if (!m_model || m_model->getDataItemCount() == 0) {
        if (m_view) {
            m_view->showMessageDialog("提示", "没有可用的数据进行保存", true);
        }
        return;
    }

    // 导出数据
    bool selectedOnly = !m_selectedRows.isEmpty();
    if (selectedOnly && m_view) {
        // 如果有选中行，询问是否只保存选中行
        QMessageBox::StandardButton result = QMessageBox::question(m_view,
            "保存选择", "是否只保存选中的行？", QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

        selectedOnly = (result == QMessageBox::Yes);
    }

    exportData(QString(), selectedOnly);
}

void DataAnalysisController::onImportDataClicked()
{
    LOG_INFO("导入数据按钮点击");
    importData();
}

void DataAnalysisController::onExportDataClicked()
{
    LOG_INFO("导出数据按钮点击");

    // 检查是否有数据
    if (!m_model || m_model->getDataItemCount() == 0) {
        if (m_view) {
            m_view->showMessageDialog("提示", "没有可用的数据进行导出", true);
        }
        return;
    }

    // 导出所有数据
    exportData();
}

void DataAnalysisController::onSelectionChanged(const QList<int>& selectedRows)
{
    m_selectedRows = selectedRows;
    LOG_INFO(QString("表格选择变更，选中 %1 行").arg(selectedRows.size()));
}

void DataAnalysisController::onStatisticsChanged(const StatisticsInfo& stats)
{
    if (m_view) {
        m_view->updateStatisticsDisplay(stats);
    }
}

void DataAnalysisController::onDataChanged()
{
    if (m_isUpdatingTable) {
        return; // 避免递归更新
    }

    loadData();
}

void DataAnalysisController::onImportCompleted(bool success, const QString& message)
{
    if (m_view) {
        m_view->showMessageDialog(success ? "导入成功" : "导入失败", message, !success);
    }

    // 如果导入成功，更新UI
    if (success) {
        loadData();
    }
}

void DataAnalysisController::onExportCompleted(bool success, const QString& message)
{
    if (m_view) {
        m_view->showMessageDialog(success ? "导出成功" : "导出失败", message, !success);
    }
}

void DataAnalysisController::updateTable(const std::vector<DataAnalysisItem>& items)
{
    if (!m_ui || !m_ui->tableWidget_Sel) {
        return;
    }

    m_isUpdatingTable = true;

    QTableWidget* table = m_ui->tableWidget_Sel;

    // 停止表格刷新以提高性能
    table->setUpdatesEnabled(false);

    // 清除当前表格内容
    table->clearContents();
    table->setRowCount(static_cast<int>(items.size()));

    // 填充表格数据
    for (size_t i = 0; i < items.size(); ++i) {
        updateTableRow(static_cast<int>(i), items[i]);
    }

    // 恢复表格刷新
    table->setUpdatesEnabled(true);

    m_isUpdatingTable = false;

    // 更新UI状态
    if (m_view) {
        m_view->updateUIState(!items.empty());
    }
}

void DataAnalysisController::updateTableRow(int row, const DataAnalysisItem& item)
{
    if (!m_ui || !m_ui->tableWidget_Sel || row < 0) {
        return;
    }

    QTableWidget* table = m_ui->tableWidget_Sel;

    // 检查行索引有效性
    if (row >= table->rowCount()) {
        return;
    }

    // 序号列
    QTableWidgetItem* indexItem = new QTableWidgetItem(QString::number(item.index));
    indexItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 0, indexItem);

    // 时间戳列
    QTableWidgetItem* timestampItem = new QTableWidgetItem(item.timeStamp);
    table->setItem(row, 1, timestampItem);

    // 数值列
    QTableWidgetItem* valueItem = new QTableWidgetItem(QString::number(item.value, 'f', 2));
    valueItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 2, valueItem);

    // 描述列
    QTableWidgetItem* descriptionItem = new QTableWidgetItem(item.description);
    table->setItem(row, 3, descriptionItem);

    // 数据点列
    for (int i = 0; i < item.dataPoints.size() && i < 8; ++i) {
        QTableWidgetItem* pointItem = new QTableWidgetItem(
            QString::number(item.dataPoints[i], 'f', 2));
        pointItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 4 + i, pointItem);
    }

    // 根据数据有效性设置行的样式
    if (!item.isValid) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem* item = table->item(row, col);
            if (item) {
                item->setForeground(QBrush(Qt::gray));
                QFont font = item->font();
                font.setItalic(true);
                item->setFont(font);
            }
        }
    }
}

QList<int> DataAnalysisController::getSelectedRows() const
{
    QList<int> selectedRows;

    if (!m_ui || !m_ui->tableWidget_Sel) {
        return selectedRows;
    }

    // 获取选中的行
    QList<QTableWidgetItem*> selectedItems = m_ui->tableWidget_Sel->selectedItems();
    for (QTableWidgetItem* item : selectedItems) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    return selectedRows;
}