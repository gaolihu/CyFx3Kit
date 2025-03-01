#include "UIStateHandler.h"
#include "Logger.h"
#include <QMessageBox>

UIStateHandler::UIStateHandler(Ui::FX3ToolMainWinClass& ui, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_isClosing(false)
    , m_lastTransferred(0)
    , m_lastSpeed(0.0)
{
    LOG_INFO(QString::fromLocal8Bit("UIStateHandler���캯�� - ��ʼ��"));

    // ������ȡ��ǰ״̬������UI
    AppState currentState = AppStateMachine::instance().currentState();
    LOG_INFO(QString::fromLocal8Bit("UIStateHandler���캯�� - ��ʼ��UI״̬Ϊ: %1")
        .arg(AppStateMachine::stateToString(currentState)));

    // ȷ��UI��ʼ״̬��ȷ
    updateButtonStates(currentState);
    updateStatusTexts(currentState);
}

void UIStateHandler::onStateChanged(AppState newState, AppState oldState, const QString& reason) {
    // ����Ƿ���Ը���UI
    if (!canUpdateUI()) {
        LOG_INFO(QString::fromLocal8Bit("UI������׼���رջ�Ӧ�������˳�������״̬����"));
        return;
    }

    LOG_INFO(QString::fromLocal8Bit("UI״̬�������յ�״̬�仯: %1 -> %2, ԭ��: %3")
        .arg(AppStateMachine::stateToString(oldState))
        .arg(AppStateMachine::stateToString(newState))
        .arg(reason));

    // ʹ��invokeMethodȷ�������߳��и���UI
    QMetaObject::invokeMethod(this, [this, newState, oldState, reason]() {
        if (!canUpdateUI()) return;

        // ���°�ť״̬
        updateButtonStates(newState);

        // ����״̬�ı�
        updateStatusTexts(newState, reason);
        }, Qt::QueuedConnection);
}

void UIStateHandler::updateButtonStates(AppState state) {
    // ����Ƿ���Ը���UI
    if (!canUpdateUI()) return;

    // ����״̬ȷ����ť����/����
    bool startEnabled = false;
    bool stopEnabled = false;
    bool resetEnabled = false;
    bool cmdDirEnabled = false;
    bool imageParamsEnabled = false;

    // ����״̬���ð�ť״̬
    switch (state) {
    case AppState::INITIALIZING:
    case AppState::STARTING:
    case AppState::STOPPING:
    case AppState::SHUTDOWN:
        // ���а�ť����
        break;

    case AppState::DEVICE_ABSENT:
        // ֻ������Ŀ¼��ť����
        cmdDirEnabled = true;
        break;

    case AppState::DEVICE_ERROR:
        // ���������豸��ѡ������Ŀ¼
        resetEnabled = true;
        cmdDirEnabled = true;
        break;

    case AppState::COMMANDS_MISSING:
        // ���������豸��ѡ������Ŀ¼
        resetEnabled = true;
        cmdDirEnabled = true;
        break;

    case AppState::CONFIGURED:
        // ���Կ�ʼ���䡢�����豸��ѡ������Ŀ¼
        startEnabled = true;
        resetEnabled = true;
        cmdDirEnabled = true;
        imageParamsEnabled = true;
        break;

    case AppState::TRANSFERRING:
        // ֻ��ֹͣ����
        stopEnabled = true;
        break;

    case AppState::IDLE:
        // ���������豸��ѡ������Ŀ¼
        resetEnabled = true;
        cmdDirEnabled = true;
        imageParamsEnabled = true;
        break;

    default:
        LOG_WARN(QString::fromLocal8Bit("updateButtonStates - δ�����״̬: %1")
            .arg(AppStateMachine::stateToString(state)));
        break;
    }

    // �̰߳�ȫ�ظ���UI
    QMetaObject::invokeMethod(QApplication::instance(), [=, this]() {
        if (!canUpdateUI()) return;

        // ֱ�Ӹ��°�ť״̬
        if (m_ui.startButton) m_ui.startButton->setEnabled(startEnabled);
        if (m_ui.stopButton) m_ui.stopButton->setEnabled(stopEnabled);
        if (m_ui.resetButton) m_ui.resetButton->setEnabled(resetEnabled);
        if (m_ui.cmdDirButton) m_ui.cmdDirButton->setEnabled(cmdDirEnabled);

        // ����ͼ������ؼ�״̬
        if (m_ui.imageWIdth) m_ui.imageWIdth->setReadOnly(!imageParamsEnabled);
        if (m_ui.imageHeight) m_ui.imageHeight->setReadOnly(!imageParamsEnabled);
        if (m_ui.imageType) m_ui.imageType->setEnabled(imageParamsEnabled);

        LOG_DEBUG(QString::fromLocal8Bit("��ť״̬�Ѹ��� - ��ʼ: %1, ֹͣ: %2, ����: %3, ����Ŀ¼: %4")
            .arg(startEnabled ? QString::fromLocal8Bit("����") : QString::fromLocal8Bit("����"))
            .arg(stopEnabled ? QString::fromLocal8Bit("����") : QString::fromLocal8Bit("����"))
            .arg(resetEnabled ? QString::fromLocal8Bit("����") : QString::fromLocal8Bit("����"))
            .arg(cmdDirEnabled ? QString::fromLocal8Bit("����") : QString::fromLocal8Bit("����")));
        }, Qt::QueuedConnection);
}

void UIStateHandler::updateStatusTexts(AppState state, const QString& additionalInfo) {
    QString statusText;
    QString transferStatusText;

    if (!canUpdateUI()) return;

    // ����״̬����״̬�ı�
    switch (state) {
    case AppState::INITIALIZING:
        statusText = QString::fromLocal8Bit("��ʼ����");
        transferStatusText = QString::fromLocal8Bit("��ʼ����");
        break;

    case AppState::DEVICE_ABSENT:
        statusText = QString::fromLocal8Bit("δ�����豸");
        transferStatusText = QString::fromLocal8Bit("δ����");
        // ���USB�ٶ���Ϣ
        m_ui.usbSpeedLabel->setText(QString::fromLocal8Bit("�豸: δ����"));
        m_ui.usbSpeedLabel->setStyleSheet("");
        break;

    case AppState::DEVICE_ERROR:
        statusText = QString::fromLocal8Bit("�豸����");
        transferStatusText = QString::fromLocal8Bit("����");
        m_ui.usbSpeedLabel->setStyleSheet("color: red;");
        break;

    case AppState::COMMANDS_MISSING:
        statusText = QString::fromLocal8Bit("�����ļ�δ����");
        transferStatusText = QString::fromLocal8Bit("����");
        // ��������״̬��ǩ
        m_ui.cmdStatusLabel->setText(QString::fromLocal8Bit("�����ļ�δ����"));
        m_ui.cmdStatusLabel->setStyleSheet("color: red;");
        break;

    case AppState::CONFIGURED:
        statusText = QString::fromLocal8Bit("����");
        transferStatusText = QString::fromLocal8Bit("������");
        // ��������״̬��ǩ
        m_ui.cmdStatusLabel->setText(QString::fromLocal8Bit("�����ļ����سɹ�"));
        m_ui.cmdStatusLabel->setStyleSheet("color: green;");
        break;

    case AppState::STARTING:
        statusText = QString::fromLocal8Bit("������");
        transferStatusText = QString::fromLocal8Bit("������");
        break;

    case AppState::TRANSFERRING:
        statusText = QString::fromLocal8Bit("������");
        transferStatusText = QString::fromLocal8Bit("������");
        break;

    case AppState::STOPPING:
        statusText = QString::fromLocal8Bit("ֹͣ��");
        transferStatusText = QString::fromLocal8Bit("ֹͣ��");
        break;

    case AppState::IDLE:
        statusText = QString::fromLocal8Bit("����");
        transferStatusText = QString::fromLocal8Bit("����");
        break;

    case AppState::SHUTDOWN:
        statusText = QString::fromLocal8Bit("�ر���");
        transferStatusText = QString::fromLocal8Bit("�ر���");
        break;

    default:
        statusText = QString::fromLocal8Bit("δ֪״̬");
        transferStatusText = QString::fromLocal8Bit("δ֪");
        break;
    }

    // ����״̬��ǩ
    m_ui.usbStatusLabel->setText(QString::fromLocal8Bit("USB״̬: %1").arg(statusText));
    m_ui.transferStatusLabel->setText(QString::fromLocal8Bit("����״̬: %1").arg(transferStatusText));
}

void UIStateHandler::updateTransferStats(uint64_t transferred, double speed, uint64_t elapsedTimeSeconds) {
    m_lastTransferred = transferred;
    m_lastSpeed = speed;

    if (!canUpdateUI()) return;

    // �����ٶ���ʾ
    QString speedText = QString::fromLocal8Bit("�ٶ�: 0 MB/s");
    if (speed > 0) {
        if (speed >= 1024) {
            speedText = QString::fromLocal8Bit("�ٶ�: %1 GB/s")
                .arg(speed / 1024.0, 0, 'f', 2);
        }
        else {
            speedText = QString::fromLocal8Bit("�ٶ�: %1 MB/s")
                .arg(speed, 0, 'f', 2);
        }
    }
    m_ui.speedLabel->setText(speedText);

    // �����ܴ�������ʾ
    m_ui.totalBytesLabel->setText(QString::fromLocal8Bit("�ܼ�: %1").arg(formatDataSize(transferred)));

    // Update elapsed time display
    QString timeText;
    if (elapsedTimeSeconds > 0) {
        timeText = QString::fromLocal8Bit("�ɼ�ʱ��: %1").arg(formatElapsedTime(elapsedTimeSeconds));
    }
    else {
        timeText = QString::fromLocal8Bit("�ɼ�ʱ��: 00:00:00");
    }
    m_ui.totalTimeLabel->setText(timeText);

    LOG_DEBUG(speedText);
}

void UIStateHandler::updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3) {
    m_ui.usbSpeedLabel->setText(QString::fromLocal8Bit("�豸: %1").arg(speedDesc));

    if (!canUpdateUI()) return;

    // ����USB�ٶ����ò�ͬ��ʽ
    if (isUSB3) {
        m_ui.usbSpeedLabel->setStyleSheet("color: blue;");
    }
    else if (!speedDesc.contains(QString::fromLocal8Bit("δ����"))) {
        m_ui.usbSpeedLabel->setStyleSheet("color: green;");
    }
    else {
        m_ui.usbSpeedLabel->setStyleSheet("");
    }

    LOG_INFO(QString::fromLocal8Bit("�����źţ�USB�ٶȸ���: %1").arg(speedDesc));
}

void UIStateHandler::showErrorMessage(const QString& title, const QString& message) {
    LOG_ERROR(QString::fromLocal8Bit("����Ի���: %1 - %2").arg(title).arg(message));
    if (!canUpdateUI()) return;

    QMessageBox::critical(nullptr, title, message);
}

QString UIStateHandler::formatDataSize(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        return QString("%1 GB").arg(gb, 0, 'f', 2);
    }
    else if (bytes >= 1024ULL * 1024ULL) {
        double mb = bytes / (1024.0 * 1024.0);
        return QString("%1 MB").arg(mb, 0, 'f', 2);
    }
    else if (bytes >= 1024ULL) {
        double kb = bytes / 1024.0;
        return QString("%1 KB").arg(kb, 0, 'f', 2);
    }
    else {
        return QString("%1 B").arg(bytes);
    }
}