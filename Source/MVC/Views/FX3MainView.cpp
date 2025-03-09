// Source/MVC/Views/FX3MainView.cpp

#include <windows.h>
#include <dbt.h>
#include <QMessageBox>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QDateTime>
#include <QApplication>
#include <QFileDialog>
#include <QIcon>

#include "FX3MainView.h"
#include "FX3MainController.h"
#include "CyAPI.h"
#include "Logger.h"

FX3MainView::FX3MainView(QWidget* parent)
    : QMainWindow(parent)
    , m_controller(nullptr)
    , m_uiStateManager(nullptr)
    , m_mainTabWidget(nullptr)
    , m_mainSplitter(nullptr)
    , m_leftSplitter(nullptr)
    , m_statusPanel(nullptr)
    , m_mainToolBar(nullptr)
    , m_homeTabIndex(-1)
    , m_channelTabIndex(-1)
    , m_dataAnalysisTabIndex(-1)
    , m_videoDisplayTabIndex(-1)
    , m_waveformTabIndex(-1)
    , m_fileSaveTabIndex(-1)
    , m_loggerInitialized(false)
{
    // 设置UI
    ui.setupUi(this);

    try {
        // 初始化日志系统
        if (!initializeLogger()) {
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("日志系统初始化失败，应用程序无法继续"));
            QTimer::singleShot(0, this, &FX3MainView::close);
            return;
        }
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序启动，Qt版本: %1").arg(QT_VERSION_STR));

        // 初始化UI状态管理器
        if (!initializeUiStateManager()) {
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("UI状态管理器初始化失败"));
            QTimer::singleShot(0, this, &FX3MainView::close);
            return;
        }

        // 设置模块按钮信号映射
        setupModuleButtonSignalMapping();

        // 初始化主控制器
        m_controller = std::make_unique<FX3MainController>(this);
        if (!m_controller->initialize()) {
            QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
                LocalQTCompat::fromLocal8Bit("控制器初始化失败"));
            QTimer::singleShot(0, this, &FX3MainView::close);
            return;
        }

        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("FX3主控制器设置完成..."));

        // 初始化UI组件引用 - 直接使用 ui.mainTabWidget
        m_mainTabWidget = ui.mainTabWidget;
        if (m_mainTabWidget) {
            // 设置默认标签页为日志
            m_mainTabWidget->setCurrentIndex(0);

            // 连接标签关闭信号 - 对于主标签页我们不设置关闭按钮
            connect(m_mainTabWidget, &QTabWidget::tabCloseRequested,
                this, &FX3MainView::slot_onTabCloseRequested);
        }

        // 状态、工具条
        m_statusPanel = findChild<QWidget*>("statusPanel");
        m_mainToolBar = findChild<QToolBar*>("mainToolBar");

        // 不再需要初始化信号连接，由UI状态管理器处理
        //initializeSignalConnections();

        // 由UI状态管理器更新状态页信息
        if (m_uiStateManager) {
            m_uiStateManager->updateDeviceInfoDisplay("未连接", "未知", "未知");
            m_uiStateManager->resetTransferStatsDisplay();
        }

        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("FX3主视图构造函数完成..."));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("初始化过程中发生异常: %1").arg(e.what()));
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化异常: %1").arg(e.what()));
        QTimer::singleShot(0, this, &FX3MainView::close);
    }
    catch (...) {
        QMessageBox::critical(this, LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("初始化过程中发生未知异常"));
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化未知异常"));
        QTimer::singleShot(0, this, &FX3MainView::close);
    }
}

FX3MainView::~FX3MainView()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主视图析构函数入口"));

    // 先通知UI状态管理器准备关闭
    if (m_uiStateManager) {
        m_uiStateManager->prepareForClose();
    }

    m_controller.reset();
    m_uiStateManager.reset();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主视图析构函数退出"));
}

