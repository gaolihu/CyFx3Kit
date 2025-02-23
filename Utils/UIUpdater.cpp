#include "UIUpdater.h"
#include "Logger.h"
#include <QTextCursor>
#include <QTextCharFormat>

UIUpdater::UIUpdater(Logger* logger)
    : QObject(nullptr)
    , m_logger(logger)
    , m_batchTimer(new QTimer(this))
{
    m_batchTimer->setInterval(100); // 100ms 批量更新
    connect(m_batchTimer, &QTimer::timeout, this, &UIUpdater::processLogBatch);
    m_batchTimer->start();
}

void UIUpdater::enqueue(const LogEntry& entry)
{
    QMutexLocker locker(&m_queueMutex);
    m_queue.enqueue(entry);
}

void UIUpdater::processLogBatch()
{
    if (!m_logger->m_logWidget) return;

    QQueue<LogEntry> batch;
    {
        QMutexLocker locker(&m_queueMutex);
        std::swap(batch, m_queue);
    }

    if (batch.isEmpty()) return;

    QTextCursor cursor = m_logger->m_logWidget->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();

    for (const auto& entry : batch) {
        QTextCharFormat format;
        format.setForeground(m_logger->getLevelColor(entry.level));

        cursor.insertText("\n");
        cursor.setCharFormat(format);
        cursor.insertText(m_logger->formatMessage(entry.message,
            entry.threadId, entry.level,
            entry.file, entry.line));
    }

    cursor.endEditBlock();
    m_logger->m_logWidget->setTextCursor(cursor);
    m_logger->m_logWidget->ensureCursorVisible();
}
