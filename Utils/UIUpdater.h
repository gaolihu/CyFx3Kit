#pragma once
#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QTimer>
#include "LoggerTypes.h"

class Logger;
class UIUpdater : public QObject {
    Q_OBJECT
public:
    explicit UIUpdater(Logger* logger);
    void enqueue(const LogEntry& entry);

public slots:
    void processLogBatch();

private:
    Logger* m_logger;
    QQueue<LogEntry> m_queue;
    QMutex m_queueMutex;
    QTimer* m_batchTimer;
};