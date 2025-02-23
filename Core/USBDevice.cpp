#include <QDebug>
#include <QThread>
#include "USBDevice.h"
#include "CommandManager.h"
#include "Logger.h"

USBDevice::USBDevice(HWND hwnd)
    : m_hwnd(hwnd)
    , m_inEndpoint(nullptr)
    , m_outEndpoint(nullptr)
    , m_isTransferring(false)
    , m_frameSize(DEFAULT_TRANSFER_SIZE)
    , m_isConfigured(false)
{
    m_device = std::make_shared<CCyUSBDevice>(hwnd, CYUSBDRV_GUID, true);
}

USBDevice::~USBDevice()
{
    close();
}

bool USBDevice::isConnected() const
{
    return m_device && m_device->DeviceCount() > 0;
}

// ���豸��ʼ��ʱ������Ի���
bool USBDevice::open()
{
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 500;

    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
        LOG_INFO(QString("Device open attempt %1 of %2").arg(attempt).arg(MAX_RETRIES));

        if (!m_device) {
            LOG_ERROR("Device not initialized");
            emitError("Device initialization error");
            return false;
        }

        // ÿ�γ���ǰˢ���豸״̬
        if (m_device->DeviceCount() <= 0) {
            m_device = std::make_shared<CCyUSBDevice>((HWND)m_hwnd, CYUSBDRV_GUID, true);
        }

        if (m_device->DeviceCount() <= 0) {
            LOG_ERROR("No USB device found");
            emitError("No USB device found");
            return false;
        }

        if (!m_device->Open(0)) {
            LOG_ERROR(QString("Failed to open device (attempt %1)").arg(attempt));
            if (attempt < MAX_RETRIES) {
                QThread::msleep(RETRY_DELAY_MS);
                continue;
            }
            emitError("Failed to open device");
            return false;
        }

        if (!validateDevice()) {
            LOG_ERROR(QString("Device validation failed (attempt %1)").arg(attempt));
            close();
            if (attempt < MAX_RETRIES) {
                QThread::msleep(RETRY_DELAY_MS);
                continue;
            }
            return false;
        }

        if (!initEndpoints()) {
            LOG_ERROR(QString("Endpoint initialization failed (attempt %1)").arg(attempt));
            close();
            if (attempt < MAX_RETRIES) {
                QThread::msleep(RETRY_DELAY_MS);
                continue;
            }
            return false;
        }

        LOG_INFO("Device initialized successfully");
        emit statusChanged("ready");
        return true;
    }

    return false;
}

void USBDevice::close()
{
    if (m_isTransferring) {
        stopTransfer();
    }

    if (m_device) {
        m_device->Close();
    }

    m_inEndpoint = nullptr;
    m_outEndpoint = nullptr;

    emit statusChanged("disconnected");
}

bool USBDevice::reset()
{
    if (!m_device) return false;

    close();
    m_device->Reset();
    return open();
}

bool USBDevice::readData(PUCHAR buffer, LONG& length)
{
    if (!m_device || !m_inEndpoint || !buffer) {
        LOG_ERROR("Device not properly initialized for reading data");
        return false;
    }

    m_inEndpoint->TimeOut = 1000;
    bool success = m_inEndpoint->XferData(buffer, length);

    if (success && length > 0) {
        // �������ֽ���
        uint64_t currentTotal = m_totalBytes.load();
        m_totalBytes.store(currentTotal + length);

        // ���´����ٶ�
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_lastSpeedUpdate).count();

        if (duration >= SPEED_UPDATE_INTERVAL_MS) {
            // ���������ٶȣ�MB/s��
            uint64_t lastTotal = m_lastTotalBytes.load();
            double intervalBytes = static_cast<double>(currentTotal + length - lastTotal);
            //double speed = (intervalBytes / duration) * 1000.0 / (1024.0 * 1024.0);

            //m_currentSpeed.store(speed);
            m_lastSpeedUpdate = now;
            m_lastTotalBytes.store(currentTotal + length);

            // ���ͽ��ȸ����ź�
            emit transferProgress(m_totalBytes.load(), length, 1, 0);
        }
    }

    return success;
}