bool FX3MainView::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    MSG* msg = reinterpret_cast<MSG*>(message);

    if (msg->message == WM_DEVICECHANGE && m_controller) {
        switch (msg->wParam) {
        case DBT_DEVICEARRIVAL: {
            DEV_BROADCAST_HDR* deviceHeader = reinterpret_cast<DEV_BROADCAST_HDR*>(msg->lParam);
            if (deviceHeader->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                DEV_BROADCAST_DEVICEINTERFACE* deviceInterface =
                    reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(msg->lParam);

                // 检查是否是我们的FX3设备
                if (IsEqualGUID(deviceInterface->dbcc_classguid, CYUSBDRV_GUID)) {
                    m_controller->handleDeviceArrival();
                }
            }
            break;
        }
        case DBT_DEVICEREMOVECOMPLETE: {
            DEV_BROADCAST_HDR* deviceHeader = reinterpret_cast<DEV_BROADCAST_HDR*>(msg->lParam);
            if (deviceHeader->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                DEV_BROADCAST_DEVICEINTERFACE* deviceInterface =
                    reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(msg->lParam);

                // 检查是否是我们的FX3设备
                if (IsEqualGUID(deviceInterface->dbcc_classguid, CYUSBDRV_GUID)) {
                    m_controller->handleDeviceRemoval();
                }
            }
            break;
        }
        }
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}

void FX3MainView::closeEvent(QCloseEvent* event)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关闭事件触发"));

    if (m_controller) {
        // 通知控制器执行关闭操作
        m_controller->handleClose();
    }

    // 接受关闭事件
    event->accept();
}

void FX3MainView::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    slot_adjustStatusBar();
}

void FX3MainView::slot_adjustStatusBar()
{
    QStatusBar* statusBar = this->statusBar();
    if (!statusBar) return;

    statusBar->setMinimumWidth(40);
    statusBar->setMinimumHeight(30);
}

bool FX3MainView::initializeLogger()
{
    if (m_loggerInitialized) {
        return true;
    }

    try {
        // 获取日志控件 - 位于标签页内部
        QTextEdit* logTextEdit = findChild<QTextEdit*>("logTextEdit");
        if (logTextEdit) {
            Logger::instance().setLogWidget(logTextEdit);
        }
        else {
            throw std::runtime_error("未找到日志控件");
        }

        m_loggerInitialized = true;
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "日志初始化失败:" << e.what();
        return false;
    }
}

void FX3MainView::initializeSignalConnections()
{
    // 连接按钮信号到视图槽
    if (ui.cmdDirButton) {
        connect(ui.cmdDirButton, &QPushButton::clicked, this, &FX3MainView::slot_onSelectCommandDirClicked);
    }

    // 连接快捷按钮动作到视图槽
    if (ui.actionStartTransfer) {
        connect(ui.actionStartTransfer, &QPushButton::clicked, this, &FX3MainView::slot_onStartButtonClicked);
    }

    if (ui.actionStopTransfer) {
        connect(ui.actionStopTransfer, &QPushButton::clicked, this, &FX3MainView::slot_onStopButtonClicked);
    }

    if (ui.actionResetDevice) {
        connect(ui.actionResetDevice, &QPushButton::clicked, this, &FX3MainView::slot_onResetButtonClicked);
    }

    // 连接快捷按钮
    QPushButton* quickChannelBtn = findChild<QPushButton*>("quickChannelBtn");
    if (quickChannelBtn) {
        connect(quickChannelBtn, &QPushButton::clicked, this, &FX3MainView::slot_onChannelConfigButtonClicked);
    }

    QPushButton* quickDataBtn = findChild<QPushButton*>("quickDataBtn");
    if (quickDataBtn) {
        connect(quickDataBtn, &QPushButton::clicked, this, &FX3MainView::slot_onDataAnalysisButtonClicked);
    }

    QPushButton* quickVideoBtn = findChild<QPushButton*>("quickVideoBtn");
    if (quickVideoBtn) {
        connect(quickVideoBtn, &QPushButton::clicked, this, &FX3MainView::slot_onVideoDisplayButtonClicked);
    }

    QPushButton* quickSaveBtn = findChild<QPushButton*>("quickSaveBtn");
    if (quickSaveBtn) {
        connect(quickSaveBtn, &QPushButton::clicked, this, &FX3MainView::slot_onSaveFileButtonClicked);
    }
}

