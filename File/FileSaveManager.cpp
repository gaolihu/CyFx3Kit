#include "FileSaveManager.h"
#include "Logger.h"
#include "DataConverters.h"
#include <QDir>
#include <QDateTime>
#include <QApplication>
#include <QFileInfo>
#include <future>
#include <chrono>

FileSaveManager& FileSaveManager::instance() {
    static FileSaveManager instance;
    return instance;
}

FileSaveManager::FileSaveManager()
    : m_running(false)
    , m_paused(false)
    , m_useAsyncWriter(false)
    , m_lastSavedBytes(0)
{
    // 初始化默认保存参数
    m_saveParams.basePath = QDir::homePath() + "/FX3Data";
    m_saveParams.format = FileFormat::RAW;
    m_saveParams.autoNaming = true;
    m_saveParams.filePrefix = "capture";
    m_saveParams.createSubfolder = true;
    m_saveParams.appendTimestamp = true;
    m_saveParams.compressionLevel = 0;
    m_saveParams.saveMetadata = true;

    // 初始化文件写入器和缓存管理器
    resetFileWriter();
    m_cacheManager = std::make_unique<FileCacheManager>();

    // 初始化速度计时器
    m_speedTimer.start();

    // 初始化统计信息
    m_statistics.totalBytes = 0;
    m_statistics.fileCount = 0;
    m_statistics.saveRate = 0.0;
    m_statistics.status = SaveStatus::FS_IDLE;

    // 注册内置转换器
    registerConverter(FileFormat::RAW, DataConverterFactory::createConverter(FileFormat::RAW));
    registerConverter(FileFormat::BMP, DataConverterFactory::createConverter(FileFormat::BMP));
    registerConverter(FileFormat::TIFF, DataConverterFactory::createConverter(FileFormat::TIFF));
    registerConverter(FileFormat::PNG, DataConverterFactory::createConverter(FileFormat::PNG));
    registerConverter(FileFormat::CSV, DataConverterFactory::createConverter(FileFormat::CSV));
}

FileSaveManager::~FileSaveManager() {
    stopSaving();
}

void FileSaveManager::setSaveParameters(const SaveParameters& params) {
    std::lock_guard<std::mutex> lock(m_paramsMutex);

    // 如果正在保存中，仅部分参数可以更改
    if (m_running) {
        // 可以在运行时更改的参数
        m_saveParams.compressionLevel = params.compressionLevel;
        m_saveParams.appendTimestamp = params.appendTimestamp;
        m_saveParams.autoNaming = params.autoNaming;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("保存过程中更新参数 - 压缩级别: %1, 时间戳: %2, 自动命名: %3")
            .arg(params.compressionLevel)
            .arg(params.appendTimestamp ? "是" : "否")
            .arg(params.autoNaming ? "是" : "否"));
    }
    else {
        // 非保存状态，可以完全更改参数
        m_saveParams = params;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("完全更新保存参数 - 路径: %1, 格式: %2")
            .arg(params.basePath)
            .arg(static_cast<int>(params.format)));
    }
}

const SaveParameters& FileSaveManager::getSaveParameters() const {
    return m_saveParams;
}

void FileSaveManager::setUseAsyncWriter(bool useAsync) {
    // 如果保存正在进行中不允许切换
    if (m_running) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("保存进行中无法切换写入器模式"));
        return;
    }

    bool oldValue = m_useAsyncWriter.exchange(useAsync);
    if (oldValue != useAsync) {
        resetFileWriter();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件写入器已切换为%1模式")
            .arg(useAsync ? "异步" : "同步"));
    }
}

void FileSaveManager::resetFileWriter() {
    if (m_useAsyncWriter) {
        m_fileWriter = std::make_unique<AsyncFileWriter>();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用异步文件写入器"));
    }
    else {
        m_fileWriter = std::make_unique<StandardFileWriter>();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用标准文件写入器"));
    }
}

