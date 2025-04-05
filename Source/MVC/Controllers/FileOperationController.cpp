// Source/MVC/Controllers/FileOperationController.cpp
#include "FileOperationController.h"
#include "Logger.h"
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QThread>
#include <QApplication>
#include <QElapsedTimer>

FileOperationController::FileOperationController(QObject* parent)
    : QObject(parent)
    , m_model(FileOperationModel::getInstance())
    , m_currentView(nullptr)
    , m_currentWidth(1920)
    , m_currentHeight(1080)
    , m_currentFormat(0x39)  // 默认 RAW10
    , m_initialized(false)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器构建"));

    // 设置定时器，定期更新统计信息
    m_statsUpdateTimer.setInterval(1000); // 每秒更新一次
    connect(&m_statsUpdateTimer, &QTimer::timeout, this, &FileOperationController::updateSaveStatistics);

    // 连接模型信号
    connectModelSignals();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器已创建"));
}

FileOperationController::~FileOperationController()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器销毁开始"));

    // 确保停止保存
    if (isSaving()) {
        slot_FS_C_stopSaving();
    }

    // 停止统计更新定时器
    if (m_statsUpdateTimer.isActive()) {
        m_statsUpdateTimer.stop();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器已销毁"));
}

bool FileOperationController::initialize()
{
    try {
        // 加载模型配置
        if (!m_model->loadConfigFromSettings()) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("加载文件保存配置失败，使用默认设置"));
            m_model->resetToDefault();
        }

        // 强制设置保存格式为RAW格式
        SaveParameters params = m_model->getSaveParameters();
        params.format = FileFormat::RAW;
        m_model->setSaveParameters(params);

        // 设置初始化标志
        m_initialized = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器初始化成功，默认使用RAW格式保存"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存控制器初始化异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存控制器初始化未知异常"));
        return false;
    }
}


bool FileOperationController::isSaving() const
{
    if (!m_initialized) {
        return false;
    }

    SaveStatus status = m_model->getStatus();
    bool saving = (status == SaveStatus::FS_SAVING);

    return saving;
}

void FileOperationController::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    m_currentWidth = width;
    m_currentHeight = height;
    m_currentFormat = format;

    // 更新模型中的图像参数
    m_model->setImageParameters(width, height, format);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));

    // 更新所有活动视图
    if (m_currentView) {
        m_currentView->setImageParameters(width, height, format);
    }
}

FileOperationView* FileOperationController::createSaveView(QWidget* parent)
{
    if (!m_currentView) {
        m_currentView = new FileOperationView(parent);
        m_currentView->setImageParameters(m_currentWidth, m_currentHeight, m_currentFormat);
        connectViewSignals(m_currentView);
    }
    return m_currentView;
}

bool FileOperationController::slot_FS_C_startSaving()
{
    if (isSaving()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("文件保存已经在进行中"));
        return false;
    }

    if (!m_initialized) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存控制器未初始化"));
        emit signal_FS_C_saveError(LocalQTCompat::fromLocal8Bit("文件保存控制器未初始化"));
        return false;
    }

    try {
        // 获取保存参数
        SaveParameters params = m_model->getSaveParameters();

        // 更新图像参数
        params.options["width"] = m_currentWidth;
        params.options["height"] = m_currentHeight;
        params.options["format"] = m_currentFormat;

        // 更新模型参数
        m_model->setSaveParameters(params);

        // 启动保存 - 直接使用Model的方法，Model会调用FileManager
        if (!m_model->startSaving()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动保存失败"));
            emit signal_FS_C_saveError(LocalQTCompat::fromLocal8Bit("启动保存失败"));
            return false;
        }

        // 启动统计更新定时器
        m_statsUpdateTimer.start();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("开始保存文件到: %1").arg(m_model->getFullSavePath()));
        emit signal_FS_C_saveStarted();
        return true;
    }
    catch (const std::exception& e) {
        QString errorMsg = LocalQTCompat::fromLocal8Bit("开始保存异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit signal_FS_C_saveError(errorMsg);
        return false;
    }
}


bool FileOperationController::slot_FS_C_stopSaving()
{
    if (!isSaving()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("没有正在进行的保存任务"));
        return false;
    }

    try {
        // 停止统计更新定时器
        if (m_statsUpdateTimer.isActive()) {
            m_statsUpdateTimer.stop();
        }

        // 设置状态为已完成
        m_model->setStatus(SaveStatus::FS_COMPLETED);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
        emit signal_FS_C_saveStopped();
        return true;
    }
    catch (const std::exception& e) {
        QString errorMsg = LocalQTCompat::fromLocal8Bit("停止保存异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit signal_FS_C_saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        return false;
    }
    catch (...) {
        QString errorMsg = LocalQTCompat::fromLocal8Bit("停止保存未知异常");
        LOG_ERROR(errorMsg);
        emit signal_FS_C_saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        return false;
    }
}

