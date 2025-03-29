// Source/MVC/Controllers/DataAnalysisController.cpp

#include "DataAnalysisController.h"
#include "DataAnalysisView.h"
#include "ui_DataAnalysis.h"
#include "DataAnalysisModel.h"
#include "IndexGenerator.h"
#include "DataPacket.h"
#include "Logger.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QThread>
#include <QCoreApplication>
#include <QMetaObject>
#include <QApplication>

DataAnalysisController::DataAnalysisController(DataAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_model(nullptr)
    , m_dataCounter(0)
    , m_isUpdatingTable(false)
    , m_isInitialized(false)
    , m_maxBatchSize(500)
    , m_processingData(false)
    , m_batchLoadPosition(0)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取模型实例（单例模式）
    m_model = DataAnalysisModel::getInstance();

    // 连接异步处理完成信号
    connect(&m_processWatcher, &QFutureWatcher<int>::finished,
        this, &DataAnalysisController::slot_DA_C_onProcessingFinished);

    // 连接批量加载完成信号
    connect(&m_loadWatcher, &QFutureWatcher<void>::finished,
        this, &DataAnalysisController::slot_DA_C_onBatchLoadFinished);

    // 连接文件监视器
    connect(&m_fileWatcher, &QFileSystemWatcher::fileChanged,
        this, &DataAnalysisController::slot_DA_C_onIndexFileChanged);

    // 启动性能计时器
    m_performanceTimer.start();
}

DataAnalysisController::~DataAnalysisController()
{
    // 等待任何未完成的异步操作
    if (m_processWatcher.isRunning()) {
        m_processWatcher.waitForFinished();
    }

    if (m_loadWatcher.isRunning()) {
        m_loadWatcher.waitForFinished();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析控制器已销毁"));
}

void DataAnalysisController::initialize()
{
    if (m_isInitialized || !m_ui) {
        return;
    }

    // 连接信号与槽
    connectSignals();

    // 初始化表格
    initializeTable();

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
    connect(m_model, &DataAnalysisModel::signal_DA_M_importCompleted,
        this, &DataAnalysisController::slot_DA_C_onImportCompleted);

    // 连接视图信号到控制器槽
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

    // 连接索引生成器信号
    connect(&IndexGenerator::getInstance(), &IndexGenerator::indexEntryAdded,
        this, &DataAnalysisController::slot_DA_C_onIndexEntryAdded, Qt::QueuedConnection);
    connect(&IndexGenerator::getInstance(), &IndexGenerator::indexUpdated,
        this, &DataAnalysisController::slot_DA_C_onIndexUpdated, Qt::QueuedConnection);
}

void DataAnalysisController::initializeTable()
{
    if (!m_ui || !m_ui->tableWidget) {
        return;
    }

    QTableWidget* table = m_ui->tableWidget;

    // 设置列标题
    QStringList headers = {
        LocalQTCompat::fromLocal8Bit("索引ID"),
        LocalQTCompat::fromLocal8Bit("时间戳"),
        LocalQTCompat::fromLocal8Bit("文件偏移"),
        LocalQTCompat::fromLocal8Bit("大小(字节)"),
        LocalQTCompat::fromLocal8Bit("文件名"),
        LocalQTCompat::fromLocal8Bit("批次ID"),
        LocalQTCompat::fromLocal8Bit("包序号")
    };
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);

    // 设置列宽
    table->setColumnWidth(0, 60);  // 索引ID
    table->setColumnWidth(1, 150); // 时间戳
    table->setColumnWidth(2, 120); // 文件偏移
    table->setColumnWidth(3, 80);  // 大小
    table->setColumnWidth(4, 200); // 文件名
    table->setColumnWidth(5, 60);  // 批次ID
    table->setColumnWidth(6, 60);  // 包序号

    // 设置选择模式
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // 设置交替行颜色
    table->setAlternatingRowColors(true);
}

