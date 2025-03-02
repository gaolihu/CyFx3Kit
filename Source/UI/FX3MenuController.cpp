// FX3MenuController.cpp
#include "FX3MenuController.h"
#include "FX3ToolMainWin.h"
#include "Logger.h"

FX3MenuController::FX3MenuController(FX3ToolMainWin* mainWindow)
    : m_mainWindow(mainWindow)
{
    setupMenuBar();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单控制器已初始化"));
}

void FX3MenuController::setupMenuBar()
{
    // 获取菜单栏
    QMenuBar* menuBar = m_mainWindow->menuBar();
    if (!menuBar) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("未找到菜单栏"));
        return;
    }

    // 文件菜单
    QMenu* fileMenu = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("文件(&F)"));
    QAction* openAction = fileMenu->addAction(LocalQTCompat::fromLocal8Bit("打开命令文件(&O)..."));
    QAction* saveAction = fileMenu->addAction(LocalQTCompat::fromLocal8Bit("保存数据(&S)..."));
    QAction* exportAction = fileMenu->addAction(LocalQTCompat::fromLocal8Bit("导出数据(&E)..."));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(LocalQTCompat::fromLocal8Bit("退出(&X)"));

    // 设备菜单
    QMenu* deviceMenu = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("设备(&D)"));
    QAction* startAction = deviceMenu->addAction(LocalQTCompat::fromLocal8Bit("开始传输(&S)"));
    QAction* stopAction = deviceMenu->addAction(LocalQTCompat::fromLocal8Bit("停止传输(&T)"));
    QAction* resetAction = deviceMenu->addAction(LocalQTCompat::fromLocal8Bit("重置设备(&R)"));
    deviceMenu->addSeparator();
    QAction* updateAction = deviceMenu->addAction(LocalQTCompat::fromLocal8Bit("设备升级(&U)..."));

    // 视图菜单
    QMenu* viewMenu = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("视图(&V)"));
    QAction* channelAction = viewMenu->addAction(LocalQTCompat::fromLocal8Bit("通道配置(&C)"));
    QAction* dataAction = viewMenu->addAction(LocalQTCompat::fromLocal8Bit("数据分析(&D)"));
    QAction* videoAction = viewMenu->addAction(LocalQTCompat::fromLocal8Bit("视频显示(&V)"));
    QAction* waveformAction = viewMenu->addAction(LocalQTCompat::fromLocal8Bit("波形分析(&W)"));

    // 工具菜单
    QMenu* toolsMenu = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("工具(&T)"));
    QAction* settingsAction = toolsMenu->addAction(LocalQTCompat::fromLocal8Bit("设置(&S)..."));
    QAction* clearLogAction = toolsMenu->addAction(LocalQTCompat::fromLocal8Bit("清除日志(&C)"));

    // 帮助菜单
    QMenu* helpMenu = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("帮助(&H)"));
    QAction* helpContentAction = helpMenu->addAction(LocalQTCompat::fromLocal8Bit("帮助内容(&H)..."));
    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction(LocalQTCompat::fromLocal8Bit("关于(&A)..."));

    // 存储菜单动作
    m_menuActions["open"] = openAction;
    m_menuActions["save"] = saveAction;
    m_menuActions["export"] = exportAction;
    m_menuActions["exit"] = exitAction;
    m_menuActions["start"] = startAction;
    m_menuActions["stop"] = stopAction;
    m_menuActions["reset"] = resetAction;
    m_menuActions["update"] = updateAction;
    m_menuActions["channel"] = channelAction;
    m_menuActions["data"] = dataAction;
    m_menuActions["video"] = videoAction;
    m_menuActions["waveform"] = waveformAction;
    m_menuActions["settings"] = settingsAction;
    m_menuActions["clearLog"] = clearLogAction;
    m_menuActions["help"] = helpContentAction;
    m_menuActions["about"] = aboutAction;

    // 连接信号槽
    connect(exitAction, &QAction::triggered, m_mainWindow, &QWidget::close);

    // 设置初始状态
    updateMenuBarState(AppState::IDLE);
}

void FX3MenuController::updateMenuBarState(AppState state)
{
    // 根据状态设置菜单项启用/禁用
    bool transferring = (state == AppState::TRANSFERRING);
    bool deviceConnected = (state != AppState::DEVICE_ABSENT && state != AppState::DEVICE_ERROR);
    bool idle = (state == AppState::IDLE || state == AppState::CONFIGURED);

    // 传输控制
    if (m_menuActions.contains("start"))
        m_menuActions["start"]->setEnabled(idle && deviceConnected);

    if (m_menuActions.contains("stop"))
        m_menuActions["stop"]->setEnabled(transferring);

    if (m_menuActions.contains("reset"))
        m_menuActions["reset"]->setEnabled(deviceConnected && !transferring);

    // 功能模块
    if (m_menuActions.contains("channel"))
        m_menuActions["channel"]->setEnabled(deviceConnected && !transferring);

    if (m_menuActions.contains("data"))
        m_menuActions["data"]->setEnabled(deviceConnected);

    if (m_menuActions.contains("video"))
        m_menuActions["video"]->setEnabled(deviceConnected);

    // 文件操作
    if (m_menuActions.contains("save"))
        m_menuActions["save"]->setEnabled(idle);

    if (m_menuActions.contains("export"))
        m_menuActions["export"]->setEnabled(idle);
}

void FX3MenuController::onMenuAction(QAction* action)
{
    // 根据触发的动作执行相应的操作
    if (action == m_menuActions["start"]) {
        emit menuActionTriggered("start");
    }
    else if (action == m_menuActions["stop"]) {
        emit menuActionTriggered("stop");
    }
    else if (action == m_menuActions["reset"]) {
        emit menuActionTriggered("reset");
    }
    else if (action == m_menuActions["channel"]) {
        emit menuActionTriggered("channel");
    }
    else if (action == m_menuActions["data"]) {
        emit menuActionTriggered("data");
    }
    else if (action == m_menuActions["video"]) {
        emit menuActionTriggered("video");
    }
    else if (action == m_menuActions["waveform"]) {
        emit menuActionTriggered("waveform");
    }
    else if (action == m_menuActions["save"]) {
        emit menuActionTriggered("save");
    }
    else if (action == m_menuActions["export"]) {
        emit menuActionTriggered("export");
    }
    else if (action == m_menuActions["settings"]) {
        emit menuActionTriggered("settings");
    }
    else if (action == m_menuActions["clearLog"]) {
        emit menuActionTriggered("clearLog");
    }
    else if (action == m_menuActions["help"]) {
        emit menuActionTriggered("help");
    }
    else if (action == m_menuActions["about"]) {
        emit menuActionTriggered("about");
    }
}