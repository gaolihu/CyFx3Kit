// Source/MVC/Controllers/VideoDisplayController.cpp
#include "VideoDisplayController.h"
#include "VideoDisplayView.h"
#include "ui_VideoDisplay.h"
#include "VideoDisplayModel.h"
#include "Logger.h"
#include "DataAccessService.h"
// #include "VideoDataProcessor.h"
#include <QMessageBox>

VideoDisplayController::VideoDisplayController(VideoDisplayView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_isInitialized(false)
    , m_isBatchUpdate(false)
    , m_isPlaying(false)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取或创建模型实例
    m_model = VideoDisplayModel::getInstance();

    // 创建播放定时器
    m_playbackTimer = new QTimer(this);
    connect(m_playbackTimer, &QTimer::timeout, this, &VideoDisplayController::onPlaybackTimerTimeout);

    // 初始化命令类型列表
    m_commandTypes = {
        {0x00, "默认"},
        {0x11, "CMD行指令数据"},
        {0x22, "CMD行BTA标志"},
        {0x33, "CMD行ULPS标志"},
        {0x44, "视频预览有效行"},
        {0x55, "复制标识行"},
        {0x66, "命令行指令"},
        {0x77, "FRAME帧开始"},
        {0x88, "监流设备"}
    };
}

VideoDisplayController::~VideoDisplayController()
{
    // 如果视频显示正在运行，停止显示
    if (m_model && m_model->getConfig().isRunning) {
        onStopButtonClicked();
    }

    // 停止播放
    if (m_playbackTimer && m_playbackTimer->isActive()) {
        m_playbackTimer->stop();
    }

    LOG_INFO("视频显示控制器已销毁");
}

bool VideoDisplayController::initialize()
{
    if (m_isInitialized || !m_ui) {
        return false;
    }

    // 连接信号与槽
    connectSignals();

    // 填充命令类型下拉列表
    populateCommandTypeComboBox();

    // 应用模型数据到UI
    applyModelToUI();

    // 更新UI状态
    updateUIState();

    m_isInitialized = true;
    LOG_INFO("视频显示控制器已初始化");
    return true;
}