void DataAnalysisController::loadData()
{
    if (!m_model || !m_view || !m_ui || !m_ui->tableWidget) {
        return;
    }

    // 避免在处理中重复加载
    if (m_loadWatcher.isRunning()) {
        return;
    }

    // 更新状态栏，显示正在加载
    if (m_view) {
        m_view->slot_DA_V_updateStatusBar(
            LocalQTCompat::fromLocal8Bit("正在加载索引数据..."),
            0
        );
    }

    // 异步获取索引条目
    QFuture<void> future = QtConcurrent::run([this]() {
        try {
            // 获取所有索引条目
            QVector<PacketIndexEntry> entries = IndexGenerator::getInstance().getAllIndexEntries();

            // 在UI线程中更新表格
            QMetaObject::invokeMethod(this, [this, entries]() {
                // 使用分批加载机制
                loadDataBatched(entries, m_maxBatchSize);

                if (m_view) {
                    m_view->slot_DA_V_updateStatusBar(
                        LocalQTCompat::fromLocal8Bit("索引数据加载中..."),
                        entries.size()
                    );
                }

                LOG_INFO(LocalQTCompat::fromLocal8Bit("已请求加载 %1 条索引数据").arg(entries.size()));
                }, Qt::QueuedConnection);
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载索引数据时发生异常: %1").arg(e.what()));

            // 在UI线程中显示错误
            QMetaObject::invokeMethod(this, [this, e]() {
                if (m_view) {
                    m_view->slot_DA_V_showMessageDialog(
                        LocalQTCompat::fromLocal8Bit("加载错误"),
                        LocalQTCompat::fromLocal8Bit("加载索引数据失败: %1").arg(e.what()),
                        true
                    );
                }
                }, Qt::QueuedConnection);
        }
        });

    m_loadWatcher.setFuture(future);
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
            LocalQTCompat::fromLocal8Bit("二进制文件 (*.bin *.raw);;所有文件 (*.*)"));

        if (selectedFilePath.isEmpty()) {
            return false; // 用户取消了选择
        }
    }

    // 设置当前数据源，使用会话ID+文件名以确保索引关联
    QString fileName = QFileInfo(selectedFilePath).fileName();
    if (!m_sessionId.isEmpty()) {
        setDataSource(m_sessionId + "_" + fileName);
    }
    else {
        setDataSource(selectedFilePath);
    }

    // 打开索引文件 - 使用会话索引或文件特定索引
    QString indexPath;
    if (!m_sessionId.isEmpty() && !m_sessionBasePath.isEmpty()) {
        // 配置IndexGenerator
        IndexGenerator::getInstance().setSessionId(m_sessionId);
        IndexGenerator::getInstance().setBasePath(m_sessionBasePath);

        indexPath = QString("%1/%2.idx").arg(m_sessionBasePath).arg(m_sessionId);
    }
    else {
        indexPath = selectedFilePath + ".idx";
    }

    if (!IndexGenerator::getInstance().open(indexPath)) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("索引创建失败"),
                LocalQTCompat::fromLocal8Bit("无法创建索引文件"),
                true
            );
        }
        return false;
    }

    // 打开并读取数据文件
    QFile file(selectedFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开数据文件: %1 - %2")
            .arg(selectedFilePath).arg(file.errorString()));

        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("文件打开失败"),
                LocalQTCompat::fromLocal8Bit("无法打开数据文件: %1").arg(file.errorString()),
                true
            );
        }
        return false;
    }

    if (m_view) {
        m_view->slot_DA_V_showProgressDialog(
            LocalQTCompat::fromLocal8Bit("正在解析数据"),
            LocalQTCompat::fromLocal8Bit("正在读取和索引数据..."),
            0, 100
        );
    }

    // 获取文件总大小用于进度计算
    qint64 fileSize = file.size();

    // 标记异步处理开始
    m_processingData = true;
    m_performanceTimer.restart();

    // 创建异步处理任务
    QFuture<int> future = QtConcurrent::run([this, selectedFilePath, fileSize]() -> int {
        // 重新打开文件（在新线程中）
        QFile threadFile(selectedFilePath);
        if (!threadFile.open(QIODevice::ReadOnly)) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("线程中无法打开数据文件: %1").arg(threadFile.errorString()));
            return 0;
        }

        // 使用更大的缓冲区提高性能
        constexpr qint64 THREAD_BUFFER_SIZE = 4 * 1024 * 1024; // 4MB缓冲区
        QByteArray buffer;
        qint64 totalRead = 0;
        int totalPackets = 0;
        QString fileName = QFileInfo(selectedFilePath).fileName();

        // 读取并处理数据块
        while (!threadFile.atEnd()) {
            buffer = threadFile.read(THREAD_BUFFER_SIZE);
            if (buffer.isEmpty()) {
                break;
            }

            // 使用IndexGenerator的parseDataStream方法分析和索引数据
            int packetsFound = IndexGenerator::getInstance().parseDataStream(
                reinterpret_cast<const uint8_t*>(buffer.constData()),
                buffer.size(),
                totalRead,
                fileName
            );

            totalPackets += packetsFound;
            totalRead += buffer.size();

            // 更新进度
            if (fileSize > 0) {
                int progress = static_cast<int>((totalRead * 100) / fileSize);

                // 使用Qt元对象系统在UI线程中更新进度
                QMetaObject::invokeMethod(this, [this, progress]() {
                    if (m_view) {
                        m_view->slot_DA_V_updateProgressDialog(progress);
                    }
                    }, Qt::QueuedConnection);
            }
        }

        threadFile.close();

        // 强制保存索引
        IndexGenerator::getInstance().saveIndex(true);

        return totalPackets;
        });

    // 设置监视器
    m_processWatcher.setFuture(future);

    return true;
}

