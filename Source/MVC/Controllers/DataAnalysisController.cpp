// Source/MVC/Controllers/DataAnalysisController.cpp

#include "DataAnalysisController.h"
#include "DataAnalysisInterface.h"
#include "DataAnalysisView.h"
#include "DataVisualization.h"
#include "ui_DataAnalysis.h"
#include "DataAnalysisModel.h"
#include "IndexGenerator.h"
#include "DataPacket.h"
#include "Logger.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QDateTime>

DataAnalysisController::DataAnalysisController(DataAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_model(nullptr)
    , m_autoExtractFeatures(false)
    , m_featureExtractInterval(10)
    , m_dataCounter(0)
    , m_isUpdatingTable(false)
    , m_isInitialized(false)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取模型实例（单例模式）
    m_model = DataAnalysisModel::getInstance();
}

DataAnalysisController::~DataAnalysisController()
{
    // 不需要释放m_model，因为它是单例
    // 不需要释放m_ui和m_view，因为它们由外部管理
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析控制器已销毁"));
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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析控制器已初始化"));
}

void DataAnalysisController::connectSignals()
{
    if (!m_model || !m_view) {
        return;
    }

    // 连接模型信号到控制器槽
    connect(m_model, &DataAnalysisModel::signal_DA_M_dataChanged,
        this, &DataAnalysisController::slot_DA_C_onDataChanged);
    connect(m_model, &DataAnalysisModel::signal_DA_M_statisticsChanged,
        this, &DataAnalysisController::slot_DA_C_onStatisticsChanged);
    connect(m_model, &DataAnalysisModel::signal_DA_M_importCompleted,
        this, &DataAnalysisController::slot_DA_C_onImportCompleted);
    connect(m_model, &DataAnalysisModel::signal_DA_M_exportCompleted,
        this, &DataAnalysisController::slot_DA_C_onExportCompleted);
    connect(m_model, &DataAnalysisModel::signal_DA_M_featuresExtracted,
        this, &DataAnalysisController::slot_DA_C_onFeaturesExtracted);

    // 连接视图信号到控制器槽
    connect(m_view, &DataAnalysisView::signal_DA_V_videoPreviewClicked,
        this, &DataAnalysisController::slot_DA_C_onVideoPreviewClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_saveDataClicked,
        this, &DataAnalysisController::slot_DA_C_onSaveDataClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_importDataClicked,
        this, &DataAnalysisController::slot_DA_C_onImportDataClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_exportDataClicked,
        this, &DataAnalysisController::slot_DA_C_onExportDataClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_selectionChanged,
        this, &DataAnalysisController::slot_DA_C_onSelectionChanged);
    connect(m_view, &DataAnalysisView::signal_DA_V_clearDataClicked,
        this, &DataAnalysisController::slot_DA_C_onClearDataClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_loadDataFromFileRequested,
        this, &DataAnalysisController::slot_DA_C_onLoadDataFromFileRequested);

    // 连接数据变更信号到视图的实时图表更新
    connect(m_model, &DataAnalysisModel::signal_DA_M_dataChanged,
        m_view, &DataAnalysisView::slot_DA_V_updateRealtimeChart);

    // 连接新增的视图信号到控制器槽
    connect(m_view, &DataAnalysisView::signal_DA_V_realTimeUpdateToggled,
        this, &DataAnalysisController::slot_DA_C_onRealTimeUpdateToggled);
    connect(m_view, &DataAnalysisView::signal_DA_V_updateIntervalChanged,
        this, &DataAnalysisController::slot_DA_C_onUpdateIntervalChanged);
    connect(m_view, &DataAnalysisView::signal_DA_V_analyzeButtonClicked,
        this, &DataAnalysisController::slot_DA_C_onAnalyzeButtonClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_visualizeButtonClicked,
        this, &DataAnalysisController::slot_DA_C_onVisualizeButtonClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_applyFilterClicked,
        this, &DataAnalysisController::slot_DA_C_onApplyFilterClicked);
    connect(m_view, &DataAnalysisView::signal_DA_V_exportChartClicked,
        this, &DataAnalysisController::exportVisualization);
}

