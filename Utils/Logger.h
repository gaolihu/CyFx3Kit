#pragma once
#include <QString>
#include <QFile>
#include <QDateTime>
#include <QTextEdit>
#include <QMutex>
#include <QDebug>
#include <QThread>
#include <QQueue>
#include <QTimer>
#include <QWaitCondition>
#include <QScopedPointer>
#include <QTextStream>
#include <QColor>
#include <atomic>
#include <memory>
#include <sstream>

class LogWriter;
class UIUpdater;

class Logger : public QObject {
    Q_OBJECT

public:
    // 将枚举保留在Logger类中，以保持原有代码的兼容性
    enum LogLevel {
        LDEBUG,
        LINFO,
        LWARNING,
        LERROR
    };

    enum LogCategory {
        CAT_NONE,
        CAT_USB,
        CAT_UI,
        CAT_SYSTEM
    };

    static Logger& instance();
    ~Logger();

    bool isInitialized() const { return m_initialized; }
    void setCategory(LogCategory category) { m_currentCategory = category; }

    static QString getThreadId();
    QString getThreadIdAsString(const std::thread& thread);
    static QString formatErrorCode(unsigned long error);

    void setLogFile(const QString& logFile, bool clearOldLog = true);
    void setLogWidget(QTextEdit* logWidget);
    void setLogLevel(LogLevel level) { m_currentLevel = level; }
    bool shouldLog(LogLevel level) const { return level >= m_currentLevel; }

    void log(const QString& message, LogLevel level = LINFO, const QString& file = "", int line = 0);
    void error(const QString& error, const QString& file = "", int line = 0);
    void warning(const QString& warning, const QString& file = "", int line = 0);
    void debug(const QString& message, const QString& file = "", int line = 0);

    // 保留原有的工具方法
    void writeToFile(const QString& message);
    void appendToWidget(const QString& message, LogLevel level);
    QString formatMessage(const QString& message, const QString& threadId, LogLevel level,
        const QString& file = "", int line = 0);
    QColor getLevelColor(LogLevel level) const;
    QString getLevelString(LogLevel level) const;

private:
    friend class LogWriter;
    friend class UIUpdater;

    Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QString getCategoryString() const;
    void clearLogFile();
    void backupOldLog();

    QFile m_logFile;
    QTextEdit* m_logWidget;
    QMutex m_mutex;
    LogCategory m_currentCategory;
    LogLevel m_currentLevel { LDEBUG };
    std::atomic<bool> m_initialized;
    std::unique_ptr<LogWriter> m_logWriter;
    std::unique_ptr<UIUpdater> m_uiUpdater;
};

// 保持原有的宏定义
#define LOG_DEBUG(msg) Logger::instance().debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg)  Logger::instance().log(msg, Logger::LINFO, __FILE__, __LINE__)
#define LOG_WARN(msg)  Logger::instance().warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::instance().error(msg, __FILE__, __LINE__)