bool DataAnalysisController::exportData(const QString& filePath, bool selectedRowsOnly)
{
    if (!m_ui || !m_ui->tableWidget) {
        return false;
    }

    QString selectedFilePath = filePath;

    // 如果未提供文件路径，弹出文件保存对话框
    if (selectedFilePath.isEmpty() && m_view) {
        selectedFilePath = QFileDialog::getSaveFileName(m_view,
            LocalQTCompat::fromLocal8Bit("导出索引"),
            QString(),
            LocalQTCompat::fromLocal8Bit("CSV文件 (*.csv);;JSON文件 (*.json);;所有文件 (*.*)"));

        if (selectedFilePath.isEmpty()) {
            return false; // 用户取消了选择
        }
    }

    // 获取索引条目
    QVector<PacketIndexEntry> entries;

    if (selectedRowsOnly && !m_selectedRows.isEmpty()) {
        // 只导出选中的行
        QVector<PacketIndexEntry> allEntries = IndexGenerator::getInstance().getAllIndexEntries();
        for (int row : m_selectedRows) {
            if (row >= 0 && row < allEntries.size()) {
                entries.append(allEntries[row]);
            }
        }
    }
    else {
        // 导出所有条目
        entries = IndexGenerator::getInstance().getAllIndexEntries();
    }

    if (entries.isEmpty()) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("导出失败"),
                LocalQTCompat::fromLocal8Bit("没有可用的索引数据"),
                true
            );
        }
        return false;
    }

    // 根据文件扩展名选择导出格式
    QFileInfo fileInfo(selectedFilePath);
    QString extension = fileInfo.suffix().toLower();

    bool success = false;

    if (extension == "csv") {
        success = exportToCsv(selectedFilePath, entries);
    }
    else if (extension == "json") {
        success = exportToJson(selectedFilePath, entries);
    }
    else {
        // 默认使用CSV格式
        if (!selectedFilePath.endsWith(".csv")) {
            selectedFilePath += ".csv";
        }
        success = exportToCsv(selectedFilePath, entries);
    }

    if (success && m_view) {
        m_view->slot_DA_V_showMessageDialog(
            LocalQTCompat::fromLocal8Bit("导出成功"),
            LocalQTCompat::fromLocal8Bit("已成功导出 %1 条索引数据至 %2")
            .arg(entries.size()).arg(selectedFilePath)
        );
    }

    return success;
}

bool DataAnalysisController::exportToCsv(const QString& filePath, const QVector<PacketIndexEntry>& entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件进行导出: %1").arg(filePath));
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("导出失败"),
                LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(file.errorString()),
                true
            );
        }
        return false;
    }

    QTextStream out(&file);

    // 写入CSV头
    out << "索引ID,时间戳,文件偏移,大小,文件名,批次ID,包序号\n";

    // 写入数据
    for (int i = 0; i < entries.size(); ++i) {
        const PacketIndexEntry& entry = entries[i];
        QString timeStr = QDateTime::fromMSecsSinceEpoch(entry.timestamp / 1000000).toString("yyyy-MM-dd hh:mm:ss.zzz");

        out << i << ","
            << timeStr << ","
            << entry.fileOffset << ","
            << entry.size << ","
            << entry.fileName << ","
            << entry.batchId << ","
            << entry.packetIndex << "\n";
    }

    file.close();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("已导出 %1 条索引到CSV文件: %2").arg(entries.size()).arg(filePath));
    return true;
}

