// Source/Core/MainUiStateManager.cpp

#include "MainUiStateManager.h"
#include "Logger.h"
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QStatusBar>
#include <QPushButton>
#include <QTextEdit>

MainUiStateManager::MainUiStateManager(Ui::FX3ToolMainWin& ui, QObject* parent)
    : QObject(parent)
    , m_ui(ui)
    , m_validUI(true)  // 默认假设UI有效
    , m_isTransferring(false)
    , m_isDeviceConnected(false)
    , m_totalElapsedTime(0)
    , m_bytesTransferred(0)
    , m_transferRate(0.0)
    , m_errorCount(0)
    , m_mainTabWidget(nullptr)
    , m_homeTabIndex(0)
    , m_channelTabIndex(-1)
    , m_dataAnalysisTabIndex(-1)
    , m_videoDisplayTabIndex(-1)
    , m_waveformTabIndex(-1)
    , m_fileSaveTabIndex(-1)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器已创建"));

    // 检查关键UI元素是否存在
    if (!m_ui.usbStatusLabel || !m_ui.transferStatusLabel ||
        !m_ui.actionStartTransfer || !m_ui.actionStopTransfer || !m_ui.actionResetDevice) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("部分UI元素不可用，UI状态处理器将使用有限功能"));
        m_validUI = false;
    }

    // 初始化传输计时器
    connect(&m_transferTimer, &QTimer::timeout, this, &MainUiStateManager::slot_updateTransferTime);
    m_transferTimer.setInterval(200);  // 200ms更新一次
}

MainUiStateManager::~MainUiStateManager()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器已销毁"));

    // 停止计时器
    if (m_transferTimer.isActive()) {
        m_transferTimer.stop();
    }
}

bool MainUiStateManager::initializeTabManagement(QTabWidget* mainTabWidget)
{
    if (!mainTabWidget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化Tab管理失败: mainTabWidget为空"));
        return false;
    }

    m_mainTabWidget = mainTabWidget;

    // 设置默认标签页为日志
    m_mainTabWidget->setCurrentIndex(0);

    // 连接标签关闭信号
    connect(m_mainTabWidget, &QTabWidget::tabCloseRequested,
        this, &MainUiStateManager::slot_onTabCloseRequested);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("Tab管理初始化成功"));
    return true;
}

void MainUiStateManager::addModuleToMainTab(QWidget* widget, const QString& tabName, int& tabIndex, const QIcon& icon)
{
    if (!m_mainTabWidget || !widget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("添加模块失败：标签控件或模块窗口为空"));
        return;
    }

    // 检查是否已添加
    if (tabIndex >= 0 && tabIndex < m_mainTabWidget->count()) {
        m_mainTabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("模块标签页已存在，切换到标签页: %1").arg(tabName));
        return;
    }

    // 添加新标签页
    tabIndex = icon.isNull() ?
        m_mainTabWidget->addTab(widget, tabName) :
        m_mainTabWidget->addTab(widget, icon, tabName);

    m_mainTabWidget->setCurrentIndex(tabIndex);

    // 设置关闭按钮，除了主页标签
    if (tabIndex != m_homeTabIndex) {
        m_mainTabWidget->setTabsClosable(true);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已添加模块标签页: %1，索引: %2").arg(tabName).arg(tabIndex));
}

void MainUiStateManager::showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示模块标签: %1, name: %2").arg(tabIndex).arg(tabName));
    if (!m_mainTabWidget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("标签控件为空，无法显示模块"));
        return;
    }

    // 如果标签页已存在
    if (tabIndex >= 0 && tabIndex < m_mainTabWidget->count()) {
        m_mainTabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("切换到模块标签页: %1").arg(tabName));
    }
    // 否则添加新标签页
    else {
        addModuleToMainTab(widget, tabName, tabIndex, icon);
    }
}

void MainUiStateManager::removeModuleTab(int& tabIndex)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("移除Tab: %1").arg(tabIndex));
    if (!m_mainTabWidget || tabIndex < 0 || tabIndex >= m_mainTabWidget->count()) {
        return;
    }

    // 保存要删除的标签名称
    QString tabName = m_mainTabWidget->tabText(tabIndex);

    // 移除标签页
    m_mainTabWidget->removeTab(tabIndex);
    tabIndex = -1;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已移除模块标签页: %1").arg(tabName));
}

void MainUiStateManager::slot_onTabCloseRequested(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭Tab请求信号槽: %1").arg(index));
    if (index == m_homeTabIndex || index < 0 || index >= m_mainTabWidget->count()) {
        return;
    }

    // 发送标签页关闭信号
    emit signal_moduleTabClosed(index);
}

