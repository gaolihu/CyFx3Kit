﻿// Source/MVC/Controllers/DeviceController.cpp
#include "DeviceController.h"
#include "Logger.h"

DeviceController::DeviceController(IDeviceView* deviceView, QObject* parent)
    : QObject(parent)
    , m_deviceModel(DeviceModel::getInstance())
    , m_deviceView(deviceView)
    , m_initialized(false)
{
    LOG_INFO("设备控制器已创建");
}

DeviceController::~DeviceController()
{
    LOG_INFO("设备控制器销毁中");
    prepareForShutdown();
    LOG_INFO("设备控制器已销毁");
}

bool DeviceController::initialize(HWND windowHandle)
{
    LOG_INFO("初始化设备控制器");

    if (m_initialized) {
        LOG_WARN("设备控制器已初始化");
        return true;
    }

    try {
        // 创建设备管理器
        m_deviceManager = std::make_unique<FX3DeviceManager>();
        if (!m_deviceManager) {
            LOG_ERROR("创建设备管理器失败");
            return false;
        }

        // 初始化设备管理器
        if (!m_deviceManager->initializeDeviceAndManager(windowHandle)) {
            LOG_ERROR("初始化设备管理器失败");
            return false;
        }

        // 初始化连接
        initializeConnections();

        // 从模型加载初始参数到视图
        if (m_deviceView) {
            m_deviceView->updateImageWidth(m_deviceModel->getImageWidth());
            m_deviceView->updateImageHeight(m_deviceModel->getImageHeight());
            m_deviceView->updateCaptureType(m_deviceModel->getCaptureType());
            m_deviceView->updateDeviceState(m_deviceModel->getDeviceState());
        }

        m_initialized = true;
        LOG_INFO("设备控制器初始化成功");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("设备控制器初始化异常: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR("设备控制器初始化未知异常");
        return false;
    }
}

void DeviceController::initializeConnections()
{
    LOG_INFO("初始化设备控制器连接");

    // 连接视图信号到控制器槽
    if (m_deviceView) {
        // 视图到控制器的信号连接
        connect(dynamic_cast<QObject*>(m_deviceView), SIGNAL(startTransferRequested()),
            this, SLOT(handleStartTransferRequest()));
        connect(dynamic_cast<QObject*>(m_deviceView), SIGNAL(stopTransferRequested()),
            this, SLOT(handleStopTransferRequest()));
        connect(dynamic_cast<QObject*>(m_deviceView), SIGNAL(resetDeviceRequested()),
            this, SLOT(handleResetDeviceRequest()));
        connect(dynamic_cast<QObject*>(m_deviceView), SIGNAL(imageParametersChanged()),
            this, SLOT(handleImageParametersChanged()));
    }

    // 连接设备管理器信号到控制器槽
    if (m_deviceManager) {
        // 设备管理器到控制器的信号连接
        connect(m_deviceManager.get(), &FX3DeviceManager::deviceConnectionChanged,
            this, &DeviceController::handleDeviceConnectionChanged);
        connect(m_deviceManager.get(), &FX3DeviceManager::transferStateChanged,
            this, &DeviceController::handleTransferStateChanged);
        connect(m_deviceManager.get(), &FX3DeviceManager::transferStatsUpdated,
            this, &DeviceController::handleTransferStatsUpdated);
        connect(m_deviceManager.get(), &FX3DeviceManager::usbSpeedUpdated,
            this, &DeviceController::handleUsbSpeedUpdated);
        connect(m_deviceManager.get(), &FX3DeviceManager::deviceError,
            this, &DeviceController::handleDeviceError);
        connect(m_deviceManager.get(), &FX3DeviceManager::dataPacketAvailable,
            this, &DeviceController::dataPacketAvailable);

        // 控制器转发一些信号
        connect(m_deviceManager.get(), &FX3DeviceManager::deviceConnectionChanged,
            this, &DeviceController::deviceConnectionChanged);
        connect(m_deviceManager.get(), &FX3DeviceManager::transferStateChanged,
            this, &DeviceController::transferStateChanged);
        connect(m_deviceManager.get(), &FX3DeviceManager::transferStatsUpdated,
            this, &DeviceController::transferStatsUpdated);
        connect(m_deviceManager.get(), &FX3DeviceManager::usbSpeedUpdated,
            this, &DeviceController::usbSpeedUpdated);
        connect(m_deviceManager.get(), &FX3DeviceManager::deviceError,
            this, &DeviceController::deviceError);
    }

    // 连接模型信号到控制器槽
    connect(m_deviceModel, &DeviceModel::deviceStateChanged,
        this, [this](DeviceState state) {
            // 更新视图中的设备状态
            if (m_deviceView) {
                m_deviceView->updateDeviceState(state);
            }
        });

    LOG_INFO("设备控制器连接初始化完成");
}

bool DeviceController::checkAndOpenDevice()
{
    LOG_INFO("检查并打开设备");
    if (!m_initialized || !m_deviceManager) {
        LOG_ERROR("设备控制器未初始化");
        return false;
    }

    return m_deviceManager->checkAndOpenDevice();
}

bool DeviceController::resetDevice()
{
    LOG_INFO("重置设备");
    if (!m_initialized || !m_deviceManager) {
        LOG_ERROR("设备控制器未初始化");
        return false;
    }

    return m_deviceManager->resetDevice();
}

bool DeviceController::setCommandDirectory(const QString& dirPath)
{
    LOG_INFO(QString("设置命令目录: %1").arg(dirPath));
    if (!m_initialized || !m_deviceManager) {
        LOG_ERROR("设备控制器未初始化");
        return false;
    }

    return m_deviceManager->loadCommandFiles(dirPath);
}

bool DeviceController::startTransfer()
{
    LOG_INFO("启动数据传输");
    if (!m_initialized || !m_deviceManager) {
        LOG_ERROR("设备控制器未初始化");
        return false;
    }

    // 获取并验证图像参数
    bool widthOk = false, heightOk = false;
    uint16_t width = 0, height = 0;
    uint8_t captureType = 0;

    if (m_deviceView) {
        width = m_deviceView->getImageWidth(&widthOk);
        height = m_deviceView->getImageHeight(&heightOk);
        captureType = m_deviceView->getCaptureType();
    }
    else {
        // 从模型获取参数
        width = m_deviceModel->getImageWidth();
        height = m_deviceModel->getImageHeight();
        captureType = m_deviceModel->getCaptureType();
        widthOk = heightOk = true;
    }

    if (!widthOk || !heightOk) {
        if (m_deviceView) {
            m_deviceView->showErrorMessage(LocalQTCompat::fromLocal8Bit("无效的图像参数，请检查宽度和高度"));
        }
        LOG_ERROR("无效的图像参数值");
        return false;
    }

    QString errorMessage;
    if (!validateImageParameters(width, height, captureType, errorMessage)) {
        if (m_deviceView) {
            m_deviceView->showErrorMessage(errorMessage);
        }
        LOG_ERROR(errorMessage);
        return false;
    }

    // 更新模型中的参数
    m_deviceModel->setImageWidth(width);
    m_deviceModel->setImageHeight(height);
    m_deviceModel->setCaptureType(captureType);

    // 启动传输
    return m_deviceManager->startTransfer(width, height, captureType);
}

bool DeviceController::stopTransfer()
{
    LOG_INFO("停止数据传输");
    if (!m_initialized || !m_deviceManager) {
        LOG_ERROR("设备控制器未初始化");
        return false;
    }

    return m_deviceManager->stopTransfer();
}

bool DeviceController::updateImageParameters()
{
    LOG_INFO("更新图像参数");
    if (!m_deviceView) {
        LOG_ERROR("设备视图未初始化");
        return false;
    }

    bool widthOk = false, heightOk = false;
    uint16_t width = m_deviceView->getImageWidth(&widthOk);
    uint16_t height = m_deviceView->getImageHeight(&heightOk);
    uint8_t captureType = m_deviceView->getCaptureType();

    if (!widthOk || !heightOk) {
        LOG_ERROR("无效的图像参数值");
        return false;
    }

    QString errorMessage;
    if (!validateImageParameters(width, height, captureType, errorMessage)) {
        m_deviceView->showErrorMessage(errorMessage);
        LOG_ERROR(errorMessage);
        return false;
    }

    // 更新模型
    m_deviceModel->setImageWidth(width);
    m_deviceModel->setImageHeight(height);
    m_deviceModel->setCaptureType(captureType);

    LOG_INFO(QString("图像参数已更新 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(captureType, 2, 16, QChar('0')));
    return true;
}

void DeviceController::getImageParameters(uint16_t& width, uint16_t& height, uint8_t& captureType)
{
    width = m_deviceModel->getImageWidth();
    height = m_deviceModel->getImageHeight();
    captureType = m_deviceModel->getCaptureType();
}

void DeviceController::setImageParameters(uint16_t width, uint16_t height, uint8_t captureType)
{
    QString errorMessage;
    if (!validateImageParameters(width, height, captureType, errorMessage)) {
        LOG_ERROR(errorMessage);
        return;
    }

    // 更新模型
    m_deviceModel->setImageWidth(width);
    m_deviceModel->setImageHeight(height);
    m_deviceModel->setCaptureType(captureType);

    // 更新视图
    if (m_deviceView) {
        m_deviceView->updateImageWidth(width);
        m_deviceView->updateImageHeight(height);
        m_deviceView->updateCaptureType(captureType);
    }

    LOG_INFO(QString("图像参数已设置 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(captureType, 2, 16, QChar('0')));
}

void DeviceController::prepareForShutdown()
{
    LOG_INFO("设备控制器准备关闭");
    if (m_deviceManager) {
        m_deviceManager->prepareForShutdown();
    }
    m_initialized = false;
    LOG_INFO("设备控制器准备关闭完成");
}

bool DeviceController::isDeviceConnected() const
{
    return m_deviceManager && m_deviceManager->isDeviceConnected();
}

bool DeviceController::isTransferring() const
{
    return m_deviceManager && m_deviceManager->isTransferring();
}

bool DeviceController::validateImageParameters(uint16_t width, uint16_t height, uint8_t captureType, QString& errorMessage)
{
    // 验证宽度
    if (width == 0 || width > 4096) {
        errorMessage = LocalQTCompat::fromLocal8Bit("无效的图像宽度，请输入1-4096之间的值");
        return false;
    }

    // 验证高度
    if (height == 0 || height > 4096) {
        errorMessage = LocalQTCompat::fromLocal8Bit("无效的图像高度，请输入1-4096之间的值");
        return false;
    }

    // 验证捕获类型（有效值为0x38、0x39、0x3A）
    if (captureType != 0x38 && captureType != 0x39 && captureType != 0x3A) {
        errorMessage = LocalQTCompat::fromLocal8Bit("无效的图像捕获类型");
        return false;
    }

    return true;
}

void DeviceController::updateViewState()
{
    if (!m_deviceView) {
        return;
    }

    // 获取当前设备状态
    DeviceState currentState = m_deviceModel->getDeviceState();

    // 更新视图状态显示
    m_deviceView->updateDeviceState(currentState);
}

// Slot handlers
void DeviceController::handleStartTransferRequest()
{
    LOG_INFO("处理开始传输请求");
    startTransfer();
}

void DeviceController::handleStopTransferRequest()
{
    LOG_INFO("处理停止传输请求");
    stopTransfer();
}

void DeviceController::handleResetDeviceRequest()
{
    LOG_INFO("处理重置设备请求");
    resetDevice();
}

void DeviceController::handleImageParametersChanged()
{
    LOG_INFO("处理图像参数变更");
    updateImageParameters();
}

void DeviceController::handleDeviceConnectionChanged(bool connected)
{
    LOG_INFO(QString("处理设备连接状态变更: %1").arg(connected ? "已连接" : "已断开"));

    // 更新模型中的设备状态
    DeviceState newState = connected ? DeviceState::DEV_CONNECTED : DeviceState::DEV_DISCONNECTED;
    m_deviceModel->setDeviceState(newState);

    // 更新视图状态
    updateViewState();
}

void DeviceController::handleTransferStateChanged(bool transferring)
{
    LOG_INFO(QString("处理传输状态变更: %1").arg(transferring ? "传输中" : "已停止"));

    // 更新模型中的设备状态
    DeviceState newState = transferring ? DeviceState::DEV_TRANSFERRING : DeviceState::DEV_CONNECTED;
    if (m_deviceModel->getDeviceState() != DeviceState::DEV_DISCONNECTED) {
        m_deviceModel->setDeviceState(newState);
    }

    // 更新视图状态
    updateViewState();
}

void DeviceController::handleTransferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint32_t errorCount)
{
    // 更新视图中的传输统计信息
    if (m_deviceView) {
        m_deviceView->updateUsbSpeed(transferRate);
        m_deviceView->updateTransferredBytes(bytesTransferred);
    }
}

void DeviceController::handleUsbSpeedUpdated(const QString& speedDesc, bool isUSB3)
{
    LOG_INFO(QString("USB速度更新: %1, 是否USB3: %2").arg(speedDesc).arg(isUSB3));
    // 可能需要更新UI或模型中的USB速度信息
}

void DeviceController::handleDeviceError(const QString& title, const QString& message)
{
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));

    // 更新模型中的设备状态和错误信息
    m_deviceModel->setDeviceState(DeviceState::DEV_ERROR);
    m_deviceModel->setErrorMessage(message);

    // 在视图中显示错误信息
    if (m_deviceView) {
        m_deviceView->showErrorMessage(message);
        m_deviceView->updateDeviceState(DeviceState::DEV_ERROR);
    }
}