bool DataAnalysisController::exportToJson(const QString& filePath, const QVector<PacketIndexEntry>& entries)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开文件进行导出: %1").arg(filePath));
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("导出失败"),
                LocalQTCompat::fromLocal8Bit("无法打开文件: %1").arg(file.errorString()),
                true
            );
        }
        return false;
    }

    QJsonArray entriesArray;

    for (int i = 0; i < entries.size(); ++i) {
        const PacketIndexEntry& entry = entries[i];
        QString timeStr = QDateTime::fromMSecsSinceEpoch(entry.timestamp / 1000000).toString("yyyy-MM-dd hh:mm:ss.zzz");

        QJsonObject entryObj;
        entryObj["id"] = i;
        entryObj["timestamp"] = timeStr;
        entryObj["fileOffset"] = QString::number(entry.fileOffset); // 使用字符串避免大整数精度问题
        entryObj["size"] = static_cast<int>(entry.size);
        entryObj["fileName"] = entry.fileName;
        entryObj["batchId"] = static_cast<int>(entry.batchId);
        entryObj["packetIndex"] = static_cast<int>(entry.packetIndex);

        entriesArray.append(entryObj);
    }

    QJsonObject rootObj;
    rootObj["version"] = "1.0";
    rootObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rootObj["entries"] = entriesArray;

    QJsonDocument doc(rootObj);
    file.write(doc.toJson(QJsonDocument::Compact));
    file.close();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已导出 %1 条索引到JSON文件: %2").arg(entries.size()).arg(filePath));
    return true;
}

void DataAnalysisController::clearData()
{
    if (!m_ui || !m_ui->tableWidget) {
        return;
    }

    // 显示确认对话框
    if (m_view) {
        QMessageBox::StandardButton result = QMessageBox::question(m_view,
            LocalQTCompat::fromLocal8Bit("确认清除"),
            LocalQTCompat::fromLocal8Bit("确定要清除所有索引数据吗？"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (result != QMessageBox::Yes) {
            return;
        }
    }

    // 等待任何异步操作完成
    if (m_processWatcher.isRunning()) {
        m_processWatcher.waitForFinished();
    }

    if (m_loadWatcher.isRunning()) {
        m_loadWatcher.waitForFinished();
    }

    // 清除索引数据
    IndexGenerator::getInstance().clearIndex();

    // 清除表格
    m_ui->tableWidget->setRowCount(0);

    // 清除加载缓存
    m_entriesToLoad.clear();
    m_batchLoadPosition = 0;

    // 更新UI状态
    if (m_view) {
        m_view->slot_DA_V_updateUIState(false);
        m_view->slot_DA_V_updateStatusBar(LocalQTCompat::fromLocal8Bit("索引数据已清除"), 0);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引数据已清除"));
}

bool DataAnalysisController::loadDataFromFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        return false;
    }

    // 如果是索引文件，直接加载
    if (filePath.endsWith(".idx") || filePath.endsWith(".json")) {
        QString basePath = filePath;
        if (filePath.endsWith(".json")) {
            basePath = filePath.left(filePath.length() - 5); // 移除".json"
        }
        else if (filePath.endsWith(".idx")) {
            basePath = filePath.left(filePath.length() - 4); // 移除".idx"
        }

        // 如果已有会话索引，先保存它
        if (!m_sessionId.isEmpty() && IndexGenerator::getInstance().isOpen()) {
            IndexGenerator::getInstance().saveIndex(true);
            IndexGenerator::getInstance().close();
        }

        bool success = IndexGenerator::getInstance().loadIndex(basePath);

        if (success) {
            // 更新当前数据源
            setDataSource(QFileInfo(basePath).fileName());

            // 如果已设置会话ID，更新监视路径
            if (!m_sessionId.isEmpty() && !m_sessionBasePath.isEmpty()) {
                // 移除旧的监视路径
                QStringList paths = m_fileWatcher.files();
                if (!paths.isEmpty()) {
                    m_fileWatcher.removePaths(paths);
                }

                // 添加新的监视路径
                QString indexPath = basePath + ".idx";
                QString jsonPath = basePath + ".json";

                if (QFile::exists(indexPath)) {
                    m_fileWatcher.addPath(indexPath);
                }
                if (QFile::exists(jsonPath)) {
                    m_fileWatcher.addPath(jsonPath);
                }
            }

            // 加载索引数据到表格
            loadData();
            return true;
        }
        else if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("加载失败"),
                LocalQTCompat::fromLocal8Bit("无法加载索引文件"),
                true
            );
        }
        return false;
    }
    else {
        // 否则作为数据文件处理
        return importData(filePath);
    }
}

