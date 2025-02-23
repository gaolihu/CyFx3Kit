#pragma once

#include <QtWidgets/QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include "USBDevice.h"
#include "DataAcquisition.h"
#include "ui_FX3TestTool.h"

class StopWorker : public QObject
{
    Q_OBJECT
public:
    StopWorker(std::shared_ptr<DataAcquisitionManager> manager,
        std::shared_ptr<USBDevice> device)
        : m_manager(manager)
        , m_device(device)
    {}

public slots:
    void doStop() {
        try {
            if (m_manager) {
                m_manager->stopAcquisition();
            }
            if (m_device) {
                m_device->stopTransfer();
            }
            emit stopCompleted();
        }
        catch (const std::exception& e) {
            emit stopError(QString("Stop failed: %1").arg(e.what()));
        }
    }

    void forceStop() {
        // 强制停止逻辑
        if (m_manager) {
            m_manager->stopAcquisition();
        }
        if (m_device) {
            m_device->stopTransfer();
        }
        emit stopCompleted();
    }

signals:
    void stopCompleted();
    void stopError(const QString& error);

private:
    std::shared_ptr<DataAcquisitionManager> m_manager;
    std::shared_ptr<USBDevice> m_device;
};

class FX3TestTool : public QMainWindow
{
    Q_OBJECT

public:
    FX3TestTool(QWidget* parent = nullptr);
    ~FX3TestTool();

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
    void onSelectCommandDirectory();
    void onStopComplete();

    bool loadCommandFiles(const QString& dir);
    void updateCommandStatus(bool valid);

    void setupUI();
    void resizeEvent(QResizeEvent* event);
    void adjustStatusBar();

    void handleAcquiredData(const DataPacket& packet);
    void updateStats(uint64_t receivedBytes, double dataRate);

private:
    bool initializeLogger();

    bool checkAndOpenDevice();
    void initConnections();
    void handleDeviceArrival();
    void handleDeviceRemoval();
    void updateUIState(bool deviceReady);

    void initStatusBar();
    void updateStatusBar(const QString& usbStatus = QString());
    void updateTransferStatus(uint64_t transferred, double speed = 0.0);
    void initializeDeviceAndManager();
    void registerDeviceNotification();
    void updateButtonStates();
    void forceStopCompletion();

private:
    // UI form class instance
    Ui::FX3TestToolClass ui;

    // USB device
    std::shared_ptr<USBDevice> m_usbDevice;
    std::shared_ptr<DataAcquisitionManager> m_acquisitionManager;

    bool m_deviceInitializing;
    bool m_loggerInitialized;
    bool m_commandsLoaded = false;
    bool m_deviceReady = false;

    QTimer m_debounceTimer;
    static const int DEBOUNCE_DELAY = 1000; // 1秒防抖时间

    std::atomic<bool> m_stopRequested{ false };
    static constexpr int STOP_TIMEOUT_MS = 5000;  // 5秒超时
    QTimer m_statusUpdateTimer;
};