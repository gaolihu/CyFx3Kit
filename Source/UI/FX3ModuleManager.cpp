// FX3ModuleManager.cpp
#include "FX3ModuleManager.h"
#include "FX3ToolMainWin.h"
#include "Logger.h"

FX3ModuleManager::FX3ModuleManager(FX3ToolMainWin* mainWindow)
    : m_mainWindow(mainWindow)
    , m_tabWidget(nullptr)
    , m_channelModule(nullptr)
    , m_dataAnalysisModule(nullptr)
    , m_videoDisplayModule(nullptr)
    , m_saveFileModule(nullptr)
    , m_channelTabIndex(-1)
    , m_dataAnalysisTabIndex(-1)
    , m_videoDisplayTabIndex(-1)
    , m_waveformTabIndex(-1)
{
    // 查找主窗口中的标签控件
    m_tabWidget = mainWindow->findChild<QTabWidget*>();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("模块管理器已初始化"));
}

void FX3ModuleManager::showChannelConfigModule()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示通道配置窗口"));

    // 延迟创建通道选择窗口（单例模式）
    if (!m_channelModule) {
        m_channelModule = new ChannelSelect(m_mainWindow);

        // 修改窗口标记，使其嵌入而非独立窗口
        m_channelModule->setWindowFlags(Qt::Widget);

        // 连接信号
        connect(m_channelModule, &ChannelSelect::channelConfigChanged,
            m_mainWindow, [this](const ChannelConfig& config) {
                emit moduleSignal("channelConfigChanged", QVariant::fromValue(config));
            });
    }

    // 将通道选择模块添加到主标签页
    showModuleTab(m_channelTabIndex, m_channelModule,
        LocalQTCompat::fromLocal8Bit("通道配置"),
        QIcon(":/icons/channel.png"));
}

void FX3ModuleManager::showDataAnalysisModule()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示数据分析窗口"));

    // 延迟创建数据分析窗口（单例模式）
    if (!m_dataAnalysisModule) {
        m_dataAnalysisModule = new DataAnalysis(m_mainWindow);

        // 修改窗口标记，使其嵌入而非独立窗口
        m_dataAnalysisModule->setWindowFlags(Qt::Widget);

        // 连接保存数据和视频预览信号
        connect(m_dataAnalysisModule, &DataAnalysis::saveDataRequested,
            m_mainWindow, [this]() {
                emit moduleSignal("showSaveFileBox", QVariant());
            });

        connect(m_dataAnalysisModule, &DataAnalysis::videoDisplayRequested,
            m_mainWindow, [this]() {
                emit moduleSignal("showVideoDisplay", QVariant());
            });
    }

    // 将数据分析模块添加到主标签页
    showModuleTab(m_dataAnalysisTabIndex, m_dataAnalysisModule,
        LocalQTCompat::fromLocal8Bit("数据分析"),
        QIcon(":/icons/analysis.png"));
}

