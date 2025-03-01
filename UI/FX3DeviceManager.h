#pragma once

#include <QObject>
#include <memory>
#include <chrono>
#include <QTimer>
#include <QElapsedTimer>
#include "USBDevice.h"
#include "DataAcquisition.h"
#include "CommandManager.h"
#include "AppStateMachine.h"

// �豸������ - ������FX3�豸�����н���
class FX3DeviceManager : public QObject {
    Q_OBJECT

public:
    explicit FX3DeviceManager(QObject* parent = nullptr);
    ~FX3DeviceManager();

    // ��ʼ���豸�Ͳɼ�������
    bool initializeDeviceAndManager(HWND windowHandle);

    // �豸����
    bool checkAndOpenDevice();
    bool resetDevice();

    // �������
    bool loadCommandFiles(const QString& directoryPath);

    // �������
    bool startTransfer(uint16_t width, uint16_t height, uint8_t capType);
    bool stopTransfer();
    void stopAllTransfers();
    void releaseResources();

    // ״̬��ѯ
    bool isDeviceConnected() const;
    bool isTransferring() const;
    QString getDeviceInfo() const;
    QString getUsbSpeedDescription() const;
    bool isUSB3() const;

public slots:
    // �豸�¼�����
    void onDeviceArrival();
    void onDeviceRemoval();

    // �豸�źŴ���
    void onUsbStatusChanged(const std::string& status);
    void onTransferProgress(uint64_t transferred, int length, int success, int failed);
    void onDeviceError(const QString& error);

    // �ɼ��������źŴ���
    void onAcquisitionStarted();
    void onAcquisitionStopped();
    void onDataReceived(const DataPacket& packet);
    void onAcquisitionError(const QString& error);
    void onStatsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeSeconds);
    void onAcquisitionStateChanged(const QString& state);

    // �źŴ��� - ����UI����
signals:
    void transferStatsUpdated(uint64_t transferred, double speed, uint64_t elapsedTimeSeconds);
    void usbSpeedUpdated(const QString& speedDesc, bool isUSB3);
    void deviceError(const QString& title, const QString& error);
    void dataProcessed(const DataPacket& packet);

private:
    // ��ʼ���ź�����
    void initConnections();

    // ��������
    void debounceDeviceEvent(std::function<void()> action);

    // �豸�Ͳɼ�������ָ��
    std::shared_ptr<USBDevice> m_usbDevice;
    std::shared_ptr<DataAcquisitionManager> m_acquisitionManager;

    // �������
    QTimer m_debounceTimer;
    static const int DEBOUNCE_DELAY = 500; // ms
    QElapsedTimer m_lastDeviceEventTime;

    // ����ͳ��
    std::chrono::steady_clock::time_point m_transferStartTime;
    uint64_t m_lastTransferred = 0;

    // ��
    std::mutex m_shutdownMutex;

    // �����ڹر��ڼ䴦���¼�
    std::atomic<bool> m_shuttingDown{ false };
    std::atomic<bool> m_isTransferring{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_errorOccurred{false};
    std::atomic<bool> m_stoppingInProgress{false};
    std::atomic<bool> m_commandsLoaded{false};
};