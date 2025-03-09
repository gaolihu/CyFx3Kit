// Source/MVC/Models/DeviceModel.cpp
#include "DeviceModel.h"
#include "Logger.h"
#include <QSettings>
#include <QMutexLocker>

DeviceModel* DeviceModel::getInstance()
{
    static DeviceModel s;
    return &s;
}

DeviceModel::DeviceModel()
    : QObject(nullptr)
    , m_imageWidth(1920)
    , m_imageHeight(1080)
    , m_captureType(0x39)  // 默认为RAW10
    , m_deviceState(DeviceState::DEV_DISCONNECTED)
    , m_usbSpeed(0.0)
    , m_transferredBytes(0)
{
    LOG_INFO("设备模型已创建");
}

DeviceModel::~DeviceModel()
{
    LOG_INFO("设备模型已销毁");
}

uint16_t DeviceModel::getImageWidth() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_imageWidth;
}

void DeviceModel::setImageWidth(uint16_t width)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_imageWidth == width) {
            return;
        }
        m_imageWidth = width;
    }

    emit imageParametersChanged(m_imageWidth, m_imageHeight, m_captureType);
    LOG_INFO(QString("图像宽度已更新为: %1").arg(width));
}

uint16_t DeviceModel::getImageHeight() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_imageHeight;
}

void DeviceModel::setImageHeight(uint16_t height)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_imageHeight == height) {
            return;
        }
        m_imageHeight = height;
    }

    emit imageParametersChanged(m_imageWidth, m_imageHeight, m_captureType);
    LOG_INFO(QString("图像高度已更新为: %1").arg(height));
}

uint8_t DeviceModel::getCaptureType() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_captureType;
}

void DeviceModel::setCaptureType(uint8_t captureType)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_captureType == captureType) {
            return;
        }
        m_captureType = captureType;
    }

    emit imageParametersChanged(m_imageWidth, m_imageHeight, m_captureType);
    LOG_INFO(QString("图像捕获类型已更新为: 0x%1").arg(captureType, 2, 16, QChar('0')));
}

DeviceState DeviceModel::getDeviceState() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_deviceState;
}

void DeviceModel::setDeviceState(DeviceState state)
{
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_deviceState == state) {
            return;
        }
        m_deviceState = state;
    }

    emit deviceStateChanged(state);
    LOG_INFO(QString("设备状态已更改为: %1").arg(static_cast<int>(state)));
}

double DeviceModel::getUsbSpeed() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_usbSpeed;
}

void DeviceModel::setUsbSpeed(double speed)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_usbSpeed = speed;
    }

    emit transferStatsUpdated(m_usbSpeed, m_transferredBytes);
}

uint64_t DeviceModel::getTransferredBytes() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_transferredBytes;
}

void DeviceModel::setTransferredBytes(uint64_t bytes)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_transferredBytes = bytes;
    }

    emit transferStatsUpdated(m_usbSpeed, m_transferredBytes);
}

QString DeviceModel::getErrorMessage() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_errorMessage;
}

void DeviceModel::setErrorMessage(const QString& message)
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_errorMessage = message;
    }

    if (!message.isEmpty()) {
        emit deviceError(message);
        LOG_ERROR(QString("设备错误: %1").arg(message));
    }
}

bool DeviceModel::validateImageParameters(QString& errorMessage) const
{
    QMutexLocker locker(&m_dataMutex);

    // 验证宽度
    if (m_imageWidth == 0 || m_imageWidth > 4096) {
        errorMessage = LocalQTCompat::fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值");
        return false;
    }

    // 验证高度
    if (m_imageHeight == 0 || m_imageHeight > 4096) {
        errorMessage = LocalQTCompat::fromLocal8Bit("无效的图像高度，请输入1-4096之间的值");
        return false;
    }

    // 验证捕获类型（有效值为0x38、0x39、0x3A）
    if (m_captureType != 0x38 && m_captureType != 0x39 && m_captureType != 0x3A) {
        errorMessage = LocalQTCompat::fromLocal8Bit("无效的图像捕获类型");
        return false;
    }

    return true;
}

bool DeviceModel::saveConfigToSettings()
{
    try {
        QSettings settings("FX3Tool", "DeviceConfig");
        QMutexLocker locker(&m_dataMutex);

        // 保存图像参数
        settings.setValue("imageWidth", m_imageWidth);
        settings.setValue("imageHeight", m_imageHeight);
        settings.setValue("captureType", m_captureType);

        LOG_INFO("设备配置已保存到系统设置");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存设备配置异常: %1").arg(e.what()));
        return false;
    }
}

bool DeviceModel::loadConfigFromSettings()
{
    try {
        QSettings settings("FX3Tool", "DeviceConfig");
        QMutexLocker locker(&m_dataMutex);

        // 加载图像参数
        m_imageWidth = settings.value("imageWidth", 1920).toUInt();
        m_imageHeight = settings.value("imageHeight", 1080).toUInt();
        m_captureType = settings.value("captureType", 0x39).toUInt();

        LOG_INFO("设备配置已从系统设置加载");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载设备配置异常: %1").arg(e.what()));
        resetToDefault();
        return false;
    }
}

void DeviceModel::resetToDefault()
{
    {
        QMutexLocker locker(&m_dataMutex);
        m_imageWidth = 1920;
        m_imageHeight = 1080;
        m_captureType = 0x39;  // RAW10
        m_deviceState = DeviceState::DEV_DISCONNECTED;
        m_usbSpeed = 0.0;
        m_transferredBytes = 0;
        m_errorMessage.clear();
    }

    emit imageParametersChanged(m_imageWidth, m_imageHeight, m_captureType);
    emit deviceStateChanged(m_deviceState);
    emit transferStatsUpdated(m_usbSpeed, m_transferredBytes);

    LOG_INFO("设备模型已重置为默认值");
}