bool FX3MainView::initializeUiStateManager() {
    try {
        m_uiStateManager = std::make_unique<MainUiStateManager>(ui, this);

        if (!m_uiStateManager->initializeSignalConnections(this)) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI状态管理器信号连接失败"));
            return false;
        }

        // 使用更简洁的信号连接方式
#define CONNECT_SIGNAL(uiSignal, viewSignal) \
            connect(m_uiStateManager.get(), &MainUiStateManager::uiSignal, \
                    this, [this](){ emit signal_##viewSignal(); })

        CONNECT_SIGNAL(startButtonClicked, startButtonClicked);
        CONNECT_SIGNAL(stopButtonClicked, stopButtonClicked);
        CONNECT_SIGNAL(resetButtonClicked, resetButtonClicked);
        CONNECT_SIGNAL(channelConfigButtonClicked, channelConfigButtonClicked);
        CONNECT_SIGNAL(dataAnalysisButtonClicked, dataAnalysisButtonClicked);
        CONNECT_SIGNAL(videoDisplayButtonClicked, videoDisplayButtonClicked);
        CONNECT_SIGNAL(waveformAnalysisButtonClicked, waveformAnalysisButtonClicked);
        CONNECT_SIGNAL(saveFileButtonClicked, saveFileButtonClicked);
        CONNECT_SIGNAL(exportDataButtonClicked, exportDataButtonClicked);
        CONNECT_SIGNAL(fileOptionsButtonClicked, FileOptionsButtonClicked);
        CONNECT_SIGNAL(settingsTriggered, settingsTriggered);
        CONNECT_SIGNAL(selectCommandDirClicked, selectCommandDirClicked);
        CONNECT_SIGNAL(updateDeviceButtonClicked, updateDeviceButtonClicked);

#undef CONNECT_SIGNAL

        LOG_INFO(LocalQTCompat::fromLocal8Bit("UI状态管理器初始化成功"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI状态管理器初始化异常: %1").arg(e.what()));
        return false;
    }
}

void FX3MainView::setupModuleButtonSignalMapping() {
    // 按钮名称和信号方法的映射结构
    struct ButtonSignalMapping {
        const char* buttonName;
        void (FX3MainView::* signalMethod)();
    };

    // 定义所有映射
    const ButtonSignalMapping mappings[] = {
        { "quickChannelBtn", &FX3MainView::signal_channelConfigButtonClicked },
        { "quickDataBtn", &FX3MainView::signal_dataAnalysisButtonClicked },
        { "quickVideoBtn", &FX3MainView::signal_videoDisplayButtonClicked },
        { "quickWaveformBtn", &FX3MainView::signal_waveformAnalysisButtonClicked },
        { "quickSaveBtn", &FX3MainView::signal_saveFileButtonClicked },
        { "quickExportBtn", &FX3MainView::signal_exportDataButtonClicked },
        { "quickFileOptionsBtn", &FX3MainView::signal_FileOptionsButtonClicked },
        { "quickUpdateBtn", &FX3MainView::signal_updateDeviceButtonClicked }
    };

    // 连接每个按钮
    for (const auto& mapping : mappings) {
        QPushButton* button = findChild<QPushButton*>(mapping.buttonName);
        if (button) {
            connect(button, &QPushButton::clicked, this, mapping.signalMethod);
            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("按钮映射已连接: %1").arg(mapping.buttonName));
        }
    }
}

//--- UI显示方法 ---//

void FX3MainView::showErrorMessage(const QString& title, const QString& message)
{
    QMessageBox::critical(this, title, message);
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));
}