void DataAnalysisController::processRawData(const uint8_t* data, size_t size, uint64_t fileOffset, const QString& fileName)
{
    if (!data || size == 0) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("处理原始数据失败：无效数据"));
        return;
    }

    // 设置当前数据源
    QString sourceFileName = fileName;
    if (sourceFileName.isEmpty()) {
        sourceFileName = m_currentDataSource.isEmpty() ? m_sessionId : m_currentDataSource;
    }

    // 避免在UI线程中直接处理大量数据
    QMutexLocker locker(&m_processMutex);

    // 如果已有处理任务在运行，等待其完成
    if (m_processingData) {
        m_processWatcher.waitForFinished();
    }

    m_processingData = true;
    m_performanceTimer.restart();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始异步处理原始数据：%1 字节，文件：%2").arg(size).arg(sourceFileName));

    // 拷贝数据（因为原始指针在异步处理完成前可能已经无效）
    std::vector<uint8_t> dataCopy(data, data + size);

    // 异步处理数据
    QFuture<int> future = QtConcurrent::run([this, dataCopy, fileOffset, sourceFileName]() -> int {
        try {
            // 确保索引文件已打开
            if (!m_sessionId.isEmpty() && !m_sessionBasePath.isEmpty()) {
                IndexGenerator::getInstance().setSessionId(m_sessionId);
                IndexGenerator::getInstance().setBasePath(m_sessionBasePath);
            }

            // 使用IndexGenerator的parseDataStream方法分析和索引数据流
            int packetsFound = IndexGenerator::getInstance().parseDataStream(
                dataCopy.data(), dataCopy.size(), fileOffset, sourceFileName
            );

            // 强制保存索引 - 只在找到数据包时保存
            if (packetsFound > 0) {
                IndexGenerator::getInstance().saveIndex(true);
            }

            LOG_INFO(QString(LocalQTCompat::fromLocal8Bit("从原始数据中解析并索引了 %1 个数据包")).arg(packetsFound));
            return packetsFound;
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("处理原始数据时发生异常: %1").arg(e.what()));
            return 0;
        }
        });

    m_processWatcher.setFuture(future);
}

void DataAnalysisController::processDataPackets(const std::vector<DataPacket>& packets, const QString& filePath)
{
    if (packets.empty()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("处理数据包失败：无数据包"));
        return;
    }

    QMutexLocker locker(&m_processMutex);

    // 如果已有处理任务在运行，等待其完成
    if (m_processingData) {
        m_processWatcher.waitForFinished();
    }

    m_processingData = true;

    // 生成有意义的会话ID
    QString sessionId;
    if (!m_sessionId.isEmpty()) {
        // 优先使用已设置的会话ID
        sessionId = m_sessionId;
    }
    else {
        // 创建基于时间的会话ID
        sessionId = QString("session_%1").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    }

    // 更新成员变量
    m_sessionId = sessionId;

    // 拷贝数据包和会话信息到局部变量，以便在异步任务中使用
    std::vector<DataPacket> packetsCopy = packets;
    QString sessionIdCopy = sessionId;
    QString basePathCopy = filePath;
    QString sourceFileName = m_sessionId + ".json";

    LOG_INFO(LocalQTCompat::fromLocal8Bit("%1包，会话：%2，源目录：%3，文件名：%4")
        .arg(packets.size())
        .arg(sessionId)
        .arg(filePath)
        .arg(sourceFileName));

    // 异步处理数据
    QFuture<int> future = QtConcurrent::run([this, packetsCopy, sessionIdCopy, basePathCopy, sourceFileName]() -> int {
        try {
            // 将多个数据包合并为一个连续的数据缓冲区
            size_t totalSize = 0;
            for (const auto& packet : packetsCopy) {
                totalSize += packet.getSize();
            }

            if (totalSize == 0) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的数据包：总大小为0"));
                return 0;
            }

            // 创建连续的数据缓冲区
            std::vector<uint8_t> combinedData;
            combinedData.reserve(totalSize);

            // 构建完整的缓冲区 - 修复的部分
            for (const auto& packet : packetsCopy) {
                if (packet.data && !packet.data->empty()) {
                    // 正确访问共享指针中的vector数据
                    combinedData.insert(combinedData.end(),
                        packet.data->begin(),
                        packet.data->end());
                }
            }

            IndexGenerator::getInstance().setBasePath(basePathCopy);
            IndexGenerator::getInstance().setSessionId(sessionIdCopy);

            // 使用IndexGenerator的parseDataStream进行数据分析和索引
            int packetsFound = IndexGenerator::getInstance().parseDataStream(
                combinedData.data(), combinedData.size(), 0, sourceFileName
            );

            LOG_INFO(LocalQTCompat::fromLocal8Bit("从数据包中解析并索引了 %1 个数据包").arg(packetsFound));
            return packetsFound;
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("处理数据包时发生异常: %1").arg(e.what()));
            return 0;
        }
        });

    m_processWatcher.setFuture(future);
}