bool FileSaveManager::startSaving() {
    if (m_running) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("保存已在进行中"));
        return false;
    }

    // 检查基本参数
    {
        std::lock_guard<std::mutex> lock(m_paramsMutex);
        if (m_saveParams.basePath.isEmpty()) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("未设置保存路径"));
            emit saveError(LocalQTCompat::fromLocal8Bit("未设置保存路径"));
            return false;
        }
    }

    // 创建保存目录
    if (!createSaveDirectory()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建保存目录失败"));
        emit saveError(LocalQTCompat::fromLocal8Bit("创建保存目录失败"));
        return false;
    }

    // 重置统计信息
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.totalBytes = 0;
        m_statistics.fileCount = 0;
        m_statistics.saveRate = 0.0;
        m_statistics.status = SaveStatus::FS_SAVING;
        m_lastSavedBytes = 0;
        m_speedTimer.restart();
    }

    // 设置运行标志
    m_running = true;
    m_paused = false;

    // 清空队列
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<DataPacket> empty;
        std::swap(m_dataQueue, empty);
    }

    // 启动保存线程
    try {
        m_saveThread = std::thread(&FileSaveManager::saveThreadFunction, this);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存线程已启动"));

        // 发送状态更新信号
        emit saveStatusChanged(SaveStatus::FS_SAVING);
        return true;
    }
    catch (const std::exception& e) {
        m_running = false;
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("启动保存线程失败: %1").arg(e.what()));
        emit saveError(LocalQTCompat::fromLocal8Bit("启动保存线程失败: %1").arg(e.what()));
        return false;
    }
}

bool FileSaveManager::stopSaving() {
    if (!m_running) {
        return true;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));

    // 设置停止标志
    m_running = false;
    m_paused = false;

    // 通知数据就绪条件变量，以便线程可以退出等待
    m_dataReady.notify_all();

    // 使用超时等待确保线程正常退出
    if (m_saveThread.joinable()) {
        try {
            // 在另一线程中尝试join，以便可以实现超时
            auto joinResult = std::async(std::launch::async, [this]() {
                if (m_saveThread.joinable()) {
                    m_saveThread.join();
                    return true;
                }
                return false;
                });

            // 等待线程结束，最多等待3秒
            if (joinResult.wait_for(std::chrono::seconds(3)) == std::future_status::timeout) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止保存线程超时，可能存在资源泄漏"));
                // 此处我们不调用detach()，因为这可能导致更严重的资源泄漏
                // 但我们也不能强制终止线程，因为这可能导致资源损坏
                return false;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("停止保存线程异常: %1").arg(e.what()));
            return false;
        }
    }

    // 确保文件已关闭
    m_fileWriter->close();

    // 更新统计信息
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.status = SaveStatus::FS_COMPLETED;
        m_statistics.saveRate = 0.0;
    }

    // 发送状态变更和完成信号
    emit saveStatusChanged(SaveStatus::FS_COMPLETED);
    emit saveCompleted(m_statistics.savePath, m_statistics.totalBytes);

    return true;
}

bool FileSaveManager::pauseSaving(bool pause) {
    if (!m_running) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("未进行保存，无法暂停/恢复"));
        return false;
    }

    if (m_paused == pause) {
        return true; // 已经是请求的状态
    }

    m_paused = pause;

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_statistics.status = pause ? SaveStatus::FS_PAUSED : SaveStatus::FS_SAVING;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存状态变更为: %1")
        .arg(pause ? "暂停" : "保存中"));

    // 如果从暂停恢复，通知线程继续处理
    if (!pause) {
        m_dataReady.notify_one();
    }

    // 发送状态变更信号
    emit saveStatusChanged(pause ? SaveStatus::FS_PAUSED : SaveStatus::FS_SAVING);

    return true;
}

SaveStatistics FileSaveManager::getStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_statistics;
}

void FileSaveManager::registerConverter(FileFormat format, std::shared_ptr<IDataConverter> converter) {
    if (!converter) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("尝试注册空转换器"));
        return;
    }

    m_converters[format] = converter;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("已注册格式转换器: %1").arg(static_cast<int>(format)));
}