bool MainUiStateManager::initializeSignalConnections(QWidget* parentWidget)
{
    if (!parentWidget || !validUI()) {
        return false;
    }

    try {
        // 连接主要按钮信号
        if (m_ui.actionStartTransfer) {
            connect(m_ui.actionStartTransfer, &QPushButton::clicked, this, &MainUiStateManager::startButtonClicked);
        }

        if (m_ui.actionStopTransfer) {
            connect(m_ui.actionStopTransfer, &QPushButton::clicked, this, &MainUiStateManager::stopButtonClicked);
        }

        if (m_ui.actionResetDevice) {
            connect(m_ui.actionResetDevice, &QPushButton::clicked, this, &MainUiStateManager::resetButtonClicked);
        }

        // 连接命令目录按钮
        if (m_ui.cmdDirButton) {
            connect(m_ui.cmdDirButton, &QPushButton::clicked, this, &MainUiStateManager::selectCommandDirClicked);
        }

        // 查找并连接其他快捷按钮
        QPushButton* quickChannelBtn = parentWidget->findChild<QPushButton*>("quickChannelBtn");
        if (quickChannelBtn) {
            connect(quickChannelBtn, &QPushButton::clicked, this, &MainUiStateManager::channelConfigButtonClicked);
        }

        QPushButton* quickDataBtn = parentWidget->findChild<QPushButton*>("quickDataBtn");
        if (quickDataBtn) {
            connect(quickDataBtn, &QPushButton::clicked, this, &MainUiStateManager::dataAnalysisButtonClicked);
        }

        QPushButton* quickVideoBtn = parentWidget->findChild<QPushButton*>("quickVideoBtn");
        if (quickVideoBtn) {
            connect(quickVideoBtn, &QPushButton::clicked, this, &MainUiStateManager::videoDisplayButtonClicked);
        }

        QPushButton* quickSaveBtn = parentWidget->findChild<QPushButton*>("quickSaveBtn");
        if (quickSaveBtn) {
            connect(quickSaveBtn, &QPushButton::clicked, this, &MainUiStateManager::saveFileButtonClicked);
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器信号连接成功"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI状态处理器信号连接失败: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI状态处理器信号连接未知异常"));
        return false;
    }
}

//--- 状态栏管理方法 ---//

void MainUiStateManager::updateTransferStats(uint64_t bytesTransferred, double transferRate, uint32_t errorCount)
{
    m_bytesTransferred = bytesTransferred;
    m_transferRate = transferRate;
    m_errorCount = errorCount;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新传输状态"));

    if (!validUI()) return;

    // 更新状态栏
    if (m_ui.totalBytesLabel) {
        m_ui.totalBytesLabel->setText(LocalQTCompat::fromLocal8Bit("总数据量：") + formatByteSize(bytesTransferred));
    }

    if (m_ui.transferRateLabel) {
        // 格式化传输速率显示 (MB/s)
        double mbpsRate = transferRate / (1024.0 * 1024.0);
        m_ui.transferRateLabel->setText(LocalQTCompat::fromLocal8Bit("速率：") + QString::number(mbpsRate, 'f', 2) + " MB/s");
    }

    // 如果有错误计数标签，也更新它
    QLabel* errorCountLabel = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("errorCountLabel");
    if (errorCountLabel) {
        errorCountLabel->setText(LocalQTCompat::fromLocal8Bit("错误：") + QString::number(errorCount));
    }

    // 更新状态标签页中的统计信息
    QLabel* bytesTransferredValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("bytesTransferredValue");
    QLabel* transferRateStatsValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("transferRateStatsValue");
    QLabel* errorCountValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("errorCountValue");

    if (bytesTransferredValue) {
        bytesTransferredValue->setText(formatByteSize(bytesTransferred));
    }

    if (transferRateStatsValue) {
        double mbpsRate = transferRate / (1024.0 * 1024.0);
        transferRateStatsValue->setText(QString::number(mbpsRate, 'f', 2) + " MB/s");
    }

    if (errorCountValue) {
        errorCountValue->setText(QString::number(errorCount));
    }
}

void MainUiStateManager::updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新USB速度显示"));
    if (!validUI()) return;

    if (m_ui.usbStatusLabel) {
        m_ui.usbStatusLabel->setText(LocalQTCompat::fromLocal8Bit("USB速度：") + speedDesc);

        // 根据USB版本设置不同样式
        if (isUSB3) {
            m_ui.usbStatusLabel->setStyleSheet("color: blue;");
        }
        else {
            m_ui.usbStatusLabel->setStyleSheet("color: green;");
        }
    }
}