void DataAnalysisController::updateIndexTable(const QVector<PacketIndexEntry>& entries)
{
    if (!m_ui || !m_ui->tableWidget || entries.isEmpty()) {
        return;
    }

    // 使用优化的批量加载方法
    loadDataBatched(entries, m_maxBatchSize);
}

void DataAnalysisController::configureIndexing(const QString& sessionId, const QString& basePath)
{
    // 等待任何正在进行的处理完成
    if (m_processWatcher.isRunning()) {
        m_processWatcher.waitForFinished();
    }

    if (m_loadWatcher.isRunning()) {
        m_loadWatcher.waitForFinished();
    }

    // 保存会话信息
    m_sessionId = sessionId;
    m_sessionBasePath = basePath;

    // 确保目录存在
    QDir dir(basePath);
    if (!dir.exists() && !dir.mkpath(".")) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建索引基本目录: %1").arg(basePath));
        return;
    }

    // 配置IndexGenerator
    IndexGenerator::getInstance().setSessionId(sessionId);
    IndexGenerator::getInstance().setBasePath(basePath);

    // 构造索引文件路径
    QString indexPath = QString("%1/%2.idx").arg(basePath).arg(sessionId);
    QString jsonPath = QString("%1/%2.idx.json").arg(basePath).arg(sessionId);

    // 监视索引文件变化
    QStringList paths = m_fileWatcher.files();
    if (!paths.isEmpty()) {
        m_fileWatcher.removePaths(paths);
    }

    if (QFile::exists(indexPath)) {
        m_fileWatcher.addPath(indexPath);
    }
    if (QFile::exists(jsonPath)) {
        m_fileWatcher.addPath(jsonPath);
    }

    // 设置当前数据源为会话ID
    setDataSource(sessionId);

    // 加载索引数据
    loadData();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已配置索引会话 - ID: %1, 路径: %2").arg(sessionId).arg(basePath));
}

void DataAnalysisController::setMaxBatchSize(int maxBatchSize)
{
    m_maxBatchSize = qMax(100, maxBatchSize);  // 确保最小批量大小为100
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置最大批处理大小为: %1").arg(m_maxBatchSize));
}

bool DataAnalysisController::isProcessing() const
{
    return m_processingData || m_loadWatcher.isRunning();
}

void DataAnalysisController::loadDataBatched(const QVector<PacketIndexEntry>& entries, int batchSize)
{
    if (!m_ui || !m_ui->tableWidget) {
        return;
    }

    // 避免在加载过程中重复加载
    if (m_loadWatcher.isRunning()) {
        m_loadWatcher.waitForFinished();
    }

    // 重置加载状态
    m_entriesToLoad = entries;
    m_batchLoadPosition = 0;

    // 准备表格
    QTableWidget* table = m_ui->tableWidget;
    table->setUpdatesEnabled(false);  // 暂停更新提高性能
    table->clearContents();
    table->setRowCount(entries.size());

    // 显示进度对话框（只有大量数据时才显示）
    if (m_view && entries.size() > 1000) {
        m_view->slot_DA_V_showProgressDialog(
            LocalQTCompat::fromLocal8Bit("加载数据"),
            LocalQTCompat::fromLocal8Bit("正在加载索引数据..."),
            0, 100
        );
    }

    // 确定第一批大小
    int firstBatchSize = qMin(batchSize, entries.size());
    m_batchLoadPosition = firstBatchSize;

    // 启动性能计时
    m_performanceTimer.restart();

    // 异步加载第一批
    QFuture<void> future = QtConcurrent::run([this, firstBatchSize]() {
        // 获取第一批的条目
        QVector<PacketIndexEntry> firstBatch;
        for (int i = 0; i < firstBatchSize && i < m_entriesToLoad.size(); ++i) {
            firstBatch.append(m_entriesToLoad[i]);
        }

        // 在UI线程中更新表格
        QMetaObject::invokeMethod(this, [this, firstBatch]() {
            optimizedTableUpdate(firstBatch, 0, firstBatch.size());

            // 更新进度
            if (m_view && !m_entriesToLoad.isEmpty()) {
                int progress = (m_batchLoadPosition * 100) / m_entriesToLoad.size();
                m_view->slot_DA_V_updateProgressDialog(progress);
            }

            // 如果只有一批数据，直接完成
            if (m_batchLoadPosition >= m_entriesToLoad.size()) {
                slot_DA_C_onBatchLoadFinished();
            }
            }, Qt::QueuedConnection);
        });

    m_loadWatcher.setFuture(future);
}

