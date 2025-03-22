// Source/Core/MainUiStateManager.cpp

#include "MainUiStateManager.h"
#include "Logger.h"
#include <QMessageBox>
#include <QApplication>
#include <QDebug>
#include <QStatusBar>
#include <QPushButton>
#include <QTextEdit>

// #define USE_LOCAL_TIMER

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

#ifdef USE_LOCAL_TIMER
    // 初始化传输计时器
    // 使用UI状态管理器本地计时器
    bool connected = connect(&m_transferTimer, &QTimer::timeout,
        this, &MainUiStateManager::slot_MainUI_STM_updateTransferTime,
        Qt::QueuedConnection);  // 使用队列连接确保线程安全
    m_transferTimer.setInterval(50);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("传输计时器连接状态: %1，更新间隔: 50ms")
        .arg(connected ? "成功" : "失败"));
#endif
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
        this, &MainUiStateManager::slot_MainUI_STM_onTabCloseRequested);

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

void MainUiStateManager::slot_MainUI_STM_onTabCloseRequested(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭Tab请求信号槽: %1").arg(index));
    if (index == m_homeTabIndex || index < 0 || index >= m_mainTabWidget->count()) {
        return;
    }

    // 发送标签页关闭信号
    emit signal_MainUI_STM_moduleTabClosed(index);
}

bool MainUiStateManager::initializeSignalConnections(QWidget* parentWidget)
{
    if (!parentWidget || !validUI()) {
        return false;
    }

    try {
        // 连接主要按钮信号
        if (m_ui.actionStartTransfer) {
            connect(m_ui.actionStartTransfer, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_startButtonClicked);
        }

        if (m_ui.actionStopTransfer) {
            connect(m_ui.actionStopTransfer, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_stopButtonClicked);
        }

        if (m_ui.actionResetDevice) {
            connect(m_ui.actionResetDevice, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_resetButtonClicked);
        }

        // 连接命令目录按钮
        if (m_ui.cmdDirButton) {
            connect(m_ui.cmdDirButton, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_selectCommandDirClicked);
        }

        // 查找并连接其他快捷按钮
        QPushButton* quickChannelBtn = parentWidget->findChild<QPushButton*>("quickChannelBtn");
        if (quickChannelBtn) {
            connect(quickChannelBtn, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_channelConfigButtonClicked);
        }

        QPushButton* quickDataBtn = parentWidget->findChild<QPushButton*>("quickDataBtn");
        if (quickDataBtn) {
            connect(quickDataBtn, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_dataAnalysisButtonClicked);
        }

        QPushButton* quickVideoBtn = parentWidget->findChild<QPushButton*>("quickVideoBtn");
        if (quickVideoBtn) {
            connect(quickVideoBtn, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_videoDisplayButtonClicked);
        }

        QPushButton* quickSaveBtn = parentWidget->findChild<QPushButton*>("quickSaveBtn");
        if (quickSaveBtn) {
            connect(quickSaveBtn, &QPushButton::clicked, this, &MainUiStateManager::signal_MainUI_STM_saveFileButtonClicked);
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

void MainUiStateManager::updateTransferStats(uint64_t bytesTransferred, double transferRate, uint64_t elapseMs)
{
    m_bytesTransferred = bytesTransferred;
    m_transferRate = transferRate;

    if (!validUI()) return;

    // 更新状态栏
    if (m_ui.totalBytesLabel) {
        m_ui.totalBytesLabel->setText(LocalQTCompat::fromLocal8Bit("总数据量：") + formatByteSize(bytesTransferred));
    }

    if (m_ui.transferRateLabel) {
        // 格式化传输速率显示 (MB/s)
        m_ui.transferRateLabel->setText(LocalQTCompat::fromLocal8Bit("速率：") + QString::number(transferRate, 'f', 2) + " MB/s");
    }

#ifndef USE_LOCAL_TIMER
    if (m_ui.totalTimeLabel) {
        m_ui.totalTimeLabel->setText(LocalQTCompat::fromLocal8Bit("传输时间：") + formatTime(elapseMs) + QString(" s"));
    }
#endif
}

void MainUiStateManager::updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3, bool isConnected)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("主UI管理器中更新USB速度显示: %1, %2, %3").
        arg(speedDesc).
        arg(isUSB3 ? "u3" : "no-u3").
        arg(isConnected ? "已连接" : "未连接"));
    if (!validUI()) return;

    if (!isConnected) {
        m_ui.usbSpeedLabel->setStyleSheet("color: red;");
        m_ui.usbStatusLabel->setStyleSheet("color: red;");
        m_ui.usbSpeedLabel->setText(LocalQTCompat::fromLocal8Bit("USB速度：") + speedDesc);
        m_ui.usbStatusLabel->setText(LocalQTCompat::fromLocal8Bit("设备状态：离线"));
        return;
    }

    if (m_ui.usbSpeedLabel) {
        m_ui.usbSpeedLabel->setText(LocalQTCompat::fromLocal8Bit("USB速度：") + speedDesc);

        // 根据USB版本设置不同样式
        if (isUSB3) {
            m_ui.usbSpeedLabel->setStyleSheet("color: blue;");
        }
        else {
            m_ui.usbSpeedLabel->setStyleSheet("color: green;");
        }
    }
}

