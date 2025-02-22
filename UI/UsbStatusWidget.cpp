#include "UsbStatusWidget.h"
#include <QStyle>

UsbStatusWidget::UsbStatusWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void UsbStatusWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);

    // 状态显示行
    auto statusLayout = new QHBoxLayout();
    m_statusIcon = new QLabel(this);
    m_statusText = new QLabel(this);

    m_statusIcon->setFixedSize(24, 24);
    statusLayout->addWidget(m_statusIcon);
    statusLayout->addWidget(m_statusText);
    statusLayout->addStretch();

    // 传输信息显示
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

    // 设置初始状态
    updateStatus("disconnected");
}

void UsbStatusWidget::updateStatus(const QString& status) {
    QString iconPath;
    QString statusText;
    QString styleSheet;

    if (status == "ready") {
        iconPath = ":/icons/ready.png";
        statusText = tr("设备就绪");
        styleSheet = "color: #10B981;"; // 绿色
    }
    else if (status == "transferring") {
        iconPath = ":/icons/transferring.png";
        statusText = tr("数据传输中");
        styleSheet = "color: #3B82F6;"; // 蓝色
    }
    else if (status == "error") {
        iconPath = ":/icons/error.png";
        statusText = tr("设备错误");
        styleSheet = "color: #EF4444;"; // 红色
    }
    else {
        iconPath = ":/icons/disconnected.png";
        statusText = tr("设备未连接");
        styleSheet = "color: #6B7280;"; // 灰色
    }

    m_statusIcon->setPixmap(QPixmap(iconPath).scaled(24, 24));
    m_statusText->setText(statusText);
    m_statusText->setStyleSheet(styleSheet);
}

void UsbStatusWidget::updateTransferStats(qint64 speed, qint64 totalBytes) {
    m_speedLabel->setText(tr("传输速度: %1").arg(formatSpeed(speed)));
    m_totalBytesLabel->setText(tr("已传输: %1").arg(formatBytes(totalBytes)));

    // 更新进度条
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
