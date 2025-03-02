#pragma once
#include <QString>
#include <QDateTime>
#include "Logger.h"  // 包含Logger以使用其枚举类型

struct LogEntry {
    QString message;
    QString threadId;
    Logger::LogLevel level;
    QString file;
    int line;
    QDateTime timestamp;
};