QStringList FileSaveManager::getSupportedFormats() const {
    QStringList formats;

    for (const auto& [format, converter] : m_converters) {
        QString extension = converter->getFileExtension();
        formats.append(extension.toUpper() + " (*." + extension.toLower() + ")");
    }

    return formats;
}

void FileSaveManager::processDataPacket(const DataPacket& packet) {
    if (!m_running) {
        return;
    }

    // 添加数据包到队列
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_dataQueue.push(packet);

        // 如果队列积累过多，发出警告
        if (m_dataQueue.size() > 100) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("数据队列积累过多: %1 个包").arg(m_dataQueue.size()));
        }
    }

    // 通知保存线程数据已就绪
    m_dataReady.notify_one();
}

QString FileSaveManager::createFileName(const DataPacket& packet) {
    std::lock_guard<std::mutex> lock(m_paramsMutex);

    QString filename = m_saveParams.filePrefix;

    // 附加帧号或其他标识符
    if (m_statistics.fileCount > 0) {
        filename += QString("_%1").arg(m_statistics.fileCount, 6, 10, QChar('0'));
    }

    // 附加时间戳
    if (m_saveParams.appendTimestamp) {
        QDateTime now = QDateTime::currentDateTime();
        filename += "_" + now.toString("yyyyMMdd_HHmmss_zzz");
    }

    // 获取对应格式的转换器并附加扩展名
    auto it = m_converters.find(m_saveParams.format);
    if (it != m_converters.end()) {
        filename += "." + it->second->getFileExtension();
    }
    else {
        // 默认扩展名
        switch (m_saveParams.format) {
        case FileFormat::RAW:
            filename += ".raw";
            break;
        case FileFormat::BMP:
            filename += ".bmp";
            break;
        case FileFormat::TIFF:
            filename += ".tiff";
            break;
        case FileFormat::PNG:
            filename += ".png";
            break;
        case FileFormat::CSV:
            filename += ".csv";
            break;
        case FileFormat::CUSTOM:
            filename += ".dat";
            break;
        default:
            filename += ".bin";
            break;
        }
    }

    return filename;
}

bool FileSaveManager::createSaveDirectory() {
    std::lock_guard<std::mutex> lock(m_paramsMutex);

    QDir baseDir(m_saveParams.basePath);

    // 创建基本目录
    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建基本目录: %1").arg(m_saveParams.basePath));
            return false;
        }
    }

    QString savePath = m_saveParams.basePath;

    // 创建子文件夹（如果需要）
    if (m_saveParams.createSubfolder) {
        QString subfolder = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        savePath = baseDir.filePath(subfolder);

        QDir subDir(savePath);
        if (!subDir.exists() && !subDir.mkpath(".")) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法创建子文件夹: %1").arg(savePath));
            return false;
        }
    }

    // 更新保存路径
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_statistics.savePath = savePath;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存目录已创建: %1").arg(savePath));
    return true;
}

void FileSaveManager::updateStatistics(uint64_t bytesWritten) {
    std::lock_guard<std::mutex> lock(m_statsMutex);

    // 更新总字节数
    m_statistics.totalBytes += bytesWritten;

    // 更新保存速率
    qint64 elapsed = m_speedTimer.elapsed();
    if (elapsed > 200) { // 每200ms更新一次速率
        uint64_t bytesDelta = m_statistics.totalBytes - m_lastSavedBytes;
        double mbPerSecond = (bytesDelta * 1000.0) / (elapsed * 1024.0 * 1024.0);

        // 使用简单的移动平均进行平滑处理
        const double alpha = 0.3;
        m_statistics.saveRate = alpha * mbPerSecond + (1.0 - alpha) * m_statistics.saveRate;

        // 重置基准值
        m_lastSavedBytes = m_statistics.totalBytes;
        m_speedTimer.restart();

        // 发送进度更新信号
        emit saveProgressUpdated(m_statistics);
    }
}

