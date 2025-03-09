// Source/MVC/Views/DeviceView.cpp
#include "DeviceView.h"
#include "Logger.h"

DeviceView::DeviceView(QObject* parent)
    : QObject(parent)
    , m_widthEdit(nullptr)
    , m_heightEdit(nullptr)
    , m_typeCombo(nullptr)
    , m_speedLabel(nullptr)
    , m_bytesLabel(nullptr)
    , m_stateLabel(nullptr)
    , m_startButton(nullptr)
    , m_stopButton(nullptr)
    , m_resetButton(nullptr)
    , m_parentWidget(nullptr)
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
    QLabel* speedLabel,
    QLabel* bytesLabel,
    QLabel* stateLabel)
{
    m_widthEdit = widthEdit;
    m_heightEdit = heightEdit;
    m_typeCombo = typeCombo;
    m_speedLabel = speedLabel;
    m_bytesLabel = bytesLabel;
    m_stateLabel = stateLabel;

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
        !m_startButton || !m_stopButton || !m_resetButton) {
        LOG_ERROR("无法连接信号：UI组件未初始化");
        return;
    }

    // 连接按钮信号
    connect(m_startButton, &QPushButton::clicked, this, &DeviceView::onStartButtonClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &DeviceView::onStopButtonClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &DeviceView::onResetButtonClicked);

    // 连接参数变更信号
    connect(m_widthEdit, &QLineEdit::textChanged, this, &DeviceView::onWidthTextChanged);
    connect(m_heightEdit, &QLineEdit::textChanged, this, &DeviceView::onHeightTextChanged);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &DeviceView::onCaptureTypeChanged);

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

void DeviceView::updateImageWidth(uint16_t width)
{
    if (m_widthEdit) {
        m_widthEdit->setText(QString::number(width));
    }
}

void DeviceView::updateImageHeight(uint16_t height)
{
    if (m_heightEdit) {
        m_heightEdit->setText(QString::number(height));
    }
}

void DeviceView::updateCaptureType(uint8_t captureType)
{
    if (!m_typeCombo) {
        return;
    }

    int index;
    switch (captureType) {
    case 0x38: index = 0; break; // RAW8
    case 0x39: index = 1; break; // RAW10
    case 0x3A: index = 2; break; // RAW12
    default: index = 1; // 默认RAW10
    }

    m_typeCombo->setCurrentIndex(index);
}

void DeviceView::updateUsbSpeed(double speed)
{
    if (m_speedLabel) {
        m_speedLabel->setText(QString("%1 MB/s").arg(speed, 0, 'f', 2));
    }
}

void DeviceView::updateTransferredBytes(uint64_t bytes)
{
    if (!m_bytesLabel) {
        return;
    }

    // 格式化显示
    QString sizeText;
    if (bytes < 1024) {
        sizeText = QString("%1 B").arg(bytes);
    }
    else if (bytes < 1024 * 1024) {
        sizeText = QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 2);
    }
    else if (bytes < 1024 * 1024 * 1024) {
        sizeText = QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
    else {
        sizeText = QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }

    m_bytesLabel->setText(sizeText);
}

void DeviceView::updateDeviceState(DeviceState state)
{
    // 更新状态标签
    if (m_stateLabel) {
        m_stateLabel->setText(getStateText(state));
    }

    // 更新按钮状态
    updateButtonStates(state);
}

void DeviceView::showErrorMessage(const QString& message)
{
    if (!m_parentWidget) {
        LOG_ERROR(QString("无法显示错误消息（缺少父窗口）: %1").arg(message));
        return;
    }

    QMessageBox::critical(m_parentWidget,
        LocalQTCompat::fromLocal8Bit("错误"),
        message);
}

bool DeviceView::showConfirmDialog(const QString& title, const QString& message)
{
    if (!m_parentWidget) {
        LOG_WARN(QString("无法显示确认对话框（缺少父窗口）: %1").arg(message));
        return false;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_parentWidget, title, message,
        QMessageBox::Yes | QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void DeviceView::onStartButtonClicked()
{
    LOG_INFO("开始传输按钮点击");
    emit startTransferRequested();
}

void DeviceView::onStopButtonClicked()
{
    LOG_INFO("停止传输按钮点击");
    emit stopTransferRequested();
}

void DeviceView::onResetButtonClicked()
{
    LOG_INFO("重置设备按钮点击");
    emit resetDeviceRequested();
}

void DeviceView::onWidthTextChanged()
{
    emit imageParametersChanged();
}

void DeviceView::onHeightTextChanged()
{
    emit imageParametersChanged();
}

void DeviceView::onCaptureTypeChanged()
{
    emit imageParametersChanged();
}

void DeviceView::updateButtonStates(DeviceState deviceState)
{
    if (!m_startButton || !m_stopButton || !m_resetButton) {
        return;
    }

    bool isConnected = (deviceState != DeviceState::DEV_DISCONNECTED);
    bool isTransferring = (deviceState == DeviceState::DEV_TRANSFERRING);
    bool isError = (deviceState == DeviceState::DEV_ERROR);

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