#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>

class UsbStatusWidget : public QWidget {
    Q_OBJECT

public:
    explicit UsbStatusWidget(QWidget* parent = nullptr);

public slots:
    void updateStatus(const QString& status);
    void updateTransferStats(qint64 speed, qint64 totalBytes);

private:
    QLabel* m_statusIcon;
    QLabel* m_statusText;
    QLabel* m_speedLabel;
    QLabel* m_totalBytesLabel;
    QProgressBar* m_speedProgress;

    void setupUI();
    void updateStatusStyle(const QString& status);
    QString formatBytes(qint64 bytes);
    QString formatSpeed(qint64 bytesPerSecond);
};