void DataAnalysisController::loadData()
{
    if (!m_model || !m_view) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载数据失败：模型或视图对象为空"));
        return;
    }

    // 获取模型数据
    const std::vector<DataAnalysisItem>& items = m_model->getDataItems();

    // 更新表格
    updateTable(items);

    // 更新统计信息
    slot_DA_C_onStatisticsChanged(m_model->getStatistics());

    // 更新UI状态
    m_view->slot_DA_V_updateUIState(!items.empty());

    // 更新状态栏
    m_view->slot_DA_V_updateStatusBar(
        LocalQTCompat::fromLocal8Bit("已加载数据"),
        static_cast<int>(items.size())
    );

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("已加载 %1 条数据")).arg(items.size()));
}

bool DataAnalysisController::importData(const QString& filePath)
{
    if (!m_model) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("导入数据失败：模型对象为空"));
        return false;
    }

    QString selectedFilePath = filePath;

    // 如果未提供文件路径，弹出文件选择对话框
    if (selectedFilePath.isEmpty() && m_view) {
        selectedFilePath = QFileDialog::getOpenFileName(m_view,
            LocalQTCompat::fromLocal8Bit("选择数据文件"),
            QString(),
            LocalQTCompat::fromLocal8Bit("CSV文件 (*.csv);;JSON文件 (*.json);;所有文件 (*.*)"));

        if (selectedFilePath.isEmpty()) {
            return false; // 用户取消了选择
        }
    }

    // 设置当前数据源
    setDataSource(selectedFilePath);

    // 导入数据
    return m_model->importData(selectedFilePath);
}

bool DataAnalysisController::exportData(const QString& filePath, bool selectedRowsOnly)
{
    if (!m_model) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("导出数据失败：模型对象为空"));
        return false;
    }

    QString selectedFilePath = filePath;

    // 如果未提供文件路径，弹出文件保存对话框
    if (selectedFilePath.isEmpty() && m_view) {
        selectedFilePath = QFileDialog::getSaveFileName(m_view,
            LocalQTCompat::fromLocal8Bit("保存数据文件"),
            QString(),
            LocalQTCompat::fromLocal8Bit("CSV文件 (*.csv);;JSON文件 (*.json);;所有文件 (*.*)"));

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
            LocalQTCompat::fromLocal8Bit("确认清除"),
            LocalQTCompat::fromLocal8Bit("确定要清除所有数据吗？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    // 清除模型数据
    m_model->clearDataItems();

    // 清除视图表格
    if (m_view) {
        m_view->slot_DA_V_clearDataTable();
        m_view->slot_DA_V_updateUIState(false);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据已清除"));
}

void DataAnalysisController::filterData(const QString& filterExpression)
{
    handleFilter(filterExpression);
}

bool DataAnalysisController::loadDataFromFile(const QString& filePath)
{
    if (!m_model) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载文件失败：模型对象为空"));
        return false;
    }

    // 设置当前数据源
    setDataSource(filePath);

    // 使用importData处理各种格式
    return m_model->importData(filePath);
}

void DataAnalysisController::updateTable(const std::vector<DataAnalysisItem>& items)
{
    if (!m_ui || !m_ui->tableWidget) {
        return;
    }

    m_isUpdatingTable = true;

    QTableWidget* table = m_ui->tableWidget;

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
        m_view->slot_DA_V_updateUIState(!items.empty());
    }
}

void DataAnalysisController::updateTableRow(int row, const DataAnalysisItem& item)
{
    if (!m_ui || !m_ui->tableWidget || row < 0) {
        return;
    }

    QTableWidget* table = m_ui->tableWidget;

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

    // 数据点列（最多显示8个数据点）
    for (int i = 0; i < item.dataPoints.size() && i < 8; ++i) {
        QTableWidgetItem* pointItem = new QTableWidgetItem(
            QString::number(item.dataPoints[i], 'f', 2));
        pointItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 4 + i, pointItem);
    }

    // 根据数据有效性设置行的样式
    if (!item.isValid) {
        for (int col = 0; col < table->columnCount(); ++col) {
            QTableWidgetItem* currentItem = table->item(row, col);
            if (currentItem) {
                currentItem->setForeground(QBrush(Qt::gray));
                QFont font = currentItem->font();
                font.setItalic(true);
                currentItem->setFont(font);
            }
        }
    }
}

