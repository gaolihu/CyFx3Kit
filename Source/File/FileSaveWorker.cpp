// Source/File/DataConverters.cpp
#include "FileSaveWorker.h"
#include "Logger.h"
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QStorageInfo>
#include <QFile>
#include <QCoreApplication>

FileSaveWorker::FileSaveWorker(QObject* parent)
    : QObject(parent)
    , m_isStopping(false)
    , m_totalBytes(0)
    , m_fileCount(0)
    , m_fileIndex(0)
{
    LOG_INFO("文件保存工作线程已创建");
}

FileSaveWorker::~FileSaveWorker()
{
    stop();
    LOG_INFO("文件保存工作线程已销毁");
}

void FileSaveWorker::setParameters(const SaveParameters& params)
{
    QMutexLocker locker(&m_mutex);
    m_parameters = params;

    // 根据文件格式创建对应的转换器
    m_converter = DataConverterFactory::createConverter(params.format);

    LOG_INFO(QString("文件保存参数已更新，格式: %1，转换器: %2")
        .arg(static_cast<int>(params.format))
        .arg(m_converter ? m_converter->getFileExtension() : "未知"));
}

void FileSaveWorker::stop()
{
    m_isStopping = true;
    LOG_INFO("文件保存已停止");
}

void FileSaveWorker::startSaving()
{
    // 重置状态
    m_isStopping = false;
    m_totalBytes = 0;
    m_fileCount = 0;
    m_fileIndex = 0;

    QMutexLocker locker(&m_mutex);

    // 创建保存目录
    m_savePath = createSavePath();
    QDir saveDir(m_savePath);

    if (!saveDir.exists()) {
        if (!saveDir.mkpath(".")) {
            QString errorMsg = QString("无法创建保存目录: %1").arg(m_savePath);
            LOG_ERROR(errorMsg);
            emit saveError(errorMsg);
            return;
        }
    }

    // 确保有足够的磁盘空间
    if (!checkDiskSpace(m_savePath, 100 * 1024 * 1024)) { // 至少100MB的空间
        QString errorMsg = QString("磁盘空间不足: %1").arg(m_savePath);
        LOG_ERROR(errorMsg);
        emit saveError(errorMsg);
        return;
    }

    // 确保转换器已创建
    if (!m_converter) {
        m_converter = DataConverterFactory::createConverter(m_parameters.format);
    }

    LOG_INFO(QString("开始保存文件到: %1，格式: %2")
        .arg(m_savePath)
        .arg(m_converter ? m_converter->getFileExtension() : "未知"));

    emit saveProgress(0, 0); // 初始化进度
}

void FileSaveWorker::processDataPacket(const DataPacket& packet)
{
    // 检查是否停止
    if (m_isStopping) {
        return;
    }

    // 单个数据包处理 - 简单将其放入只有一个元素的批次中处理
    DataPacketBatch singlePacketBatch = { packet };
    processDataBatch(singlePacketBatch);
}

void FileSaveWorker::processDataBatch(const DataPacketBatch& packets)
{
    // 检查是否停止
    if (m_isStopping) {
        return;
    }

    // 检查批次是否为空
    if (packets.empty()) {
        LOG_WARN("收到空的数据包批次，忽略");
        return;
    }

    // 计算批次的总大小
    uint64_t batchSize = 0;
    for (const auto& packet : packets) {
        batchSize += packet.getSize();
    }

    // 保存数据批次
    if (saveDataBatch(packets)) {
        QMutexLocker locker(&m_mutex);
        m_totalBytes += batchSize;
        m_fileCount++;

        // 通知进度更新
        emit saveProgress(m_totalBytes, m_fileCount);

        LOG_INFO(QString("已保存数据批次，大小: %1 字节，包含 %2 个数据包")
            .arg(batchSize)
            .arg(packets.size()));
    }

    // 定期处理Qt事件，确保信号正常传递
    QCoreApplication::processEvents();
}

bool FileSaveWorker::saveDataPacket(const DataPacket& packet)
{
    SaveParameters params;
    QString savePath;
    std::shared_ptr<IDataConverter> converter;

    // 获取线程安全的参数和路径
    {
        QMutexLocker locker(&m_mutex);
        params = m_parameters;
        savePath = m_savePath;
        converter = m_converter;
    }

    if (!converter) {
        LOG_ERROR("数据转换器未初始化");
        return false;
    }

    try {
        // 生成文件名
        QString filename = generateFileName();

        // 添加扩展名
        filename = addFileExtension(filename, params.format);

        // 完整文件路径
        QString fullPath = QDir(savePath).filePath(filename);

        // 转换数据
        QByteArray convertedData = converter->convert(packet, params);
        if (convertedData.isEmpty()) {
            LOG_ERROR("数据转换失败，无法获取有效数据");
            return false;
        }

        // 写入文件
        if (!writeToFile(fullPath, convertedData)) {
            return false;
        }

        LOG_INFO(QString("数据包已保存到: %1，大小: %2 字节")
            .arg(fullPath)
            .arg(convertedData.size()));

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存数据包异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR("保存数据包未知异常");
        return false;
    }
}

