#include "FX3ToolMainWin.h"
#include <QtWidgets/QApplication>
#include <QTextCodec>

int main(int argc, char* argv[])
{
    // ���ø�DPI֧��
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);

    // ����Ӧ�ó�����Ϣ
    QCoreApplication::setOrganizationName("Your Company");
    QCoreApplication::setApplicationName("FX3 Test Tool");

    // ����UTF-8����
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("System"));

    // ��������ʾ������
    FX3ToolMainWin w;
    w.show();

    return a.exec();
}