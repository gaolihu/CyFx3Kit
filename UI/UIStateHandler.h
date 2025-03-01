#pragma once

#include <QObject>
#include "AppStateMachine.h"
#include "ui_FX3ToolMainWin.h"
#include "Logger.h"

// UI״̬������ - �������Ӧ��״̬����UI
class UIStateHandler : public QObject {
    Q_OBJECT

public:
    explicit UIStateHandler(Ui::FX3ToolMainWinClass& ui, QObject* parent = nullptr);
    ~UIStateHandler() = default;

    // ׼���رգ�ֹͣ����UI����
    void prepareForClose() {
        m_isClosing = true;
        LOG_INFO(QString::fromLocal8Bit("UI״̬��������׼���ر�"));
    }

    // ����Ƿ���԰�ȫ����UI
    bool canUpdateUI() const {
        // ���Ӧ�ó����Ƿ����ڹر�
        if (QApplication::closingDown()) {
            return false;
        }

        // ���UI�������Ƿ����ڹر�
        if (m_isClosing.load()) {
            return false;
        }

        // ȷ�������߳��е���
        if (QThread::currentThread() != QApplication::instance()->thread()) {
            // ����������̣߳�ֻ����ĳЩ����
            return false;
        }

        return true;
    }

public slots:
    // ��Ӧ��״̬�ı�ʱ����UI
    void onStateChanged(AppState newState, AppState oldState, const QString& reason);

    // ���´����ٶȺ�����ͳ��
    void updateTransferStats(uint64_t transferred, double speed, uint64_t elapsedTimeSeconds = 0);

    // ����USB�豸�ٶ���ʾ
    void updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3);

    // ��ʾ������Ϣ
    void showErrorMessage(const QString& title, const QString& message);

private:
    // ����Ӧ��״̬���°�ť״̬
    void updateButtonStates(AppState state);

    // ����״̬���ı�
    void updateStatusTexts(AppState state, const QString& additionalInfo = QString());

    // ��ʽ�����ݴ�С��ʾ
    QString formatDataSize(uint64_t bytes);

    QString formatElapsedTime(uint64_t seconds) {
        uint64_t hours = seconds / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        uint64_t secs = seconds % 60;

        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }

    // UI����
    Ui::FX3ToolMainWinClass& m_ui;

    // �ϴθ��µĴ���״̬
    uint64_t m_lastTransferred = 0;
    double m_lastSpeed = 0.0;
    std::atomic<bool> m_isClosing{ false };
};