void FX3MainView::showWarningMessage(const QString& title, const QString& message)
{
    QMessageBox::warning(this, title, message);
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));
}

void FX3MainView::showInfoMessage(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
    LOG_INFO(QString("%1: %2").arg(title).arg(message));
}

void FX3MainView::showAboutDialog()
{
    QMessageBox::about(this,
        LocalQTCompat::fromLocal8Bit("关于FX3传输测试工具"),
        LocalQTCompat::fromLocal8Bit("FX3传输测试工具 v3.0\n\n\
用于FX3设备的数据传输和测试\n\n\
  © 2025 公司名称\n\n\
email: lihugao@gmail.com")
    );
}

void FX3MainView::clearLog()
{
    QTextEdit* logTextEdit = findChild<QTextEdit*>("logTextEdit");
    if (logTextEdit) {
        logTextEdit->clear();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("日志已清除"));
    }
}

void FX3MainView::updateStatusBar(const QString& message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void FX3MainView::updateWindowTitle(const QString& toolInfo)
{
    QString title = LocalQTCompat::fromLocal8Bit("FX3传输测试工具");
    if (!toolInfo.isEmpty()) {
        title += " - " + toolInfo;
    }
    setWindowTitle(title);
}

void FX3MainView::updateTransferStatsDisplay(uint64_t bytesTransferred, double transferRate, uint32_t errorCount)
{
    // 更新状态栏
    if (ui.totalBytesLabel) {
        // 格式化数据量显示 (MB或GB)
        QString sizeStr;
        if (bytesTransferred < 1024 * 1024 * 1024) {
            // 显示为MB
            double mbSize = bytesTransferred / (1024.0 * 1024.0);
            sizeStr = QString::number(mbSize, 'f', 2) + " MB";
        }
        else {
            // 显示为GB
            double gbSize = bytesTransferred / (1024.0 * 1024.0 * 1024.0);
            sizeStr = QString::number(gbSize, 'f', 2) + " GB";
        }
        ui.totalBytesLabel->setText(LocalQTCompat::fromLocal8Bit("总数据量：") + sizeStr);
    }

    if (ui.transferRateLabel) {
        // 格式化传输速率显示 (MB/s)
        double mbpsRate = transferRate / (1024.0 * 1024.0);
        ui.transferRateLabel->setText(LocalQTCompat::fromLocal8Bit("速率：") + QString::number(mbpsRate, 'f', 2) + " MB/s");
    }

    // 如果有错误计数标签，也更新它
    QLabel* errorCountLabel = findChild<QLabel*>("errorCountLabel");
    if (errorCountLabel) {
        errorCountLabel->setText(LocalQTCompat::fromLocal8Bit("错误：") + QString::number(errorCount));
    }

    // 更新状态标签页中的统计信息
    QLabel* bytesTransferredValue = findChild<QLabel*>("bytesTransferredValue");
    QLabel* transferRateStatsValue = findChild<QLabel*>("transferRateStatsValue");
    QLabel* errorCountValue = findChild<QLabel*>("errorCountValue");

    if (bytesTransferredValue) {
        // 使用与状态栏相同的格式逻辑
        QString sizeStr;
        if (bytesTransferred < 1024 * 1024 * 1024) {
            double mbSize = bytesTransferred / (1024.0 * 1024.0);
            sizeStr = QString::number(mbSize, 'f', 2) + " MB";
        }
        else {
            double gbSize = bytesTransferred / (1024.0 * 1024.0 * 1024.0);
            sizeStr = QString::number(gbSize, 'f', 2) + " GB";
        }
        bytesTransferredValue->setText(sizeStr);
    }

    if (transferRateStatsValue) {
        double mbpsRate = transferRate / (1024.0 * 1024.0);
        transferRateStatsValue->setText(QString::number(mbpsRate, 'f', 2) + " MB/s");
    }

    if (errorCountValue) {
        errorCountValue->setText(QString::number(errorCount));
    }
}

void FX3MainView::updateUsbSpeedDisplay(const QString& speed)
{
    if (ui.usbSpeedLabel) {
        ui.usbSpeedLabel->setText(LocalQTCompat::fromLocal8Bit("USB速度：") + speed);
    }
}

void FX3MainView::setCommandDirDisplay(const QString& dir)
{
    if (ui.cmdDirEdit) {
        ui.cmdDirEdit->setText(dir);
    }
}

void FX3MainView::updateTransferTimeDisplay(const QString& timeMs)
{
    // TODO: 将时间转成 HH:MM:SS.MS
    if (ui.totalTimeLabel) {
        ui.totalTimeLabel->setText(timeMs);
    }
}

void FX3MainView::setVideoParamsDisplay(uint16_t width, uint16_t height, int format)
{
    if (ui.imageWIdth) {
        ui.imageWIdth->setText(QString::number(width));
    }

    if (ui.imageHeight) {
        ui.imageHeight->setText(QString::number(height));
    }

    if (ui.imageType && format >= 0 && format < ui.imageType->count()) {
        ui.imageType->setCurrentIndex(format);
    }
}

void FX3MainView::updateDeviceInfoDisplay(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber)
{
    QLabel* deviceNameValue = findChild<QLabel*>("deviceNameValue");
    QLabel* firmwareVersionValue = findChild<QLabel*>("firmwareVersionValue");
    QLabel* serialNumberValue = findChild<QLabel*>("serialNumberValue");

    if (deviceNameValue) {
        deviceNameValue->setText(deviceName);
    }

    if (firmwareVersionValue) {
        firmwareVersionValue->setText(firmwareVersion);
    }

    if (serialNumberValue) {
        serialNumberValue->setText(serialNumber);
    }
}

void FX3MainView::resetTransferStatsDisplay()
{
    QLabel* bytesTransferredValue = findChild<QLabel*>("bytesTransferredValue");
    QLabel* transferRateStatsValue = findChild<QLabel*>("transferRateStatsValue");
    QLabel* errorCountValue = findChild<QLabel*>("errorCountValue");

    if (bytesTransferredValue) {
        bytesTransferredValue->setText("0 字节");
    }

    if (transferRateStatsValue) {
        transferRateStatsValue->setText("0 MB/s");
    }

    if (errorCountValue) {
        errorCountValue->setText("0");
    }
}

//--- 模块管理方法 ---//

void FX3MainView::addModuleToMainTab(QWidget* widget, const QString& tabName, int& tabIndex, const QIcon& icon)
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

void FX3MainView::showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon)
{
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

void FX3MainView::removeModuleTab(int& tabIndex)
{
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

//--- UI槽函数 ---//

void FX3MainView::slot_onStartButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始传输按钮点击"));
    emit signal_startButtonClicked();
}

void FX3MainView::slot_onStopButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止传输按钮点击"));
    emit signal_stopButtonClicked();
}