DataPacket DataAnalysisController::createDataPacketFromItem(const DataAnalysisItem& item) const
{
    // 创建数据包
    DataPacket packet;

    // 转换QVector到std::vector
    std::vector<uint8_t> data;
    for (const double& point : item.dataPoints) {
        // 简单转换 - 实际代码中需要适当的类型转换
        data.push_back(static_cast<uint8_t>(point));
    }

    // 设置数据包数据 (TODO: 需要实现setData方法)
    // packet.setData(data.data(), data.size());

    // 解析时间戳
    QDateTime dt = QDateTime::fromString(item.timeStamp, "yyyy-MM-dd hh:mm:ss.zzz");
    packet.timestamp = dt.toMSecsSinceEpoch() * 1000000; // 转换为纳秒

    // 设置其他字段
    packet.batchId = item.index / 100; // 示例映射

    return packet;
}

QString DataAnalysisController::serializeFeatures(const QMap<QString, QVariant>& features) const
{
    QStringList featureStrings;

    for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
        QString valueStr;

        if (it.value().canConvert<double>()) {
            valueStr = QString::number(it.value().toDouble(), 'f', 4);
        }
        else if (it.value().canConvert<int>()) {
            valueStr = QString::number(it.value().toInt());
        }
        else if (it.value().canConvert<bool>()) {
            valueStr = it.value().toBool() ? "true" : "false";
        }
        else if (it.value().canConvert<QVariantList>()) {
            QStringList valueList;
            QVariantList list = it.value().toList();
            for (const QVariant& v : list) {
                valueList.append(v.toString());
            }
            valueStr = "[" + valueList.join(",") + "]";
        }
        else {
            valueStr = it.value().toString();
        }

        featureStrings.append(it.key() + ":" + valueStr);
    }

    return featureStrings.join(";");
}

QList<int> DataAnalysisController::getSelectedRows() const
{
    QList<int> selectedRows;

    if (!m_ui || !m_ui->tableWidget) {
        return selectedRows;
    }

    // 获取选中的行
    QList<QTableWidgetItem*> selectedItems = m_ui->tableWidget->selectedItems();
    for (QTableWidgetItem* item : selectedItems) {
        int row = item->row();
        if (!selectedRows.contains(row)) {
            selectedRows.append(row);
        }
    }

    return selectedRows;
}

void DataAnalysisController::processDataPackets(const std::vector<DataPacket>& packets)
{
    if (!m_model) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("处理数据包失败：模型对象为空"));
        return;
    }

    std::vector<DataAnalysisItem> items;
    for (const auto& packet : packets) {
        DataAnalysisItem item = convertPacketToAnalysisItem(packet);

        // 如果启用了自动特征提取
        if (m_autoExtractFeatures) {
            // 提取特征
            QMap<QString, QVariant> features =
                FeatureExtractor::getInstance().extractFeatures(packet);

            // 存储特征
            m_model->getFeatures(item.index) = features;

            // 如果有活动的数据源，更新索引
            if (!m_currentDataSource.isEmpty()) {
                // 添加到索引（暂时估计偏移量为0）
                int indexId = IndexGenerator::getInstance().addPacketIndex(
                    packet, 0, m_currentDataSource);

                // 添加提取的特征到索引
                for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
                    IndexGenerator::getInstance().addFeature(indexId, it.key(), it.value());
                }
            }
        }

        items.push_back(item);
    }

    // 批量添加项目到模型
    if (!items.empty()) {
        m_model->addDataItems(items);
    }
}

