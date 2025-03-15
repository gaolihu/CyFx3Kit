#include "Logger.h"
#include "FX3MainView.h"
#include <QtWidgets/QApplication>
#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#endif

int main(int argc, char* argv[])
{
    // 设置高DPI支持
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);

    // 设置应用程序信息
    QCoreApplication::setOrganizationName("Your Company");
    QCoreApplication::setApplicationName("FX3 Test Tool");

    QString logPath = QApplication::applicationDirPath() + "/fx3_t.log";
    Logger::instance().setLogFile(logPath);

    // 初始化 Logger 的文件路径
    LOG_INFO(LocalQTCompat::fromLocal8Bit("日志: %1").arg(logPath));

#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    // 设置UTF-8编码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("System"));
#endif

    // 创建并显示主窗口
    FX3MainView w;
    w.show();
    w.updateWindowTitle("3.15");

    //return a.exec();
    int result = a.exec();
    qDebug() << "应用程序退出，退出码：" << result;
    return result;
}