bool USBDevice::startTransfer()
{
    if (!m_device || !m_inEndpoint || !m_outEndpoint) {
        LOG_ERROR("Device not properly initialized");
        emitError("Device not properly initialized");
        return false;
    }

    LOG_INFO("Start hw data transfer...");

    if (m_isTransferring) {
        LOG_WARN("Transfer already in progress");
        return true;
    }

    // ʹ�������������ȡ��������
    auto startCmd = CommandManager::instance().getCommand(CommandManager::CommandType::CMD_START);
    if (startCmd.empty()) {
        LOG_ERROR("Failed to get start command");
        return false;
    }

    // ���Ϳ�ʼ����
    if (!sendCommand(startCmd.data(), startCmd.size())) {
        LOG_ERROR("Failed to send start command");
        return false;
    }

    // ֻ�����ض������²����ô����־
    if (m_channelMode != 0xfe && m_invertPn != 0xfe) {
        LOG_INFO("Transfering start OK");
        m_isTransferring = true;
        m_transferStartTime = std::chrono::steady_clock::now();
        emit statusChanged("transferring");
    }

    return true;
}

bool USBDevice::stopTransfer()
{
    LOG_INFO("Stopping hardware data transfer...");

    if (!m_isTransferring) {
        return true;
    }

    // �ȱ�������ͳ������
    uint64_t finalBytes = m_totalBytes.load();
    m_isTransferring = false;

    if (m_inEndpoint) {
        m_inEndpoint->Abort();
    }

    // �������յĴ���ͳ�ƣ��ٶ���Ϊ0��ʾ��ֹͣ��
    emit transferProgress(finalBytes, 0, 0, 0);

    // ����ֹͣ�߼�
    auto stopCmd = CommandManager::instance().getCommand(CommandManager::CommandType::CMD_END);
    if (!stopCmd.empty() && sendCommand(stopCmd.data(), stopCmd.size())) {
        LOG_DEBUG("Stop command sent successfully");
    }

    if (m_inEndpoint) {
        m_inEndpoint->Reset();
    }
    if (m_outEndpoint) {
        m_outEndpoint->Reset();
    }

    // �����ٶȣ����������ֽ���
    m_currentSpeed.store(0.0);
    emit statusChanged("ready");
    return true;
}

QString USBDevice::getDeviceInfo() const
{
    if (!m_device) return "No Device";

    return QString("VID:0x%1 PID:0x%2 %3")
        .arg(m_device->VendorID, 4, 16, QChar('0'))
        .arg(m_device->ProductID, 4, 16, QChar('0'))
        .arg(QString::fromLocal8Bit(m_device->FriendlyName));
}

bool USBDevice::isUSB3() const
{
    return m_device && m_device->bSuperSpeed;  // CCyUSBDevice ֱ���ṩ���������
}

bool USBDevice::initEndpoints()
{
    LOG_INFO("Initializing endpoints...");

    m_device->SetAltIntfc(0);

    int endpointCount = m_device->EndPointCount();

    for (int i = 1; i < endpointCount; i++) {
        CCyUSBEndPoint* endpoint = m_device->EndPoints[i];
        if (endpoint->Attributes == 2) {  // Bulk endpoint
            if (endpoint->bIn) {
                m_inEndpoint = dynamic_cast<CCyBulkEndPoint*>(endpoint);
                LOG_DEBUG(QString("Found IN endpoint: 0x%1")
                    .arg(endpoint->Address, 2, 16, QChar('0')));
            }
            else {
                m_outEndpoint = dynamic_cast<CCyBulkEndPoint*>(endpoint);
                LOG_DEBUG(QString("Found OUT endpoint: 0x%1")
                    .arg(endpoint->Address, 2, 16, QChar('0')));
            }
        }
    }

    if (!m_inEndpoint || !m_outEndpoint) {
        LOG_ERROR("Required endpoints not found");
        emitError("Required endpoints not found");
        return false;
    }

    LOG_INFO("Endpoints initialized successfully");
    return true;
}

