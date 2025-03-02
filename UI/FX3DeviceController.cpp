// FX3DeviceController.cpp
#include "FX3DeviceController.h"
#include "FX3ToolMainWin.h"
#include "Logger.h"

FX3DeviceController::FX3DeviceController(FX3ToolMainWin* mainWindow, FX3DeviceManager* deviceManager)
    : m_mainWindow(mainWindow)
    , m_deviceManager(deviceManager)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备控制器已初始化"));
}

void FX3DeviceController::startTransfer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始传输"));

    // 验证参数
    uint16_t width;
    uint16_t height;
    uint8_t capType;

    if (!validateImageParameters(width, height, capType)) {
        return;
    }

    // 调用设备管理器启动传输
    if (m_deviceManager) {
        m_deviceManager->startTransfer(width, height, capType);
    }
}

void FX3DeviceController::stopTransfer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止传输"));

    // 调用设备管理器停止传输
    if (m_deviceManager) {
        m_deviceManager->stopTransfer();
    }
}

void FX3DeviceController::resetDevice()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置设备"));

    // 调用设备管理器重置设备
    if (m_deviceManager) {
        m_deviceManager->resetDevice();
    }
}

bool FX3DeviceController::validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType)
{
    bool conversionOk = false;

    // 获取主窗口UI元素
    QLineEdit* widthEdit = m_mainWindow->findChild<QLineEdit*>("imageWIdth");
    QLineEdit* heightEdit = m_mainWindow->findChild<QLineEdit*>("imageHeight");
    QComboBox* typeCombo = m_mainWindow->findChild<QComboBox*>("imageType");

    if (!widthEdit || !heightEdit || !typeCombo) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法获取图像参数控件"));
        return false;
    }

    // 获取宽度
    QString widthText = widthEdit->text();
    if (widthText.contains("Width")) {
        widthText = widthText.remove("Width").trimmed();
    }
    width = widthText.toUInt(&conversionOk);
    if (!conversionOk || width == 0 || width > 4096) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像宽度"));
        QMessageBox::warning(m_mainWindow, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值"));
        return false;
    }

    // 获取高度
    QString heightText = heightEdit->text();
    if (heightText.contains("Height")) {
        heightText = heightText.remove("Height").trimmed();
    }
    height = heightText.toUInt(&conversionOk);
    if (!conversionOk || height == 0 || height > 4096) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像高度"));
        QMessageBox::warning(m_mainWindow, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("无效的图像高度，请输入1-4096之间的值"));
        return false;
    }

    // 获取采集类型
    capType = 0x39; // 默认为RAW10
    int typeIndex = typeCombo->currentIndex();
    switch (typeIndex) {
    case 0: capType = 0x38; break; // RAW8
    case 1: capType = 0x39; break; // RAW10
    case 2: capType = 0x3A; break; // RAW12
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("图像参数验证通过 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    return true;
}