void DataAnalysisController::optimizedTableUpdate(const QVector<PacketIndexEntry>& entries, int startRow, int count)
{
    if (!m_ui || !m_ui->tableWidget || entries.isEmpty() || count <= 0) {
        return;
    }

    m_isUpdatingTable = true;

    QTableWidget* table = m_ui->tableWidget;

    for (int i = 0; i < count && (i < entries.size()); ++i) {
        int row = startRow + i;
        if (row >= table->rowCount()) {
            continue; // 防止越界
        }

        const PacketIndexEntry& entry = entries[i];

        // 索引ID
        QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(row));
        idItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 0, idItem);

        // 时间戳
        QString timeStr = QDateTime::fromMSecsSinceEpoch(entry.timestamp / 1000000).toString("yyyy-MM-dd hh:mm:ss.zzz");
        QTableWidgetItem* timestampItem = new QTableWidgetItem(timeStr);
        table->setItem(row, 1, timestampItem);

        // 文件偏移
        QTableWidgetItem* offsetItem = new QTableWidgetItem(QString("0x%1").arg(entry.fileOffset, 8, 16, QChar('0')));
        offsetItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 2, offsetItem);

        // 大小
        QTableWidgetItem* sizeItem = new QTableWidgetItem(QString::number(entry.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, 3, sizeItem);

        // 文件名
        QTableWidgetItem* fileNameItem = new QTableWidgetItem(entry.fileName);
        table->setItem(row, 4, fileNameItem);

        // 批次ID
        QTableWidgetItem* batchItem = new QTableWidgetItem(QString::number(entry.batchId));
        batchItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 5, batchItem);

        // 包序号
        QTableWidgetItem* packetIndexItem = new QTableWidgetItem(QString::number(entry.packetIndex));
        packetIndexItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 6, packetIndexItem);
    }

    // 处理完后重新启用UI更新
    if (startRow + count >= table->rowCount() || count >= entries.size()) {
        table->setUpdatesEnabled(true);
    }

    m_isUpdatingTable = false;
}

void DataAnalysisController::slot_DA_C_onBatchLoadFinished()
{
    // 检查是否有更多数据需要加载
    if (m_batchLoadPosition < m_entriesToLoad.size()) {
        // 确定下一批大小
        int remainingItems = m_entriesToLoad.size() - m_batchLoadPosition;
        int batchSize = qMin(m_maxBatchSize, remainingItems);

        // 准备下一批数据
        QVector<PacketIndexEntry> batchEntries;
        for (int i = 0; i < batchSize && (m_batchLoadPosition + i) < m_entriesToLoad.size(); ++i) {
            batchEntries.append(m_entriesToLoad[m_batchLoadPosition + i]);
        }

        int startRow = m_batchLoadPosition;
        m_batchLoadPosition += batchSize;

        // 异步加载下一批
        QFuture<void> future = QtConcurrent::run([this, batchEntries, startRow]() {
            // 在UI线程中更新表格
            QMetaObject::invokeMethod(this, [this, batchEntries, startRow]() {
                optimizedTableUpdate(batchEntries, startRow, batchEntries.size());

                // 更新进度
                if (m_view && !m_entriesToLoad.isEmpty()) {
                    int progress = (m_batchLoadPosition * 100) / m_entriesToLoad.size();
                    m_view->slot_DA_V_updateProgressDialog(progress);
                }
                }, Qt::QueuedConnection);
            });

        m_loadWatcher.setFuture(future);
    }
    else {
        // 全部加载完成
        if (m_ui && m_ui->tableWidget) {
            m_ui->tableWidget->setUpdatesEnabled(true);
        }

        if (m_view) {
            m_view->slot_DA_V_hideProgressDialog();
            m_view->slot_DA_V_updateUIState(!m_entriesToLoad.isEmpty());
            m_view->slot_DA_V_updateStatusBar(
                LocalQTCompat::fromLocal8Bit("索引数据已加载"),
                m_entriesToLoad.size()
            );
        }

        // 记录性能数据
        qint64 elapsedMs = m_performanceTimer.elapsed();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("表格数据加载完成，共 %1 条记录，耗时 %2 毫秒").arg(m_entriesToLoad.size()).arg(elapsedMs));

        // 清空加载缓存
        m_entriesToLoad.clear();
        m_batchLoadPosition = 0;
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

void DataAnalysisController::slot_DA_C_onImportDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("导入数据按钮点击"));
    importData();
}