void VideoDisplayController::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    if (!m_model) {
        return;
    }

    LOG_INFO(QString("设置图像参数: 宽度=%1, 高度=%2, 格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 更新参数
    config.width = width;
    config.height = height;
    config.format = format;

    // 根据图像格式选择合适的色彩模式
    switch (format) {
    case 0x38: // RAW8
        config.colorMode = 2; // 24Bit RGB
        break;
    case 0x39: // RAW10
        config.colorMode = 1; // 30Bit RGB
        break;
    case 0x3A: // RAW12
        config.colorMode = 0; // 36Bit RGB
        break;
    default:
        config.colorMode = 1; // 默认30Bit RGB
        break;
    }

    // 更新模型
    m_model->setConfig(config);

    // 更新UI
    m_isBatchUpdate = true;
    applyModelToUI();
    m_isBatchUpdate = false;
}

void VideoDisplayController::updateVideoFrame(const QByteArray& frameData)
{
    if (!m_model) {
        return;
    }

    // 更新模型中的帧数据
    m_model->setFrameData(frameData);

    // 如果正在运行，渲染新帧
    if (m_model->getConfig().isRunning) {
        renderVideoFrame();
    }
}

void VideoDisplayController::handlePaintEvent(QPainter* painter)
{
    if (!m_model || !m_ui || !painter) {
        return;
    }

    // 如果不在运行状态，不需要绘制
    if (!m_model->getConfig().isRunning) {
        return;
    }

    // 获取视频预览区域
    QRect targetRect = m_ui->frame->geometry();

    // 获取当前渲染图像
    QImage renderImage = m_model->getRenderImage();

    // 检查图像是否有效
    if (renderImage.isNull() || renderImage.width() <= 0 || renderImage.height() <= 0) {
        return;
    }

    // 计算保持宽高比的绘制区域
    QRect drawRect = targetRect;

    // 计算宽高比
    double imgRatio = static_cast<double>(renderImage.width()) / renderImage.height();
    double frameRatio = static_cast<double>(targetRect.width()) / targetRect.height();

    if (imgRatio > frameRatio) {
        // 图像更宽，以宽度为准
        int newHeight = static_cast<int>(targetRect.width() / imgRatio);
        drawRect.setHeight(newHeight);
        drawRect.moveTop(targetRect.top() + (targetRect.height() - newHeight) / 2);
    }
    else {
        // 图像更高，以高度为准
        int newWidth = static_cast<int>(targetRect.height() * imgRatio);
        drawRect.setWidth(newWidth);
        drawRect.moveLeft(targetRect.left() + (targetRect.width() - newWidth) / 2);
    }

    // 绘制图像
    painter->drawImage(drawRect, renderImage);

    // 绘制帧信息
    if (m_model->getCurrentFrameIndex() >= 0) {
        QString frameInfo = QString("帧 %1/%2").arg(m_model->getCurrentFrameIndex() + 1)
            .arg(m_model->getTotalFrames());

        // 获取当前帧的索引条目
        PacketIndexEntry entry = m_model->getCurrentEntry();
        QString timestampInfo = QString("时间戳: %1").arg(entry.timestamp);
        QString commandInfo = QString("命令类型: 0x%1").arg(entry.commandType, 2, 16, QChar('0'));

        // 绘制帧信息文本
        painter->setPen(Qt::white);
        painter->setFont(QFont("Arial", 10));
        painter->fillRect(QRect(10, 10, 300, 60), QColor(0, 0, 0, 128));
        painter->drawText(QRect(15, 15, 290, 20), frameInfo);
        painter->drawText(QRect(15, 35, 290, 20), timestampInfo);
        painter->drawText(QRect(15, 55, 290, 20), commandInfo);
    }
}

int VideoDisplayController::loadFramesByCommandType(uint8_t commandType, int limit)
{
    // 停止当前播放
    if (m_playbackTimer->isActive()) {
        m_playbackTimer->stop();
        m_isPlaying = false;
    }

    LOG_INFO(QString("加载命令类型 0x%1 的帧数据，限制数量: %2")
        .arg(commandType, 2, 16, QChar('0'))
        .arg(limit));

    // 使用DataAccessService查询指定命令类型的数据包
    QVector<PacketIndexEntry> entries;

    try {
        // 创建查询
        IndexQuery query;
        query.featureFilters.append(QString("commandType=%1").arg(commandType));
        if (limit > 0) {
            query.limit = limit;
        }

        // 执行查询
        entries = DataAccessService::getInstance().getIndexAccess()->queryIndex(query);

        if (entries.isEmpty()) {
            LOG_WARN(QString("未找到命令类型为 0x%1 的数据包").arg(commandType, 2, 16, QChar('0')));
            return 0;
        }

        LOG_INFO(QString("找到 %1 个命令类型为 0x%2 的数据包")
            .arg(entries.size())
            .arg(commandType, 2, 16, QChar('0')));

        // 更新模型
        m_model->setLoadedFrames(entries);

        // 加载第一帧
        if (!entries.isEmpty()) {
            m_model->setCurrentFrameIndex(0);
            loadCurrentFrameData();
        }

        // 更新UI状态
        updatePlaybackControls();

        return entries.size();
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载命令类型数据包失败: %1").arg(e.what()));
        return 0;
    }
}

int VideoDisplayController::loadFramesByTimeRange(uint64_t startTime, uint64_t endTime)
{
    // 停止当前播放
    if (m_playbackTimer->isActive()) {
        m_playbackTimer->stop();
        m_isPlaying = false;
    }

    LOG_INFO(QString("加载时间范围 %1 - %2 的帧数据")
        .arg(startTime).arg(endTime));

    // 使用DataAccessService查询指定时间范围的数据包
    QVector<PacketIndexEntry> entries;

    try {
        // 创建查询
        IndexQuery query;
        query.timestampStart = startTime;
        query.timestampEnd = endTime;

        // 如果设置了命令类型过滤
        VideoConfig config = m_model->getConfig();
        if (config.commandType > 0) {
            query.featureFilters.append(QString("commandType=%1").arg(config.commandType));
        }

        // 执行查询
        entries = DataAccessService::getInstance().getIndexAccess()->queryIndex(query);

        if (entries.isEmpty()) {
            LOG_WARN(QString("未找到时间范围 %1 - %2 内的数据包")
                .arg(startTime).arg(endTime));
            return 0;
        }

        LOG_INFO(QString("找到 %1 个时间范围内的数据包").arg(entries.size()));

        // 更新模型
        m_model->setLoadedFrames(entries);

        // 加载第一帧
        if (!entries.isEmpty()) {
            m_model->setCurrentFrameIndex(0);
            loadCurrentFrameData();
        }

        // 更新UI状态
        updatePlaybackControls();

        return entries.size();
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载时间范围数据包失败: %1").arg(e.what()));
        return 0;
    }
}

bool VideoDisplayController::setCurrentFrame(int index)
{
    if (!m_model || !m_model->setCurrentFrameIndex(index)) {
        return false;
    }

    // 加载并渲染帧数据
    return loadCurrentFrameData();
}

bool VideoDisplayController::moveToNextFrame()
{
    if (!m_model || !m_model->moveToNextFrame()) {
        // 如果到达末尾并启用了循环播放，返回第一帧
        if (m_model && m_model->getCurrentFrameIndex() == m_model->getTotalFrames() - 1) {
            return setCurrentFrame(0);
        }
        return false;
    }

    // 加载并渲染新帧
    return loadCurrentFrameData();
}

bool VideoDisplayController::moveToPreviousFrame()
{
    if (!m_model || !m_model->moveToPreviousFrame()) {
        return false;
    }

    // 加载并渲染新帧
    return loadCurrentFrameData();
}

void VideoDisplayController::setAutoPlay(bool enable, int interval)
{
    if (!m_model) {
        return;
    }

    m_isPlaying = enable;

    if (enable) {
        // 计算播放间隔，考虑速度因子
        VideoConfig config = m_model->getConfig();
        int adjustedInterval = interval / config.playbackSpeed;

        // 启动播放定时器
        m_playbackTimer->start(adjustedInterval);
        LOG_INFO(QString("自动播放已启动，间隔: %1 毫秒").arg(adjustedInterval));
    }
    else {
        // 停止播放定时器
        m_playbackTimer->stop();
        LOG_INFO("自动播放已停止");
    }

    // 更新UI状态
    updatePlaybackControls();
}

void VideoDisplayController::onStartButtonClicked()
{
    if (!m_model || !m_ui) {
        return;
    }

    LOG_INFO("开始视频显示按钮点击");

    // 获取并验证参数
    if (!validateConfig()) {
        return;
    }

    // 确保UI数据已经应用到模型
    applyUIToModel();

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 根据UI设置加载数据
    if (config.startTimestamp > 0 || config.endTimestamp > 0) {
        // 根据时间范围加载
        int frameCount = loadFramesByTimeRange(config.startTimestamp, config.endTimestamp);
        if (frameCount == 0) {
            QMessageBox::warning(m_view, "加载失败", "未找到指定时间范围内的数据包");
            return;
        }
    }
    else if (config.commandType > 0) {
        // 根据命令类型加载
        int frameCount = loadFramesByCommandType(config.commandType, 1000);
        if (frameCount == 0) {
            QMessageBox::warning(m_view, "加载失败",
                QString("未找到命令类型为 0x%1 的数据包").arg(config.commandType, 2, 16, QChar('0')));
            return;
        }
    }
    else {
        // 加载默认帧（时间最早的几帧）
        IndexQuery query;
        query.limit = 100;
        auto entries = DataAccessService::getInstance().getIndexAccess()->queryIndex(query);
        if (entries.isEmpty()) {
            QMessageBox::warning(m_view, "加载失败", "未找到可显示的数据包");
            return;
        }

        m_model->setLoadedFrames(entries);
        m_model->setCurrentFrameIndex(0);
        loadCurrentFrameData();
    }

    // 更新运行状态
    config = m_model->getConfig(); // 重新获取配置，因为可能已更新
    config.isRunning = true;
    m_model->setConfig(config);

    // 更新UI状态
    updateUIState();

    // 触发视图重绘
    if (m_view) {
        m_view->update();
    }

    // 发送状态变更信号
    if (m_view) {
        emit m_view->videoDisplayStatusChanged(true);
    }

    LOG_INFO(QString("视频显示已启动: 分辨率=%1x%2").arg(config.width).arg(config.height));

    // 如果配置为自动播放，启动播放
    if (config.autoAdvance) {
        setAutoPlay(true);
    }
}

void VideoDisplayController::onStopButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("停止视频显示按钮点击");

    // 停止播放
    if (m_playbackTimer->isActive()) {
        m_playbackTimer->stop();
        m_isPlaying = false;
    }

    // 更新运行状态
    VideoConfig config = m_model->getConfig();
    config.isRunning = false;
    m_model->setConfig(config);

    // 更新UI状态
    updateUIState();

    // 发送状态变更信号
    if (m_view) {
        emit m_view->videoDisplayStatusChanged(false);
    }

    LOG_INFO("视频显示已停止");
}

