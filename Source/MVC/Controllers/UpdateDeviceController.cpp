// Source/MVC/Controllers/UpdateDeviceController.cpp
#include "UpdateDeviceController.h"
#include "Logger.h"

UpdateDeviceController::UpdateDeviceController(UpdateDeviceView* view, QObject* parent)
    : QObject(parent)
    , m_model(UpdateDeviceModel::getInstance())
    , m_view(view)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级控制器已创建"));
}

UpdateDeviceController::~UpdateDeviceController()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级控制器已销毁"));
}

bool UpdateDeviceController::initialize()
{
    // 连接模型和视图信号
    connectSignals();

    // 初始化视图状态
    updateViewState();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级控制器已初始化"));
    return true;
}

void UpdateDeviceController::connectSignals()
{
    // 连接视图信号到控制器槽
    connect(m_view, &UpdateDeviceView::socFileSelectClicked, this, &UpdateDeviceController::handleSOCFileSelect);
    connect(m_view, &UpdateDeviceView::phyFileSelectClicked, this, &UpdateDeviceController::handlePHYFileSelect);
    connect(m_view, &UpdateDeviceView::socUpdateClicked, this, &UpdateDeviceController::handleSOCUpdate);
    connect(m_view, &UpdateDeviceView::phyUpdateClicked, this, &UpdateDeviceController::handlePHYUpdate);

    // 连接模型信号到控制器槽
    connect(m_model, &UpdateDeviceModel::statusChanged, this, &UpdateDeviceController::handleModelStatusChanged);
    connect(m_model, &UpdateDeviceModel::progressChanged, this, &UpdateDeviceController::handleModelProgressChanged);
    connect(m_model, &UpdateDeviceModel::updateCompleted, this, &UpdateDeviceController::handleModelUpdateCompleted);
    connect(m_model, &UpdateDeviceModel::filePathChanged, this, &UpdateDeviceController::handleModelFilePathChanged);

    // 转发模型的升级完成信号
    connect(m_model, &UpdateDeviceModel::updateCompleted, this, &UpdateDeviceController::updateCompleted);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级控制器信号已连接"));
}

void UpdateDeviceController::updateViewState()
{
    // 获取模型当前状态
    UpdateStatus status = m_model->getStatus();
    DeviceType currentDevice = m_model->getCurrentDeviceType();
    bool isUpdating = (status == UpdateStatus::UPDATING);

    // 更新视图状态
    m_view->updateUIState(isUpdating, currentDevice);

    // 更新文件路径
    m_view->updateSOCFilePath(m_model->getSOCFilePath());
    m_view->updatePHYFilePath(m_model->getPHYFilePath());

    // 更新状态消息
    m_view->updateStatusMessage(m_model->getStatusMessage());

    LOG_INFO(LocalQTCompat::fromLocal8Bit("视图状态已更新"));
}

void UpdateDeviceController::handleSOCFileSelect()
{
    // 显示文件选择对话框
    QString filePath = m_view->showFileSelectDialog(DeviceType::SOC);

    // 如果用户取消，返回
    if (filePath.isEmpty()) {
        return;
    }

    // 验证文件
    QString errorMessage;
    if (!m_model->validateFile(filePath, "SOC", errorMessage)) {
        m_view->showMessageDialog(
            LocalQTCompat::fromLocal8Bit("文件错误"),
            errorMessage,
            true // 显示为错误消息
        );
        return;
    }

    // 设置模型SOC文件路径
    m_model->setSOCFilePath(filePath);

    LOG_INFO(QString("SOC文件已选择: %1").arg(filePath));
}

void UpdateDeviceController::handlePHYFileSelect()
{
    // 显示文件选择对话框
    QString filePath = m_view->showFileSelectDialog(DeviceType::PHY);

    // 如果用户取消，返回
    if (filePath.isEmpty()) {
        return;
    }

    // 验证文件
    QString errorMessage;
    if (!m_model->validateFile(filePath, "ISO", errorMessage)) {
        m_view->showMessageDialog(
            LocalQTCompat::fromLocal8Bit("文件错误"),
            errorMessage,
            true // 显示为错误消息
        );
        return;
    }

    // 设置模型PHY文件路径
    m_model->setPHYFilePath(filePath);

    LOG_INFO(QString("PHY文件已选择: %1").arg(filePath));
}

