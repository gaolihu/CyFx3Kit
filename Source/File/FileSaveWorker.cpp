// Source/MVC/Workers/FileSaveWorker.cpp
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
    m_packetQueue.clear();
    LOG_INFO("文件保存工作线程已销毁");
}

void FileSaveWorker::setParameters(const SaveParameters& params)
{
    QMutexLocker locker(&m_mutex);
    m_parameters = params;
}

void FileSaveWorker::stop()
{
    m_isStopping = true;

    // 等待队列处理完成
    QMutexLocker locker(&m_mutex);
    m_packetQueue.clear();
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

    // 确保有足够的磁盘空间 (为简化，只进行基本检查)
    if (!checkDiskSpace(m_savePath, 1024 * 1024)) { // 至少1MB的空间
        QString errorMsg = QString("磁盘空间不足: %1").arg(m_savePath);
        LOG_ERROR(errorMsg);
        emit saveError(errorMsg);
        return;
    }

    LOG_INFO(QString("开始保存文件到: %1").arg(m_savePath));
}

void FileSaveWorker::processDataPacket(const DataPacket& packet)
{
    // 检查是否停止
    if (m_isStopping) {
        return;
    }

    // 保存数据包
    if (saveDataPacket(packet)) {
        QMutexLocker locker(&m_mutex);
        m_totalBytes += packet.size;
        m_fileCount++;

        // 通知进度更新
        emit saveProgress(m_totalBytes, m_fileCount);
    }

    // 定期处理Qt事件，确保信号正常传递
    QCoreApplication::processEvents();
}

bool FileSaveWorker::saveDataPacket(const DataPacket& packet)
{
    SaveParameters params;
    QString savePath;

    // 获取线程安全的参数和路径
    {
        QMutexLocker locker(&m_mutex);
        params = m_parameters;
        savePath = m_savePath;
    }

    try {
        // 生成文件名
        QString filename = generateFileName();

        // 添加扩展名
        filename = addFileExtension(filename);

        // 完整文件路径
        QString fullPath = QDir(savePath).filePath(filename);

        // 实际数据保存逻辑
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly)) {
            LOG_ERROR(QString("无法打开文件进行写入: %1, 错误: %2")
                .arg(fullPath).arg(file.errorString()));
            return false;
        }

        // 根据不同格式处理文件保存
        bool result = false;
        switch (params.format) {
        case FileFormat::RAW:
            // 直接写入原始数据
            //result = (file.write(reinterpret_cast<const char*>(packet.data), packet.size) == packet.size);
            // TODO
            break;

        case FileFormat::CSV:
            // 将数据格式化为CSV
        {
            QByteArray csvData;
            uint32_t width = params.options.value("width", 1920).toUInt();
            uint32_t bytesPerLine = params.options.value("bytes_per_line", 16).toUInt();

            // 格式化数据为CSV
            for (uint32_t i = 0; i < packet.size; i++) {
                csvData.append(QString::number(packet.data[i]).toUtf8());

                // 添加分隔符或换行
                if (i < packet.size - 1) {
                    if ((i + 1) % bytesPerLine == 0 || (i + 1) % width == 0) {
                        csvData.append("\n");
                    }
                    else {
                        csvData.append(",");
                    }
                }
            }

            result = (file.write(csvData) == csvData.size());
        }
        break;

        case FileFormat::TEXT:
            // 将数据格式化为文本
        {
            QByteArray textData;
            uint32_t bytesPerLine = params.options.value("bytes_per_line", 16).toUInt();

            // 格式化数据为文本
            for (uint32_t i = 0; i < packet.size; i++) {
                textData.append(QString("%1 ").arg(packet.data[i], 2, 16, QChar('0')).toUpper().toUtf8());

                // 添加换行
                if ((i + 1) % bytesPerLine == 0 && i < packet.size - 1) {
                    textData.append("\n");
                }
            }

            result = (file.write(textData) == textData.size());
        }
        break;

        case FileFormat::BMP:
            // 处理BMP格式 (简化实现，实际需要根据图像格式构建BMP文件头和数据)
            //result = (file.write(reinterpret_cast<const char*>(packet.data), packet.size) == packet.size);
            //TODO
            break;

        default:
            LOG_ERROR(QString("不支持的文件格式: %1").arg(static_cast<int>(params.format)));
            result = false;
            break;
        }

        file.close();

        if (result) {
            LOG_INFO(QString("数据包已保存到: %1").arg(fullPath));
        }
        else {
            LOG_ERROR(QString("保存数据包失败: %1").arg(fullPath));
        }

        return result;
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

QString FileSaveWorker::addFileExtension(const QString& baseName)
{
    QMutexLocker locker(&m_mutex);

    // 根据格式添加扩展名
    switch (m_parameters.format) {
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