void FX3ModuleManager::showVideoDisplayModule()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示视频窗口"));

    // 获取图像参数
    QLineEdit* widthEdit = m_mainWindow->findChild<QLineEdit*>("imageWIdth");
    QLineEdit* heightEdit = m_mainWindow->findChild<QLineEdit*>("imageHeight");
    QComboBox* typeCombo = m_mainWindow->findChild<QComboBox*>("imageType");

    if (!widthEdit || !heightEdit || !typeCombo) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法获取图像参数控件"));
        QMessageBox::warning(m_mainWindow,
            LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("无法获取图像参数"));
        return;
    }

    // 验证参数
    bool ok = false;
    uint16_t width = widthEdit->text().toUInt(&ok);
    if (!ok || width <= 0 || width > 4096) {
        QMessageBox::warning(m_mainWindow,
            LocalQTCompat::fromLocal8Bit("参数错误"),
            LocalQTCompat::fromLocal8Bit("无效的图像宽度"));
        return;
    }

    ok = false;
    uint16_t height = heightEdit->text().toUInt(&ok);
    if (!ok || height <= 0 || height > 4096) {
        QMessageBox::warning(m_mainWindow,
            LocalQTCompat::fromLocal8Bit("参数错误"),
            LocalQTCompat::fromLocal8Bit("无效的图像高度"));
        return;
    }

    uint8_t format = 0x39; // 默认RAW10
    switch (typeCombo->currentIndex()) {
    case 0: format = 0x38; break; // RAW8
    case 1: format = 0x39; break; // RAW10
    case 2: format = 0x3A; break; // RAW12
    }

    // 延迟创建视频显示窗口（单例模式）
    if (!m_videoDisplayModule) {
        m_videoDisplayModule = new VideoDisplay(m_mainWindow);

        // 修改窗口标记，使其嵌入而非独立窗口
        m_videoDisplayModule->setWindowFlags(Qt::Widget);

        // 连接视频状态变更信号
        connect(m_videoDisplayModule, &VideoDisplay::videoDisplayStatusChanged,
            m_mainWindow, [this](bool isRunning) {
                emit moduleSignal("videoDisplayStatusChanged", QVariant(isRunning));

                // 更新标签页图标或文本，指示状态
                if (m_tabWidget && m_videoDisplayTabIndex >= 0) {
                    QString tabText = LocalQTCompat::fromLocal8Bit("视频显示");
                    if (isRunning) {
                        tabText += LocalQTCompat::fromLocal8Bit(" [运行中]");
                    }
                    m_tabWidget->setTabText(m_videoDisplayTabIndex, tabText);
                }
            });
    }

    // 设置图像参数
    m_videoDisplayModule->setImageParameters(width, height, format);

    // 将视频显示模块添加到主标签页
    showModuleTab(m_videoDisplayTabIndex, m_videoDisplayModule,
        LocalQTCompat::fromLocal8Bit("视频显示"),
        QIcon(":/icons/video.png"));
}

void FX3ModuleManager::showSaveFileModule()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示文件保存对话框"));

    // 这里直接发射信号，让主窗口处理
    emit moduleSignal("showSaveFileBox", QVariant());
}

void FX3ModuleManager::showWaveformModule()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("显示波形分析窗口"));

    // 这个功能尚未实现，显示提示消息
    QMessageBox::information(m_mainWindow,
        LocalQTCompat::fromLocal8Bit("功能开发中"),
        LocalQTCompat::fromLocal8Bit("波形分析功能正在开发中，敬请期待！"));
}

void FX3ModuleManager::addModuleTab(QWidget* widget, const QString& tabName, int& tabIndex)
{
    if (!m_tabWidget || !widget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("添加模块失败：标签控件或模块窗口为空"));
        return;
    }

    // 检查是否已添加
    if (tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("模块标签页已存在，切换到标签页: %1").arg(tabName));
        return;
    }

    // 添加新标签页
    tabIndex = m_tabWidget->addTab(widget, tabName);
    m_tabWidget->setCurrentIndex(tabIndex);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已添加模块标签页: %1，索引: %2").arg(tabName).arg(tabIndex));
}

void FX3ModuleManager::showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon)
{
    if (!m_tabWidget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("标签控件为空，无法显示模块"));
        return;
    }

    // 如果标签页已存在
    if (tabIndex >= 0 && tabIndex < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("切换到模块标签页: %1").arg(tabName));
    }
    // 否则添加新标签页
    else {
        tabIndex = icon.isNull() ?
            m_tabWidget->addTab(widget, tabName) :
            m_tabWidget->addTab(widget, icon, tabName);

        m_tabWidget->setCurrentIndex(tabIndex);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("已添加新模块标签页: %1，索引: %2").arg(tabName).arg(tabIndex));
    }
}

void FX3ModuleManager::removeModuleTab(int& tabIndex)
{
    if (!m_tabWidget || tabIndex < 0 || tabIndex >= m_tabWidget->count()) {
        return;
    }

    // 保存要删除的标签名称
    QString tabName = m_tabWidget->tabText(tabIndex);

    // 移除标签页
    m_tabWidget->removeTab(tabIndex);
    tabIndex = -1;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已移除模块标签页: %1").arg(tabName));
}