#include "FX3ToolMainWin.h"
#include <QtWidgets/QApplication>
#include <QTextCodec>

int main(int argc, char* argv[])
{
    // 设置高DPI支持
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);

    // 设置应用程序信息
    QCoreApplication::setOrganizationName("Your Company");
    QCoreApplication::setApplicationName("FX3 Test Tool");

    // 设置UTF-8编码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("System"));

    // 创建并显示主窗口
    FX3ToolMainWin w;
    w.show();

    return a.exec();
}