#include "LogWriter.h"
#include "Logger.h"
#include <QTextStream>

LogWriter::LogWriter(Logger* logger)
    : m_logger(logger)
    , m_running(true)
{
    start();
}

LogWriter::~LogWriter()
{
    qDebug() << "LogWriter销毁";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop();
    wait();
}

void LogWriter::stop()
{
    m_running = false;
    m_condition.wakeOne();
}

void LogWriter::enqueue(const LogEntry& entry)
{
    QMutexLocker locker(&m_queueMutex);
    m_queue.enqueue(entry);
    m_condition.wakeOne();
}

void LogWriter::run()
{
    while (m_running) {
        QQueue<LogEntry> batch;
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_queue.isEmpty()) {
                m_condition.wait(&m_queueMutex, 100);
                if (!m_running) break;
                continue;
            }
            while (!m_queue.isEmpty() && batch.size() < 100) {
                batch.enqueue(m_queue.dequeue());
            }
        }

        if (!batch.isEmpty() && m_logger->m_logFile.isOpen()) {
            QTextStream stream(&m_logger->m_logFile);
            for (const auto& entry : batch) {
                stream << m_logger->formatMessage(entry.message,
                    entry.threadId, entry.level,
                    entry.file, entry.line) << "\n";
            }
            stream.flush();
        }
    }
    qDebug() << "LogWriter线程退出";
}
