#include <QDateTime>
#include <QFileInfo>
#include <QApplication>

#include "Logger.h"
#include "LoggerTypes.h" 
#include "LogWriter.h"
#include "UIUpdater.h"

Logger::Logger()
    : QObject(nullptr)
    , m_logWidget(nullptr)
    , m_initialized(false)
    , m_currentCategory(CAT_NONE)
    , m_logWriter(std::make_unique<LogWriter>(this))
    , m_uiUpdater(std::make_unique<UIUpdater>(this))
{
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

Logger& Logger::instance()
{
    static Logger instance;
    return instance;
}

QString Logger::getThreadId()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return QString::fromStdString(ss.str());
}
QString Logger::getThreadIdAsString(const std::thread& thread)
{
    std::stringstream ss;
    ss << thread.get_id();
    return QString::fromStdString(ss.str());
}

QString Logger::formatErrorCode(unsigned long error)
{
    return QString("0x%1").arg(error, 8, 16, QChar('0'));
}

void Logger::setLogFile(const QString& logFile, bool clearOldLog)
{
    QMutexLocker locker(&m_mutex);

    qDebug() << "Set log file:" << logFile << "" << (clearOldLog ? ", clear old" : "append old");

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    m_logFile.setFileName(logFile);

    if (clearOldLog) {
        QFileInfo fileInfo(logFile);
        if (fileInfo.exists() && fileInfo.size() > 1024 * 1024) {
            backupOldLog();
        }
        clearLogFile();
    }

    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Failed to open log file:" << logFile;
        return;
    }

    QTextStream stream(&m_logFile);
    stream << "=== Log started at " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " ===" << "\n";
    stream.flush();

    m_initialized = true;
}

void Logger::setLogWidget(QTextEdit* logWidget)
{
    QMutexLocker locker(&m_mutex);
    m_logWidget = logWidget;
    if (m_logWidget) {
        m_logWidget->setReadOnly(true);
        m_logWidget->document()->setMaximumBlockCount(5000);
    }
}

void Logger::log(const QString& message, LogLevel level, const QString& file, int line)
{
    if (!m_initialized || !shouldLog(level)) return;

    LogEntry entry{
        message,
        getThreadId(),
        level,
        file,
        line,
        QDateTime::currentDateTime()
    };

    m_logWriter->enqueue(entry);
    m_uiUpdater->enqueue(entry);
}

void Logger::error(const QString& error, const QString& file, int line)
{
    log(error, LERROR, file, line);
}

void Logger::warning(const QString& warning, const QString& file, int line)
{
    log(warning, LWARNING, file, line);
}

void Logger::debug(const QString& message, const QString& file, int line)
{
    log(message, LDEBUG, file, line);
}

void Logger::writeToFile(const QString& message)
{
    if (!m_logFile.isOpen()) {
        if (!m_logFile.fileName().isEmpty()) {
            m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        }
    }

    if (m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << message << "\n";
        stream.flush();
    }
}

void Logger::appendToWidget(const QString& message, LogLevel level)
{
    if (!m_logWidget) return;

    // 确保在主线程中更新UI
    if (QThread::currentThread() != QApplication::instance()->thread()) {
        QMetaObject::invokeMethod(this, [this, message, level]() {
            appendToWidget(message, level);
            }, Qt::QueuedConnection);
        return;
    }

    QTextCharFormat format;
    format.setForeground(getLevelColor(level));

    QTextCursor cursor = m_logWidget->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n");
    cursor.setCharFormat(format);
    cursor.insertText(message);

    m_logWidget->setTextCursor(cursor);
    m_logWidget->ensureCursorVisible();
}

QString Logger::formatMessage(const QString& message,
    const QString& threadId, LogLevel level,
    const QString& file, int line)
{
    QString category = getCategoryString();
    QString location;

    if (!file.isEmpty()) {
        QString fileName = QFileInfo(file).fileName();
        location = QString(" [%1:%2]").arg(fileName).arg(line);
    }

    QString categoryStr = category.isEmpty() ? "" : QString("[%1]").arg(category);

    return QString("[%1][%2][Tid:%3]%4%5 %6")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(getLevelString(level))
        .arg(threadId)  // 使用传入的线程ID
        .arg(categoryStr)
        .arg(location)
        .arg(message);
}

QColor Logger::getLevelColor(LogLevel level) const
{
    switch (level) {
    case LDEBUG:   return QColor("#0000AA");
    case LINFO:    return QColor("#000000");
    case LWARNING: return QColor("#FFA500");
    case LERROR:   return QColor("#FF0000");
    default:       return QColor("#AAAAAA");
    }
}

QString Logger::getLevelString(LogLevel level) const
{
    switch (level) {
    case LDEBUG:   return "DEBUG";
    case LINFO:    return "INFO";
    case LWARNING: return "WARN";
    case LERROR:   return "ERROR";
    default:       return "UNKNOWN";
    }
}

QString Logger::getCategoryString() const
{
    switch (m_currentCategory) {
    case CAT_USB:    return "USB";
    case CAT_UI:     return "UI";
    case CAT_SYSTEM: return "SYS";
    default:         return "";
    }
}

void Logger::clearLogFile()
{
    QFile file(m_logFile.fileName());
    if (file.exists()) {
        file.remove();
    }
}

void Logger::backupOldLog()
{
    QString oldLogPath = m_logFile.fileName();
    QString backupPath = QString("%1.%2.bak")
        .arg(oldLogPath)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    qDebug() << "Backup old: " << oldLogPath << ", to: " << backupPath;
    QFile::rename(oldLogPath, backupPath);
}