DataAnalysisItem DataAnalysisController::convertPacketToAnalysisItem(const DataPacket& packet)
{
    // 创建时间戳
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    // 获取数据包的第一个字节作为主值（示例）
    double value = 0.0;
    if (packet.getSize() > 0) {
        value = static_cast<double>(packet.getData()[0]);
    }

    // 创建描述
    QString description = QString(LocalQTCompat::fromLocal8Bit("数据包大小: %1 B, 批次: %2/%3"))
        .arg(packet.getSize())
        .arg(packet.batchId)
        .arg(packet.packetsInBatch);

    // 提取数据点（示例：取前8个字节作为数据点）
    QVector<double> dataPoints;
    const size_t maxPoints = std::min(packet.getSize(), static_cast<size_t>(8));
    for (size_t i = 0; i < maxPoints; ++i) {
        dataPoints.append(static_cast<double>(packet.getData()[i]));
    }

    // 创建分析项
    static int itemIndex = 0;
    return DataAnalysisItem(itemIndex++, timeStamp, value, description, dataPoints);
}

void DataAnalysisController::enableAutoFeatureExtraction(bool enable, int interval)
{
    m_autoExtractFeatures = enable;

    if (interval > 0) {
        m_featureExtractInterval = interval;
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("自动特征提取: %1, 间隔: %2项"))
        .arg(enable ? LocalQTCompat::fromLocal8Bit("启用") : LocalQTCompat::fromLocal8Bit("禁用"))
        .arg(m_featureExtractInterval));
}

void DataAnalysisController::handleFilter(const QString& filterText)
{
    if (!m_model) {
        return;
    }

    if (filterText.isEmpty()) {
        // 如果过滤条件为空，显示所有数据
        loadData();
        if (m_view) {
            m_view->slot_DA_V_updateStatusBar(
                LocalQTCompat::fromLocal8Bit("显示所有数据"),
                m_model->getDataItemCount()
            );
        }
        return;
    }

    // 应用过滤
    QVector<int> filteredIndices = m_model->filterData(filterText);

    if (filteredIndices.isEmpty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("过滤结果"),
                LocalQTCompat::fromLocal8Bit("没有找到匹配的数据")
            );
            m_view->slot_DA_V_updateStatusBar(
                LocalQTCompat::fromLocal8Bit("过滤：无匹配数据"),
                0
            );
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

    if (m_view) {
        m_view->slot_DA_V_updateStatusBar(
            QString(LocalQTCompat::fromLocal8Bit("过滤：找到 %1 条匹配数据")).arg(filteredItems.size()),
            static_cast<int>(filteredItems.size())
        );
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("过滤得到 %1 条匹配数据")).arg(filteredItems.size()));
}

void DataAnalysisController::handleAnalyzeRequest(int analyzerType)
{
    QList<int> selectedRows = getSelectedRows();
    if (selectedRows.isEmpty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("分析提示"),
                LocalQTCompat::fromLocal8Bit("请先选择要分析的数据行"),
                true
            );
        }
        return;
    }

    // 获取分析器类型
    QString analyzerName;
    switch (analyzerType) {
    case 0:
        analyzerName = "basic_statistics";
        break;
    case 1:
        analyzerName = "trend_analysis";
        break;
    case 2:
        analyzerName = "anomaly_detection";
        break;
    default:
        analyzerName = "basic_statistics";
        break;
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("开始分析选中的 %1 行数据，分析器类型: %2"))
        .arg(selectedRows.size())
        .arg(analyzerName));

    // 提取要分析的数据项
    const std::vector<DataAnalysisItem>& allItems = m_model->getDataItems();
    std::vector<DataAnalysisItem> itemsToAnalyze;

    for (int row : selectedRows) {
        if (row >= 0 && row < static_cast<int>(allItems.size())) {
            itemsToAnalyze.push_back(allItems[row]);
        }
    }

    if (itemsToAnalyze.empty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("分析错误"),
                LocalQTCompat::fromLocal8Bit("没有有效的数据进行分析"),
                true
            );
        }
        return;
    }

    // 执行分析
    AnalysisResult result = DataAnalysisManager::getInstance().analyzeBatch(
        itemsToAnalyze, analyzerName);

    if (!result.success) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("分析错误"),
                QString(LocalQTCompat::fromLocal8Bit("分析失败: %1")).arg(result.error),
                true
            );
        }
        return;
    }

    // 显示分析结果
    QString resultText = QString(LocalQTCompat::fromLocal8Bit("分析结果 (%1 项数据):\n"))
        .arg(itemsToAnalyze.size());

    // 限制显示的指标数量，防止UI过载
    int metricCount = 0;
    for (auto it = result.metrics.constBegin();
        it != result.metrics.constEnd() && metricCount < 10;
        ++it, ++metricCount) {

        QString valueStr;
        if (it.value().canConvert<double>()) {
            valueStr = QString::number(it.value().toDouble(), 'f', 2);
        }
        else {
            valueStr = it.value().toString();
        }

        resultText += QString("%1: %2\n").arg(it.key()).arg(valueStr);
    }

    if (result.metrics.size() > 10) {
        resultText += QString(LocalQTCompat::fromLocal8Bit("... 以及 %1 个其他指标"))
            .arg(result.metrics.size() - 10);
    }

    if (m_view) {
        m_view->slot_DA_V_showAnalysisResult(resultText);
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("分析完成，计算了 %1 个指标")).arg(result.metrics.size()));
}