void VideoDisplayController::onExitButtonClicked()
{
    LOG_INFO("退出视频显示按钮点击");

    // 如果正在运行，先停止
    if (m_model && m_model->getConfig().isRunning) {
        onStopButtonClicked();
    }

    // 关闭窗口
    if (m_view) {
        m_view->close();
    }
}

void VideoDisplayController::onColorModeChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("色彩模式已更改为: %1").arg(m_ui->comboBox_2->currentText()));

    // 更新模型
    VideoConfig config = m_model->getConfig();
    config.colorMode = index;
    m_model->setConfig(config);

    // 如果正在显示，更新渲染
    if (config.isRunning) {
        renderVideoFrame();
    }
}

void VideoDisplayController::onDataModeChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("数据模式已更改为: %1").arg(index));

    // 更新模型
    VideoConfig config = m_model->getConfig();
    config.dataMode = index;
    m_model->setConfig(config);
}

void VideoDisplayController::onColorArrangementChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("色彩排布已更改为: %1").arg(m_ui->comboBox_4->currentText()));

    // 更新模型
    VideoConfig config = m_model->getConfig();
    config.colorArrangement = index;
    m_model->setConfig(config);

    // 如果正在显示，更新渲染
    if (config.isRunning) {
        renderVideoFrame();
    }
}

