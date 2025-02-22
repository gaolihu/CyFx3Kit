#include "UsbStatusWidget.h"
#include <QStyle>

UsbStatusWidget::UsbStatusWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void UsbStatusWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);

    // ״̬��ʾ��
    auto statusLayout = new QHBoxLayout();
    m_statusIcon = new QLabel(this);
    m_statusText = new QLabel(this);

    m_statusIcon->setFixedSize(24, 24);
    statusLayout->addWidget(m_statusIcon);
    statusLayout->addWidget(m_statusText);
    statusLayout->addStretch();

    // ������Ϣ��ʾ
    m_speedLabel = new QLabel(this);
    m_totalBytesLabel = new QLabel(this);
    m_speedProgress = new QProgressBar(this);
    m_speedProgress->setTextVisible(false);
    m_speedProgress->setRange(0, 100);

    mainLayout->addLayout(statusLayout);
    mainLayout->addWidget(m_speedLabel);
    mainLayout->addWidget(m_totalBytesLabel);
    mainLayout->addWidget(m_speedProgress);

    setLayout(mainLayout);

    // ���ó�ʼ״̬
    updateStatus("disconnected");
}

void UsbStatusWidget::updateStatus(const QString& status) {
    QString iconPath;
    QString statusText;
    QString styleSheet;

    if (status == "ready") {
        iconPath = ":/icons/ready.png";
        statusText = tr("�豸����");
        styleSheet = "color: #10B981;"; // ��ɫ
    }
    else if (status == "transferring") {
        iconPath = ":/icons/transferring.png";
        statusText = tr("���ݴ�����");
        styleSheet = "color: #3B82F6;"; // ��ɫ
    }
    else if (status == "error") {
        iconPath = ":/icons/error.png";
        statusText = tr("�豸����");
        styleSheet = "color: #EF4444;"; // ��ɫ
    }
    else {
        iconPath = ":/icons/disconnected.png";
        statusText = tr("�豸δ����");
        styleSheet = "color: #6B7280;"; // ��ɫ
    }

    m_statusIcon->setPixmap(QPixmap(iconPath).scaled(24, 24));
    m_statusText->setText(statusText);
    m_statusText->setStyleSheet(styleSheet);
}

void UsbStatusWidget::updateTransferStats(qint64 speed, qint64 totalBytes) {
    m_speedLabel->setText(tr("�����ٶ�: %1").arg(formatSpeed(speed)));
    m_totalBytesLabel->setText(tr("�Ѵ���: %1").arg(formatBytes(totalBytes)));

    // ���½�����
    int progressValue = qBound(0, static_cast<int>((speed * 100) / (10 * 1024 * 1024)), 100);
    m_speedProgress->setValue(progressValue);
}

void UsbStatusWidget::updateStatusStyle(const QString& status)
{
}

QString UsbStatusWidget::formatBytes(qint64 bytes)
{
    return {};
}

QString UsbStatusWidget::formatSpeed(qint64 bytesPerSecond)
{
    return {};
}
