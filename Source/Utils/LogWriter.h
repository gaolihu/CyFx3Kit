#pragma once
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include "LoggerTypes.h"

class Logger;
class LogWriter : public QThread {
    Q_OBJECT
public:
    explicit LogWriter(Logger* logger);
    ~LogWriter();
    void enqueue(const LogEntry& entry);
    void stop();

protected:
    void run() override;

private:
    Logger* m_logger;
    QQueue<LogEntry> m_queue;
    QMutex m_queueMutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_running;
};