void VideoDisplayController::onVirtualChannelChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("虚拟通道已更改为: %1").arg(index));

    // 更新模型
    VideoConfig config = m_model->getConfig();
    config.virtualChannel = index;
    m_model->setConfig(config);
}

void VideoDisplayController::onVideoHeightChanged(const QString& text)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("视频高度已更改为: %1").arg(text));

    // 尝试转换为整数
    bool ok = false;
    int height = text.toInt(&ok);

    if (ok && height > 0 && height <= MAX_RESOLUTION) {
        // 更新模型
        VideoConfig config = m_model->getConfig();
        config.height = height;
        m_model->setConfig(config);
    }
}

void VideoDisplayController::onVideoWidthChanged(const QString& text)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("视频宽度已更改为: %1").arg(text));

    // 尝试转换为整数
    bool ok = false;
    int width = text.toInt(&ok);

    if (ok && width > 0 && width <= MAX_RESOLUTION) {
        // 更新模型
        VideoConfig config = m_model->getConfig();
        config.width = width;
        m_model->setConfig(config);
    }
}

void VideoDisplayController::onCommandTypeChanged(int index)
{
    if (m_isBatchUpdate || !m_model || index < 0 || index >= m_commandTypes.size()) {
        return;
    }

    uint8_t commandType = m_commandTypes[index].code;
    LOG_INFO(QString("命令类型已更改为: 0x%1 - %2")
        .arg(commandType, 2, 16, QChar('0'))
        .arg(m_commandTypes[index].name));

    // 更新模型
    VideoConfig config = m_model->getConfig();
    config.commandType = commandType;
    m_model->setConfig(config);
}

void VideoDisplayController::onStartTimeChanged(const QString& text)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("开始时间戳已更改为: %1").arg(text));

    // 尝试转换为整数
    bool ok = false;
    uint64_t timestamp = text.toULongLong(&ok);

    if (ok) {
        // 更新模型
        VideoConfig config = m_model->getConfig();
        config.startTimestamp = timestamp;
        m_model->setConfig(config);
    }
}

void VideoDisplayController::onEndTimeChanged(const QString& text)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("结束时间戳已更改为: %1").arg(text));

    // 尝试转换为整数
    bool ok = false;
    uint64_t timestamp = text.toULongLong(&ok);

    if (ok) {
        // 更新模型
        VideoConfig config = m_model->getConfig();
        config.endTimestamp = timestamp;
        m_model->setConfig(config);
    }
}

void VideoDisplayController::onPlayButtonClicked()
{
    if (!m_model || m_model->getTotalFrames() == 0) {
        return;
    }

    LOG_INFO("播放按钮点击");

    // 启动自动播放
    VideoConfig config = m_model->getConfig();
    config.autoAdvance = true;
    m_model->setConfig(config);

    setAutoPlay(true);
}

void VideoDisplayController::onPauseButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("暂停按钮点击");

    // 停止自动播放
    VideoConfig config = m_model->getConfig();
    config.autoAdvance = false;
    m_model->setConfig(config);

    setAutoPlay(false);
}

void VideoDisplayController::onNextFrameButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("下一帧按钮点击");

    // 移动到下一帧
    moveToNextFrame();
}

void VideoDisplayController::onPrevFrameButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("上一帧按钮点击");

    // 移动到上一帧
    moveToPreviousFrame();
}

void VideoDisplayController::onSpeedChanged(int value)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("播放速度已更改为: %1").arg(value));

    // 更新模型
    VideoConfig config = m_model->getConfig();
    config.playbackSpeed = value;
    m_model->setConfig(config);

    // 如果正在播放，更新播放间隔
    if (m_isPlaying && m_playbackTimer->isActive()) {
        int baseInterval = 33; // 约30fps
        int adjustedInterval = baseInterval / value;
        m_playbackTimer->setInterval(adjustedInterval);
        LOG_INFO(QString("播放间隔已调整为: %1 毫秒").arg(adjustedInterval));
    }
}