void FileOperationController::slot_FS_C_showSettings(QWidget* parent)
{
    // 如果已有视图，显示它
    if (m_currentView) {
        m_currentView->prepareForShow();
        m_currentView->show();
        m_currentView->raise();
        m_currentView->activateWindow();
        return;
    }

    // 创建新视图
    FileOperationView* view = createSaveView(parent);
    view->prepareForShow();
    view->show();
}

void FileOperationController::slot_FS_C_processDataPacket(const DataPacket& packet)
{
    // 如果不在保存状态，忽略数据包
    if (!isSaving()) {
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("收到数据包但未在保存状态，忽略"));
        return;
    }

#if 0
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("处理数据包: 大小=%1 字节, 时间戳=%2")
        .arg(packet.getSize())
        .arg(packet.timestamp));
#endif

    try {
        // 始终使用RAW格式，无需检测
        m_model->processDataPacket(packet);
    }
    catch (const std::exception& e) {
        QString errorMsg = LocalQTCompat::fromLocal8Bit("处理数据包异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit signal_FS_C_saveError(errorMsg);
        slot_FS_C_stopSaving();
    }
}

void FileOperationController::slot_FS_C_processDataBatch(const DataPacketBatch& packets)
{
    // 如果不在保存状态，忽略数据包
    if (!isSaving() || packets.empty()) {
        return;
    }

    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("处理数据批次: %1 个包").arg(packets.size()));

        // 更新统计信息
        SaveStatistics stats = m_model->getStatistics();
        stats.packetCount += packets.size(); // 增加包计数

        // 计算批次总大小
        uint64_t batchSize = 0;
        for (const auto& packet : packets) {
            batchSize += packet.getSize();
        }
        stats.totalBytes += batchSize;

        m_model->updateStatistics(stats);
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("处理数据包批次异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit signal_FS_C_saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        slot_FS_C_stopSaving();
    }
    catch (...) {
        QString errorMsg = "处理数据包批次未知异常";
        LOG_ERROR(errorMsg);
        emit signal_FS_C_saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        slot_FS_C_stopSaving();
    }
}

bool FileOperationController::slot_FS_C_isAutoSaveEnabled() const
{
    return m_model->getOption("auto_save", false).toBool();
}

void FileOperationController::slot_FS_C_setAutoSaveEnabled(bool enabled)
{
    m_model->setOption("auto_save", enabled);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("自动保存设置为: %1").arg(enabled ? "启用" : "禁用"));
}

void FileOperationController::slot_FS_C_onModelStatusChanged(SaveStatus status)
{
    // 根据状态采取相应操作
    switch (status) {
    case SaveStatus::FS_IDLE:
        break;
    case SaveStatus::FS_SAVING:
        break;
    case SaveStatus::FS_PAUSED:
        break;
    case SaveStatus::FS_COMPLETED: {
        // 保存完成后的操作
        SaveStatistics stats = m_model->getStatistics();
        QString path = m_model->getFullSavePath();
        emit signal_FS_C_saveCompleted(path, stats.totalBytes);
        break;
    }
    case SaveStatus::FS_ERROR:
        // 错误处理已经在其他地方完成
        break;
    }
}

void FileOperationController::slot_FS_C_onModelStatisticsUpdated(const SaveStatistics& statistics)
{
#if 0
    // 记录关键统计信息
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("接收统计信息更新: 速率=%1 MB/s, 总大小=%2 MB, 文件数=%3")
        .arg(statistics.saveRate, 0, 'f', 2)
        .arg(statistics.totalBytes / (1024.0 * 1024.0), 0, 'f', 2)
        .arg(statistics.fileCount));

    // 更新内部状态
    m_currentSaveRate = statistics.saveRate;
    m_totalSavedBytes = statistics.totalBytes;

    // 检查是否需要分析实时流数据
    if (m_realTimeAnalysisEnabled && statistics.totalBytes > m_lastAnalyzedPosition) {
        uint64_t newDataSize = statistics.totalBytes - m_lastAnalyzedPosition;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("触发实时数据分析，新增数据: %1 KB")
            .arg(newDataSize / 1024.0, 0, 'f', 2));

        emit signal_FS_C_newDataAvailable(m_lastAnalyzedPosition, newDataSize);
        m_lastAnalyzedPosition = statistics.totalBytes;
    }

    // 转发统计信息到视图层
    emit signal_FS_C_statisticsUpdated(statistics);
#endif
}

void FileOperationController::slot_FS_C_onModelSaveCompleted(const QString& path, uint64_t totalBytes)
{
    // 转发保存完成信号
    emit signal_FS_C_saveCompleted(path, totalBytes);
}

void FileOperationController::slot_FS_C_onModelSaveError(const QString& error)
{
    // 处理保存错误
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误: %1").arg(error));

    // 停止保存
    if (isSaving()) {
        slot_FS_C_stopSaving();
    }

    // 转发错误信号
    emit signal_FS_C_saveError(error);
}

