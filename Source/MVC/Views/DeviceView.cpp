// Source/MVC/Views/DeviceView.cpp
#include "DeviceView.h"
#include "Logger.h"

DeviceView::DeviceView(QObject* parent)
    : QObject(parent)
{
    LOG_INFO("设备视图已创建");
}

DeviceView::~DeviceView()
{
    LOG_INFO("设备视图已销毁");
}

void DeviceView::initUIComponents(
    QLineEdit* widthEdit,
    QLineEdit* heightEdit,
    QComboBox* typeCombo,
    QLabel* usbSpeedLabel,
    QLabel* usbStatusLabel,
    QLabel* transferStatusLabel,
    QLabel* transferRateLabel,
    QLabel* totalBytesLabel,
    QLabel* totalTimeLabel,
    QPushButton* startButton,
    QPushButton* stopButton,
    QPushButton* resetButton
    )
{
    m_widthEdit = widthEdit;
    m_heightEdit = heightEdit;
    m_typeCombo = typeCombo;

    m_usbSpeedLabel = usbSpeedLabel;
    m_usbStatusLabel = usbStatusLabel;
    m_transferStatusLabel = transferStatusLabel;
    m_transferRateLabel = transferRateLabel;
    m_totalBytesLabel = totalBytesLabel;
    m_totalTimeLabel = totalTimeLabel;

    m_startButton = startButton;
    m_stopButton = stopButton;
    m_resetButton = resetButton;

    // 设置父窗口，用于显示对话框
    if (widthEdit) {
        m_parentWidget = widthEdit->window();
    }

    connectSignals();
    LOG_INFO("设备视图UI组件已初始化");
}

void DeviceView::connectSignals()
{
    // 检查UI组件是否已初始化
    if (!m_widthEdit || !m_heightEdit || !m_typeCombo ||
        !m_usbSpeedLabel ||
        !m_usbStatusLabel ||
        !m_transferStatusLabel ||
        !m_transferRateLabel ||
        !m_totalBytesLabel ||
        !m_totalTimeLabel ||
        !m_startButton || !m_stopButton || !m_resetButton) {
        LOG_ERROR("无法连接信号：UI组件未初始化");
        return;
    }

    // 连接按钮信号
    connect(m_startButton, &QPushButton::clicked, this, &DeviceView::onStartButtonClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &DeviceView::onStopButtonClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &DeviceView::onResetButtonClicked);

    LOG_INFO("设备视图信号已连接");
}

uint16_t DeviceView::getImageWidth(bool* ok) const
{
    if (!m_widthEdit) {
        if (ok) *ok = false;
        return 0;
    }

    QString widthText = m_widthEdit->text();
    if (widthText.contains("Width")) {
        widthText = widthText.remove("Width").trimmed();
    }
    return widthText.toUInt(ok);
}

uint16_t DeviceView::getImageHeight(bool* ok) const
{
    if (!m_heightEdit) {
        if (ok) *ok = false;
        return 0;
    }

    QString heightText = m_heightEdit->text();
    if (heightText.contains("Height")) {
        heightText = heightText.remove("Height").trimmed();
    }
    return heightText.toUInt(ok);
}

uint8_t DeviceView::getCaptureType() const
{
    if (!m_typeCombo) {
        return 0x39; // 默认RAW10
    }

    int typeIndex = m_typeCombo->currentIndex();
    switch (typeIndex) {
    case 0: return 0x38; // RAW8
    case 1: return 0x39; // RAW10
    case 2: return 0x3A; // RAW12
    default: return 0x39; // 默认RAW10
    }
}

void DeviceView::showErrorMessage(const QString& message)
{
    if (!m_parentWidget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法显示错误消息（缺少父窗口）: %1").arg(message));
        return;
    }

    QMessageBox::critical(m_parentWidget,
        LocalQTCompat::fromLocal8Bit("错误"),
        message);
}

bool DeviceView::showConfirmDialog(const QString& title, const QString& message)
{
    if (!m_parentWidget) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("无法显示确认对话框（缺少父窗口）: %1").arg(message));
        return false;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_parentWidget, title, message,
        QMessageBox::Yes | QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void DeviceView::onStartButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图开始传输按钮点击"));
    emit signal_Dev_V_startTransferRequested();
}

void DeviceView::onStopButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图停止传输按钮点击"));
    emit signal_Dev_V_stopTransferRequested();
}

void DeviceView::onResetButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图重置设备按钮点击"));
    emit signal_Dev_V_resetDeviceRequested();
}

void DeviceView::onWidthTextChanged()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图视频宽度变化"));
    emit signal_Dev_V_imageParametersChanged();
}

void DeviceView::onHeightTextChanged()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图视频高度变化"));
    emit signal_Dev_V_imageParametersChanged();
}

void DeviceView::onCaptureTypeChanged()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图视频格式变化"));
    emit signal_Dev_V_imageParametersChanged();
}

void DeviceView::updateButtonStates(DeviceState deviceState)
{
    if (!m_startButton || !m_stopButton || !m_resetButton) {
        return;
    }

    bool isConnected = (deviceState != DeviceState::DEV_DISCONNECTED);
    bool isTransferring = (deviceState == DeviceState::DEV_TRANSFERRING);
    bool isError = (deviceState == DeviceState::DEV_ERROR);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备视图更新按钮状态, 连接: %1, 传输: %2, 错误: %3")
        .arg(isConnected).arg(isTransferring).arg(isError));

    // 设置按钮启用状态
    m_startButton->setEnabled(isConnected && !isTransferring && !isError);
    m_stopButton->setEnabled(isTransferring);
    m_resetButton->setEnabled(isConnected && !isTransferring);
}

QString DeviceView::getStateText(DeviceState state) const
{
    switch (state) {
    case DeviceState::DEV_DISCONNECTED:
        return LocalQTCompat::fromLocal8Bit("未连接");
    case DeviceState::DEV_CONNECTED:
        return LocalQTCompat::fromLocal8Bit("已连接");
    case DeviceState::DEV_TRANSFERRING:
        return LocalQTCompat::fromLocal8Bit("传输中");
    case DeviceState::DEV_ERROR:
        return LocalQTCompat::fromLocal8Bit("错误");
    default:
        return LocalQTCompat::fromLocal8Bit("未知");
    }
}