void VideoDisplayController::onPlaybackTimerTimeout()
{
    // 移动到下一帧
    if (!moveToNextFrame()) {
        // 如果无法移动到下一帧，检查是否到达末尾
        if (m_model && m_model->getCurrentFrameIndex() == m_model->getTotalFrames() - 1) {
            // 循环播放 - 返回第一帧
            setCurrentFrame(0);
        }
    }
}

void VideoDisplayController::onConfigChanged(const VideoConfig& config)
{
    // 更新UI
    m_isBatchUpdate = true;
    applyModelToUI();
    m_isBatchUpdate = false;

    // 更新UI状态
    updateUIState();

    // 重绘视图
    if (m_view) {
        m_view->update();
    }
}

void VideoDisplayController::onFrameDataChanged(const QByteArray& data)
{
    // 如果正在运行，更新渲染
    if (m_model && m_model->getConfig().isRunning) {
        renderVideoFrame();
    }
}

void VideoDisplayController::onRenderImageChanged(const QImage& image)
{
    // 重绘视图
    if (m_view) {
        m_view->update();
    }
}

void VideoDisplayController::onCurrentFrameChanged(int index, int total)
{
    // 更新当前帧指示器
    if (m_ui && total > 0) {
        m_ui->lblFrameCounter->setText(QString("%1/%2").arg(index + 1).arg(total));

        // 更新播放控制状态
        m_ui->btnPrevFrame->setEnabled(index > 0);
        m_ui->btnNextFrame->setEnabled(index < total - 1);
    }

    // 重绘视图
    if (m_view) {
        m_view->update();
    }
}

void VideoDisplayController::onCurrentEntryChanged(const PacketIndexEntry& entry)
{
    if (m_ui) {
        // 更新时间戳显示
        m_ui->lblTimestamp->setText(QString("时间戳: %1").arg(entry.timestamp));

        // 更新命令类型显示
        m_ui->lblCommandType->setText(QString("命令类型: 0x%1").arg(entry.commandType, 2, 16, QChar('0')));
    }

    // 加载当前帧数据
    loadCurrentFrameData();
}

void VideoDisplayController::connectSignals()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 连接UI控件信号

    // 色彩模式
    connect(m_ui->comboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VideoDisplayController::onColorModeChanged);

    // 数据模式
    connect(m_ui->comboBox_3, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VideoDisplayController::onDataModeChanged);

    // 色彩排布
    connect(m_ui->comboBox_4, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VideoDisplayController::onColorArrangementChanged);

    // 虚拟通道
    connect(m_ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VideoDisplayController::onVirtualChannelChanged);

    // 视频高度
    connect(m_ui->lineEdit, &QLineEdit::textChanged,
        this, &VideoDisplayController::onVideoHeightChanged);

    // 视频宽度
    connect(m_ui->lineEdit_2, &QLineEdit::textChanged,
        this, &VideoDisplayController::onVideoWidthChanged);

    // 命令类型
    connect(m_ui->cmbCommandType, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VideoDisplayController::onCommandTypeChanged);

    // 开始时间戳
    connect(m_ui->txtStartTime, &QLineEdit::textChanged,
        this, &VideoDisplayController::onStartTimeChanged);

    // 结束时间戳
    connect(m_ui->txtEndTime, &QLineEdit::textChanged,
        this, &VideoDisplayController::onEndTimeChanged);

    // 播放控制
    connect(m_ui->btnPlay, &QPushButton::clicked,
        this, &VideoDisplayController::onPlayButtonClicked);

    connect(m_ui->btnPause, &QPushButton::clicked,
        this, &VideoDisplayController::onPauseButtonClicked);

    connect(m_ui->btnNextFrame, &QPushButton::clicked,
        this, &VideoDisplayController::onNextFrameButtonClicked);

    connect(m_ui->btnPrevFrame, &QPushButton::clicked,
        this, &VideoDisplayController::onPrevFrameButtonClicked);

    // 播放速度
    connect(m_ui->sliderSpeed, &QSlider::valueChanged,
        this, &VideoDisplayController::onSpeedChanged);

    // 连接主要控制按钮
    connect(m_ui->pushButton_2, &QPushButton::clicked,
        this, &VideoDisplayController::onStartButtonClicked);
    connect(m_ui->pushButton_3, &QPushButton::clicked,
        this, &VideoDisplayController::onStopButtonClicked);
    connect(m_ui->pushButton, &QPushButton::clicked,
        this, &VideoDisplayController::onExitButtonClicked);

    // 连接模型信号
    connect(m_model, &VideoDisplayModel::configChanged,
        this, &VideoDisplayController::onConfigChanged);
    connect(m_model, &VideoDisplayModel::frameDataChanged,
        this, &VideoDisplayController::onFrameDataChanged);
    connect(m_model, &VideoDisplayModel::renderImageChanged,
        this, &VideoDisplayController::onRenderImageChanged);
    connect(m_model, &VideoDisplayModel::currentFrameChanged,
        this, &VideoDisplayController::onCurrentFrameChanged);
    connect(m_model, &VideoDisplayModel::currentEntryChanged,
        this, &VideoDisplayController::onCurrentEntryChanged);
}

