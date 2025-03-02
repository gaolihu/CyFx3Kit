// FX3UILayoutManager.cpp
#include "FX3UILayoutManager.h"
#include "FX3ToolMainWin.h"
#include "Logger.h"

FX3UILayoutManager::FX3UILayoutManager(FX3ToolMainWin* mainWindow)
    : m_mainWindow(mainWindow)
    , m_tabWidget(nullptr)
    , m_mainSplitter(nullptr)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("UI布局管理器已初始化"));
}

void FX3UILayoutManager::initializeMainLayout()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化主界面布局"));

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, m_mainWindow);

    // 创建左侧分割器
    QSplitter* leftSplitter = new QSplitter(Qt::Vertical, m_mainWindow);

    // 1. 创建控制面板
    QWidget* controlPanel = createControlPanel();

    // 2. 创建状态面板
    QWidget* statusPanel = createStatusPanel();

    // 添加控制面板和状态面板到左侧分割器
    leftSplitter->addWidget(controlPanel);
    leftSplitter->addWidget(statusPanel);

    // 设置左侧分割比例
    leftSplitter->setStretchFactor(0, 3);
    leftSplitter->setStretchFactor(1, 2);

    // 创建标签页控件
    m_tabWidget = new QTabWidget(m_mainWindow);
    m_tabWidget->setTabsClosable(true);  // 允许关闭标签页
    m_tabWidget->setDocumentMode(true);  // 使用文档模式外观
    m_tabWidget->setMovable(true);       // 允许拖动标签页

    // 添加主页标签
    QWidget* homeTab = createHomeTabContent();
    int homeTabIndex = m_tabWidget->addTab(homeTab, QIcon(":/icons/home.png"),
        LocalQTCompat::fromLocal8Bit("主页"));

    // 连接标签页关闭信号
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, m_mainWindow, [this, homeTabIndex](int index) {
        // 主页不能关闭
        if (index == homeTabIndex) {
            return;
        }

        // 移除标签页
        m_tabWidget->removeTab(index);
        });

    // 创建日志区域
    QWidget* logContainer = createLogPanel();

    // 添加主要部分到主分割器
    m_mainSplitter->addWidget(leftSplitter);
    m_mainSplitter->addWidget(m_tabWidget);
    m_mainSplitter->addWidget(logContainer);

    // 设置主分割比例
    m_mainSplitter->setStretchFactor(0, 2);  // 左侧控制面板
    m_mainSplitter->setStretchFactor(1, 5);  // 中间内容区域
    m_mainSplitter->setStretchFactor(2, 3);  // 右侧日志区域

    // 设置为中央控件
    m_mainWindow->setCentralWidget(m_mainSplitter);

    // 创建工具栏
    createToolBar();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("主界面布局初始化完成"));
}

