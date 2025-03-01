#include "FX3DeviceManager.h"
#include "Logger.h"
#include <QThread>
#include <QCoreApplication>

FX3DeviceManager::FX3DeviceManager(QObject* parent)
    : QObject(parent)
    , m_debounceTimer(this)
{
    LOG_INFO(QString::fromLocal8Bit("FX3DeviceManager���캯��"));

    // ��ʼ��������ʱ��
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(DEBOUNCE_DELAY);
    m_lastDeviceEventTime.start();

    // ��ʼ����Ա����
    m_isTransferring.store(false);
    m_running.store(false);
    m_errorOccurred.store(false);
    m_stoppingInProgress.store(false);
    m_commandsLoaded.store(false);
}

FX3DeviceManager::~FX3DeviceManager() {
    LOG_INFO(QString::fromLocal8Bit("FX3DeviceManager�����������"));

    // ʹ�û�������ֹ���������еľ�̬����
    std::unique_lock<std::mutex> shutdownLock(m_shutdownMutex);

    try {
        // ���ùرձ�־����ֹ�ڹرչ����д������¼�
        m_shuttingDown = true;
        LOG_INFO(QString::fromLocal8Bit("���ùرձ�־"));

        // ȷ��ֹͣ���ж�ʱ��
        LOG_INFO(QString::fromLocal8Bit("ֹͣ��ʱ��"));
        m_debounceTimer.stop();

        // �Ͽ������ź�����
        LOG_INFO(QString::fromLocal8Bit("�Ͽ������ź�����"));
        disconnect(this, nullptr, nullptr, nullptr);

        // ֹͣ�������ݴ���
        stopAllTransfers();

        // ��������˳���ͷ���Դ
        releaseResources();

        LOG_INFO(QString::fromLocal8Bit("FX3DeviceManager���������˳� - �ɹ�"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString::fromLocal8Bit("���������쳣: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(QString::fromLocal8Bit("��������δ֪�쳣"));
    }
}

bool FX3DeviceManager::initializeDeviceAndManager(HWND windowHandle) {
    try {
        LOG_INFO(QString::fromLocal8Bit("��ʼ��USB�豸�͹�����"));

        // ���ȴ��� USB �豸
        m_usbDevice = std::make_shared<USBDevice>(windowHandle);
        if (!m_usbDevice) {
            LOG_ERROR(QString::fromLocal8Bit("����USB�豸ʧ��"));
            AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("����USB�豸ʧ��"));
            return false;
        }

        // Ȼ�󴴽��ɼ�������
        try {
            m_acquisitionManager = DataAcquisitionManager::create(m_usbDevice);
        }
        catch (const std::exception& e) {
            LOG_ERROR(QString::fromLocal8Bit("�����ɼ��������쳣: %1").arg(e.what()));
            m_usbDevice.reset();  // �����Ѵ�������Դ
            AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�����ɼ�������ʧ��: %1").arg(e.what()));
            return false;
        }

        // ��ʼ�������ź�����
        initConnections();

        // ����豸״̬
        if (checkAndOpenDevice()) {
            return true;
        }
        else {
            // �豸δ���ӻ��ʧ�ܣ�����ʼ���ɹ�
            AppStateMachine::instance().processEvent(StateEvent::APP_INIT, QString::fromLocal8Bit("��ʼ����ɵ��豸δ����"));
            return true;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString::fromLocal8Bit("�豸��ʼ���쳣: %1").arg(e.what()));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�豸��ʼ���쳣: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR(QString::fromLocal8Bit("�豸��ʼ��δ֪�쳣"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�豸��ʼ��δ֪�쳣"));
        return false;
    }
}

void FX3DeviceManager::initConnections() {
    // USB�豸�ź�����
    if (m_usbDevice) {
        connect(m_usbDevice.get(), &USBDevice::statusChanged,
            this, &FX3DeviceManager::onUsbStatusChanged, Qt::QueuedConnection);
        connect(m_usbDevice.get(), &USBDevice::transferProgress,
            this, &FX3DeviceManager::onTransferProgress, Qt::QueuedConnection);
        connect(m_usbDevice.get(), &USBDevice::deviceError,
            this, &FX3DeviceManager::onDeviceError, Qt::QueuedConnection);
    }

    // �ɼ��������ź�����
    if (m_acquisitionManager) {
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::dataReceived,
            this, &FX3DeviceManager::onDataReceived, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::errorOccurred,
            this, &FX3DeviceManager::onAcquisitionError, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::statsUpdated,
            this, &FX3DeviceManager::onStatsUpdated, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::acquisitionStateChanged,
            this, &FX3DeviceManager::onAcquisitionStateChanged, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::acquisitionStarted,
            this, &FX3DeviceManager::onAcquisitionStarted, Qt::QueuedConnection);
        connect(m_acquisitionManager.get(), &DataAcquisitionManager::acquisitionStopped,
            this, &FX3DeviceManager::onAcquisitionStopped, Qt::QueuedConnection);
    }
}

bool FX3DeviceManager::checkAndOpenDevice() {
    LOG_INFO(QString::fromLocal8Bit("����豸����״̬..."));

    if (!m_usbDevice) {
        LOG_ERROR(QString::fromLocal8Bit("USB�豸����δ��ʼ��"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("USB�豸����δ��ʼ��"));
        return false;
    }

    // ����豸����״̬
    if (!m_usbDevice->isConnected()) {
        LOG_WARN(QString::fromLocal8Bit("δ��⵽�豸����"));
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, QString::fromLocal8Bit("δ��⵽�豸����"));
        return false;
    }

    // ��ȡ����¼�豸��Ϣ
    QString deviceInfo = m_usbDevice->getDeviceInfo();
    LOG_INFO(QString::fromLocal8Bit("�����豸: %1").arg(deviceInfo));

    // ���Դ��豸
    if (!m_usbDevice->open()) {
        LOG_ERROR(QString::fromLocal8Bit("���豸ʧ��"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("���豸ʧ��"));
        return false;
    }

    LOG_INFO(QString::fromLocal8Bit("�豸���ʹ򿪳ɹ�"));

    // �����豸�����¼�
    AppStateMachine::instance().processEvent(StateEvent::DEVICE_CONNECTED, QString::fromLocal8Bit("�豸�ѳɹ����Ӻʹ�"));

    // ֪ͨUI����USB�ٶ���Ϣ
    emit usbSpeedUpdated(getUsbSpeedDescription(), isUSB3());

    return true;
}

bool FX3DeviceManager::resetDevice() {
    LOG_INFO(QString::fromLocal8Bit("�����豸"));

    if (!m_usbDevice) {
        LOG_ERROR(QString::fromLocal8Bit("USB�豸����δ��ʼ��"));
        return false;
    }

    // �����豸���ÿ�ʼ�¼�
    AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, QString::fromLocal8Bit("���������豸"));

    if (m_usbDevice->reset()) {
        LOG_INFO(QString::fromLocal8Bit("�豸���óɹ�"));

        // �����豸�����¼�����Ϊ�����൱����������
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_CONNECTED, QString::fromLocal8Bit("�豸���óɹ�"));

        // ֪ͨUI����USB�ٶ���Ϣ
        emit usbSpeedUpdated(getUsbSpeedDescription(), isUSB3());

        return true;
    }
    else {
        LOG_ERROR(QString::fromLocal8Bit("�豸����ʧ��"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�豸����ʧ��"));
        return false;
    }
}

bool FX3DeviceManager::loadCommandFiles(const QString& directoryPath) {
    LOG_INFO(QString::fromLocal8Bit("��Ŀ¼���������ļ�: %1").arg(directoryPath));

    // ��������Ŀ¼
    if (!CommandManager::instance().setCommandDirectory(directoryPath)) {
        LOG_ERROR(QString::fromLocal8Bit("��������Ŀ¼ʧ��"));
        m_commandsLoaded = false;
        return false;
    }

    // ��֤���б���������ļ�
    if (!CommandManager::instance().validateCommands()) {
        LOG_ERROR(QString::fromLocal8Bit("������֤ʧ��"));
        m_commandsLoaded = false;
        return false;
    }

    m_commandsLoaded = true;

    // ������������¼�
    LOG_INFO(QString::fromLocal8Bit("�����ļ����سɹ�������COMMANDS_LOADED�¼�"));

    // ȷ�������߳���ִ��״̬����
    QMetaObject::invokeMethod(QCoreApplication::instance(), [this]() {
        AppStateMachine::instance().processEvent(StateEvent::COMMANDS_LOADED,
            QString::fromLocal8Bit("�����ļ����سɹ�"));
        }, Qt::QueuedConnection);

    LOG_INFO(QString::fromLocal8Bit("�����ļ��������"));
    return true;
}

bool FX3DeviceManager::startTransfer(uint16_t width, uint16_t height, uint8_t capType) {
    LOG_INFO(QString::fromLocal8Bit("�������ݴ���"));

    if (m_shuttingDown) {
        LOG_INFO(QString::fromLocal8Bit("Ӧ�����ڹرգ�������������"));
        return false;
    }

    if (!m_usbDevice || !m_acquisitionManager) {
        LOG_ERROR(QString::fromLocal8Bit("�豸��ɼ�������δ��ʼ��"));
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�豸��ɼ�������δ��ʼ��"));
        return false;
    }

    LOG_INFO(QString::fromLocal8Bit("�����ɼ��Ĳ��� - ���: %1, �߶�: %2, ����: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    // ���Ϳ�ʼ���������¼�
    AppStateMachine::instance().processEvent(StateEvent::START_REQUESTED, QString::fromLocal8Bit("����ʼ����"));

    // ����USB�豸��ͼ�����
    m_usbDevice->setImageParams(width, height, capType);

    // ��¼���俪ʼʱ��
    m_transferStartTime = std::chrono::steady_clock::now();
    m_lastTransferred = 0;

    // �������ɼ�������
    if (!m_acquisitionManager->startAcquisition(width, height, capType)) {
        LOG_ERROR(QString::fromLocal8Bit("�����ɼ�������ʧ��"));
        AppStateMachine::instance().processEvent(StateEvent::START_FAILED, QString::fromLocal8Bit("�����ɼ�������ʧ��"));
        return false;
    }

    // ������USB����
    if (!m_usbDevice->startTransfer()) {
        LOG_ERROR(QString::fromLocal8Bit("����USB����ʧ��"));
        m_acquisitionManager->stopAcquisition();
        AppStateMachine::instance().processEvent(StateEvent::START_FAILED, QString::fromLocal8Bit("����USB����ʧ��"));
        return false;
    }

    LOG_INFO(QString::fromLocal8Bit("���ݲɼ������ɹ�"));
    return true;
}

bool FX3DeviceManager::stopTransfer() {
    LOG_INFO(QString::fromLocal8Bit("ֹͣ���ݴ���"));

    // ���Ӧ�ó������ڹرգ���ֹͣ����
    if (m_shuttingDown) {
        LOG_INFO(QString::fromLocal8Bit("Ӧ�����ڹرգ�ִ�м�ֹͣ"));
        try {
            if (m_acquisitionManager && m_acquisitionManager->isRunning()) {
                m_acquisitionManager->stopAcquisition();
            }
            if (m_usbDevice && m_usbDevice->isTransferring()) {
                m_usbDevice->stopTransfer();
            }
        }
        catch (...) {
            LOG_ERROR(QString::fromLocal8Bit("�رչ�����ֹͣ�����쳣"));
        }
        return true;
    }

    // �����ε���ֹͣ
    if (m_stoppingInProgress) {
        LOG_WARN(QString::fromLocal8Bit("ֹͣ�������ڽ�����"));
        return true;
    }

    m_stoppingInProgress = true;
    AppStateMachine::instance().processEvent(StateEvent::STOP_REQUESTED, QString::fromLocal8Bit("����ֹͣ����"));

    try {
        // ����ֹͣUSB����
        if (m_usbDevice && m_usbDevice->isTransferring()) {
            LOG_INFO(QString::fromLocal8Bit("ֹͣUSB�豸����"));
            if (!m_usbDevice->stopTransfer()) {
                LOG_WARN(QString::fromLocal8Bit("ֹͣUSB���䷵��ʧ��"));
            }
        }

        // Ȼ��ֹͣ�ɼ�������
        if (m_acquisitionManager && m_acquisitionManager->isRunning()) {
            LOG_INFO(QString::fromLocal8Bit("ֹͣ�ɼ�������"));
            m_acquisitionManager->stopAcquisition();
        }

        // �����ﲻ���óɹ��¼����ȴ����Բɼ���������ֹ֪ͣͨ
        LOG_INFO(QString::fromLocal8Bit("ֹͣ�����ѷ���"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString::fromLocal8Bit("ֹͣ�����쳣: %1").arg(e.what()));
        m_stoppingInProgress = false;
        AppStateMachine::instance().processEvent(StateEvent::STOP_FAILED, QString::fromLocal8Bit("ֹͣ�����쳣: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR(QString::fromLocal8Bit("ֹͣ����δ֪�쳣"));
        m_stoppingInProgress = false;
        AppStateMachine::instance().processEvent(StateEvent::STOP_FAILED, QString::fromLocal8Bit("ֹͣ����δ֪�쳣"));
        return false;
    }
}

void FX3DeviceManager::stopAllTransfers() {
    // ����ɼ����ڻ״̬��ǿ��ֹͣ
    if (isTransferring()) {
        LOG_INFO(QString::fromLocal8Bit("ǿ��ֹͣ����"));

        try {
            stopTransfer();

            // �ȴ�һС��ʱ����ֹͣ�������
            QElapsedTimer timer;
            timer.start();
            const int MAX_WAIT_MS = 200;

            while (isTransferring() && timer.elapsed() < MAX_WAIT_MS) {
                QThread::msleep(10);
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(QString::fromLocal8Bit("ֹͣ�����쳣: %1").arg(e.what()));
        }
        catch (...) {
            LOG_ERROR(QString::fromLocal8Bit("ֹͣ����δ֪�쳣"));
        }
    }
}

void FX3DeviceManager::releaseResources() {
    LOG_INFO(QString::fromLocal8Bit("�ͷ���Դ - ��ʼ"));

    // 1. �����ͷŲɼ�������
    if (m_acquisitionManager) {
        LOG_INFO(QString::fromLocal8Bit("���òɼ�������"));
        try {
            // �Ͽ������ź�����
            disconnect(m_acquisitionManager.get(), nullptr, nullptr, nullptr);

            // �ڹر�ǰ׼��
            m_acquisitionManager->prepareForShutdown();

            // �ͷ�����ָ��
            m_acquisitionManager.reset();
        }
        catch (const std::exception& e) {
            LOG_ERROR(QString::fromLocal8Bit("�ͷŲɼ��������쳣: %1").arg(e.what()));
        }
    }

    // ������ʱȷ���ɼ�����������ȫ�ͷ�
    QThread::msleep(20);

    // 2. Ȼ���ͷ�USB�豸
    if (m_usbDevice) {
        LOG_INFO(QString::fromLocal8Bit("����USB�豸"));
        try {
            // �Ͽ������ź�����
            disconnect(m_usbDevice.get(), nullptr, nullptr, nullptr);

            // ȷ���豸�ѹر�
            if (m_usbDevice->isConnected()) {
                m_usbDevice->close();
            }

            // �ͷ�����ָ��
            m_usbDevice.reset();
        }
        catch (const std::exception& e) {
            LOG_ERROR(QString::fromLocal8Bit("�ͷ�USB�豸�쳣: %1").arg(e.what()));
        }
    }

    LOG_INFO(QString::fromLocal8Bit("�ͷ���Դ - ���"));
}

void FX3DeviceManager::onDeviceArrival() {
    debounceDeviceEvent([this]() {
        LOG_WARN(QString::fromLocal8Bit("��⵽USB�豸����"));

        if (m_shuttingDown) {
            LOG_INFO(QString::fromLocal8Bit("Ӧ�����ڹرգ������豸�����¼�"));
            return;
        }

        if (!m_usbDevice) {
            LOG_ERROR(QString::fromLocal8Bit("USB�豸����δ��ʼ��"));
            return;
        }

        // ��鲢���豸
        checkAndOpenDevice();
        });
}

void FX3DeviceManager::onDeviceRemoval() {
    debounceDeviceEvent([this]() {
        LOG_WARN(QString::fromLocal8Bit("��⵽USB�豸�Ƴ�"));

        if (m_shuttingDown) {
            LOG_INFO(QString::fromLocal8Bit("Ӧ�����ڹرգ������豸�Ƴ��¼�"));
            return;
        }

        // ȷ���豸��ȷ�ر�
        if (m_usbDevice) {
            m_usbDevice->close();
        }

        // �����豸�Ͽ��¼�
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, QString::fromLocal8Bit("�豸�ѶϿ�����"));
        });
}

void FX3DeviceManager::debounceDeviceEvent(std::function<void()> action) {
    static QElapsedTimer lastEventTime;
    const int MIN_EVENT_INTERVAL = 500; // ��С�¼����(ms)

    // ��ֹ�¼��ظ�����
    if (lastEventTime.isValid() &&
        lastEventTime.elapsed() < MIN_EVENT_INTERVAL) {
        LOG_DEBUG(QString::fromLocal8Bit("�����ظ����豸�¼�"));
        return;
    }
    lastEventTime.restart();

    // ʹ�ö�ʱ���ӳٴ����ȴ��豸ö�����
    QTimer::singleShot(DEBOUNCE_DELAY, this, action);
}

void FX3DeviceManager::onUsbStatusChanged(const std::string& status) {
    LOG_INFO(QString::fromLocal8Bit("FX3 USB�豸״̬���: ") + QString::fromStdString(status));

    if (m_shuttingDown) {
        return;
    }

    if (status == "ready") {
        if (m_commandsLoaded) {
            AppStateMachine::instance().processEvent(StateEvent::COMMANDS_LOADED, QString::fromLocal8Bit("�豸�����������Ѽ���"));
        }
        else {
            AppStateMachine::instance().processEvent(StateEvent::DEVICE_CONNECTED, QString::fromLocal8Bit("�豸����������δ����"));
        }
    }
    else if (status == "transferring") {
        AppStateMachine::instance().processEvent(StateEvent::START_SUCCEEDED, QString::fromLocal8Bit("USB״̬��Ϊ������"));
    }
    else if (status == "disconnected") {
        AppStateMachine::instance().processEvent(StateEvent::DEVICE_DISCONNECTED, QString::fromLocal8Bit("USB״̬��Ϊ�ѶϿ�"));
    }
    else if (status == "error") {
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("USB�豸����"));
    }
}

void FX3DeviceManager::onTransferProgress(uint64_t transferred, int length, int success, int failed) {
    if (m_shuttingDown) {
        return;
    }

    static QElapsedTimer speedTimer;
    static uint64_t lastTransferred = 0;

    // ��ʼ����ʱ��
    if (!speedTimer.isValid()) {
        speedTimer.start();
        lastTransferred = transferred;
        return;
    }

    // �����ٶ�
    qint64 elapsed = speedTimer.elapsed();
    if (elapsed > 0) {
        double intervalBytes = static_cast<double>(transferred - lastTransferred);
        // ת��ΪMB/s
        double speed = (intervalBytes * 1000.0) / (elapsed * 1024.0 * 1024.0);

        // ����ͳ������
        emit transferStatsUpdated(transferred, speed, 0);

        // ���»�׼ֵ
        speedTimer.restart();
        lastTransferred = transferred;
    }
}

void FX3DeviceManager::onDeviceError(const QString& error) {
    LOG_ERROR(QString::fromLocal8Bit("FX3 USB�豸����: ") + error);

    if (m_shuttingDown) {
        return;
    }

    // �����豸�����¼�
    AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("USB�豸����: ") + error);

    // ֪ͨUI��ʾ����
    emit deviceError(QString::fromLocal8Bit("�豸����"), error);
}

void FX3DeviceManager::onAcquisitionStarted() {
    LOG_INFO(QString::fromLocal8Bit("�ɼ��ѿ�ʼ"));

    if (m_shuttingDown) {
        return;
    }

    // ���ʹ��俪ʼ�ɹ��¼�
    AppStateMachine::instance().processEvent(StateEvent::START_SUCCEEDED, QString::fromLocal8Bit("�ɼ��ѳɹ���ʼ"));
}

void FX3DeviceManager::onAcquisitionStopped() {
    LOG_INFO(QString::fromLocal8Bit("�ɼ���ֹͣ"));

    if (m_shuttingDown) {
        return;
    }

    // ����ֹͣ��־
    m_stoppingInProgress = false;

    // ����ֹͣ�ɹ��¼�
    AppStateMachine::instance().processEvent(StateEvent::STOP_SUCCEEDED, QString::fromLocal8Bit("�ɼ��ѳɹ�ֹͣ"));
}

void FX3DeviceManager::onDataReceived(const DataPacket& packet) {
    if (m_shuttingDown) {
        return;
    }

    // ת�����ݰ���UI�������������
    emit dataProcessed(packet);
}

void FX3DeviceManager::onAcquisitionError(const QString& error) {
    LOG_ERROR(QString::fromLocal8Bit("�ɼ�����: ") + error);

    if (m_shuttingDown) {
        return;
    }

    // ����ֹͣ��־
    m_stoppingInProgress = false;

    // ���ʹ����¼�
    AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�ɼ�����: ") + error);

    // ֪ͨUI��ʾ����
    emit deviceError(QString::fromLocal8Bit("�ɼ�����"), error);
}

void FX3DeviceManager::onStatsUpdated(uint64_t receivedBytes, double dataRate, uint64_t elapsedTimeSeconds) {
    if (m_shuttingDown) {
        return;
    }

    // ת��ͳ�����ݸ�UI
    emit transferStatsUpdated(receivedBytes, dataRate, elapsedTimeSeconds);
}

void FX3DeviceManager::onAcquisitionStateChanged(const QString& state) {
    LOG_INFO(QString::fromLocal8Bit("�ɼ�״̬���Ϊ: ") + state);

    if (m_shuttingDown) {
        return;
    }

    // ���ݲɼ�״̬������Ӧ�¼�
    if (state == QString::fromLocal8Bit("����") ||
        state == QString::fromLocal8Bit("��ֹͣ")) {
        if (m_stoppingInProgress) {
            // ���ֹͣ�������ڽ����У����ʾֹͣ���������
            m_stoppingInProgress = false;
            AppStateMachine::instance().processEvent(StateEvent::STOP_SUCCEEDED, QString::fromLocal8Bit("�ɼ�״̬��Ϊ����/��ֹͣ"));
        }
    }
    else if (state == QString::fromLocal8Bit("�ɼ���")) {
        AppStateMachine::instance().processEvent(StateEvent::START_SUCCEEDED, QString::fromLocal8Bit("�ɼ�״̬��Ϊ�ɼ���"));
    }
    else if (state == QString::fromLocal8Bit("����")) {
        m_stoppingInProgress = false;
        AppStateMachine::instance().processEvent(StateEvent::ERROR_OCCURRED, QString::fromLocal8Bit("�ɼ�״̬��Ϊ����"));
    }
}

bool FX3DeviceManager::isDeviceConnected() const {
    return m_usbDevice && m_usbDevice->isConnected();
}

bool FX3DeviceManager::isTransferring() const {
    return m_usbDevice && m_usbDevice->isTransferring();
}

QString FX3DeviceManager::getDeviceInfo() const {
    if (!m_usbDevice) {
        return QString::fromLocal8Bit("���豸��Ϣ");
    }
    return m_usbDevice->getDeviceInfo();
}

QString FX3DeviceManager::getUsbSpeedDescription() const {
    if (!m_usbDevice) {
        return QString::fromLocal8Bit("δ����");
    }
    return m_usbDevice->getUsbSpeedDescription();
}

bool FX3DeviceManager::isUSB3() const {
    return m_usbDevice && m_usbDevice->isUSB3();
}