void VideoDisplayController::updateUIState()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 根据运行状态更新UI控件状态
    bool isRunning = config.isRunning;

    // 启动/停止按钮状态
    m_ui->pushButton_2->setEnabled(!isRunning);
    m_ui->pushButton_3->setEnabled(isRunning);

    // 如果正在运行，禁用参数修改
    m_ui->lineEdit->setReadOnly(isRunning);
    m_ui->lineEdit_2->setReadOnly(isRunning);
    m_ui->comboBox_2->setEnabled(!isRunning);
    m_ui->comboBox_3->setEnabled(!isRunning);
    m_ui->comboBox_4->setEnabled(!isRunning);
    m_ui->comboBox->setEnabled(!isRunning);

    // 新增UI控件状态
    m_ui->cmbCommandType->setEnabled(!isRunning);
    m_ui->txtStartTime->setReadOnly(isRunning);
    m_ui->txtEndTime->setReadOnly(isRunning);

    // 播放控制
    m_ui->framePlayback->setVisible(isRunning);
    m_ui->btnPlay->setEnabled(isRunning && !m_isPlaying);
    m_ui->btnPause->setEnabled(isRunning && m_isPlaying);

    // 根据已加载的帧启用/禁用导航按钮
    bool hasFrames = m_model->getTotalFrames() > 0;
    int currentIndex = m_model->getCurrentFrameIndex();
    m_ui->btnPrevFrame->setEnabled(hasFrames && currentIndex > 0);
    m_ui->btnNextFrame->setEnabled(hasFrames && currentIndex < m_model->getTotalFrames() - 1);
}

bool VideoDisplayController::validateConfig()
{
    if (!m_ui) {
        return false;
    }

    bool isValid = true;
    QString errorMessage;

    // 验证视频高度
    bool ok = false;
    int height = m_ui->lineEdit->text().toInt(&ok);
    if (!ok || height <= 0 || height > MAX_RESOLUTION) {
        isValid = false;
        errorMessage += QString("无效的视频高度，请输入1-%1之间的数值\n").arg(MAX_RESOLUTION);
    }

    // 验证视频宽度
    ok = false;
    int width = m_ui->lineEdit_2->text().toInt(&ok);
    if (!ok || width <= 0 || width > MAX_RESOLUTION) {
        isValid = false;
        errorMessage += QString("无效的视频宽度，请输入1-%1之间的数值\n").arg(MAX_RESOLUTION);
    }

    // 验证时间戳范围
    if (!m_ui->txtStartTime->text().isEmpty() && !m_ui->txtEndTime->text().isEmpty()) {
        ok = false;
        uint64_t startTime = m_ui->txtStartTime->text().toULongLong(&ok);
        if (!ok) {
            isValid = false;
            errorMessage += "无效的开始时间戳\n";
        }

        ok = false;
        uint64_t endTime = m_ui->txtEndTime->text().toULongLong(&ok);
        if (!ok) {
            isValid = false;
            errorMessage += "无效的结束时间戳\n";
        }

        if (startTime > endTime) {
            isValid = false;
            errorMessage += "开始时间戳不能大于结束时间戳\n";
        }
    }

    // 如果验证失败，显示错误消息
    if (!isValid && m_view) {
        QMessageBox::warning(m_view, "参数错误", errorMessage);
    }

    return isValid;
}

void VideoDisplayController::applyModelToUI()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 设置视频参数
    m_ui->lineEdit->setText(QString::number(config.height));
    m_ui->lineEdit_2->setText(QString::number(config.width));

    // 设置色彩模式
    m_ui->comboBox_2->setCurrentIndex(config.colorMode);

    // 设置数据模式
    if (m_ui->comboBox_3->count() > config.dataMode) {
        m_ui->comboBox_3->setCurrentIndex(config.dataMode);
    }

    // 设置色彩排布
    m_ui->comboBox_4->setCurrentIndex(config.colorArrangement);

    // 设置虚拟通道
    m_ui->comboBox->setCurrentIndex(config.virtualChannel);

    // 设置PPS (帧率)
    m_ui->lineEdit_3->setText(QString::number(config.fps, 'f', 1));

    // 设置命令类型
    for (int i = 0; i < m_ui->cmbCommandType->count(); i++) {
        if (m_ui->cmbCommandType->itemData(i).toUInt() == config.commandType) {
            m_ui->cmbCommandType->setCurrentIndex(i);
            break;
        }
    }

    // 设置时间戳
    m_ui->txtStartTime->setText(config.startTimestamp > 0 ? QString::number(config.startTimestamp) : "");
    m_ui->txtEndTime->setText(config.endTimestamp > 0 ? QString::number(config.endTimestamp) : "");

    // 设置播放速度
    m_ui->sliderSpeed->setValue(config.playbackSpeed);

    // 更新播放控制
    updatePlaybackControls();
}

