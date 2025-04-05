// Source/MVC/Models/FileOperationModel.cpp
#include "FileOperationModel.h"
#include "Logger.h"
#include <QSettings>
#include <QDir>
#include <QDate>

FileOperationModel* FileOperationModel::getInstance()
{
    static FileOperationModel s;
    return &s;
}

FileOperationModel::FileOperationModel()
    : QObject(nullptr)
    , m_fileManager(FileManager::instance())
    , m_status(SaveStatus::FS_IDLE)
{
    // 将FileManager的信号连接到Model的信号
    connect(&m_fileManager, &FileManager::signal_FSM_saveStatusChanged,
        this, &FileOperationModel::onSaveManagerStatusChanged);
    connect(&m_fileManager, &FileManager::signal_FSM_saveProgressUpdated,
        this, &FileOperationModel::onSaveManagerProgressUpdated);
    connect(&m_fileManager, &FileManager::signal_FSM_saveCompleted,
        this, &FileOperationModel::onSaveManagerCompleted);
    connect(&m_fileManager, &FileManager::signal_FSM_saveError,
        this, &FileOperationModel::onSaveManagerError);

    // 加载相关信号连接
    connect(&m_fileManager, &FileManager::signal_FSM_loadStarted,
        this, &FileOperationModel::signal_FS_M_loadStarted);
    connect(&m_fileManager, &FileManager::signal_FSM_loadProgress,
        this, &FileOperationModel::signal_FS_M_loadProgress);
    connect(&m_fileManager, &FileManager::signal_FSM_loadCompleted,
        this, &FileOperationModel::signal_FS_M_loadCompleted);
    connect(&m_fileManager, &FileManager::signal_FSM_loadError,
        this, &FileOperationModel::signal_FS_M_loadError);
    connect(&m_fileManager, &FileManager::signal_FSM_newDataAvailable,
        this, &FileOperationModel::signal_FS_M_newDataAvailable);

    // 数据读取相关信号连接
    connect(&m_fileManager, &FileManager::signal_FSM_dataReadCompleted,
        this, &FileOperationModel::signal_FS_M_dataReadCompleted);
    connect(&m_fileManager, &FileManager::signal_FSM_dataReadError,
        this, &FileOperationModel::signal_FS_M_dataReadError);

    // 从Manager同步初始状态
    syncFromManager();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存模型已创建"));
}

FileOperationModel::~FileOperationModel()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存模型已销毁"));
}

SaveParameters FileOperationModel::getSaveParameters() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_parameters;
}

void FileOperationModel::setSaveParameters(const SaveParameters& parameters)
{
    // 创建一个参数副本
    SaveParameters modifiedParams = parameters;

    // 始终强制使用RAW格式
    modifiedParams.format = FileFormat::RAW;

    // 将修改后的参数传递给FileManager
    m_fileManager.setSaveParameters(modifiedParams);

    // 同步状态
    syncFromManager();

    emit signal_FS_M_parametersChanged(modifiedParams);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存参数已更新，已强制设置为RAW格式"));
}

bool FileOperationModel::startSaving()
{
    return m_fileManager.startSaving();
}

bool FileOperationModel::stopSaving()
{
    return m_fileManager.stopSaving();
}

void FileOperationModel::processDataPacket(const DataPacket& packet)
{
    m_fileManager.slot_FSM_processDataPacket(packet);
}

bool FileOperationModel::startLoading(const QString& filePath) {
    if (m_fileManager.startLoading(filePath)) {
        m_loadedFilePath = filePath;
        return true;
    }
    return false;
}

bool FileOperationModel::stopLoading() {
    return m_fileManager.stopLoading();
}

bool FileOperationModel::isLoading() const {
    return m_fileManager.isLoading();
}

DataPacket FileOperationModel::getNextPacket() {
    return m_fileManager.getNextPacket();
}

bool FileOperationModel::hasMorePackets() const {
    return m_fileManager.hasMorePackets();
}

void FileOperationModel::seekTo(uint64_t position) {
    m_fileManager.seekTo(position);
}