void DataAnalysisController::handleVisualizeRequest(int chartType)
{
    if (!m_model) {
        return;
    }

    const std::vector<DataAnalysisItem>& items = m_model->getDataItems();
    if (items.empty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("可视化提示"),
                LocalQTCompat::fromLocal8Bit("没有可用数据进行可视化"),
                true
            );
        }
        return;
    }

    // 创建可视化
    DataVisualizationOptions::ChartType type;
    switch (chartType) {
    case 0:
        type = DataVisualizationOptions::LINE_CHART;
        break;
    case 1:
        type = DataVisualizationOptions::BAR_CHART;
        break;
    case 2:
        type = DataVisualizationOptions::HISTOGRAM;
        break;
    case 3:
        type = DataVisualizationOptions::SCATTER_PLOT;
        break;
    default:
        type = DataVisualizationOptions::LINE_CHART;
        break;
    }

    visualizeData(static_cast<int>(type));
}

void DataAnalysisController::extractFeaturesAndIndex(const DataAnalysisItem& item, const QString& fileName) {
    // 提取特征 - 暂时禁用以等待完整实现
#if 0
    QMap<QString, QVariant> features = FeatureExtractor::getInstance().extractFeatures(item);

    // 创建和更新索引
    if (!fileName.isEmpty()) {
        int indexId = IndexGenerator::getInstance().addPacketIndex(
            convertToDataPacket(item),
            0, // 需要从FileSaveManager获取实际偏移量
            fileName
        );

        // 添加特征到索引
        for (auto it = features.begin(); it != features.end(); ++it) {
            IndexGenerator::getInstance().addFeature(indexId, it.key(), it.value());
        }
    }

    // 更新模型
    m_model->setFeatures(item.index, features);
#endif
}

bool DataAnalysisController::loadDataFromIndex(const QString& indexPath)
{
    // 加载索引文件
    if (!IndexGenerator::getInstance().loadIndex(indexPath)) {
        return false;
    }

    // 获取索引条目
    QVector<PacketIndexEntry> entries = IndexGenerator::getInstance().getAllIndexEntries();

    // 清除当前数据
    m_model->clearDataItems();

    // 批量处理索引条目
    std::vector<DataAnalysisItem> items;
    for (const auto& entry : entries) {
        DataAnalysisItem item = convertIndexEntryToAnalysisItem(entry);
        items.push_back(item);
    }

    // 添加到模型
    m_model->addDataItems(items);

    return true;
}

DataAnalysisItem DataAnalysisController::convertIndexEntryToAnalysisItem(const PacketIndexEntry& entry)
{
    // 创建DataAnalysisItem对象并转换相关字段
    QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestamp / 1000000).toString("yyyy-MM-dd hh:mm:ss.zzz");
    double value = 0.0;

    // 如果特征中包含主值，使用它
    if (entry.features.contains("main_value")) {
        value = entry.features["main_value"].toDouble();
    }

    QVector<double> dataPoints;
    if (entry.features.contains("data_points")) {
        QVariantList points = entry.features["data_points"].toList();
        for (const QVariant& point : points) {
            dataPoints.append(point.toDouble());
        }
    }

    QString description = QString(LocalQTCompat::fromLocal8Bit("来源: %1, 偏移: %2"))
        .arg(entry.fileName).arg(entry.fileOffset);

    return DataAnalysisItem(entry.packetIndex, timestamp, value, description, dataPoints);
}