bool USBDevice::validateDevice()
{
    if (!m_device || !m_device->IsOpen()) {
        LOG_ERROR("Device not open");
        emitError("Device not open");
        return false;
    }

    // ��¼�������豸��Ϣ
    QString deviceInfo = QString("Device Info:\n")
        + QString("  VID: 0x%1\n").arg(m_device->VendorID, 4, 16, QChar('0'))
        + QString("  PID: 0x%1\n").arg(m_device->ProductID, 4, 16, QChar('0'))
        + QString("  USB Version: 0x%1\n").arg(m_device->BcdUSB, 4, 16, QChar('0'))
        + QString("  Name: %1\n").arg(m_device->FriendlyName)
        + QString("  Manufacturer: %1\n").arg(QString::fromWCharArray(m_device->Manufacturer))
        + QString("  Product: %1").arg(QString::fromWCharArray(m_device->Product));

    LOG_INFO(deviceInfo);

    // FX3�豸�ٶȼ�� - �豸���ܴ��ڲ�ͬ���ٶ�ģʽ
    QString speedMode;
    if (m_device->bSuperSpeed) {
        speedMode = "SuperSpeed (USB 3.0)";
    }
    else if (m_device->bHighSpeed) {
        speedMode = "HighSpeed (USB 2.0)";
    }
    else {
        speedMode = "FullSpeed/LowSpeed";
    }
    LOG_INFO(QString("USB Speed Mode: %1").arg(speedMode));

    // ����Ƿ�ΪFX3�豸
    if (m_device->VendorID == 0x04b4 && m_device->ProductID == 0x00f1) {
        LOG_INFO("Detected Cypress FX3 device");
        // FX3�豸��֤ͨ��������Ҫǿ��Ҫ��USB 3.0ģʽ
    }
    else {
        LOG_ERROR("Not a Cypress FX3 device");
        emitError("Unsupported device type");
        return false;
    }

    // ��ϸ���USB�ٶ�����
    LOG_INFO("Device Speed Capabilities:");
    LOG_INFO(QString("  SuperSpeed Capable: %1").arg(m_device->bSuperSpeed ? "Yes" : "No"));
    LOG_INFO(QString("  HighSpeed Capable: %1").arg(m_device->bHighSpeed ? "Yes" : "No"));
    LOG_INFO(QString("  USB Version: 0x%1").arg(m_device->BcdUSB, 4, 16, QChar('0')));

    // ��ȡBOS������
    USB_BOS_DESCRIPTOR bosDesc;
    if (m_device->GetBosDescriptor(&bosDesc)) {
        LOG_INFO("BOS Descriptor Info:");
        LOG_INFO(QString("  Length: %1").arg(bosDesc.bLength));
        LOG_INFO(QString("  Descriptor Type: 0x%1").arg(bosDesc.bDescriptorType, 2, 16, QChar('0')));
        LOG_INFO(QString("  Total Length: %1").arg(bosDesc.wTotalLength));
        LOG_INFO(QString("  Device Capabilities Count: %1").arg(bosDesc.bNumDeviceCaps));

        // USB 2.0��չ������
        USB_BOS_USB20_DEVICE_EXTENSION usb20Desc;
        if (m_device->GetBosUSB20DeviceExtensionDescriptor(&usb20Desc)) {
            LOG_INFO("USB 2.0 Extension Descriptor:");
            LOG_INFO(QString("  Length: %1").arg(usb20Desc.bLength));
            LOG_INFO(QString("  Descriptor Type: 0x%1").arg(usb20Desc.bDescriptorType, 2, 16, QChar('0')));
            LOG_INFO(QString("  Device Capability Type: 0x%1").arg(usb20Desc.bDevCapabilityType, 2, 16, QChar('0')));
            LOG_INFO(QString("  Attributes: 0x%1").arg(usb20Desc.bmAttribute, 8, 16, QChar('0')));
        }

        // SuperSpeed�豸����������
        USB_BOS_SS_DEVICE_CAPABILITY ssDesc;
        if (m_device->GetBosSSCapabilityDescriptor(&ssDesc)) {
            LOG_INFO("SuperSpeed Device Capability:");
            LOG_INFO(QString("  Length: %1").arg(ssDesc.bLength));
            LOG_INFO(QString("  Descriptor Type: 0x%1").arg(ssDesc.bDescriptorType, 2, 16, QChar('0')));
            LOG_INFO(QString("  Device Capability Type: 0x%1").arg(ssDesc.bDevCapabilityType, 2, 16, QChar('0')));
            LOG_INFO(QString("  Attributes: 0x%1").arg(ssDesc.bmAttribute, 2, 16, QChar('0')));
            LOG_INFO(QString("  Speeds Supported: 0x%1").arg(ssDesc.wSpeedsSuported, 4, 16, QChar('0')));
            LOG_INFO(QString("  Functionality Supported: 0x%1").arg(ssDesc.bFunctionalitySupporte, 2, 16, QChar('0')));
            LOG_INFO(QString("  U1 Exit Latency: %1").arg(ssDesc.bU1DevExitLat));
            LOG_INFO(QString("  U2 Exit Latency: %1").arg(ssDesc.bU2DevExitLat));
        }

        // Container ID������
        USB_BOS_CONTAINER_ID containerDesc;
        if (m_device->GetBosContainedIDDescriptor(&containerDesc)) {
            LOG_INFO("Container ID Descriptor:");
            LOG_INFO(QString("  Length: %1").arg(containerDesc.bLength));
            LOG_INFO(QString("  Descriptor Type: 0x%1").arg(containerDesc.bDescriptorType, 2, 16, QChar('0')));
            LOG_INFO(QString("  Device Capability Type: 0x%1").arg(containerDesc.bDevCapabilityType, 2, 16, QChar('0')));

            QString containerIdStr;
            for (int i = 0; i < USB_BOS_CAPABILITY_TYPE_CONTAINER_ID_SIZE; i++) {
                containerIdStr += QString("%1").arg(containerDesc.ContainerID[i], 2, 16, QChar('0'));
            }
            LOG_INFO(QString("  Container ID: %1").arg(containerIdStr));
        }
    }

    // ��֤�˵�����
    int endpointCount = m_device->EndPointCount();
    LOG_INFO(QString("Found %1 endpoints").arg(endpointCount));

    if (endpointCount < 2) {
        LOG_ERROR(QString("Invalid %1 endpoint configuration").arg(endpointCount));
        emitError("Device endpoint configuration error");
        return false;
    }

    // ����豸״̬
    if (m_device->UsbdStatus != 0) {
        char statusStr[256];
        m_device->UsbdStatusString(m_device->UsbdStatus, statusStr);
        LOG_ERROR(QString("Device status error: %1").arg(statusStr));
        emitError(QString("Device status error: %1").arg(statusStr));
        return false;
    }

    LOG_INFO("Device validation completed successfully");
    return true;
}

