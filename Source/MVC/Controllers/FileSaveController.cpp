// Source/MVC/Controllers/FileSaveController.cpp
#include "FileSaveController.h"
#include "Logger.h"
#include <QDir>
#include <QDateTime>
#include <QMessageBox>
#include <QThread>
#include <QApplication>
#include <QElapsedTimer>

FileSaveController::FileSaveController(QObject* parent)
    : QObject(parent)
    , m_model(FileSaveModel::getInstance())
    , m_currentView(nullptr)
    , m_currentWidth(1920)
    , m_currentHeight(1080)
    , m_currentFormat(0x39)  // 默认 RAW10
    , m_saveWorker(nullptr)
    , m_workerThread(nullptr)
    , m_initialized(false)
{
    LOG_INFO("文件保存控制器构建");

    // 设置定时器，定期更新统计信息
    m_statsUpdateTimer.setInterval(1000); // 每秒更新一次
    connect(&m_statsUpdateTimer, &QTimer::timeout, this, &FileSaveController::updateSaveStatistics);

    // 连接模型信号
    connectModelSignals();

    // 创建工作线程
    m_workerThread = new QThread(this);
    m_saveWorker = new FileSaveWorker();
    m_saveWorker->moveToThread(m_workerThread);

    // 连接工作线程信号
    connectWorkerSignals();

    // 启动工作线程
    m_workerThread->start();

    LOG_INFO("文件保存控制器已创建");
}

FileSaveController::~FileSaveController()
{
    LOG_INFO("文件保存控制器销毁开始");

    // 确保停止保存
    if (isSaving()) {
        stopSaving();
    }

    // 停止统计更新定时器
    if (m_statsUpdateTimer.isActive()) {
        m_statsUpdateTimer.stop();
    }

    // 停止并清理工作线程
    if (m_workerThread) {
        if (m_saveWorker) {
            m_saveWorker->stop();
        }

        m_workerThread->quit();
        m_workerThread->wait(1000); // 等待最多1秒

        if (m_workerThread->isRunning()) {
            LOG_WARN("工作线程未能正常退出，强制终止");
            m_workerThread->terminate();
            m_workerThread->wait();
        }

        delete m_saveWorker;
        m_saveWorker = nullptr;
    }

    LOG_INFO("文件保存控制器已销毁");
}

bool FileSaveController::initialize()
{
    try {
        // 加载模型配置
        if (!m_model->loadConfigFromSettings()) {
            LOG_WARN("加载文件保存配置失败，使用默认设置");
            m_model->resetToDefault();
        }

        // 设置初始化标志
        m_initialized = true;

        LOG_INFO("文件保存控制器初始化成功");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("文件保存控制器初始化异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR("文件保存控制器初始化未知异常");
        return false;
    }
}

bool FileSaveController::isSaving() const
{
    return m_model->getStatus() == SaveStatus::FS_SAVING;
}

void FileSaveController::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    m_currentWidth = width;
    m_currentHeight = height;
    m_currentFormat = format;

    // 更新模型中的图像参数
    m_model->setImageParameters(width, height, format);

    LOG_INFO(QString("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));

    // 更新所有活动视图
    if (m_currentView) {
        m_currentView->setImageParameters(width, height, format);
    }
}

FileSaveView* FileSaveController::createSaveView(QWidget* parent)
{
    if (!m_currentView) {
        m_currentView = new FileSaveView(parent);
        m_currentView->setImageParameters(m_currentWidth, m_currentHeight, m_currentFormat);
        connectViewSignals(m_currentView);
    }
    return m_currentView;
}

bool FileSaveController::startSaving()
{
    if (isSaving()) {
        LOG_WARN("文件保存已经在进行中");
        return false;
    }

    if (!m_initialized) {
        LOG_ERROR("文件保存控制器未初始化");
        emit saveError(LocalQTCompat::fromLocal8Bit("文件保存控制器未初始化"));
        return false;
    }

    try {
        // 获取保存参数
        SaveParameters params = m_model->getSaveParameters();

        // 更新图像参数
        params.options["width"] = m_currentWidth;
        params.options["height"] = m_currentHeight;
        params.options["format"] = m_currentFormat;

        // 设置工作线程参数
        m_saveWorker->setParameters(params);

        // 重置统计信息
        m_model->resetStatistics();

        // 设置状态为保存中
        m_model->setStatus(SaveStatus::FS_SAVING);

        // 启动工作线程处理
        QMetaObject::invokeMethod(m_saveWorker, "startSaving", Qt::QueuedConnection);

        // 启动统计更新定时器
        m_statsUpdateTimer.start();

        LOG_INFO(QString("开始保存文件到: %1").arg(m_model->getFullSavePath()));
        emit saveStarted();
        return true;
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("开始保存异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        return false;
    }
    catch (...) {
        QString errorMsg = "开始保存未知异常";
        LOG_ERROR(errorMsg);
        emit saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        return false;
    }
}

bool FileSaveController::stopSaving()
{
    if (!isSaving()) {
        LOG_WARN("没有正在进行的保存任务");
        return false;
    }

    try {
        // 停止统计更新定时器
        if (m_statsUpdateTimer.isActive()) {
            m_statsUpdateTimer.stop();
        }

        // 停止工作线程
        if (m_saveWorker) {
            m_saveWorker->stop();
        }

        // 设置状态为已完成
        m_model->setStatus(SaveStatus::FS_COMPLETED);

        LOG_INFO("停止文件保存");
        emit saveStopped();
        return true;
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("停止保存异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        return false;
    }
    catch (...) {
        QString errorMsg = "停止保存未知异常";
        LOG_ERROR(errorMsg);
        emit saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        return false;
    }
}

void FileSaveController::showSettings(QWidget* parent)
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
    FileSaveView* view = createSaveView(parent);
    view->prepareForShow();
    view->show();
}

void FileSaveController::processDataPacket(const DataPacket& packet)
{
    // 如果不在保存状态，忽略数据包
    if (!isSaving()) {
        return;
    }

    try {
        // 发送数据包到工作线程进行保存
        QMetaObject::invokeMethod(m_saveWorker, "processDataPacket",
            Qt::QueuedConnection,
            Q_ARG(DataPacket, packet));

        // 更新统计信息
        SaveStatistics stats = m_model->getStatistics();
        stats.packetCount++; // 增加包计数
        m_model->updateStatistics(stats);
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("处理数据包异常: %1").arg(e.what());
        LOG_ERROR(errorMsg);
        emit saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        stopSaving();
    }
    catch (...) {
        QString errorMsg = "处理数据包未知异常";
        LOG_ERROR(errorMsg);
        emit saveError(LocalQTCompat::fromLocal8Bit(errorMsg.toUtf8().constData()));
        stopSaving();
    }
}

void FileSaveController::onModelStatusChanged(SaveStatus status)
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
        emit saveCompleted(path, stats.totalBytes);
        break;
    }
    case SaveStatus::FS_ERROR:
        // 错误处理已经在其他地方完成
        break;
    }
}