uint64_t FileOperationModel::getTotalFileSize() const {
    return m_fileManager.getTotalFileSize();
}

QByteArray FileOperationModel::readFileRange(const QString& filePath, uint64_t startOffset, uint64_t size) {
    return m_fileManager.readFileRange(filePath, startOffset, size);
}

QByteArray FileOperationModel::readLoadedFileRange(uint64_t startOffset, uint64_t size) {
    return m_fileManager.readLoadedFileRange(startOffset, size);
}

bool FileOperationModel::readFileRangeAsync(const QString& filePath, uint64_t startOffset, uint64_t size, uint32_t requestId) {
    return m_fileManager.readFileRangeAsync(filePath, startOffset, size, requestId);
}

QString FileOperationModel::getCurrentFileName() const {
    // 返回当前加载的文件名
    if (m_fileManager.isLoading()) {
        return m_loadedFilePath;
    }
    else {
        return m_fileManager.getCurrentFileName();
    }
}

// 同步FileManager的状态到Model
void FileOperationModel::syncFromManager()
{
    m_parameters = m_fileManager.getSaveParameters();
    SaveStatistics stats = m_fileManager.getStatistics();
    updateStatistics(stats);
    setStatus(stats.status);
}

// FileManager信号处理函数
void FileOperationModel::onSaveManagerStatusChanged(SaveStatus status)
{
    setStatus(status);
}

void FileOperationModel::onSaveManagerProgressUpdated(const SaveStatistics& stats)
{
    updateStatistics(stats);
}

void FileOperationModel::onSaveManagerCompleted(const QString& path, uint64_t totalBytes)
{
    // 更新状态并转发信号
    setStatus(SaveStatus::FS_COMPLETED);
    emit signal_FS_M_saveCompleted(path, totalBytes);
}

void FileOperationModel::onSaveManagerError(const QString& error)
{
    // 更新状态并转发信号
    setStatus(SaveStatus::FS_ERROR);
    emit signal_FS_M_saveError(error);
}

SaveStatus FileOperationModel::getStatus() const
{
    return m_status.load();
}

void FileOperationModel::setStatus(SaveStatus status)
{
    SaveStatus oldStatus = m_status.exchange(status);

    if (oldStatus != status) {
        emit signal_FS_M_statusChanged(status);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存状态已更改: %1").arg(static_cast<int>(status)));

        // 如果状态已完成或错误，发送对应信号
        if (status == SaveStatus::FS_COMPLETED) {
            SaveStatistics stats = getStatistics();
            emit signal_FS_M_saveCompleted(getFullSavePath(), stats.totalBytes);
        }
    }
}

SaveStatistics FileOperationModel::getStatistics() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_statistics;
}

void FileOperationModel::updateStatistics(const SaveStatistics& statistics)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_statistics = statistics;
    }

    emit signal_FS_M_statisticsUpdated(statistics);
}

void FileOperationModel::resetStatistics()
{
    SaveStatistics statistics;
    statistics.startTime = QDateTime::currentDateTime();
    statistics.lastUpdateTime = statistics.startTime;

    updateStatistics(statistics);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存统计已重置"));
}

QString FileOperationModel::getFullSavePath() const
{
    QMutexLocker locker(&m_dataMutex);

    QString path = m_parameters.basePath;

    // 如果设置了创建子文件夹，添加日期子文件夹
    if (m_parameters.createSubfolder) {
        QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
        path = QDir(path).filePath(dateStr);
    }

    return path;
}

QVariant FileOperationModel::getOption(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_dataMutex);
    return m_parameters.options.value(key, defaultValue);
}

void FileOperationModel::setOption(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_dataMutex);
    m_parameters.options[key] = value;
}

void FileOperationModel::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    QMutexLocker locker(&m_dataMutex);
    m_parameters.options["width"] = width;
    m_parameters.options["height"] = height;
    m_parameters.options["format"] = format;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));
}

void FileOperationModel::setUseAsyncWriter(bool use)
{
    m_useAsyncWriter = use;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("异步文件写入模式: %1").arg(use ? "已启用" : "已禁用"));
}