void FX3MainView::slot_onResetButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置设备按钮点击"));
    emit signal_resetButtonClicked();
}

void FX3MainView::slot_onSelectCommandDirClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择命令目录按钮点击"));
    emit signal_selectCommandDirClicked();
}

void FX3MainView::slot_onChannelConfigButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置按钮点击"));
    emit signal_channelConfigButtonClicked();
}

void FX3MainView::slot_onDataAnalysisButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据分析按钮点击"));
    emit signal_dataAnalysisButtonClicked();
}

void FX3MainView::slot_onVideoDisplayButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("视频显示按钮点击"));
    emit signal_videoDisplayButtonClicked();
}

void FX3MainView::slot_onWaveformAnalysisButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析按钮点击"));
    emit signal_waveformAnalysisButtonClicked();
}

void FX3MainView::slot_onSaveFileButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存文件按钮点击"));
    emit signal_saveFileButtonClicked();
}

void FX3MainView::slot_onExportDataButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("导出数据按钮点击"));
    emit signal_exportDataButtonClicked();
}

void FX3MainView::slot_onFileOptionsButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件选项按钮点击"));
    emit signal_FileOptionsButtonClicked();
}

void FX3MainView::slot_onSettingsTriggered()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置菜单触发"));
    emit signal_settingsTriggered();
}