bool FileSaveWorker::saveDataBatch(const DataPacketBatch& packets)
{
    if (packets.empty()) {
        return false;
    }

    SaveParameters params;
    QString savePath;
    std::shared_ptr<IDataConverter> converter;

    // 获取线程安全的参数和路径
    {
        QMutexLocker locker(&m_mutex);
        params = m_parameters;
        savePath = m_savePath;
        converter = m_converter;
    }

    if (!converter) {
        LOG_ERROR("数据转换器未初始化");
        return false;
    }

    try {
        // 生成文件名
        QString filename = generateFileName();

        // 添加扩展名
        filename = addFileExtension(filename, params.format);

        // 完整文件路径
        QString fullPath = QDir(savePath).filePath(filename);

        // 使用批量转换方法
        QByteArray convertedData = converter->convertBatch(packets, params);
        if (convertedData.isEmpty()) {
            LOG_ERROR("批量数据转换失败，无法获取有效数据");
            return false;
        }

        // 写入文件
        if (!writeToFile(fullPath, convertedData)) {
            return false;
        }

        LOG_INFO(QString("数据批次已保存到: %1，大小: %2 字节，包含 %3 个数据包")
            .arg(fullPath)
            .arg(convertedData.size())
            .arg(packets.size()));

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存数据批次异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR("保存数据批次未知异常");
        return false;
    }
}

bool FileSaveWorker::writeToFile(const QString& fullPath, const QByteArray& data)
{
    QFile file(fullPath);

    // 使用"无缓冲"的方式打开文件，优化大文件写入性能
    if (!file.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) {
        LOG_ERROR(QString("无法打开文件进行写入: %1, 错误: %2")
            .arg(fullPath).arg(file.errorString()));
        return false;
    }

    // 写入数据
    qint64 written = file.write(data);
    bool success = (written == data.size());

    if (!success) {
        LOG_ERROR(QString("文件写入失败: %1, 写入: %2/%3 字节, 错误: %4")
            .arg(fullPath)
            .arg(written)
            .arg(data.size())
            .arg(file.errorString()));
    }

    // 确保数据写入磁盘
    file.flush();
    file.close();

    return success;
}

QString FileSaveWorker::generateFileName()
{
    QMutexLocker locker(&m_mutex);

    QString filename;
    if (m_parameters.autoNaming) {
        // 使用时间戳作为文件名
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
        filename = m_parameters.filePrefix + "_" + timestamp;
    }
    else {
        // 使用前缀加序号
        filename = QString("%1_%2").arg(m_parameters.filePrefix)
            .arg(m_fileIndex++, 6, 10, QChar('0'));

        // 如果需要附加时间戳
        if (m_parameters.appendTimestamp) {
            QString timestamp = QDateTime::currentDateTime().toString("_yyyyMMdd_hhmmss");
            filename += timestamp;
        }
    }

    return filename;
}

QString FileSaveWorker::createSavePath()
{
    QMutexLocker locker(&m_mutex);
    QString basePath = m_parameters.basePath;

    // 如果需要创建日期子文件夹
    if (m_parameters.createSubfolder) {
        QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
        return QDir(basePath).filePath(dateStr);
    }

    return basePath;
}

QString FileSaveWorker::addFileExtension(const QString& baseName, FileFormat format)
{
    if (m_converter) {
        return baseName + "." + m_converter->getFileExtension();
    }

    // 回退到基于格式的默认扩展名
    switch (format) {
    case FileFormat::RAW:
        return baseName + ".raw";
    case FileFormat::BMP:
        return baseName + ".bmp";
    case FileFormat::TIFF:
        return baseName + ".tiff";
    case FileFormat::PNG:
        return baseName + ".png";
    case FileFormat::CSV:
        return baseName + ".csv";
    case FileFormat::TEXT:
        return baseName + ".txt";
    default:
        return baseName + ".dat";
    }
}

bool FileSaveWorker::checkDiskSpace(const QString& path, uint64_t requiredBytes)
{
    QStorageInfo storage(path);

    if (!storage.isValid() || !storage.isReady()) {
        LOG_ERROR(QString("存储设备无效或未就绪: %1").arg(path));
        return false;
    }

    quint64 freeBytes = storage.bytesAvailable();
    LOG_INFO(QString("存储设备可用空间: %1 MB").arg(freeBytes / (1024 * 1024)));

    return freeBytes >= requiredBytes;
}