QWidget* FX3UILayoutManager::createControlPanel()
{
    QWidget* controlPanel = new QWidget(m_mainWindow);
    QVBoxLayout* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(6, 6, 6, 6);
    controlLayout->setSpacing(8);

    // 控制面板标题
    QLabel* controlTitle = new QLabel(LocalQTCompat::fromLocal8Bit("设备控制"), m_mainWindow);
    QFont titleFont = controlTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    controlTitle->setFont(titleFont);
    controlTitle->setAlignment(Qt::AlignCenter);
    controlLayout->addWidget(controlTitle);

    // 从主窗口找到控制按钮
    QPushButton* startButton = m_mainWindow->findChild<QPushButton*>("startButton");
    QPushButton* stopButton = m_mainWindow->findChild<QPushButton*>("stopButton");
    QPushButton* resetButton = m_mainWindow->findChild<QPushButton*>("resetButton");

    // 添加设备控制按钮组
    QFrame* buttonFrame = new QFrame(m_mainWindow);
    buttonFrame->setFrameShape(QFrame::StyledPanel);
    buttonFrame->setFrameShadow(QFrame::Raised);
    QVBoxLayout* buttonLayout = new QVBoxLayout(buttonFrame);

    // 按钮样式设置
    QString buttonStyle = "QPushButton { min-height: 30px; }";
    startButton->setStyleSheet(buttonStyle);
    stopButton->setStyleSheet(buttonStyle);
    resetButton->setStyleSheet(buttonStyle);

    // 添加按钮到布局
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(stopButton);
    buttonLayout->addWidget(resetButton);

    // 添加按钮框架到控制面板
    controlLayout->addWidget(buttonFrame);

    // 参数设置组
    QGroupBox* paramBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("图像参数"), m_mainWindow);
    QGridLayout* paramLayout = new QGridLayout(paramBox);

    // 从主窗口找到参数控件
    QLineEdit* widthEdit = m_mainWindow->findChild<QLineEdit*>("imageWIdth");
    QLineEdit* heightEdit = m_mainWindow->findChild<QLineEdit*>("imageHeight");
    QComboBox* typeCombo = m_mainWindow->findChild<QComboBox*>("imageType");

    // 水平布局包装每组参数
    QHBoxLayout* widthLayout = new QHBoxLayout();
    QLabel* widthLabel = new QLabel(LocalQTCompat::fromLocal8Bit("宽度:"), m_mainWindow);
    widthLayout->addWidget(widthLabel);
    widthLayout->addWidget(widthEdit);
    paramLayout->addLayout(widthLayout, 0, 0);

    QHBoxLayout* heightLayout = new QHBoxLayout();
    QLabel* heightLabel = new QLabel(LocalQTCompat::fromLocal8Bit("高度:"), m_mainWindow);
    heightLayout->addWidget(heightLabel);
    heightLayout->addWidget(heightEdit);
    paramLayout->addLayout(heightLayout, 1, 0);

    QHBoxLayout* typeLayout = new QHBoxLayout();
    QLabel* typeLabel = new QLabel(LocalQTCompat::fromLocal8Bit("类型:"), m_mainWindow);
    typeLayout->addWidget(typeLabel);
    typeLayout->addWidget(typeCombo);
    paramLayout->addLayout(typeLayout, 2, 0);

    controlLayout->addWidget(paramBox);

    // 添加功能模块快速访问区
    QGroupBox* quickAccessBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("功能模块"), m_mainWindow);
    QGridLayout* quickAccessLayout = new QGridLayout(quickAccessBox);

    // 创建功能按钮
    QPushButton* channelBtn = new QPushButton(QIcon(":/icons/channel.png"),
        LocalQTCompat::fromLocal8Bit("通道配置"), m_mainWindow);
    QPushButton* dataAnalysisBtn = new QPushButton(QIcon(":/icons/analysis.png"),
        LocalQTCompat::fromLocal8Bit("数据分析"), m_mainWindow);
    QPushButton* videoDisplayBtn = new QPushButton(QIcon(":/icons/video.png"),
        LocalQTCompat::fromLocal8Bit("视频显示"), m_mainWindow);
    QPushButton* waveformBtn = new QPushButton(QIcon(":/icons/waveform.png"),
        LocalQTCompat::fromLocal8Bit("波形分析"), m_mainWindow);
    QPushButton* saveFileBtn = new QPushButton(QIcon(":/icons/save.png"),
        LocalQTCompat::fromLocal8Bit("文件保存"), m_mainWindow);

    // 设置按钮样式
    channelBtn->setStyleSheet(buttonStyle);
    dataAnalysisBtn->setStyleSheet(buttonStyle);
    videoDisplayBtn->setStyleSheet(buttonStyle);
    waveformBtn->setStyleSheet(buttonStyle);
    saveFileBtn->setStyleSheet(buttonStyle);

    // 设置对象名，便于主窗口连接信号
    channelBtn->setObjectName("quickChannelBtn");
    dataAnalysisBtn->setObjectName("quickDataBtn");
    videoDisplayBtn->setObjectName("quickVideoBtn");
    waveformBtn->setObjectName("quickWaveformBtn");
    saveFileBtn->setObjectName("quickSaveBtn");

    // 添加按钮到网格布局
    quickAccessLayout->addWidget(channelBtn, 0, 0);
    quickAccessLayout->addWidget(dataAnalysisBtn, 0, 1);
    quickAccessLayout->addWidget(videoDisplayBtn, 1, 0);
    quickAccessLayout->addWidget(waveformBtn, 1, 1);
    quickAccessLayout->addWidget(saveFileBtn, 2, 0, 1, 2);

    controlLayout->addWidget(quickAccessBox);

    // 添加伸缩空间
    controlLayout->addStretch();

    return controlPanel;
}