void MainUiStateManager::updateTransferTimeDisplay()
{
    if (!validUI()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("UI无效，无法更新传输时间"));
        return;
    }

    if (!m_isTransferring) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("非传输状态，无法更新传输时间"));
        return;
    }

    if (!m_ui.totalTimeLabel) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("传输时间标签不存在，无法更新"));
        return;
    }

    // 获取当前计时器时间
    qint64 currentElapsed = m_isTransferring ?
        m_elapsedTimer.elapsed() + m_totalElapsedTime :
        m_totalElapsedTime;

    // 更新UI
    QString timeStr = formatTime(currentElapsed);
    m_ui.totalTimeLabel->setText(LocalQTCompat::fromLocal8Bit("传输时间：") + timeStr + QString(" s"));
}

void MainUiStateManager::startTransferTimer()
{
    if (!m_isTransferring) {
        m_isTransferring = true;

        // 如果是新的传输，重置总时间
        if (m_totalElapsedTime == 0) {
            m_totalElapsedTime = 0;
        }

        // 启动计时器
        m_elapsedTimer.start();

        // 确保定时器先停止再启动
        if (m_transferTimer.isActive()) {
            m_transferTimer.stop();
        }

        // 提高更新频率以更好地显示毫秒变化
        m_transferTimer.setInterval(67);
        m_transferTimer.start();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("传输计时开始，定时器状态: %1，更新间隔: 67ms")
            .arg(m_transferTimer.isActive() ? "活动" : "非活动"));
    }
}

void MainUiStateManager::stopTransferTimer()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止传输计时"));
    if (m_isTransferring) {
        m_isTransferring = false;

        // 停止计时器并累加总时间
        if (m_transferTimer.isActive()) {
            m_transferTimer.stop();
            LOG_INFO(LocalQTCompat::fromLocal8Bit("传输计时器已停止"));
        }

        m_totalElapsedTime += m_elapsedTimer.elapsed();

        // 更新最终时间显示
        updateTransferTimeDisplay();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("传输计时停止，总时间: %1 毫秒").arg(m_totalElapsedTime));
    }
}

void MainUiStateManager::resetTransferStatsDisplay()
{
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
        // 设备未连接时所有设备操作按钮禁用
        enableConfigButtons = true;
        break;

    case AppState::DEVICE_ERROR:
        // 设备错误时只能重置
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::IDLE:
        // 空闲状态，只有重置可用（因为没有命令）
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::COMMANDS_MISSING:
        // 命令未加载状态，只有重置可用
        enableResetButton = true;
        enableConfigButtons = true;
        break;

    case AppState::CONFIGURED:
        // 已配置（设备已连接且命令已加载），可以开始和重置
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
        // 传输中，可以停止，但不能重置
        enableStopButton = true;
        enableResetButton = false;
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
}

void MainUiStateManager::updateQuickButtonsForTransfer(bool isTransferring)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新快捷传输按钮: %1").arg(isTransferring ? "开始" : "结束"));

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

void MainUiStateManager::updateDeviceParameters(uint16_t width, uint16_t height, uint8_t captureType)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新设备参数显示 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(captureType, 2, 16, QChar('0')));

    if (!validUI()) return;

    // 更新宽度显示
    if (m_ui.imageWIdth) {
        m_ui.imageWIdth->setText(QString::number(width));
    }

    // 更新高度显示
    if (m_ui.imageHeight) {
        m_ui.imageHeight->setText(QString::number(height));
    }

    // 更新类型下拉框
    if (m_ui.imageType) {
        int index = 1; // 默认RAW10
        switch (captureType) {
        case 0x38: index = 0; break; // RAW8
        case 0x39: index = 1; break; // RAW10
        case 0x3A: index = 2; break; // RAW12
        }
        m_ui.imageType->setCurrentIndex(index);
    }
}

