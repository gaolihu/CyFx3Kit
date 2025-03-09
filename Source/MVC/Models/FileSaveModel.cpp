// Source/MVC/Models/FileSaveModel.cpp
#include "FileSaveModel.h"
#include "Logger.h"
#include <QSettings>
#include <QDir>
#include <QDate>

FileSaveModel* FileSaveModel::getInstance()
{
    static FileSaveModel s;
    return &s;
}

FileSaveModel::FileSaveModel()
    : QObject(nullptr)
    , m_status(SaveStatus::FS_IDLE)
    , m_useAsyncWriter(false)
{
    // 设置默认值
    resetToDefault();
    LOG_INFO("文件保存模型已创建");
}

FileSaveModel::~FileSaveModel()
{
    LOG_INFO("文件保存模型已销毁");
}

SaveParameters FileSaveModel::getSaveParameters() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_parameters;
}

void FileSaveModel::setSaveParameters(const SaveParameters& parameters)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_parameters = parameters;
    }

    emit parametersChanged(parameters);
    LOG_INFO("文件保存参数已更新");
}

SaveStatus FileSaveModel::getStatus() const
{
    return m_status.load();
}

void FileSaveModel::setStatus(SaveStatus status)
{
    SaveStatus oldStatus = m_status.exchange(status);

    if (oldStatus != status) {
        emit statusChanged(status);

        LOG_INFO(QString("文件保存状态已更改: %1").arg(static_cast<int>(status)));

        // 如果状态已完成或错误，发送对应信号
        if (status == SaveStatus::FS_COMPLETED) {
            SaveStatistics stats = getStatistics();
            emit saveCompleted(getFullSavePath(), stats.totalBytes);
        }
    }
}

SaveStatistics FileSaveModel::getStatistics() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_statistics;
}

void FileSaveModel::updateStatistics(const SaveStatistics& statistics)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_statistics = statistics;
    }

    emit statisticsUpdated(statistics);
}

void FileSaveModel::resetStatistics()
{
    SaveStatistics statistics;
    statistics.startTime = QDateTime::currentDateTime();
    statistics.lastUpdateTime = statistics.startTime;

    updateStatistics(statistics);
    LOG_INFO("文件保存统计已重置");
}

QString FileSaveModel::getFullSavePath() const
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

QVariant FileSaveModel::getOption(const QString& key, const QVariant& defaultValue) const
{
    QMutexLocker locker(&m_dataMutex);
    return m_parameters.options.value(key, defaultValue);
}

void FileSaveModel::setOption(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_dataMutex);
    m_parameters.options[key] = value;
}

void FileSaveModel::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    QMutexLocker locker(&m_dataMutex);
    m_parameters.options["width"] = width;
    m_parameters.options["height"] = height;
    m_parameters.options["format"] = format;

    LOG_INFO(QString("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));
}

void FileSaveModel::setUseAsyncWriter(bool use)
{
    m_useAsyncWriter = use;
    LOG_INFO(QString("异步文件写入模式: %1").arg(use ? "已启用" : "已禁用"));
}

bool FileSaveModel::isUsingAsyncWriter() const
{
    return m_useAsyncWriter;
}

bool FileSaveModel::saveConfigToSettings()
{
    try {
        QSettings settings("FX3Tool", "FileSaveSettings");
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

        LOG_INFO("文件保存配置已保存到系统设置");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存配置异常: %1").arg(e.what()));
        return false;
    }
}

bool FileSaveModel::loadConfigFromSettings()
{
    try {
        QSettings settings("FX3Tool", "FileSaveSettings");
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
        LOG_INFO("文件保存配置已从系统设置加载");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载配置异常: %1").arg(e.what()));
        resetToDefault();
        return false;
    }
}

void FileSaveModel::resetToDefault()
{
    SaveParameters params;

    // 设置默认值
    params.basePath = QDir::homePath() + "/FX3Data";
    params.filePrefix = "FX3_";
    params.format = FileFormat::RAW;
    params.autoNaming = true;
    params.createSubfolder = false;
    params.appendTimestamp = false;
    params.saveMetadata = false;
    params.compressionLevel = 0;
    m_useAsyncWriter = false;

    // 设置默认图像参数
    params.options["width"] = 1920;
    params.options["height"] = 1080;
    params.options["format"] = 0x39;  // RAW10

    setSaveParameters(params);
    resetStatistics();
    setStatus(SaveStatus::FS_IDLE);

    LOG_INFO("文件保存模型已重置为默认值");
}