void FileSaveController::onModelStatisticsUpdated(const SaveStatistics& statistics)
{
    // 控制器可以在这里处理统计信息更新
    // 例如，可以记录日志、更新内部状态等
}

void FileSaveController::onModelSaveCompleted(const QString& path, uint64_t totalBytes)
{
    // 转发保存完成信号
    emit saveCompleted(path, totalBytes);
}

void FileSaveController::onModelSaveError(const QString& error)
{
    // 处理保存错误
    LOG_ERROR(QString("文件保存错误: %1").arg(error));

    // 停止保存
    if (isSaving()) {
        stopSaving();
    }

    // 转发错误信号
    emit saveError(error);
}

void FileSaveController::onViewParametersChanged(const SaveParameters& parameters)
{
    // 更新模型参数
    m_model->setSaveParameters(parameters);

    // 保存配置到设置
    m_model->saveConfigToSettings();

    // 同步到工作线程
    if (m_saveWorker) {
        m_saveWorker->setParameters(parameters);
    }
}

void FileSaveController::onViewStartSaveRequested()
{
    startSaving();
}

void FileSaveController::onViewStopSaveRequested()
{
    stopSaving();
}

void FileSaveController::onWorkerSaveProgress(uint64_t bytesWritten, int fileCount)
{
    // 更新模型中的统计信息
    SaveStatistics stats = m_model->getStatistics();
    stats.totalBytes = bytesWritten;
    stats.fileCount = fileCount;
    m_model->updateStatistics(stats);
}

void FileSaveController::onWorkerSaveCompleted(const QString& path, uint64_t totalBytes)
{
    // 处理工作线程完成信号
    m_model->setStatus(SaveStatus::FS_COMPLETED);
    emit saveCompleted(path, totalBytes);
}

void FileSaveController::onWorkerSaveError(const QString& error)
{
    // 处理工作线程错误
    LOG_ERROR(QString("工作线程保存错误: %1").arg(error));
    m_model->setStatus(SaveStatus::FS_ERROR);
    emit saveError(error);
}

void FileSaveController::updateSaveStatistics()
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

void FileSaveController::connectModelSignals()
{
    connect(m_model, &FileSaveModel::statusChanged,
        this, &FileSaveController::onModelStatusChanged);
    connect(m_model, &FileSaveModel::statisticsUpdated,
        this, &FileSaveController::onModelStatisticsUpdated);
    connect(m_model, &FileSaveModel::saveCompleted,
        this, &FileSaveController::onModelSaveCompleted);
    connect(m_model, &FileSaveModel::saveError,
        this, &FileSaveController::onModelSaveError);
}

void FileSaveController::connectViewSignals(FileSaveView* view)
{
    if (!view) return;

    connect(view, &FileSaveView::saveParametersChanged,
        this, &FileSaveController::onViewParametersChanged);
    connect(view, &FileSaveView::startSaveRequested,
        this, &FileSaveController::onViewStartSaveRequested);
    connect(view, &FileSaveView::stopSaveRequested,
        this, &FileSaveController::onViewStopSaveRequested);

    // 连接控制器信号到视图
    connect(this, &FileSaveController::saveStarted,
        view, &FileSaveView::onSaveStarted);
    connect(this, &FileSaveController::saveStopped,
        view, &FileSaveView::onSaveStopped);
    connect(this, &FileSaveController::saveCompleted,
        view, &FileSaveView::onSaveCompleted);
    connect(this, &FileSaveController::saveError,
        view, &FileSaveView::onSaveError);

    // 连接模型信号到视图
    connect(m_model, &FileSaveModel::statisticsUpdated,
        view, &FileSaveView::updateStatisticsDisplay);
    connect(m_model, &FileSaveModel::statusChanged,
        view, &FileSaveView::updateStatusDisplay);
}

void FileSaveController::connectWorkerSignals()
{
    if (!m_saveWorker) return;

    // 连接工作线程信号
    connect(m_saveWorker, &FileSaveWorker::saveProgress,
        this, &FileSaveController::onWorkerSaveProgress,
        Qt::QueuedConnection);

    connect(m_saveWorker, &FileSaveWorker::saveCompleted,
        this, &FileSaveController::onWorkerSaveCompleted,
        Qt::QueuedConnection);

    connect(m_saveWorker, &FileSaveWorker::saveError,
        this, &FileSaveController::onWorkerSaveError,
        Qt::QueuedConnection);
}