void MainUiStateManager::updateTransferStatus(bool isTransferring, const QString& statusText)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新传输状态显示 - 传输中: %1, 文本: %2")
        .arg(isTransferring).arg(statusText));

    if (!validUI()) return;

    // 更新传输状态文本
    if (m_ui.transferStatusLabel) {
        m_ui.transferStatusLabel->setText(LocalQTCompat::fromLocal8Bit("传输状态：") + statusText);
    }

    // 更新传输计时器
    if (isTransferring) {
        startTransferTimer();
    }
    else {
        stopTransferTimer();
    }

    // 更新是否正在传输的标志
    m_isTransferring = isTransferring;
}

void MainUiStateManager::updateButtonStates(bool enableStart, bool enableStop, bool enableReset)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新按钮状态 - 开始: %1, 停止: %2, 重置: %3")
        .arg(enableStart).arg(enableStop).arg(enableReset));

    if (!validUI()) return;

    // 更新开始按钮状态
    if (m_ui.actionStartTransfer) {
        m_ui.actionStartTransfer->setEnabled(enableStart);
    }

    // 更新停止按钮状态
    if (m_ui.actionStopTransfer) {
        m_ui.actionStopTransfer->setEnabled(enableStop);
    }

    // 更新重置按钮状态
    if (m_ui.actionResetDevice) {
        m_ui.actionResetDevice->setEnabled(enableReset);
    }

    // 可以在这里添加更新其他按钮状态的代码，如模块按钮等
}

void MainUiStateManager::updateDeviceState(DeviceState state)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新设备状态显示 - 状态: %1")
        .arg(static_cast<int>(state)));

    if (!validUI()) return;

    // 获取设备状态文本
    QString stateText = getDeviceStateText(state);

    // 更新USB状态标签
    if (m_ui.usbStatusLabel) {
        m_ui.usbStatusLabel->setText(LocalQTCompat::fromLocal8Bit("设备状态：") + stateText);

        // 根据状态设置不同样式
        switch (state) {
        case DeviceState::DEV_CONNECTED:
        case DeviceState::DEV_TRANSFERRING:
            m_ui.usbStatusLabel->setStyleSheet("color: green;");
            break;
        case DeviceState::DEV_ERROR:
            m_ui.usbStatusLabel->setStyleSheet("color: red;");
            break;
        case DeviceState::DEV_DISCONNECTED:
        default:
            m_ui.usbStatusLabel->setStyleSheet("");
            break;
        }
    }

    // 根据设备状态决定按钮启用状态
    bool enableStart = (state == DeviceState::DEV_CONNECTED);
    bool enableStop = (state == DeviceState::DEV_TRANSFERRING);
    bool enableReset = (state == DeviceState::DEV_CONNECTED ||
        state == DeviceState::DEV_ERROR);

    // 更新按钮状态
    updateButtonStates(enableStart, enableStop, enableReset);

    // 如果正在传输状态变化，更新传输状态
    bool isTransferring = (state == DeviceState::DEV_TRANSFERRING);
    if (m_isTransferring != isTransferring) {
        QString transferText = isTransferring ?
            LocalQTCompat::fromLocal8Bit("传输中") :
            LocalQTCompat::fromLocal8Bit("已停止");
        updateTransferStatus(isTransferring, transferText);
    }
}

void MainUiStateManager::slot_MainUI_STM_onDeviceStateChanged(DeviceState state)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备状态已变更为: %1")
        .arg(static_cast<int>(state)));

    // 更新UI显示
    updateDeviceState(state);

    // 根据设备状态转换为应用状态并更新
    AppState appState;
    switch (state) {
    case DeviceState::DEV_DISCONNECTED:
        appState = AppState::DEVICE_ABSENT;
        break;
    case DeviceState::DEV_CONNECTING:
        appState = AppState::INITIALIZING;
        break;
    case DeviceState::DEV_CONNECTED:
        appState = AppState::IDLE;
        break;
    case DeviceState::DEV_TRANSFERRING:
        appState = AppState::TRANSFERRING;
        break;
    case DeviceState::DEV_ERROR:
        appState = AppState::DEVICE_ERROR;
        break;
    default:
        appState = AppState::IDLE;
        break;
    }

    // 通知状态机更新应用状态
    // 注意：这可能导致循环调用，取决于应用程序的信号连接方式
    // 如果状态机已经连接到此类的onStateChanged方法，应该考虑避免循环调用
    // AppStateMachine::instance().processEvent(StateEvent::STATE_CHANGED, getDeviceStateText(state));
}