void VideoDisplayController::applyUIToModel()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 获取视频参数
    bool ok = false;
    int height = m_ui->lineEdit->text().toInt(&ok);
    if (ok && height > 0 && height <= MAX_RESOLUTION) {
        config.height = height;
    }

    ok = false;
    int width = m_ui->lineEdit_2->text().toInt(&ok);
    if (ok && width > 0 && width <= MAX_RESOLUTION) {
        config.width = width;
    }

    // 获取色彩模式
    config.colorMode = m_ui->comboBox_2->currentIndex();

    // 获取数据模式
    config.dataMode = m_ui->comboBox_3->currentIndex();

    // 获取色彩排布
    config.colorArrangement = m_ui->comboBox_4->currentIndex();

    // 获取虚拟通道
    config.virtualChannel = m_ui->comboBox->currentIndex();

    // 获取命令类型
    int cmdTypeIndex = m_ui->cmbCommandType->currentIndex();
    if (cmdTypeIndex >= 0 && cmdTypeIndex < m_commandTypes.size()) {
        config.commandType = m_commandTypes[cmdTypeIndex].code;
    }

    // 获取时间戳
    ok = false;
    if (!m_ui->txtStartTime->text().isEmpty()) {
        uint64_t startTime = m_ui->txtStartTime->text().toULongLong(&ok);
        if (ok) {
            config.startTimestamp = startTime;
        }
    }
    else {
        config.startTimestamp = 0;
    }

    ok = false;
    if (!m_ui->txtEndTime->text().isEmpty()) {
        uint64_t endTime = m_ui->txtEndTime->text().toULongLong(&ok);
        if (ok) {
            config.endTimestamp = endTime;
        }
    }
    else {
        config.endTimestamp = 0;
    }

    // 获取播放速度
    config.playbackSpeed = m_ui->sliderSpeed->value();

    // 更新模型
    m_model->setConfig(config);
}

void VideoDisplayController::renderVideoFrame()
{
    if (!m_model) {
        return;
    }

    // 获取当前帧数据
    QByteArray frameData = m_model->getFrameData();

    // 如果没有数据，直接返回
    if (frameData.isEmpty()) {
        return;
    }

    LOG_INFO(QString("渲染视频帧: 数据大小=%1字节").arg(frameData.size()));

    // 获取当前配置和索引条目
    VideoConfig config = m_model->getConfig();
    PacketIndexEntry entry = m_model->getCurrentEntry();

    // 使用VideoDataProcessor处理帧数据（如果可用）
    QImage image;
    try {
        // 首先尝试使用VideoDataProcessor
        // image = VideoDataProcessor::getInstance().processFrame(frameData, entry);

        // 如果处理失败，使用本地解码方法
        if (image.isNull()) {
            image = decodeRawData(frameData);
        }
    }
    catch (...) {
        // 如果出现异常，回退到本地解码方法
        image = decodeRawData(frameData);
    }

    // 更新模型中的渲染图像
    m_model->setRenderImage(image);
}

