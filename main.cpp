#include "FX3TestTool.h"
#include "Logger.h"

#include <QtWidgets/QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    // ����Ӧ�ó�����Ϣ
    QApplication::setApplicationName("FX3TestTool");
    QApplication::setApplicationVersion("1.0.0");

    FX3TestTool w;

    // ����ʾ����֮ǰ�����־��ʼ��
    w.initLogger();
    w.show();

    return a.exec();
}