void MainUiStateManager::updateTransferTimeDisplay()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新USB传输时间"));
    if (!validUI() || !m_isTransferring) return;

    // 获取当前计时器时间
    qint64 currentElapsed = m_elapsedTimer.elapsed() + m_totalElapsedTime;

    // 更新UI
    if (m_ui.totalTimeLabel) {
        m_ui.totalTimeLabel->setText(LocalQTCompat::fromLocal8Bit("传输时间：") + formatTime(currentElapsed));
    }
}

void MainUiStateManager::startTransferTimer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始传输计时"));
    if (!m_isTransferring) {
        m_isTransferring = true;

        // 如果是新的传输，重置总时间
        if (m_totalElapsedTime == 0) {
            m_totalElapsedTime = 0;
        }

        // 启动计时器
        m_elapsedTimer.start();
        m_transferTimer.start();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("传输计时开始"));
    }
}

void MainUiStateManager::stopTransferTimer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止传输计时"));
    if (m_isTransferring) {
        m_isTransferring = false;

        // 停止计时器并累加总时间
        m_transferTimer.stop();
        m_totalElapsedTime += m_elapsedTimer.elapsed();

        // 更新最终时间显示
        updateTransferTimeDisplay();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("传输计时停止，总时间: %1 毫秒").arg(m_totalElapsedTime));
    }
}

void MainUiStateManager::resetTransferStatsDisplay()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置传输状态显示"));
    m_bytesTransferred = 0;
    m_transferRate = 0.0;
    m_errorCount = 0;
    m_totalElapsedTime = 0;

    // 更新状态栏显示
    if (validUI()) {
        if (m_ui.totalBytesLabel) {
            m_ui.totalBytesLabel->setText(LocalQTCompat::fromLocal8Bit("总数据量：0 KB"));
        }

        if (m_ui.transferRateLabel) {
            m_ui.transferRateLabel->setText(LocalQTCompat::fromLocal8Bit("速率：0 MB/s"));
        }

        if (m_ui.totalTimeLabel) {
            m_ui.totalTimeLabel->setText(LocalQTCompat::fromLocal8Bit("传输时间：00:00"));
        }
    }

    // 更新详细统计页
    QLabel* bytesTransferredValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("bytesTransferredValue");
    QLabel* transferRateStatsValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("transferRateStatsValue");
    QLabel* errorCountValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("errorCountValue");

    if (bytesTransferredValue) {
        bytesTransferredValue->setText("0 KB");
    }

    if (transferRateStatsValue) {
        transferRateStatsValue->setText("0 MB/s");
    }

    if (errorCountValue) {
        errorCountValue->setText("0");
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("传输统计已重置"));
}

//--- 按钮状态管理 ---//

void MainUiStateManager::updateButtonStates(AppState state)
{
    bool enableStartButton = false;
    bool enableStopButton = false;
    bool enableResetButton = false;
    bool enableConfigButtons = false;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("根据APP状态: %1, 更新按钮状态").arg(static_cast<int>(state)));

    // 设置各种状态下的按钮启用/禁用
    switch (state) {
    case AppState::INITIALIZING:
        // 初始化中所有按钮禁用
        break;

    case AppState::DEVICE_ABSENT:
        // 设备未连接时仅设备相关操作禁用
        enableConfigButtons = true;
        break;

    case AppState::DEVICE_ERROR:
        // 设备错误时只能重置
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::IDLE:
        // 空闲状态，可以开始和重置
        enableStartButton = true;
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::CONFIGURED:
        // 已配置，可以开始和重置
        enableStartButton = true;
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::STARTING:
        // 启动中，只能重置
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::TRANSFERRING:
        // 传输中，可以停止和重置
        enableStopButton = true;
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::STOPPING:
        // 停止中，不能进行任何操作
        enableConfigButtons = true;
        break;

    case AppState::SHUTDOWN:
        // 关闭中，禁用所有操作
        break;

    default:
        enableConfigButtons = true;
        break;
    }

    // 更新按钮状态
    updateButtons(enableStartButton, enableStopButton, enableResetButton);

    // 更新配置相关按钮状态
    QPushButton* quickChannelBtn = qobject_cast<QWidget*>(parent())->findChild<QPushButton*>("quickChannelBtn");
    QPushButton* quickDataBtn = qobject_cast<QWidget*>(parent())->findChild<QPushButton*>("quickDataBtn");
    QPushButton* quickVideoBtn = qobject_cast<QWidget*>(parent())->findChild<QPushButton*>("quickVideoBtn");
    QPushButton* quickSaveBtn = qobject_cast<QWidget*>(parent())->findChild<QPushButton*>("quickSaveBtn");

    if (quickChannelBtn) quickChannelBtn->setEnabled(enableConfigButtons);
    if (quickDataBtn) quickDataBtn->setEnabled(enableConfigButtons);
    if (quickVideoBtn) quickVideoBtn->setEnabled(enableConfigButtons);
    if (quickSaveBtn) quickSaveBtn->setEnabled(enableConfigButtons);
}