QImage VideoDisplayController::decodeRawData(const QByteArray& data)
{
    if (!m_model) {
        return QImage();
    }

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 创建目标图像
    QImage image(config.width, config.height, QImage::Format_RGB888);

    // 根据色彩模式确定每像素字节数
    int bytesPerPixel = 3; // 默认24位RGB

    switch (config.colorMode) {
    case 0: // 36Bit RGB
        bytesPerPixel = 5; // 实际是4.5字节，向上取整
        break;
    case 1: // 30Bit RGB
        bytesPerPixel = 4; // 实际是3.75字节，向上取整
        break;
    case 2: // 24Bit RGB
        bytesPerPixel = 3;
        break;
    case 3: // 18Bit RGB
        bytesPerPixel = 3; // 实际是2.25字节，向上取整
        break;
    case 4: // 16Bit RGB
        bytesPerPixel = 2;
        break;
    default:
        bytesPerPixel = 3;
        break;
    }

    // 检查数据量是否足够
    int totalPixels = config.width * config.height;
    int requiredBytes = totalPixels * bytesPerPixel;

    if (data.size() < requiredBytes) {
        LOG_WARN(QString("数据量不足以填充完整图像: 需要%1字节，实际%2字节")
            .arg(requiredBytes).arg(data.size()));

        // 用黑色填充的图像
        image.fill(Qt::black);
        return image;
    }

    // 填充图像数据
    for (int y = 0; y < config.height; y++) {
        for (int x = 0; x < config.width; x++) {
            int pixelOffset = (y * config.width + x) * bytesPerPixel;

            if (pixelOffset + 2 < data.size()) {
                // 根据不同的色彩模式处理
                if (bytesPerPixel >= 3) {
                    // 24位以上的RGB模式
                    uchar r = static_cast<uchar>(data[pixelOffset]);
                    uchar g = static_cast<uchar>(data[pixelOffset + 1]);
                    uchar b = static_cast<uchar>(data[pixelOffset + 2]);

                    // 根据色彩排布交换RGB顺序
                    switch (config.colorArrangement) {
                    case 0: // R-G-B
                        break; // 默认顺序，不需要交换
                    case 1: // R-B-G
                        std::swap(g, b);
                        break;
                    case 2: // G-B-R
                        std::swap(r, g);
                        std::swap(g, b);
                        break;
                    case 3: // G-R-B
                        std::swap(r, g);
                        break;
                    case 4: // B-G-R
                        std::swap(r, b);
                        break;
                    case 5: // B-R-G
                        std::swap(r, b);
                        std::swap(g, b);
                        break;
                    }

                    image.setPixelColor(x, y, QColor(r, g, b));
                }
                else if (bytesPerPixel == 2) {
                    // 16位RGB模式 (假设是5-6-5格式)
                    uchar b1 = static_cast<uchar>(data[pixelOffset]);
                    uchar b2 = static_cast<uchar>(data[pixelOffset + 1]);

                    // 从16位值提取RGB分量
                    int value = (b2 << 8) | b1;
                    int r = ((value >> 11) & 0x1F) << 3;
                    int g = ((value >> 5) & 0x3F) << 2;
                    int b = (value & 0x1F) << 3;

                    image.setPixelColor(x, y, QColor(r, g, b));
                }
            }
        }
    }

    return image;
}

bool VideoDisplayController::loadCurrentFrameData()
{
    if (!m_model) {
        return false;
    }

    // 获取当前索引条目
    PacketIndexEntry entry = m_model->getCurrentEntry();
    if (entry.fileName.isEmpty() || entry.size == 0) {
        LOG_WARN("当前索引条目无效，无法加载帧数据");
        return false;
    }

    try {
        // 使用DataAccessService读取数据
        QByteArray data = DataAccessService::getInstance().readPacketData(entry);

        if (data.isEmpty()) {
            LOG_ERROR(QString("读取帧数据失败: 文件=%1, 偏移=%2, 大小=%3")
                .arg(entry.fileName)
                .arg(entry.fileOffset)
                .arg(entry.size));
            return false;
        }

        // 更新模型中的帧数据
        m_model->setFrameData(data);

        // 渲染新帧
        renderVideoFrame();

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载帧数据异常: %1").arg(e.what()));
        return false;
    }
}

void VideoDisplayController::updatePlaybackControls()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 获取当前配置
    VideoConfig config = m_model->getConfig();

    // 更新帧计数器
    int currentIndex = m_model->getCurrentFrameIndex();
    int totalFrames = m_model->getTotalFrames();

    m_ui->lblFrameCounter->setText(QString("%1/%2").arg(currentIndex + 1).arg(totalFrames));

    // 更新播放控制状态
    m_ui->btnPlay->setEnabled(!m_isPlaying && totalFrames > 0);
    m_ui->btnPause->setEnabled(m_isPlaying);
    m_ui->btnPrevFrame->setEnabled(currentIndex > 0);
    m_ui->btnNextFrame->setEnabled(currentIndex < totalFrames - 1);
    m_ui->sliderSpeed->setValue(config.playbackSpeed);
}

void VideoDisplayController::populateCommandTypeComboBox()
{
    if (!m_ui) {
        return;
    }

    // 清空并填充命令类型下拉列表
    m_ui->cmbCommandType->clear();

    // 添加"全部"选项
    m_ui->cmbCommandType->addItem("全部", 0);

    // 添加各命令类型
    for (const auto& cmd : m_commandTypes) {
        if (cmd.code > 0) { // 跳过默认类型，已经作为"全部"添加
            m_ui->cmbCommandType->addItem(
                QString("0x%1 - %2").arg(cmd.code, 2, 16, QChar('0')).arg(cmd.name),
                cmd.code);
        }
    }
}