void FileOperationController::slot_FS_C_onViewParametersChanged(const SaveParameters& parameters)
{
    // 更新模型参数
    m_model->setSaveParameters(parameters);

    // 保存配置到设置
    m_model->saveConfigToSettings();

    bool autoSave = parameters.options.value("auto_save", false).toBool();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("接收参数变更 - 自动保存: %1")
        .arg(autoSave ? "启用" : "禁用"));
}

void FileOperationController::slot_FS_C_onViewStartSaveRequested()
{
    slot_FS_C_startSaving();
}

void FileOperationController::slot_FS_C_onViewStopSaveRequested()
{
    slot_FS_C_stopSaving();
}

void FileOperationController::slot_FS_C_onWorkerSaveProgress(uint64_t bytesWritten, int fileCount)
{
    // 更新模型中的统计信息
    SaveStatistics stats = m_model->getStatistics();
    stats.totalBytes = bytesWritten;
    stats.fileCount = fileCount;
    m_model->updateStatistics(stats);
}

void FileOperationController::slot_FS_C_onWorkerSaveCompleted(const QString& path, uint64_t totalBytes)
{
    // 处理工作线程完成信号
    m_model->setStatus(SaveStatus::FS_COMPLETED);
    emit signal_FS_C_saveCompleted(path, totalBytes);
}

void FileOperationController::slot_FS_C_onWorkerSaveError(const QString& error)
{
    // 处理工作线程错误
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("工作线程保存错误: %1").arg(error));
    m_model->setStatus(SaveStatus::FS_ERROR);
    emit signal_FS_C_saveError(error);
}

void FileOperationController::updateSaveStatistics()
{
    if (!isSaving()) {
        return;
    }

    SaveStatistics stats = m_model->getStatistics();
    QDateTime now = QDateTime::currentDateTime();

    // 计算保存速率
    qint64 elapsedMs = stats.startTime.msecsTo(now);
    if (elapsedMs > 0) {
        // 计算MB/s
        stats.saveRate = (stats.totalBytes / 1024.0 / 1024.0) / (elapsedMs / 1000.0);
    }

    // 更新最后更新时间
    stats.lastUpdateTime = now;

    // 计算进度 (如果有总大小预估)
    if (stats.estimatedTotalBytes > 0) {
        stats.progress = (static_cast<double>(stats.totalBytes) / stats.estimatedTotalBytes) * 100.0;
    }
    else {
        stats.progress = -1; // 未知进度
    }

    // 更新模型
    m_model->updateStatistics(stats);
}

void FileOperationController::connectModelSignals()
{
    connect(m_model, &FileOperationModel::signal_FS_M_statusChanged,
        this, &FileOperationController::slot_FS_C_onModelStatusChanged);
    connect(m_model, &FileOperationModel::signal_FS_M_statisticsUpdated,
        this, &FileOperationController::slot_FS_C_onModelStatisticsUpdated);
    connect(m_model, &FileOperationModel::signal_FS_M_saveCompleted,
        this, &FileOperationController::slot_FS_C_onModelSaveCompleted);
    connect(m_model, &FileOperationModel::signal_FS_M_saveError,
        this, &FileOperationController::slot_FS_C_onModelSaveError);
}

void FileOperationController::connectViewSignals(FileOperationView* view)
{
    if (!view) return;

    connect(view, &FileOperationView::signal_FS_V_saveParametersChanged,
        this, &FileOperationController::slot_FS_C_onViewParametersChanged);
    connect(view, &FileOperationView::signal_FS_V_startSaveRequested,
        this, &FileOperationController::slot_FS_C_onViewStartSaveRequested);
    connect(view, &FileOperationView::signal_FS_V_stopSaveRequested,
        this, &FileOperationController::slot_FS_C_onViewStopSaveRequested);

    // 连接控制器信号到视图
    connect(this, &FileOperationController::signal_FS_C_saveStarted,
        view, &FileOperationView::slot_FS_V_onSaveStarted);
    connect(this, &FileOperationController::signal_FS_C_saveStopped,
        view, &FileOperationView::slot_FS_V_onSaveStopped);
    connect(this, &FileOperationController::signal_FS_C_saveCompleted,
        view, &FileOperationView::slot_FS_V_onSaveCompleted);
    connect(this, &FileOperationController::signal_FS_C_saveError,
        view, &FileOperationView::slot_FS_V_onSaveError);

    // 连接模型信号到视图
    connect(m_model, &FileOperationModel::signal_FS_M_statisticsUpdated,
        view, &FileOperationView::slot_FS_V_updateStatisticsDisplay);
    connect(m_model, &FileOperationModel::signal_FS_M_statusChanged,
        view, &FileOperationView::slot_FS_V_updateStatusDisplay);
}

void FileOperationController::connectWorkerSignals()
{
}