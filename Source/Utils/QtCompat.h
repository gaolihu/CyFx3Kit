#pragma once

#include <QtGlobal>
#include <QString>

namespace LocalQTCompat {

    inline QString fromLocal8Bit(const char* str)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        return QString::fromLocal8Bit(str);
#else
        return QString::fromUtf8(str);
#endif
    }

    // 可以添加更多兼容性函数
    inline void enableHighDpiScaling()
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    }

} // namespace