void MainUiStateManager::updateQuickButtonsForTransfer(bool isTransferring)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新快捷传输按钮"));
    m_isTransferring = isTransferring;

    // 更新传输时间计时器
    if (isTransferring) {
        startTransferTimer();
    }
    else {
        stopTransferTimer();
    }
}

//--- 设备信息管理 ---//

void MainUiStateManager::updateDeviceInfoDisplay(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber)
{
    if (!validUI()) return;

    QLabel* deviceNameValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("deviceNameValue");
    QLabel* firmwareVersionValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("firmwareVersionValue");
    QLabel* serialNumberValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("serialNumberValue");

    if (deviceNameValue) {
        deviceNameValue->setText(deviceName);
    }

    if (firmwareVersionValue) {
        firmwareVersionValue->setText(firmwareVersion);
    }

    if (serialNumberValue) {
        serialNumberValue->setText(serialNumber);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备信息已更新：%1, %2, %3").arg(deviceName).arg(firmwareVersion).arg(serialNumber));
}

void MainUiStateManager::setVideoParamsDisplay(uint16_t width, uint16_t height, int format)
{
    if (!validUI()) return;

    if (m_ui.imageWIdth) {
        m_ui.imageWIdth->setText(QString::number(width));
    }

    if (m_ui.imageHeight) {
        m_ui.imageHeight->setText(QString::number(height));
    }

    if (m_ui.imageType && format >= 0 && format < m_ui.imageType->count()) {
        m_ui.imageType->setCurrentIndex(format);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频参数已更新：宽度=%1, 高度=%2, 格式=%3").arg(width).arg(height).arg(format));
}

void MainUiStateManager::setCommandDirDisplay(const QString& dir)
{
    if (!validUI()) return;

    if (m_ui.cmdDirEdit) {
        m_ui.cmdDirEdit->setText(dir);
    }

    if (m_ui.cmdStatusLabel) {
        if (dir.isEmpty()) {
            m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("未加载CMD目录"));
            m_ui.cmdStatusLabel->setStyleSheet("color: red;");
        }
        else {
            m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("已加载CMD目录"));
            m_ui.cmdStatusLabel->setStyleSheet("color: green;");
        }
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("命令目录已更新：%1").arg(dir));
}

//--- 消息显示方法 ---//

void MainUiStateManager::showErrorMessage(const QString& title, const QString& message)
{
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));

    QMetaObject::invokeMethod(QApplication::activeWindow(), [title, message]() {
        QMessageBox::critical(QApplication::activeWindow(), title, message);
        }, Qt::QueuedConnection);
}

void MainUiStateManager::showWarnMessage(const QString& title, const QString& message)
{
    LOG_INFO(QString("%1: %2").arg(title).arg(message));

    QMetaObject::invokeMethod(QApplication::activeWindow(), [title, message]() {
        QMessageBox::warning(QApplication::activeWindow(), title, message);
        }, Qt::QueuedConnection);
}

void MainUiStateManager::showInfoMessage(const QString& title, const QString& message)
{
    LOG_INFO(QString("%1: %2").arg(title).arg(message));

    QMetaObject::invokeMethod(QApplication::activeWindow(), [title, message]() {
        QMessageBox::information(QApplication::activeWindow(), title, message);
        }, Qt::QueuedConnection);
}

void MainUiStateManager::showAboutMessage(const QString& title, const QString& message)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示关于对话框"));

    QMetaObject::invokeMethod(QApplication::activeWindow(), [title, message]() {
        QMessageBox::about(QApplication::activeWindow(),
            title, message);
        }, Qt::QueuedConnection);
}

void MainUiStateManager::clearLogbox()
{
    QTextEdit* logTextEdit = qobject_cast<QWidget*>(parent())->findChild<QTextEdit*>("logTextEdit");
    if (logTextEdit) {
        logTextEdit->clear();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("日志已清除"));
    }
}

//--- 状态处理槽 ---//

