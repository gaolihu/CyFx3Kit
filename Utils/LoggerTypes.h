#pragma once
#include <QString>
#include <QDateTime>
#include "Logger.h"  // ����Logger��ʹ����ö������

struct LogEntry {
    QString message;
    QString threadId;
    Logger::LogLevel level;
    QString file;
    int line;
    QDateTime timestamp;
};