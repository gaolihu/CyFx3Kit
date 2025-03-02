#include "FX3ToolMainWin.h"
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

#if QT_VERSION <= QT_VERSION_CHECK(6, 0, 0)
    // 设置UTF-8编码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("System"));
#endif

    // 创建并显示主窗口
    FX3ToolMainWin w;
    w.show();

    return a.exec();
}