// 2. 添加到MainUiStateManager.cpp中，用于初始化UI状态
void MainUiStateManager::slot_MainUI_STM_initializeUIState()
{
    // 初始化状态栏标签
    if (m_ui.usbSpeedLabel)
        m_ui.usbSpeedLabel->setText(LocalQTCompat::fromLocal8Bit("USB速度：未连接"));

    if (m_ui.usbStatusLabel)
        m_ui.usbStatusLabel->setText(LocalQTCompat::fromLocal8Bit("设备状态：未连接"));

    if (m_ui.transferStatusLabel)
        m_ui.transferStatusLabel->setText(LocalQTCompat::fromLocal8Bit("传输状态：未开始"));

    if (m_ui.transferRateLabel)
        m_ui.transferRateLabel->setText(LocalQTCompat::fromLocal8Bit("速率：0 MB/s"));

    if (m_ui.totalBytesLabel)
        m_ui.totalBytesLabel->setText(LocalQTCompat::fromLocal8Bit("总数据量：0 KB"));

    if (m_ui.totalTimeLabel)
        m_ui.totalTimeLabel->setText(LocalQTCompat::fromLocal8Bit("传输时间：00:00"));

    // 初始化命令目录显示
    if (m_ui.cmdDirEdit)
        m_ui.cmdDirEdit->setText("");

    if (m_ui.cmdStatusLabel) {
        m_ui.cmdStatusLabel->setStyleSheet("color: red;");
        m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("未加载CMD目录"));
    }

    // 初始化按钮状态 - 默认全部禁用
    updateButtons(false, false, false);

    // 初始化设备信息
    QLabel* deviceNameValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("deviceNameValue");
    QLabel* firmwareVersionValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("firmwareVersionValue");
    QLabel* serialNumberValue = qobject_cast<QWidget*>(parent())->findChild<QLabel*>("serialNumberValue");

    if (deviceNameValue) deviceNameValue->setText(LocalQTCompat::fromLocal8Bit("未连接"));
    if (firmwareVersionValue) firmwareVersionValue->setText(LocalQTCompat::fromLocal8Bit("未知"));
    if (serialNumberValue) serialNumberValue->setText(LocalQTCompat::fromLocal8Bit("未知"));
}

// 3. 修改MainUiStateManager.cpp中的初始化视频参数方法
void MainUiStateManager::slot_MainUI_STM_initializeVideoParameters()
{
    // 设置默认视频参数 (1080p)
    if (m_ui.imageWIdth) {
        m_ui.imageWIdth->setText("1920");
        m_ui.imageWIdth->setReadOnly(false); // 允许用户输入
    }

    if (m_ui.imageHeight) {
        m_ui.imageHeight->setText("1080");
        m_ui.imageHeight->setReadOnly(false); // 允许用户输入
    }

    if (m_ui.imageType && m_ui.imageType->count() > 1) {
        m_ui.imageType->setCurrentIndex(1); // RAW10 (索引1)
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频参数已初始化为默认值 - 宽度: 1920, 高度: 1080, 格式: RAW10"));
}

QString MainUiStateManager::getDeviceStateText(DeviceState state) const
{
    switch (state) {
    case DeviceState::DEV_DISCONNECTED:
        return LocalQTCompat::fromLocal8Bit("未连接");
    case DeviceState::DEV_CONNECTING:
        return LocalQTCompat::fromLocal8Bit("连接中");
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
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("显示错误框, 标题: %1, 信息: %2").arg(title).arg(message));

    QMetaObject::invokeMethod(QApplication::activeWindow(), [title, message]() {
        QMessageBox::critical(QApplication::activeWindow(), title, message);
        }, Qt::QueuedConnection);
}

void MainUiStateManager::showWarnMessage(const QString& title, const QString& message)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示警告框, 标题: %1, 信息: %2").arg(title).arg(message));

    QMetaObject::invokeMethod(QApplication::activeWindow(), [title, message]() {
        QMessageBox::warning(QApplication::activeWindow(), title, message);
        }, Qt::QueuedConnection);
}

void MainUiStateManager::showInfoMessage(const QString& title, const QString& message)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示信息框, 标题: %1, 信息: %2").arg(title).arg(message));

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

void MainUiStateManager::slot_MainUI_STM_onStateChanged(AppState newState, AppState oldState, const QString& reason)
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

void MainUiStateManager::slot_MainUI_STM_onTransferStateChanged(bool transferring)
{
#ifdef USE_LOCAL_TIMER
    updateQuickButtonsForTransfer(transferring);
#endif

    // 如果是开始传输，可能需要重置统计信息
    if (transferring && m_bytesTransferred == 0) {
        resetTransferStatsDisplay();
    }
}

//--- 私有槽方法 ---//

void MainUiStateManager::slot_MainUI_STM_updateTransferTime()
{
    updateTransferTimeDisplay();
}