void UpdateDeviceController::handleSOCUpdate()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理SOC升级请求"));

    // 如果当前正在升级，则返回
    if (m_model->getStatus() == UpdateStatus::UPDATING) {
        return;
    }

    // 验证SOC文件
    QString errorMessage;
    if (!m_model->validateFile(m_model->getSOCFilePath(), "SOC", errorMessage)) {
        m_view->showMessageDialog(
            LocalQTCompat::fromLocal8Bit("文件错误"),
            errorMessage,
            true // 显示为错误消息
        );
        return;
    }

    // 显示确认对话框
    bool confirmed = m_view->showConfirmDialog(
        LocalQTCompat::fromLocal8Bit("确定要开始SOC固件升级吗？\n升级过程中请勿断开设备电源！")
    );

    if (!confirmed) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("用户取消SOC升级"));
        return;
    }

    // 开始升级
    if (m_model->startUpdate(DeviceType::SOC)) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("SOC升级已启动"));
    }
    else {
        m_view->showMessageDialog(
            LocalQTCompat::fromLocal8Bit("升级错误"),
            LocalQTCompat::fromLocal8Bit("无法启动SOC升级"),
            true // 显示为错误消息
        );
    }
}

void UpdateDeviceController::handlePHYUpdate()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理PHY升级请求"));

    // 如果当前正在升级，则返回
    if (m_model->getStatus() == UpdateStatus::UPDATING) {
        return;
    }

    // 验证PHY文件
    QString errorMessage;
    if (!m_model->validateFile(m_model->getPHYFilePath(), "ISO", errorMessage)) {
        m_view->showMessageDialog(
            LocalQTCompat::fromLocal8Bit("文件错误"),
            errorMessage,
            true // 显示为错误消息
        );
        return;
    }

    // 显示确认对话框
    bool confirmed = m_view->showConfirmDialog(
        LocalQTCompat::fromLocal8Bit("确定要开始PHY固件升级吗？\n升级过程中请勿断开设备电源！")
    );

    if (!confirmed) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("用户取消PHY升级"));
        return;
    }

    // 开始升级
    if (m_model->startUpdate(DeviceType::PHY)) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("PHY升级已启动"));
    }
    else {
        m_view->showMessageDialog(
            LocalQTCompat::fromLocal8Bit("升级错误"),
            LocalQTCompat::fromLocal8Bit("无法启动PHY升级"),
            true // 显示为错误消息
        );
    }
}

void UpdateDeviceController::handleModelStatusChanged(UpdateStatus status)
{
    // 根据状态更新视图
    updateViewState();

    LOG_INFO(QString("处理模型状态变更: %1").arg(static_cast<int>(status)));
}

void UpdateDeviceController::handleModelProgressChanged(int progress)
{
    // 根据当前设备类型更新进度条
    DeviceType currentDevice = m_model->getCurrentDeviceType();

    if (currentDevice == DeviceType::SOC) {
        m_view->updateSOCProgress(progress);
    }
    else {
        m_view->updatePHYProgress(progress);
    }

    LOG_INFO(QString("处理模型进度变更: %1%").arg(progress));
}

void UpdateDeviceController::handleModelUpdateCompleted(bool success, const QString& message)
{
    // 更新状态消息
    m_view->updateStatusMessage(message);

    // 更新视图状态
    updateViewState();

    // 显示完成消息
    QString title = success ?
        LocalQTCompat::fromLocal8Bit("升级成功") :
        LocalQTCompat::fromLocal8Bit("升级失败");

    m_view->showMessageDialog(title, message, !success);

    LOG_INFO(QString("处理模型升级完成: 成功=%1, 消息=%2")
        .arg(success ? "是" : "否")
        .arg(message));
}

void UpdateDeviceController::handleModelFilePathChanged(DeviceType deviceType, const QString& filePath)
{
    // 更新视图中的文件路径
    if (deviceType == DeviceType::SOC) {
        m_view->updateSOCFilePath(filePath);
    }
    else {
        m_view->updatePHYFilePath(filePath);
    }

    // 更新视图状态
    updateViewState();

    LOG_INFO(QString("处理模型文件路径变更: 设备类型=%1, 路径=%2")
        .arg(deviceType == DeviceType::SOC ? "SOC" : "PHY")
        .arg(filePath));
}