bool DataAnalysisController::exportAnalysisResults(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(QString(LocalQTCompat::fromLocal8Bit("无法打开文件进行导出: %1")).arg(filePath));
        return false;
    }

    QTextStream out(&file);

    // 写入标题行
    out << LocalQTCompat::fromLocal8Bit("索引,时间戳,数值,描述,特征,关联文件,文件偏移\n");

    // 获取所有选中行
    QList<int> selectedRows = getSelectedRows();
    const std::vector<DataAnalysisItem>& items = m_model->getDataItems();

    for (int row : selectedRows) {
        if (row >= 0 && row < static_cast<int>(items.size())) {
            const DataAnalysisItem& item = items[row];

            // 查找对应的索引条目
            // 这里需要实现查找逻辑，可能需要额外存储映射关系
            QString relatedFile = LocalQTCompat::fromLocal8Bit("未知");
            uint64_t fileOffset = 0;

            // 获取特征
            QMap<QString, QVariant> features = m_model->getFeatures(row);

            // 暂时禁用，等待完整实现
#if 0
            QString featuresStr = serializeFeatures(features);

            out << item.index << ","
                << item.timeStamp << ","
                << item.value << ","
                << item.description << ","
                << featuresStr << ","
                << relatedFile << ","
                << fileOffset << "\n";
#endif
        }
    }

    file.close();
    return true;
}

void DataAnalysisController::analyzeSelectedData()
{
    QList<int> selectedRows = getSelectedRows();

    if (selectedRows.isEmpty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("分析提示"),
                LocalQTCompat::fromLocal8Bit("请先选择要分析的数据行"),
                true
            );
        }
        return;
    }

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("开始分析选中的 %1 行数据")).arg(selectedRows.size()));

    // 提取特征
    bool success = m_model->extractFeaturesBatch(selectedRows.toVector());

    if (!success) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("分析错误"),
                LocalQTCompat::fromLocal8Bit("特征提取失败"),
                true
            );
        }
        return;
    }

    // 获取第一个选中项的特征用于展示
    int firstIndex = selectedRows.first();
    QMap<QString, QVariant> features = m_model->getFeatures(firstIndex);

    // 显示分析结果对话框
    if (m_view && !features.isEmpty()) {
        QString featureInfo;

        for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
            QString valueStr;

            if (it.value().canConvert<double>()) {
                valueStr = QString::number(it.value().toDouble(), 'f', 2);
            }
            else if (it.value().canConvert<QVariantList>()) {
                valueStr = QString(LocalQTCompat::fromLocal8Bit("[%1 个元素]")).arg(it.value().toList().size());
            }
            else {
                valueStr = it.value().toString();
            }

            featureInfo += QString("%1: %2\n").arg(it.key()).arg(valueStr);
        }

        m_view->slot_DA_V_showMessageDialog(LocalQTCompat::fromLocal8Bit("特征分析结果"), featureInfo);
    }

    // 自动生成可视化（如果特征中包含直方图数据）
    if (features.contains("histogram")) {
        visualizeData(DataVisualizationOptions::HISTOGRAM);
    }
}

