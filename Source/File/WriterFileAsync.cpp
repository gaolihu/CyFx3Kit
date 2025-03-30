// Source/File/WriterFileAsync.cpp

#include "FileSaveManager.h"
#include "Logger.h"

WriterFileAsync::WriterFileAsync()
    : m_isOpen(false)
    , m_running(false)
{
}

WriterFileAsync::~WriterFileAsync() {
    close();
}

bool WriterFileAsync::open(const QString& filename) {
    close(); // 确保之前的文件已关闭

    // 验证文件名并规范化路径
    if (filename.isEmpty()) {
        m_lastError = LocalQTCompat::fromLocal8Bit("文件名为空");
        LOG_ERROR(m_lastError);
        return false;
    }

    QString normalizedPath = QDir::cleanPath(filename);

    // 确保目录存在
    QFileInfo fileInfo(normalizedPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件目录不存在，尝试创建: %1").arg(dir.path()));
        if (!dir.mkpath(".")) {
            m_lastError = LocalQTCompat::fromLocal8Bit("无法创建文件目录: %1").arg(dir.path());
            LOG_ERROR(m_lastError);
            return false;
        }

        // 验证目录是否真的创建成功
        if (!dir.exists()) {
            m_lastError = LocalQTCompat::fromLocal8Bit("目录创建失败: %1").arg(dir.path());
            LOG_ERROR(m_lastError);
            return false;
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建文件目录: %1").arg(dir.path()));
    }

    m_file.setFileName(normalizedPath);
    m_isOpen = m_file.open(QIODevice::WriteOnly);

    if (!m_isOpen) {
        m_lastError = m_file.errorString();
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("打开文件失败: %1 - %2").arg(normalizedPath).arg(m_lastError));
        return false;
    }

    // 启动写入线程
    m_running = true;
    try {
        m_writerThread = std::thread(&WriterFileAsync::writerThreadFunc, this);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("异步写入线程已启动，文件已打开: %1").arg(normalizedPath));
    }
    catch (const std::exception& e) {
        m_lastError = LocalQTCompat::fromLocal8Bit("创建写入线程失败: %1").arg(e.what());
        LOG_ERROR(m_lastError);
        m_file.close();
        m_isOpen = false;
        m_running = false;
        return false;
    }

    return true;
}

bool WriterFileAsync::write(const QByteArray& data) {
    if (!m_isOpen) {
        m_lastError = LocalQTCompat::fromLocal8Bit("文件未打开");
        return false;
    }

    // 如果队列已满，等待清空一些空间
    std::unique_lock<std::mutex> lock(m_queueMutex);
    bool queueWasFull = false;

    if (m_writeQueue.size() >= MAX_QUEUE_SIZE) {
        queueWasFull = true;
        LOG_WARN(LocalQTCompat::fromLocal8Bit("写入队列已满 (%1 个项目), 等待空间...").arg(MAX_QUEUE_SIZE));

        // 添加条件等待，避免忙等
        m_queueCondition.wait(lock, [this]() {
            return m_writeQueue.size() < MAX_QUEUE_SIZE * 0.8 || !m_running;
            });

        if (!m_running) {
            return false;
        }
    }

    // 将数据添加到写入队列
    m_writeQueue.push(data);
    lock.unlock();

    // 通知写入线程新数据到达
    m_queueCondition.notify_one();

    if (queueWasFull) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("写入队列恢复可用"));
    }

    return true;
}

bool WriterFileAsync::close() {
    if (m_running) {
        // 设置停止标志并通知线程
        m_running = false;
        m_queueCondition.notify_all();

        // 等待线程完成所有待处理写入后退出
        if (m_writerThread.joinable()) {
            m_writerThread.join();
        }
    }

    if (m_isOpen) {
        m_file.flush();
        m_file.close();
        m_isOpen = false;
    }

    return true;
}

QString WriterFileAsync::getLastError() const {
    return m_lastError;
}

bool WriterFileAsync::isOpen() const {
    return m_isOpen;
}

void WriterFileAsync::writerThreadFunc() {
    LOG_INFO(LocalQTCompat::fromLocal8Bit("异步写入线程已启动"));

    while (m_running) {
        QByteArray data;
        bool hasData = false;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);

            // 等待数据到达或者线程停止信号
            m_queueCondition.wait(lock, [this]() {
                return !m_running || !m_writeQueue.empty();
                });

            // 如果线程被要求停止且队列已空，则退出
            if (!m_running && m_writeQueue.empty()) {
                break;
            }

            // 取出队列中的数据
            if (!m_writeQueue.empty()) {
                data = std::move(m_writeQueue.front());
                m_writeQueue.pop();
                hasData = true;
            }
        }

        // 写入文件
        if (hasData) {
            qint64 written = m_file.write(data);
            if (written != data.size()) {
                m_lastError = m_file.errorString();
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("异步文件写入错误: %1").arg(m_lastError));
            }

            // 每次写入后刷新以确保数据写入磁盘
            m_file.flush();
        }
    }

    // 处理剩余的写入队列
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        while (!m_writeQueue.empty()) {
            QByteArray data = std::move(m_writeQueue.front());
            m_writeQueue.pop();

            m_file.write(data);
        }
        m_file.flush();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("异步写入线程已退出"));
}