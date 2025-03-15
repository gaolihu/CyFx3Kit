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
            // 初始化Tab管理
            if (m_uiStateManager && !m_uiStateManager->initializeTabManagement(m_mainTabWidget)) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("Tab管理初始化失败"));
            }
        }

        // 状态、工具条
        m_statusPanel = findChild<QWidget*>("statusPanel");
        m_mainToolBar = findChild<QToolBar*>("mainToolBar");

        // 不再需要初始化信号连接，由UI状态管理器处理
        initializeSignalConnections();

        updateDeviceInfoDisplay("FX cypress高速USB传输设备", "2.1", "SN-");

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("resize事件"));

    QMainWindow::resizeEvent(event);
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
    // 连接快捷按钮
    // 注意：大部分信号连接已移至UiStateManager
}

bool FX3MainView::initializeUiStateManager() {
    try {
        m_uiStateManager = std::make_unique<MainUiStateManager>(ui, this);

        if (!m_uiStateManager->initializeSignalConnections(this)) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("UI状态管理器信号连接失败"));
            return false;
        }

#define CONNECT_SIGNAL(uiSignal, viewSignal) \
            connect(m_uiStateManager.get(), &MainUiStateManager::uiSignal, \
                    this, [this](){ \
                        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("主视图UI管理器发出信号: %1").arg(#uiSignal)); \
                        emit signal_##viewSignal(); \
                    })

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

        // 连接模块Tab关闭信号
        connect(m_uiStateManager.get(), &MainUiStateManager::signal_moduleTabClosed,
            this, &FX3MainView::signal_moduleTabClosed);

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

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置模块按钮信号映射"));

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
    if (m_uiStateManager) {
        m_uiStateManager->showErrorMessage(title, message);
    }
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));
}

void FX3MainView::showWarningMessage(const QString& title, const QString& message)
{
    if (m_uiStateManager) {
        m_uiStateManager->showInfoMessage(title, message);
    }
    LOG_ERROR(QString("%1: %2").arg(title).arg(message));
}

void FX3MainView::showInfoMessage(const QString& title, const QString& message)
{
    if (m_uiStateManager) {
        m_uiStateManager->showInfoMessage(title, message);
    }
    LOG_INFO(QString("%1: %2").arg(title).arg(message));
}

void FX3MainView::showAboutDialog()
{
    const auto title = LocalQTCompat::fromLocal8Bit("关于FX3传输测试工具");
    const auto message =
        LocalQTCompat::fromLocal8Bit("FX3传输测试工具 v3.0\n\n\
用于FX3设备的数据传输和测试\n\n\
  © 2025 公司名称\n\n\
email: lihugao@gmail.com");

    if (m_uiStateManager) {
        m_uiStateManager->showInfoMessage(title, message);
    }
    LOG_INFO(QString("About, title: %1, text: %2").arg(title).arg(message));
}

void FX3MainView::clearLogbox()
{
    if (m_uiStateManager) {
        m_uiStateManager->clearLogbox();
    }
    LOG_INFO("清除日志框");
}

void FX3MainView::updateStatusBar(const QString& message, int timeout)
{
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("更新状态栏: %1, timeout: %2").arg(message).arg(timeout));
    // 交给m_uiStateManager处理, TODO
}

void FX3MainView::updateWindowTitle(const QString& toolInfo)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("更新窗口标题: %1").arg(toolInfo));
    QString title = LocalQTCompat::fromLocal8Bit("FX3传输测试工具");
    if (!toolInfo.isEmpty()) {
        title += " - " + toolInfo;
    }
    setWindowTitle(title);
}

void FX3MainView::updateTransferStatsDisplay(uint64_t bytesTransferred, double transferRate, uint32_t errorCount)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("更新传输状态显示"));
    if (m_uiStateManager) {
        m_uiStateManager->updateTransferStats(bytesTransferred, transferRate, errorCount);
    }
}

void FX3MainView::updateUsbSpeedDisplay(const QString& speed)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("更新USB速度状态"));
    if (m_uiStateManager) {
        m_uiStateManager->updateUsbSpeedDisplay(speed, true);
    }
}

void FX3MainView::setCommandDirDisplay(const QString& dir)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("设置命令目录: %1").arg(dir));
    if (m_uiStateManager) {
        m_uiStateManager->setCommandDirDisplay(dir);
    }
}

void FX3MainView::updateTransferTimeDisplay()
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("更新传输时间"));
    if (m_uiStateManager) {
        m_uiStateManager->updateTransferTimeDisplay();
    }
}

void FX3MainView::setVideoParamsDisplay(uint16_t width, uint16_t height, int format)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("设置视频显示参数, w: %1, h: %2, format: %3").arg(width).arg(height).arg(format));

    if (m_uiStateManager) {
        m_uiStateManager->setVideoParamsDisplay(width, height, format);
    }
}

void FX3MainView::updateDeviceInfoDisplay(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber)
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("更新设备信息, name: %1, version: %2, SN: %3").arg(deviceName).arg(firmwareVersion).arg(serialNumber));

    if (m_uiStateManager) {
        m_uiStateManager->updateDeviceInfoDisplay(deviceName, firmwareVersion, serialNumber);
    }
}

void FX3MainView::resetTransferStatsDisplay()
{
    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("重置传输显示"));
    if (m_uiStateManager) {
        m_uiStateManager->resetTransferStatsDisplay();
    }
}