void USBDevice::emitError(const QString& error)
{
    qDebug() << "USB Error:" << error;
    emit deviceError(error);
    emit statusChanged("error");
}

bool USBDevice::sendCommand(const UCHAR* cmdTemplate, size_t length)
{
    if (!m_device || !m_outEndpoint) {
        LOG_ERROR("Device not properly initialized for sending command");
        return false;
    }

    // �����������
    std::unique_ptr<UCHAR[]> cmdBuffer(new UCHAR[CMD_BUFFER_SIZE]);

    // ׼����������
    if (!prepareCommandBuffer(cmdBuffer.get(), cmdTemplate)) {
        LOG_WARN("Prepare commond failed");
        return false;
    }

    // ���ö˵����
    m_outEndpoint->TimeOut = 500;  // ��ԭ�������ó�ʱΪ500ms
    m_outEndpoint->SetXferSize(CMD_BUFFER_SIZE);

    // �����������
    if (m_channelMode == 0xfe && m_invertPn == 0xfe) {
        memset(cmdBuffer.get(), 0x00, CMD_BUFFER_SIZE);
        LOG_DEBUG("Special channel case");
    }

    // ��ֹ����˵㵱ǰ����
    if (m_inEndpoint) {
        m_inEndpoint->Abort();
        //LOG_DEBUG("Abort");
    }

    // �����ʱ
    QThread::msleep(12);

    // ��������
    LONG actualLength = CMD_BUFFER_SIZE;
    bool success = m_outEndpoint->XferData(cmdBuffer.get(), actualLength);

    if (!success) {
        LOG_ERROR(QString("Command send failed, error: 0x%1")
            .arg(m_outEndpoint->LastError, 8, 16, QChar('0')));
        return false;
    }

    // ��֤���䳤��
    if (actualLength != CMD_BUFFER_SIZE) {
        LOG_ERROR(QString("Command length mismatch: sent %1, expected %2")
            .arg(actualLength).arg(CMD_BUFFER_SIZE));
        return false;
    }

    LOG_DEBUG(QString("Command sent successfully, length: %1").arg(actualLength));
    return true;
}

