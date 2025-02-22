#pragma once

#include <QtWidgets/QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "USBDevice.h"
#include "DataAcquisition.h"
#include "ui_FX3TestTool.h"

class FX3TestTool : public QMainWindow
{
    Q_OBJECT

public:
    FX3TestTool(QWidget* parent = nullptr);
    ~FX3TestTool();

    void initLogger();

protected:
    // Add native event handler for USB device detection
    virtual bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private slots:
    void onDeviceInitialize();
    void onStartTransfer();
    void onStopTransfer();
    void onResetDevice();
    void onUsbStatusChanged(const std::string& status);
    void onTransferProgress(uint64_t transferred, int length, int success, int failed);
    void onDeviceError(const QString& error);
    void handleAcquiredData(const DataPacket& packet);
    void updateStats(uint64_t receivedBytes, double dataRate);

private:
    bool checkAndOpenDevice();
    void initConnections();
    void handleDeviceArrival();
    void handleDeviceRemoval();
    void updateUIState(bool deviceReady);

private:
    // UI form class instance
    Ui::FX3TestToolClass ui;

    // USB device
    std::shared_ptr<USBDevice> m_usbDevice;
    std::unique_ptr<DataAcquisitionManager> m_acquisitionManager;

    void initStatusBar();
    void updateStatusBar(const QString& usbStatus = QString());
    void updateTransferStatus(uint64_t transferred, double speed = 0.0);

    bool m_deviceInitializing;
    QTimer m_debounceTimer;
    static const int DEBOUNCE_DELAY = 1000; // 1秒防抖时间
};