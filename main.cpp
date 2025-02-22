#include "FX3TestTool.h"
#include "Logger.h"

#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    // 设置应用程序信息
    QApplication::setApplicationName("FX3TestTool");
    QApplication::setApplicationVersion("1.0.0");

    FX3TestTool w;

    // 在显示窗口之前完成日志初始化
    w.initLogger();
    w.show();

    return a.exec();
}