bool USBDevice::configureTransfer(ULONG frameSize)
{
    // ʹ�������������ȡ֡��С��������
    auto frameSizeCmd = CommandManager::instance().getCommand(CommandManager::CommandType::CMD_FRAME_SIZE);
    if (frameSizeCmd.empty()) {
        LOG_ERROR("Failed to get frame size command");
        return false;
    }

    if (!sendCommand(frameSizeCmd.data(), frameSizeCmd.size())) {
        LOG_ERROR("Failed to configure frame size");
        return false;
    }

    m_frameSize = frameSize;
    m_isConfigured = true;
    return true;
}

bool USBDevice::prepareCommandBuffer(PUCHAR buffer, const UCHAR* cmdTemplate)
{
    if (!buffer || !cmdTemplate) {
        LOG_ERROR("Invalid buffer or template");
        return false;
    }

    // ���ƻ�������ģ��
    memcpy_s(buffer, CMD_BUFFER_SIZE, cmdTemplate, CMD_BUFFER_SIZE);

    // �����豸�����޸���������
    if (m_capType == 0x39) {
        buffer[80] = ((m_width * 3 + 1) & 0xff00) >> 8;
        buffer[81] = (m_width * 3 + 1) & 0xff;
    }
    else {
        buffer[80] = ((m_width * 3) & 0xff00) >> 8;
        buffer[81] = (m_width * 3) & 0xff;
    }

    buffer[84] = (m_height & 0xff00) >> 8;
    buffer[85] = m_height & 0xff;
    buffer[88] = m_lanSeq;
    buffer[89] = m_lanSeq;
    buffer[92] = m_capType;
    buffer[93] = m_capType;
    buffer[0x48] = m_channelMode | (m_channelMode << 4);
    buffer[0x4C] = m_invertPn;

    return true;
}

void USBDevice::updateTransferStats()
{
    // �Ƴ��κεȴ����ӳ��߼�
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_transferStartTime).count();

    // ��ʹdurationΪ0Ҳ����ͳ����Ϣ������ȴ�
    double totalMB = static_cast<double>(m_totalTransferred) / (1024 * 1024);
    double rateMBps = (duration > 0) ? (totalMB / duration) : 0.0;

    LOG_INFO(QString("Transfer complete - Total: %1 MB, Duration: %2s, Rate: %3 MB/s")
        .arg(totalMB, 0, 'f', 2)
        .arg(duration)
        .arg(rateMBps, 0, 'f', 2));

    // ����ѡ����ͳ����Ϣ��UI
    emit transferProgress(m_totalTransferred, 0, 0, 0);
}