bool FileSaveManager::saveMetadata() {
    if (!m_saveParams.saveMetadata) {
        return true;
    }

    // 创建元数据文件名
    QString metadataPath = m_statistics.savePath + "/metadata.json";

    // 创建元数据对象
    QJsonObject metadata;
    metadata["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["totalBytes"] = static_cast<double>(m_statistics.totalBytes);
    metadata["fileCount"] = static_cast<int>(m_statistics.fileCount);
    metadata["format"] = static_cast<int>(m_saveParams.format);

    // 添加其他选项
    QJsonObject options;
    for (auto it = m_saveParams.options.begin(); it != m_saveParams.options.end(); ++it) {
        options[it.key()] = QJsonValue::fromVariant(it.value());
    }
    metadata["options"] = options;

    // 写入文件
    QJsonDocument doc(metadata);
    QFile file(metadataPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法打开元数据文件: %1").arg(metadataPath));
        return false;
    }

    file.write(doc.toJson());
    file.close();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("元数据已保存: %1").arg(metadataPath));
    return true;
}

void FileSaveManager::saveThreadFunction() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存线程已启动"));

    // 设置线程优先级
    QThread::currentThread()->setPriority(QThread::LowPriority);

    while (m_running) {
        DataPacket packet;

        // 等待数据就绪或停止信号
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_dataReady.wait(lock, [this]() {
                return !m_running || (!m_paused && !m_dataQueue.empty());
                });

            // 检查线程是否应该退出
            if (!m_running) {
                break;
            }

            // 如果暂停，继续等待
            if (m_paused) {
                continue;
            }

            // 获取队首数据包
            if (!m_dataQueue.empty()) {
                packet = std::move(m_dataQueue.front());
                m_dataQueue.pop();
            }
            else {
                continue;
            }
        }

        // 处理数据包
        try {
            // 转换数据格式
            QByteArray formattedData;

            {
                std::lock_guard<std::mutex> lock(m_paramsMutex);
                auto it = m_converters.find(m_saveParams.format);
                if (it != m_converters.end()) {
                    formattedData = it->second->convert(packet, m_saveParams);
                }
                else {
                    // 如果没有找到转换器，使用原始数据
                    formattedData = QByteArray(reinterpret_cast<const char*>(packet.data.data()),
                        static_cast<int>(packet.size));
                }
            }

            // 创建新文件（如有必要）
            if (!m_fileWriter->isOpen()) {
                QString filename = createFileName(packet);
                QString fullPath = m_statistics.savePath + "/" + filename;

                if (!m_fileWriter->open(fullPath)) {
                    throw std::runtime_error(LocalQTCompat::fromLocal8Bit("无法打开文件: %1 - %2")
                        .arg(fullPath)
                        .arg(m_fileWriter->getLastError()).toStdString());
                }

                // 更新当前文件名
                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    m_statistics.currentFileName = filename;
                    m_statistics.fileCount++;
                }

                LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建新文件: %1").arg(fullPath));
            }

            // 写入数据
            if (!m_fileWriter->write(formattedData)) {
                throw std::runtime_error(LocalQTCompat::fromLocal8Bit("写入文件失败: %1")
                    .arg(m_fileWriter->getLastError()).toStdString());
            }

            // 更新统计信息
            updateStatistics(formattedData.size());

        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("保存数据异常: %1").arg(e.what()));

            // 更新状态为错误
            {
                std::lock_guard<std::mutex> lock(m_statsMutex);
                m_statistics.status = SaveStatus::FS_ERROR;
                m_statistics.lastError = LocalQTCompat::fromLocal8Bit("保存数据异常: %1").arg(e.what());
            }

            // 发送错误信号
            emit saveError(LocalQTCompat::fromLocal8Bit("保存数据异常: %1").arg(e.what()));

            // 关闭文件并重置
            m_fileWriter->close();

            // 短暂暂停以避免在错误条件下持续高负载
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    // 关闭当前文件
    m_fileWriter->close();

    // 保存元数据
    saveMetadata();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存线程已退出"));
}