//--- 私有工具方法 ---//

void MainUiStateManager::updateStatusLabels(AppState state)
{
    QString statusText;
    QString transferStatusText;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("由APP状态更新标签状态: %1").arg(static_cast<int>(state)));

    // 设置各种状态下的文本
    switch (state) {
    case AppState::INITIALIZING:
        m_ui.usbStatusLabel->setStyleSheet("color: black;");
        statusText = LocalQTCompat::fromLocal8Bit("初始化中");
        transferStatusText = LocalQTCompat::fromLocal8Bit("未开始");
        break;

    case AppState::DEVICE_ABSENT:
        m_ui.usbSpeedLabel->setStyleSheet("color: red;");
        m_ui.usbSpeedLabel->setText(LocalQTCompat::fromLocal8Bit("USB速度：无设备"));

        m_ui.usbStatusLabel->setStyleSheet("color: red;");
        statusText = LocalQTCompat::fromLocal8Bit("设备未连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("未连接");
        break;

    case AppState::DEVICE_ERROR:
        m_ui.usbStatusLabel->setStyleSheet("color: red;");
        statusText = LocalQTCompat::fromLocal8Bit("设备错误");
        transferStatusText = LocalQTCompat::fromLocal8Bit("错误");
        break;

    case AppState::IDLE:
        m_ui.usbStatusLabel->setStyleSheet("color: green;");
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("空闲");
        break;

    case AppState::COMMANDS_MISSING:
        m_ui.usbStatusLabel->setStyleSheet("color: red;");
        statusText = LocalQTCompat::fromLocal8Bit("命令文件未加载");
        transferStatusText = LocalQTCompat::fromLocal8Bit("空闲");
        // 更新命令状态标签
        if (validUI() && m_ui.cmdStatusLabel) {
            m_ui.cmdStatusLabel->setStyleSheet("color: red;");
            m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("请加载命令文件目录"));
        }
        break;

    case AppState::CONFIGURED:
        m_ui.usbStatusLabel->setStyleSheet("color: green;");
        statusText = LocalQTCompat::fromLocal8Bit("设备已配置");
        transferStatusText = LocalQTCompat::fromLocal8Bit("就绪");
        // 更新命令状态标签
        if (validUI() && m_ui.cmdStatusLabel) {
            m_ui.cmdStatusLabel->setStyleSheet("color: green;");
            m_ui.cmdStatusLabel->setText(LocalQTCompat::fromLocal8Bit("命令文件已加载"));
        }
        break;

    case AppState::STARTING:
        m_ui.usbStatusLabel->setStyleSheet("color: green;");
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("启动中");
        break;

    case AppState::TRANSFERRING:
        m_ui.usbStatusLabel->setStyleSheet("color: green;");
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("传输中");
        break;

    case AppState::STOPPING:
        m_ui.usbStatusLabel->setStyleSheet("color: green;");
        statusText = LocalQTCompat::fromLocal8Bit("设备已连接");
        transferStatusText = LocalQTCompat::fromLocal8Bit("停止中");
        break;

    case AppState::SHUTDOWN:
        m_ui.usbStatusLabel->setStyleSheet("color: black;");
        statusText = LocalQTCompat::fromLocal8Bit("关闭中");
        transferStatusText = LocalQTCompat::fromLocal8Bit("关闭中");
        break;

    default:
        m_ui.usbStatusLabel->setStyleSheet("color: red;");
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
        m_ui.usbStatusLabel->setText(LocalQTCompat::fromLocal8Bit("设备状态：") + statusText);

    if (m_ui.transferStatusLabel)
        m_ui.transferStatusLabel->setText(LocalQTCompat::fromLocal8Bit("传输状态：") + transferStatusText);
}

void MainUiStateManager::updateButtons(bool enableStart, bool enableStop, bool enableReset)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新所有按钮状态, 开始: %1, 停止: %2, 重置: %3")
        .arg(enableStart ? "启用" : "禁止")
        .arg(enableStop ? "启用" : "禁止")
        .arg(enableReset ? "启用" : "禁止"));
    if (!validUI()) return;

    // 防御性检查所有使用的UI元素
    if (m_ui.actionStartTransfer) {
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
    // 计算时、分、秒和毫秒
    int seconds = static_cast<int>(milliseconds / 1000);
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    seconds = seconds % 60;
    int ms = milliseconds % 1000;

    // 根据时长选择不同的格式
    if (hours > 0) {
        return QString("%1:%2:%3.%4")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(ms, 3, 10, QChar('0'));
    }
    else {
        return QString("%1:%2.%3")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(ms, 3, 10, QChar('0'));
    }
}