void MainUiStateManager::onStateChanged(AppState newState, AppState oldState, const QString& reason)
{
    // 记录状态变化
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器状态变更 %1 -> %2, 原因: %3")
        .arg(AppStateMachine::stateToString(oldState))
        .arg(AppStateMachine::stateToString(newState))
        .arg(reason));

    // 根据新状态更新UI
    updateButtonStates(newState);
    updateStatusLabels(newState);
}

void MainUiStateManager::onDeviceConnectionChanged(bool connected)
{
    m_isDeviceConnected = connected;

    // 如果断开连接并且正在传输，停止传输计时
    if (!connected && m_isTransferring) {
        stopTransferTimer();
    }
}

void MainUiStateManager::onTransferStateChanged(bool transferring)
{
    updateQuickButtonsForTransfer(transferring);

    // 如果是开始传输，可能需要重置统计信息
    if (transferring && m_bytesTransferred == 0) {
        resetTransferStatsDisplay();
    }
}

//--- 私有槽方法 ---//

void MainUiStateManager::slot_updateTransferTime()
{
    updateTransferTimeDisplay();
}

//--- 私有工具方法 ---//

void MainUiStateManager::updateStatusLabels(AppState state)
{
    QString statusText;
    QString transferStatusText;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("由APP状态更新状态标签: %1").arg(static_cast<int>(state)));

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
        // 更新命令状态标签
        if (validUI() && m_ui.cmdStatusLabel) {
            m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("命令文件未加载"));
            m_ui.cmdStatusLabel->setStyleSheet("color: red;");
        }
        break;

    case AppState::CONFIGURED:
        statusText = LocalQTCompat::fromLocal8Bit("设备已配置");
        transferStatusText = LocalQTCompat::fromLocal8Bit("就绪");
        // 更新命令状态标签
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

void MainUiStateManager::updateStatusLabels(const QString& statusText, const QString& transferStatusText)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新标签- 状态: %1, 传输状态: %2")
        .arg(statusText).arg(transferStatusText));
    if (!validUI()) return;

    // 防御性检查所有使用的UI元素
    if (m_ui.usbStatusLabel)
        m_ui.usbStatusLabel->setText(statusText);

    if (m_ui.transferStatusLabel)
        m_ui.transferStatusLabel->setText(transferStatusText);
}

void MainUiStateManager::updateButtons(bool enableStart, bool enableStop, bool enableReset)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新所有按钮状态, 开始: %1, 停止: %2, 重置: %3")
        .arg(enableStart).arg(enableStop).arg(enableReset));
    if (!validUI()) return;

    // 防御性检查所有使用的UI元素
    if (m_ui.actionStartTransfer) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("开始按钮: %1").arg(enableStart));
        m_ui.actionStartTransfer->setEnabled(enableStart);
    }

    if (m_ui.actionStopTransfer)
        m_ui.actionStopTransfer->setEnabled(enableStop);

    if (m_ui.actionResetDevice)
        m_ui.actionResetDevice->setEnabled(enableReset);
}

void MainUiStateManager::prepareForClose()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态处理器准备关闭"));

    // 停止计时器
    if (m_transferTimer.isActive()) {
        m_transferTimer.stop();
    }

    m_validUI = false;  // 标记UI为无效，防止后续访问
}

bool MainUiStateManager::validUI() const
{
    if (!m_validUI) {
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("UI状态不正确"));
    }
    // 检查指向UI的指针是否有效
    return m_validUI;
}

QString MainUiStateManager::formatByteSize(uint64_t bytes) const
{
    QString sizeStr;

    if (bytes < 1024) {
        sizeStr = QString::number(bytes) + " B";
    }
    else if (bytes < 1024 * 1024) {
        double kbSize = bytes / 1024.0;
        sizeStr = QString::number(kbSize, 'f', 2) + " KB";
    }
    else if (bytes < 1024 * 1024 * 1024) {
        double mbSize = bytes / (1024.0 * 1024.0);
        sizeStr = QString::number(mbSize, 'f', 2) + " MB";
    }
    else {
        double gbSize = bytes / (1024.0 * 1024.0 * 1024.0);
        sizeStr = QString::number(gbSize, 'f', 2) + " GB";
    }

    return sizeStr;
}

QString MainUiStateManager::formatTime(qint64 milliseconds) const
{
    int seconds = static_cast<int>(milliseconds / 1000);
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    seconds = seconds % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    else {
        return QString("%1:%2")
            .arg(minutes)
            .arg(seconds, 2, 10, QChar('0'));
    }
}