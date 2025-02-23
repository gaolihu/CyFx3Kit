#include <QMetaType>
#include <QtWidgets/QApplication>

#include "FX3TestTool.h"

void registerMetaTypes() {
    qRegisterMetaType<uint64_t>("uint64_t");
    qRegisterMetaType<std::string>("std::string");
}

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    // ע���Զ�������
    registerMetaTypes();

    // ����Ӧ�ó�����Ϣ
    QApplication::setApplicationName("FX3TestTool");
    QApplication::setApplicationVersion("1.0.0");

    FX3TestTool w;
    w.show();

    return a.exec();
}