void DataAnalysisController::visualizeData(int chartType)
{
    // 延迟初始化可视化组件
    if (!m_visualization) {
        m_visualization = std::make_unique<DataVisualization>();
        connect(m_visualization.get(), &DataVisualization::pointClicked,
            this, [this](double x, double y) {
                LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("图表点击事件: x=%1, y=%2")).arg(x).arg(y));
            });
    }

    // 获取所有数据项或选中的数据项
    QList<int> selectedRows = getSelectedRows();
    const std::vector<DataAnalysisItem>& allItems = m_model->getDataItems();
    std::vector<DataAnalysisItem> itemsToVisualize;

    if (selectedRows.isEmpty()) {
        // 使用所有数据
        itemsToVisualize = allItems;
    }
    else {
        // 使用选中的数据
        for (int row : selectedRows) {
            if (row >= 0 && row < static_cast<int>(allItems.size())) {
                itemsToVisualize.push_back(allItems[row]);
            }
        }
    }

    if (itemsToVisualize.empty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("可视化提示"),
                LocalQTCompat::fromLocal8Bit("没有可用数据进行可视化"),
                true
            );
        }
        return;
    }

    // 设置可视化选项
    DataVisualizationOptions options;
    options.type = static_cast<DataVisualizationOptions::ChartType>(chartType);

    switch (chartType) {
    case DataVisualizationOptions::LINE_CHART:
        options.title = LocalQTCompat::fromLocal8Bit("数据趋势图");
        options.xAxisTitle = LocalQTCompat::fromLocal8Bit("时间");
        options.yAxisTitle = LocalQTCompat::fromLocal8Bit("数值");
        break;

    case DataVisualizationOptions::BAR_CHART:
        options.title = LocalQTCompat::fromLocal8Bit("数据分布图");
        options.xAxisTitle = LocalQTCompat::fromLocal8Bit("数据项");
        options.yAxisTitle = LocalQTCompat::fromLocal8Bit("数值");
        break;

    case DataVisualizationOptions::HISTOGRAM:
        options.title = LocalQTCompat::fromLocal8Bit("直方图分析");
        options.xAxisTitle = LocalQTCompat::fromLocal8Bit("数值区间");
        options.yAxisTitle = LocalQTCompat::fromLocal8Bit("频率");
        break;

    case DataVisualizationOptions::SCATTER_PLOT:
        options.title = LocalQTCompat::fromLocal8Bit("数据散点图");
        options.xAxisTitle = LocalQTCompat::fromLocal8Bit("X值");
        options.yAxisTitle = LocalQTCompat::fromLocal8Bit("Y值");
        break;

    default:
        break;
    }

    // 生成可视化
    m_visualization->visualizeFromItems(itemsToVisualize, options);

    // 显示可视化对话框
    m_visualization->setWindowTitle(options.title);
    m_visualization->resize(800, 500);
    m_visualization->show();
    m_visualization->raise();
    m_visualization->activateWindow();

    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("已创建%1，包含 %2 个数据点"))
        .arg(options.title)
        .arg(itemsToVisualize.size()));
}

void DataAnalysisController::exportVisualization(const QString& filePath)
{
    if (!m_visualization) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("导出提示"),
                LocalQTCompat::fromLocal8Bit("请先创建可视化图表"),
                true
            );
        }
        return;
    }

    QString selectedFilePath = filePath;

    // 如果未提供文件路径，弹出文件保存对话框
    if (selectedFilePath.isEmpty() && m_view) {
        selectedFilePath = QFileDialog::getSaveFileName(m_view,
            LocalQTCompat::fromLocal8Bit("导出图表"),
            QString(),
            LocalQTCompat::fromLocal8Bit("PNG图片 (*.png);;JPG图片 (*.jpg);;所有文件 (*.*)"));

        if (selectedFilePath.isEmpty()) {
            return; // 用户取消了选择
        }
    }

    bool success = m_visualization->saveChart(selectedFilePath);

    if (m_view) {
        if (success) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("导出成功"),
                QString(LocalQTCompat::fromLocal8Bit("图表已保存到：%1")).arg(selectedFilePath)
            );
        }
        else {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("导出失败"),
                LocalQTCompat::fromLocal8Bit("保存图表时出错"),
                true
            );
        }
    }
}

/*************************** 槽函数实现 ***************************/

void DataAnalysisController::slot_DA_C_onVideoPreviewClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频预览按钮点击"));

    // 检查是否有数据
    if (!m_model || m_model->getDataItemCount() == 0) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("提示"),
                LocalQTCompat::fromLocal8Bit("没有可用的数据进行视频预览"),
                true
            );
        }
        return;
    }

    // 实际应用中，这里可能会触发一个信号给主窗口，让主窗口打开视频预览窗口
    // 例如：emit signal_DA_C_videoPreviewRequested();

    if (m_view) {
        m_view->slot_DA_V_showMessageDialog(
            LocalQTCompat::fromLocal8Bit("视频预览"),
            LocalQTCompat::fromLocal8Bit("视频预览功能正在开发中")
        );
    }
}