QWidget* FX3UILayoutManager::createStatusPanel()
{
    QWidget* statusPanel = new QWidget(m_mainWindow);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusPanel);
    statusLayout->setContentsMargins(4, 4, 4, 4);
    statusLayout->setSpacing(4);

    // 状态面板标题
    QLabel* statusTitle = new QLabel(LocalQTCompat::fromLocal8Bit("设备状态"), m_mainWindow);
    QFont titleFont = statusTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    statusTitle->setFont(titleFont);
    statusTitle->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(statusTitle);

    // 从主窗口查找状态标签
    QLabel* usbStatusLabel = m_mainWindow->findChild<QLabel*>("usbStatusLabel");
    QLabel* usbSpeedLabel = m_mainWindow->findChild<QLabel*>("usbSpeedLabel");
    QLabel* transferStatusLabel = m_mainWindow->findChild<QLabel*>("transferStatusLabel");
    QLabel* speedLabel = m_mainWindow->findChild<QLabel*>("speedLabel");
    QLabel* totalBytesLabel = m_mainWindow->findChild<QLabel*>("totalBytesLabel");
    QLabel* totalTimeLabel = m_mainWindow->findChild<QLabel*>("totalTimeLabel");

    // 创建网格布局来整齐显示状态信息
    QGridLayout* statusGridLayout = new QGridLayout();
    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("USB状态:")), 0, 0);
    statusGridLayout->addWidget(usbStatusLabel, 0, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("USB速度:")), 1, 0);
    statusGridLayout->addWidget(usbSpeedLabel, 1, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输状态:")), 2, 0);
    statusGridLayout->addWidget(transferStatusLabel, 2, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输速率:")), 3, 0);
    statusGridLayout->addWidget(speedLabel, 3, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("总字节数:")), 4, 0);
    statusGridLayout->addWidget(totalBytesLabel, 4, 1);

    statusGridLayout->addWidget(new QLabel(LocalQTCompat::fromLocal8Bit("传输时间:")), 5, 0);
    statusGridLayout->addWidget(totalTimeLabel, 5, 1);

    statusLayout->addLayout(statusGridLayout);
    statusLayout->addStretch();

    return statusPanel;
}

QWidget* FX3UILayoutManager::createLogPanel()
{
    QWidget* logContainer = new QWidget(m_mainWindow);
    QVBoxLayout* logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(4, 4, 4, 4);

    // 日志区域标题
    QLabel* logTitle = new QLabel(LocalQTCompat::fromLocal8Bit("系统日志"), m_mainWindow);
    QFont titleFont = logTitle->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    logTitle->setFont(titleFont);
    logTitle->setAlignment(Qt::AlignCenter);
    logLayout->addWidget(logTitle);

    // 从主窗口获取日志控件
    QTextEdit* logTextEdit = m_mainWindow->findChild<QTextEdit*>("logTextEdit");
    logLayout->addWidget(logTextEdit);

    return logContainer;
}

QWidget* FX3UILayoutManager::createHomeTabContent()
{
    QWidget* homeWidget = new QWidget(m_mainWindow);
    QVBoxLayout* homeLayout = new QVBoxLayout(homeWidget);

    // 添加欢迎标题
    QLabel* welcomeTitle = new QLabel(LocalQTCompat::fromLocal8Bit("FX3传输测试工具"), m_mainWindow);
    QFont titleFont;
    titleFont.setBold(true);
    titleFont.setPointSize(16);
    welcomeTitle->setFont(titleFont);
    welcomeTitle->setAlignment(Qt::AlignCenter);
    homeLayout->addWidget(welcomeTitle);

    // 添加版本信息
    QLabel* versionLabel = new QLabel(LocalQTCompat::fromLocal8Bit("V1.0.0 (2025-03)"), m_mainWindow);
    versionLabel->setAlignment(Qt::AlignCenter);
    homeLayout->addWidget(versionLabel);

    // 添加分隔线
    QFrame* line = new QFrame(m_mainWindow);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    homeLayout->addWidget(line);

    // 添加说明文本
    QTextEdit* descriptionEdit = new QTextEdit(m_mainWindow);
    descriptionEdit->setReadOnly(true);
    descriptionEdit->setHtml(LocalQTCompat::fromLocal8Bit(
        "<h3>欢迎使用FX3传输测试工具</h3>"
        "<p>本工具用于FX3设备的数据传输和测试，提供以下功能：</p>"
        "<ul>"
        "<li><b>通道配置：</b> 设置通道参数和使能状态</li>"
        "<li><b>数据分析：</b> 分析采集的数据，提供统计和图表</li>"
        "<li><b>视频显示：</b> 实时显示视频流并调整参数</li>"
        "<li><b>波形分析：</b> 分析信号波形（开发中）</li>"
        "<li><b>文件保存：</b> 保存采集的数据到本地文件</li>"
        "</ul>"
        "<p>使用左侧控制面板控制设备或打开相应的功能模块。设备和传输状态信息将显示在状态栏和左下方状态面板中。</p>"
        "<p>常见设备状态：</p>"
        "<table border='1' cellspacing='0' cellpadding='3'>"
        "<tr><th>状态</th><th>说明</th></tr>"
        "<tr><td>已连接</td><td>设备已连接，可以开始传输</td></tr>"
        "<tr><td>传输中</td><td>设备正在传输数据</td></tr>"
        "<tr><td>已断开</td><td>设备未连接，请检查连接</td></tr>"
        "<tr><td>错误</td><td>设备出现错误，请查看日志</td></tr>"
        "</table>"
    ));
    homeLayout->addWidget(descriptionEdit);

    // 添加功能快速链接
    QGroupBox* quickLinksBox = new QGroupBox(LocalQTCompat::fromLocal8Bit("快速链接"), m_mainWindow);
    QGridLayout* linksLayout = new QGridLayout(quickLinksBox);

    // 创建功能链接按钮
    QPushButton* channelLink = new QPushButton(QIcon(":/icons/channel.png"),
        LocalQTCompat::fromLocal8Bit("通道配置"), m_mainWindow);
    QPushButton* analysisLink = new QPushButton(QIcon(":/icons/analysis.png"),
        LocalQTCompat::fromLocal8Bit("数据分析"), m_mainWindow);
    QPushButton* videoLink = new QPushButton(QIcon(":/icons/video.png"),
        LocalQTCompat::fromLocal8Bit("视频显示"), m_mainWindow);
    QPushButton* saveLink = new QPushButton(QIcon(":/icons/save.png"),
        LocalQTCompat::fromLocal8Bit("文件保存"), m_mainWindow);

    // 设置对象名，便于主窗口连接信号
    channelLink->setObjectName("homeChannelBtn");
    analysisLink->setObjectName("homeDataBtn");
    videoLink->setObjectName("homeVideoBtn");
    saveLink->setObjectName("homeSaveBtn");

    // 添加到布局
    linksLayout->addWidget(channelLink, 0, 0);
    linksLayout->addWidget(analysisLink, 0, 1);
    linksLayout->addWidget(videoLink, 1, 0);
    linksLayout->addWidget(saveLink, 1, 1);

    homeLayout->addWidget(quickLinksBox);
    homeLayout->addStretch();

    return homeWidget;
}