bool FileOperationModel::isUsingAsyncWriter() const
{
    return m_useAsyncWriter;
}

bool FileOperationModel::saveConfigToSettings()
{
    try {
        QSettings settings("FX3Tool", "FileOperationSettings");
        QMutexLocker locker(&m_dataMutex);

        // 保存基本设置
        settings.setValue("basePath", m_parameters.basePath);
        settings.setValue("filePrefix", m_parameters.filePrefix);
        settings.setValue("format", static_cast<int>(m_parameters.format));
        settings.setValue("autoNaming", m_parameters.autoNaming);
        settings.setValue("createSubfolder", m_parameters.createSubfolder);
        settings.setValue("appendTimestamp", m_parameters.appendTimestamp);
        settings.setValue("saveMetadata", m_parameters.saveMetadata);
        settings.setValue("compressionLevel", m_parameters.compressionLevel);
        settings.setValue("useAsyncWriter", m_useAsyncWriter);

        // 保存选项
        settings.beginGroup("Options");
        for (auto it = m_parameters.options.constBegin(); it != m_parameters.options.constEnd(); ++it) {
            settings.setValue(it.key(), it.value());
        }
        settings.endGroup();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存配置已保存到系统设置"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("保存配置异常: %1").arg(e.what()));
        return false;
    }
}

bool FileOperationModel::loadConfigFromSettings()
{
    try {
        QSettings settings("FX3Tool", "FileOperationSettings");
        SaveParameters params;

        // 加载基本设置
        params.basePath = settings.value("basePath", QDir::homePath() + "/FX3Data").toString();
        params.filePrefix = settings.value("filePrefix", "FX3_").toString();
        params.format = static_cast<FileFormat>(settings.value("format", static_cast<int>(FileFormat::RAW)).toInt());
        params.autoNaming = settings.value("autoNaming", true).toBool();
        params.createSubfolder = settings.value("createSubfolder", false).toBool();
        params.appendTimestamp = settings.value("appendTimestamp", false).toBool();
        params.saveMetadata = settings.value("saveMetadata", false).toBool();
        params.compressionLevel = settings.value("compressionLevel", 0).toInt();
        m_useAsyncWriter = settings.value("useAsyncWriter", false).toBool();

        // 加载选项
        settings.beginGroup("Options");
        QStringList keys = settings.childKeys();
        for (const QString& key : keys) {
            params.options[key] = settings.value(key);
        }
        settings.endGroup();

        setSaveParameters(params);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存配置已从系统设置加载"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载配置异常: %1").arg(e.what()));
        resetToDefault();
        return false;
    }
}

void FileOperationModel::resetToDefault()
{
    SaveParameters params;

    // 设置默认值
    params.basePath = QDir::homePath() + "/FX3Data";
    params.filePrefix = "capture";
    // 强制使用RAW格式
    params.format = FileFormat::RAW;
    params.autoNaming = true;
    params.createSubfolder = true;  // 默认创建子文件夹
    params.appendTimestamp = true;  // 默认添加时间戳
    params.saveMetadata = true;     // 默认保存元数据
    params.compressionLevel = 0;
    m_useAsyncWriter = true;        // 默认使用异步写入

    // 设置默认图像参数
    params.options["width"] = 1920;
    params.options["height"] = 1080;
    params.options["format"] = 0x39;  // RAW10

    // 设置高级选项
    params.options["max_file_size"] = static_cast<qulonglong>(100ULL * 1024 * 1024 * 1024); // 100GB 最大文件大小
    params.options["auto_split_time"] = 300;  // 5分钟自动分割
    params.options["buffer_size"] = 64 * 1024 * 1024;  // 64MB 缓冲区
    params.options["auto_save"] = true;  // 默认启用自动保存
    params.options["create_index"] = true;  // 创建索引文件

    setSaveParameters(params);
    resetStatistics();
    setStatus(SaveStatus::FS_IDLE);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存模型已重置为默认值，使用RAW格式"));
}