void DataAnalysisController::slot_DA_C_onSaveDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存数据按钮点击"));

    // 检查是否有数据
    if (!m_model || m_model->getDataItemCount() == 0) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("提示"),
                LocalQTCompat::fromLocal8Bit("没有可用的数据进行保存"),
                true
            );
        }
        return;
    }

    // 导出数据
    bool selectedOnly = !m_selectedRows.isEmpty();
    if (selectedOnly && m_view) {
        // 如果有选中行，询问是否只保存选中行
        QMessageBox::StandardButton result = QMessageBox::question(m_view,
            LocalQTCompat::fromLocal8Bit("保存选择"),
            LocalQTCompat::fromLocal8Bit("是否只保存选中的行？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        selectedOnly = (result == QMessageBox::Yes);
    }

    exportData(QString(), selectedOnly);
}

void DataAnalysisController::slot_DA_C_onImportDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("导入数据按钮点击"));
    importData();
}

void DataAnalysisController::slot_DA_C_onExportDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("导出数据按钮点击"));

    // 检查是否有数据
    if (!m_model || m_model->getDataItemCount() == 0) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("提示"),
                LocalQTCompat::fromLocal8Bit("没有可用的数据进行导出"),
                true
            );
        }
        return;
    }

    // 导出所有数据
    exportData();
}

void DataAnalysisController::slot_DA_C_onSelectionChanged(const QList<int>& selectedRows)
{
    m_selectedRows = selectedRows;
    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("表格选择变更，选中 %1 行")).arg(selectedRows.size()));
}

void DataAnalysisController::slot_DA_C_onStatisticsChanged(const StatisticsInfo& stats)
{
    if (m_view) {
        m_view->slot_DA_V_updateStatisticsDisplay(stats);
    }
}

void DataAnalysisController::slot_DA_C_onDataChanged()
{
    if (m_isUpdatingTable) {
        return; // 避免递归更新
    }

    loadData();
}

void DataAnalysisController::slot_DA_C_onImportCompleted(bool success, const QString& message)
{
    if (m_view) {
        m_view->slot_DA_V_showMessageDialog(
            success ? LocalQTCompat::fromLocal8Bit("导入成功") : LocalQTCompat::fromLocal8Bit("导入失败"),
            message,
            !success
        );
    }

    // 如果导入成功，更新UI
    if (success) {
        loadData();
    }
}

void DataAnalysisController::slot_DA_C_onExportCompleted(bool success, const QString& message)
{
    if (m_view) {
        m_view->slot_DA_V_showMessageDialog(
            success ? LocalQTCompat::fromLocal8Bit("导出成功") : LocalQTCompat::fromLocal8Bit("导出失败"),
            message,
            !success
        );
    }
}

void DataAnalysisController::slot_DA_C_onClearDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("清除数据按钮点击"));
    clearData();
}

void DataAnalysisController::slot_DA_C_onLoadDataFromFileRequested(const QString& filePath)
{
    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("从文件加载数据请求: %1")).arg(filePath));
    loadDataFromFile(filePath);
}

void DataAnalysisController::slot_DA_C_onFeaturesExtracted(int index, const QMap<QString, QVariant>& features)
{
    LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("项目 %1 的特征提取完成，共 %2 个特征"))
        .arg(index).arg(features.size()));

    // 可以在这里实现特征提取完成后的处理逻辑
}

void DataAnalysisController::slot_DA_C_onRealTimeUpdateToggled(bool enabled)
{
    if (m_view) {
        m_view->slot_DA_V_enableRealTimeUpdate(enabled);
    }
}

void DataAnalysisController::slot_DA_C_onUpdateIntervalChanged(int interval)
{
    if (m_view) {
        m_view->slot_DA_V_setUpdateInterval(interval);
    }
}

void DataAnalysisController::slot_DA_C_onAnalyzeButtonClicked(int analyzerType)
{
    handleAnalyzeRequest(analyzerType);
}

void DataAnalysisController::slot_DA_C_onVisualizeButtonClicked(int chartType)
{
    handleVisualizeRequest(chartType);
}

void DataAnalysisController::slot_DA_C_onApplyFilterClicked(const QString& filterText)
{
    handleFilter(filterText);
}