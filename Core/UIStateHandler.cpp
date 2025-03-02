#include "UIStateHandler.h"
#include "Logger.h"
#include <QMessageBox>

UIStateHandler::UIStateHandler(Ui::FX3ToolMainWinClass& ui, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_validUI(true)  // 默认假设UI有效
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器已创建"));

    // 检查关键UI元素是否存在
    if (!m_ui.usbStatusLabel || !m_ui.transferStatusLabel ||
        !m_ui.startButton || !m_ui.stopButton || !m_ui.resetButton) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("部分UI元素不可用，UI状态处理器将使用有限功能"));
        m_validUI = false;
    }
}

void UIStateHandler::onStateChanged(AppState newState, AppState oldState, const QString& reason)
{
    // 记录状态变化
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器：状态变更 %1 -> %2, 原因: %3")
        .arg(AppStateMachine::stateToString(oldState))
        .arg(AppStateMachine::stateToString(newState))
        .arg(reason));

    // 根据新状态更新UI
    updateButtonStates(newState);
    updateStatusLabels(newState);
}

void UIStateHandler::updateButtonStates(AppState state)
{
    bool enableStartButton = false;
    bool enableStopButton = false;
    bool enableResetButton = false;

    // 设置各种状态下的按钮启用/禁用
    switch (state) {
    case AppState::INITIALIZING:
        // 初始化中所有按钮禁用
        break;

    case AppState::DEVICE_ABSENT:
        // 设备未连接时所有操作按钮禁用
        break;

    case AppState::DEVICE_ERROR:
        // 设备错误时只能重置
        enableResetButton = true;
        break;

    case AppState::IDLE:
        // 空闲状态，可以开始和重置
        enableStartButton = true;
        enableResetButton = true;
        break;

    case AppState::COMMANDS_MISSING:
        // 命令文件未加载，可以重置
        enableResetButton = true;
        break;

    case AppState::CONFIGURED:
        // 已配置，可以开始和重置
        enableStartButton = true;
        enableResetButton = true;
        break;

    case AppState::STARTING:
        // 启动中，只能重置
        enableResetButton = true;
        break;

    case AppState::TRANSFERRING:
        // 传输中，可以停止和重置
        enableStopButton = true;
        enableResetButton = true;
        break;

    case AppState::STOPPING:
        // 停止中，不能进行任何操作
        break;

    case AppState::SHUTDOWN:
        // 关闭中，禁用所有操作
        break;
    }

    // 更新按钮状态
    updateButtons(enableStartButton, enableStopButton, enableResetButton);
}

void UIStateHandler::updateStatusLabels(AppState state)
{
    QString statusText;
    QString transferStatusText;

    // 设置各种状态下的文本
    switch (state) {
    case AppState::INITIALIZING:
        statusText = LocalQTCompat::fromLocal8Bit("初始化中");
        transferStatusText = LocalQTCompat::fromLocal8Bit("未开始");
        break;

    case AppState::DEVICE_ABSENT:
        statusText = LocalQTCompat::fromLocal8Bit("设备未连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("未连接");
        break;

    case AppState::DEVICE_ERROR:
        statusText = LocalQTCompat::fromLocal8Bit("设备错误");
        transferStatusText = LocalQTCompat::fromLocal8Bit("错误");
        break;

    case AppState::IDLE:
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("空闲");
        break;

    case AppState::COMMANDS_MISSING:
        statusText = LocalQTCompat::fromLocal8Bit("命令文件未加载");
        transferStatusText = LocalQTCompat::fromLocal8Bit("空闲");
        // 防御性检查：确保cmdStatusLabel存在再访问
        if (validUI() && m_ui.cmdStatusLabel) {
            //m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("命令文件未加载"));
            //m_ui.cmdStatusLabel->setStyleSheet("color: red;");
        }
        break;

    case AppState::CONFIGURED:
        statusText = LocalQTCompat::fromLocal8Bit("设备已配置");
        transferStatusText = LocalQTCompat::fromLocal8Bit("就绪");
        // 防御性检查：确保cmdStatusLabel存在再访问
        if (validUI() && m_ui.cmdStatusLabel) {
            m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("命令文件已加载"));
            m_ui.cmdStatusLabel->setStyleSheet("color: green;");
        }
        break;

    case AppState::STARTING:
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("启动中");
        break;

    case AppState::TRANSFERRING:
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("传输中");
        break;

    case AppState::STOPPING:
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("停止中");
        break;

    case AppState::SHUTDOWN:
        statusText = LocalQTCompat::fromLocal8Bit("关闭中");
        transferStatusText = LocalQTCompat::fromLocal8Bit("关闭中");
        break;

    default:
        statusText = LocalQTCompat::fromLocal8Bit("未知状态");
        transferStatusText = LocalQTCompat::fromLocal8Bit("未知");
        break;
    }

    updateStatusLabels(statusText, transferStatusText);
}

