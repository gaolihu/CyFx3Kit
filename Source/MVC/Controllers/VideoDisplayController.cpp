// Source/MVC/Controllers/VideoDisplayController.cpp
#include "VideoDisplayController.h"
#include "VideoDisplayView.h"
#include "ui_VideoDisplay.h"
#include "VideoDisplayModel.h"
#include "Logger.h"
#include <QMessageBox>

VideoDisplayController::VideoDisplayController(VideoDisplayView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_isInitialized(false)
    , m_isBatchUpdate(false)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取或创建模型实例
    m_model = VideoDisplayModel::getInstance();
}

VideoDisplayController::~VideoDisplayController()
{
    // 如果视频显示正在运行，停止显示
    if (m_model && m_model->getConfig().isRunning) {
        onStopButtonClicked();
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

    // 更新运行状态
    VideoConfig config = m_model->getConfig();
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
}

void VideoDisplayController::onStopButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("停止视频显示按钮点击");

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

    // 连接按钮
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

    // 解码RAW数据为QImage
    QImage renderImage = decodeRawData(frameData);

    // 更新模型中的渲染图像
    m_model->setRenderImage(renderImage);
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