void FX3MainView::slot_onClearLogTriggered()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("清除日志触发"));
    emit signal_clearLogTriggered();
}

void FX3MainView::slot_onHelpContentTriggered()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("帮助内容触发"));
    emit signal_helpContentTriggered();
}

void FX3MainView::slot_onAboutDialogTriggered()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("关于对话框触发"));
    emit signal_aboutDialogTriggered();
}

void FX3MainView::slot_onUpdateDeviceButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级按钮点击"));
    emit signal_updateDeviceButtonClicked();
}

void FX3MainView::slot_onTabCloseRequested(int index)
{
    if (index == m_homeTabIndex) {
        // 主页标签不应关闭
        return;
    }

    // 发送标签页关闭信号
    emit signal_moduleTabClosed(index);
}

void FX3MainView::slot_onShowChannelSelectTriggered() { slot_onChannelConfigButtonClicked(); }
void FX3MainView::slot_onShowUpdataDeviceTriggered() { slot_onUpdateDeviceButtonClicked(); }
void FX3MainView::slot_onShowDataAnalysisTriggered() { slot_onDataAnalysisButtonClicked(); }
void FX3MainView::slot_onShowVideoDisplayTriggered() { slot_onVideoDisplayButtonClicked(); }
void FX3MainView::slot_onShowWaveformAnalysisTriggered() { slot_onWaveformAnalysisButtonClicked(); }
void FX3MainView::slot_onShowSaveFileBoxTriggered() { slot_onSaveFileButtonClicked(); } 
void FX3MainView::slot_onSaveButtonClicked() { slot_onSaveFileButtonClicked(); }
void FX3MainView::slot_onSelectCommandDirectory() { slot_onSelectCommandDirClicked(); }
void FX3MainView::slot_showAboutDialog() { slot_onAboutDialogTriggered(); }
void FX3MainView::slot_onExportDataTriggered() { slot_onExportDataButtonClicked(); }
void FX3MainView::slot_onFileOptionsTriggered() { slot_onFileOptionsButtonClicked(); }

// 转发信号
void FX3MainView::slot_forwardStartButtonSignal() { emit signal_startButtonClicked(); }
void FX3MainView::slot_forwardStopButtonSignal() { emit signal_stopButtonClicked(); }
void FX3MainView::slot_forwardResetButtonSignal() { emit signal_resetButtonClicked(); }
void FX3MainView::slot_forwardChannelConfigButtonSignal() { emit signal_channelConfigButtonClicked(); }
void FX3MainView::slot_forwardDataAnalysisButtonSignal() { emit signal_dataAnalysisButtonClicked(); }
void FX3MainView::slot_forwardVideoDisplayButtonSignal() { emit signal_videoDisplayButtonClicked(); }
void FX3MainView::slot_forwardWaveformAnalysisButtonSignal() { emit signal_waveformAnalysisButtonClicked(); }
void FX3MainView::slot_forwardSaveFileButtonSignal() { emit signal_saveFileButtonClicked(); }
void FX3MainView::slot_forwardExportDataButtonSignal() { emit signal_exportDataButtonClicked(); }
void FX3MainView::slot_forwardFileOptionsButtonSignal() { emit signal_FileOptionsButtonClicked(); }
void FX3MainView::slot_forwardSettingsTriggeredSignal() { emit signal_settingsTriggered(); }
void FX3MainView::slot_forwardSelectCommandDirSignal() { emit signal_selectCommandDirClicked(); }
void FX3MainView::slot_forwardUpdateDeviceButtonSignal() { emit signal_updateDeviceButtonClicked(); }