void UIStateHandler::updateTransferStats(double transferSpeed, uint64_t totalBytes, uint32_t totalTime)
{
    if (!validUI()) return;

    // 更新速度标签
    if (m_ui.speedLabel) {
        m_ui.speedLabel->setText(LocalQTCompat::fromLocal8Bit("%1 MB/s").arg(transferSpeed, 0, 'f', 2));
    }

    // 更新总字节标签
    if (m_ui.totalBytesLabel) {
        QString bytesText;
        if (totalBytes < 1024 * 1024) {
            bytesText = LocalQTCompat::fromLocal8Bit("%1 KB").arg(totalBytes / 1024.0, 0, 'f', 2);
        }
        else if (totalBytes < 1024 * 1024 * 1024) {
            bytesText = LocalQTCompat::fromLocal8Bit("%1 MB").arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2);
        }
        else {
            bytesText = LocalQTCompat::fromLocal8Bit("%1 GB").arg(totalBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
        }
        m_ui.totalBytesLabel->setText(bytesText);
    }

    // 更新总时间标签
    if (m_ui.totalTimeLabel) {
        int hours = totalTime / 3600;
        int minutes = (totalTime % 3600) / 60;
        int seconds = totalTime % 60;

        QString timeText;
        if (hours > 0) {
            timeText = LocalQTCompat::fromLocal8Bit("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        }
        else {
            timeText = LocalQTCompat::fromLocal8Bit("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        }
        m_ui.totalTimeLabel->setText(timeText);
    }
}

void UIStateHandler::updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3)
{
    if (!validUI()) return;

    if (m_ui.usbSpeedLabel) {
        m_ui.usbSpeedLabel->setText(speedDesc);

        // 可选：根据是否是USB3.0设置不同的样式
        if (isUSB3) {
            m_ui.usbSpeedLabel->setStyleSheet("color: blue;");
        }
        else {
            m_ui.usbSpeedLabel->setStyleSheet("");
        }
    }
}

void UIStateHandler::showErrorMessage(const QString& errorMsg, const QString& details)
{
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI错误: %1 - %2").arg(errorMsg).arg(details));

    // 处理错误显示
    QString fullMessage = errorMsg;
    if (!details.isEmpty()) {
        fullMessage += "\n\n" + details;
    }

    // 在UI线程中显示消息框
    QMetaObject::invokeMethod(QApplication::activeWindow(), [fullMessage]() {
        QMessageBox::critical(QApplication::activeWindow(),
            LocalQTCompat::fromLocal8Bit("错误"),
            fullMessage);
        }, Qt::QueuedConnection);
}

void UIStateHandler::prepareForClose()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器准备关闭"));
    m_validUI = false;  // 标记UI为无效，防止后续访问
}

bool UIStateHandler::validUI() const
{
    // 检查指向UI的指针是否有效
    return m_validUI;
}

void UIStateHandler::updateButtons(bool enableStart, bool enableStop, bool enableReset)
{
    if (!validUI()) return;

    // 防御性检查所有使用的UI元素
    if (m_ui.startButton)
        m_ui.startButton->setEnabled(enableStart);

    if (m_ui.stopButton)
        m_ui.stopButton->setEnabled(enableStop);

    if (m_ui.resetButton)
        m_ui.resetButton->setEnabled(enableReset);
}

void UIStateHandler::updateStatusLabels(const QString& statusText, const QString& transferStatusText)
{
    if (!validUI()) return;

    // 防御性检查所有使用的UI元素
    if (m_ui.usbStatusLabel)
        m_ui.usbStatusLabel->setText(statusText);

    if (m_ui.transferStatusLabel)
        m_ui.transferStatusLabel->setText(transferStatusText);
}