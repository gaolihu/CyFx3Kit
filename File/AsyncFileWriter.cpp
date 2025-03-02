#include "FileSaveManager.h"
#include "Logger.h"

AsyncFileWriter::AsyncFileWriter()
    : m_isOpen(false)
    , m_running(false)
{
}

AsyncFileWriter::~AsyncFileWriter() {
    close();
}

bool AsyncFileWriter::open(const QString& filename) {
    close(); // 确保之前的文件已关闭

    m_file.setFileName(filename);
    m_isOpen = m_file.open(QIODevice::WriteOnly);

    if (!m_isOpen) {
        m_lastError = m_file.errorString();
        return false;
    }

    // 启动写入线程
    m_running = true;
    m_writerThread = std::thread(&AsyncFileWriter::writerThreadFunc, this);

    return true;
}

bool AsyncFileWriter::write(const QByteArray& data) {
    if (!m_isOpen) {
        m_lastError = fromLocal8Bit("文件未打开");
        return false;
    }

    // 如果队列已满，等待清空一些空间
    std::unique_lock<std::mutex> lock(m_queueMutex);
    bool queueWasFull = false;

    if (m_writeQueue.size() >= MAX_QUEUE_SIZE) {
        queueWasFull = true;
        LOG_WARN(fromLocal8Bit("写入队列已满 (%1 个项目), 等待空间...").arg(MAX_QUEUE_SIZE));
    }

    // 将数据添加到写入队列
    m_writeQueue.push(data);
    lock.unlock();

    // 通知写入线程新数据到达
    m_queueCondition.notify_one();

    if (queueWasFull) {
        LOG_INFO(fromLocal8Bit("写入队列恢复可用"));
    }

    return true;
}

bool AsyncFileWriter::close() {
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

QString AsyncFileWriter::getLastError() const {
    return m_lastError;
}

bool AsyncFileWriter::isOpen() const {
    return m_isOpen;
}

void AsyncFileWriter::writerThreadFunc() {
    LOG_INFO(fromLocal8Bit("异步写入线程已启动"));

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
                LOG_ERROR(fromLocal8Bit("异步文件写入错误: %1").arg(m_lastError));
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

    LOG_INFO(fromLocal8Bit("异步写入线程已退出"));
}