void FX3UILayoutManager::createToolBar()
{
    QToolBar* toolBar = new QToolBar(LocalQTCompat::fromLocal8Bit("主工具栏"), m_mainWindow);
    toolBar->setIconSize(QSize(24, 24));
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    // 添加工具栏按钮
    QAction* startAction = toolBar->addAction(QIcon(":/icons/start.png"),
        LocalQTCompat::fromLocal8Bit("开始传输"));
    QAction* stopAction = toolBar->addAction(QIcon(":/icons/stop.png"),
        LocalQTCompat::fromLocal8Bit("停止传输"));
    QAction* resetAction = toolBar->addAction(QIcon(":/icons/reset.png"),
        LocalQTCompat::fromLocal8Bit("重置设备"));
    toolBar->addSeparator();

    QAction* channelAction = toolBar->addAction(QIcon(":/icons/channel.png"),
        LocalQTCompat::fromLocal8Bit("通道配置"));
    QAction* dataAction = toolBar->addAction(QIcon(":/icons/analysis.png"),
        LocalQTCompat::fromLocal8Bit("数据分析"));
    QAction* videoAction = toolBar->addAction(QIcon(":/icons/video.png"),
        LocalQTCompat::fromLocal8Bit("视频显示"));
    QAction* waveformAction = toolBar->addAction(QIcon(":/icons/waveform.png"),
        LocalQTCompat::fromLocal8Bit("波形分析"));
    toolBar->addSeparator();

    QAction* saveAction = toolBar->addAction(QIcon(":/icons/save.png"),
        LocalQTCompat::fromLocal8Bit("保存文件"));

    // 设置对象名，便于主窗口连接信号
    startAction->setObjectName("toolbarStartAction");
    stopAction->setObjectName("toolbarStopAction");
    resetAction->setObjectName("toolbarResetAction");
    channelAction->setObjectName("toolbarChannelAction");
    dataAction->setObjectName("toolbarDataAction");
    videoAction->setObjectName("toolbarVideoAction");
    waveformAction->setObjectName("toolbarWaveformAction");
    saveAction->setObjectName("toolbarSaveAction");

    // 添加工具栏到主窗口
    m_mainWindow->addToolBar(Qt::TopToolBarArea, toolBar);
}

void FX3UILayoutManager::adjustStatusBar()
{
    QStatusBar* statusBar = m_mainWindow->statusBar();
    if (!statusBar) return;

    statusBar->setMinimumWidth(40);
    statusBar->setMinimumHeight(30);
}