void DataAnalysisController::slot_DA_C_onExportDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("导出数据按钮点击"));

    // 检查是否有数据
    if (IndexGenerator::getInstance().getIndexCount() == 0) {
        if (m_view) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("提示"),
                LocalQTCompat::fromLocal8Bit("没有可用的索引数据进行导出"),
                true
            );
        }
        return;
    }

    // 导出所有数据
    exportData();
}

void DataAnalysisController::slot_DA_C_onClearDataClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("清除数据按钮点击"));
    clearData();
}

void DataAnalysisController::slot_DA_C_onSelectionChanged(const QList<int>& selectedRows)
{
    m_selectedRows = selectedRows;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("表格选择变更，选中 %1 行").arg(selectedRows.size()));
}

void DataAnalysisController::slot_DA_C_onLoadDataFromFileRequested(const QString& filePath)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("从文件加载数据请求: %1").arg(filePath));
    loadDataFromFile(filePath);
}

void DataAnalysisController::slot_DA_C_onIndexEntryAdded(const PacketIndexEntry& entry)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新索引条目"));
    // 设置标志防止重入
    if (!m_ui || !m_ui->tableWidget || m_isUpdatingTable) {
        return;
    }

    m_isUpdatingTable = true;

    QTableWidget* table = m_ui->tableWidget;

    // 获取当前行数
    int row = table->rowCount();
    table->setRowCount(row + 1);

    // 索引ID
    QTableWidgetItem* idItem = new QTableWidgetItem(QString::number(row));
    idItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 0, idItem);

    // 时间戳
    QString timeStr = QDateTime::fromMSecsSinceEpoch(entry.timestamp / 1000000).toString("yyyy-MM-dd hh:mm:ss.zzz");
    QTableWidgetItem* timestampItem = new QTableWidgetItem(timeStr);
    table->setItem(row, 1, timestampItem);

    // 文件偏移
    QTableWidgetItem* offsetItem = new QTableWidgetItem(QString("0x%1").arg(entry.fileOffset, 8, 16, QChar('0')));
    offsetItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 2, offsetItem);

    // 大小
    QTableWidgetItem* sizeItem = new QTableWidgetItem(QString::number(entry.size));
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, 3, sizeItem);

    // 文件名
    QTableWidgetItem* fileNameItem = new QTableWidgetItem(entry.fileName);
    table->setItem(row, 4, fileNameItem);

    // 批次ID
    QTableWidgetItem* batchItem = new QTableWidgetItem(QString::number(entry.batchId));
    batchItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 5, batchItem);

    // 包序号
    QTableWidgetItem* packetIndexItem = new QTableWidgetItem(QString::number(entry.packetIndex));
    packetIndexItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(row, 6, packetIndexItem);

    // 滚动到最新行
    table->scrollToItem(idItem);

    // 更新UI状态
    if (m_view) {
        m_view->slot_DA_V_updateUIState(true);
    }

    m_isUpdatingTable = false;
}

void DataAnalysisController::slot_DA_C_onIndexUpdated(int count)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引已更新"));
    if (m_view) {
        m_view->slot_DA_V_updateStatusBar(
            LocalQTCompat::fromLocal8Bit("索引已更新"),
            count
        );
    }
}

void DataAnalysisController::slot_DA_C_onProcessingFinished()
{
    // 标记处理完成
    m_processingData = false;

    // 获取处理结果
    int processedCount = m_processWatcher.result();

    // 记录性能数据
    qint64 elapsedMs = m_performanceTimer.elapsed();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("异步处理完成 - 处理了 %1 个数据包，耗时 %2 毫秒").arg(processedCount).arg(elapsedMs));

    // 隐藏进度对话框
    if (m_view) {
        m_view->slot_DA_V_hideProgressDialog();

        // 如果有数据包被处理，显示成功消息
        if (processedCount > 0) {
            m_view->slot_DA_V_showMessageDialog(
                LocalQTCompat::fromLocal8Bit("处理完成"),
                LocalQTCompat::fromLocal8Bit("成功处理 %1 个数据包，耗时 %2 毫秒")
                .arg(processedCount).arg(elapsedMs)
            );
        }
    }

    // 重新加载索引数据
    loadData();
}

void DataAnalysisController::slot_DA_C_onIndexFileChanged(const QString& path)
{
    // 索引文件发生变化，需要重新加载数据
    LOG_INFO(LocalQTCompat::fromLocal8Bit("检测到索引文件变化: %1").arg(path));

    // 但避免在正在处理的状态下重新加载
    if (!m_processingData && !m_loadWatcher.isRunning()) {
        loadData();
    }
}