#include "VideoDisplay.h"
#include "Logger.h"
#include <QPainter>
#include <QTimer>
#include <QDebug>
#include <climits>

// 静态常量定义
namespace {
    const int MAX_RESOLUTION = 4096;     // 最大分辨率限制
    const int DEFAULT_WIDTH = 1920;      // 默认宽度
    const int DEFAULT_HEIGHT = 1080;     // 默认高度
    const uint8_t DEFAULT_FORMAT = 0x39; // 默认RAW10格式
}

VideoDisplay::VideoDisplay(QWidget* parent)
    : QWidget(parent)
    , m_width(DEFAULT_WIDTH)
    , m_height(DEFAULT_HEIGHT)
    , m_format(DEFAULT_FORMAT)
    , m_isRunning(false)
{
    ui.setupUi(this);

    initializeUI();
    connectSignals();

    LOG_INFO("视频显示窗口已创建");
}

VideoDisplay::~VideoDisplay()
{
    // 如果正在运行，停止显示
    if (m_isRunning) {
        onStopButtonClicked();
    }

    LOG_INFO("视频显示窗口被销毁");
}

void VideoDisplay::initializeUI()
{
    // 设置窗口标题
    setWindowTitle("视频预览");

    // 初始化默认值
    ui.lineEdit->setText(QString::number(m_height));   // 默认高度
    ui.lineEdit_2->setText(QString::number(m_width));  // 默认宽度

    // 禁用停止按钮（未开始显示时）
    ui.pushButton_3->setEnabled(false);

    // 初始状态下选择第一个标签页
    ui.tabWidget->setCurrentIndex(0);

    // 创建初始的空白图像
    m_renderImage = QImage(m_width, m_height, QImage::Format_RGB888);
    m_renderImage.fill(Qt::black); // 初始化为黑色

    // 更新UI状态
    updateUIState();
}

void VideoDisplay::connectSignals()
{
    // 连接按钮信号
    connect(ui.pushButton_2, &QPushButton::clicked, this, &VideoDisplay::onStartButtonClicked);
    connect(ui.pushButton_3, &QPushButton::clicked, this, &VideoDisplay::onStopButtonClicked);
    connect(ui.pushButton, &QPushButton::clicked, this, &VideoDisplay::onExitButtonClicked);

    // 连接色彩模式下拉框信号
    connect(ui.comboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &VideoDisplay::onColorModeChanged);
}

void VideoDisplay::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    LOG_INFO(QString("设置图像参数: 宽度=%1, 高度=%2, 格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));

    // 保存参数
    m_width = width;
    m_height = height;
    m_format = format;

    // 更新UI
    ui.lineEdit_2->setText(QString::number(width));
    ui.lineEdit->setText(QString::number(height));

    // 根据图像格式选择合适的色彩模式
    int colorModeIndex = 0;
    switch (format) {
    case 0x38: // RAW8
        colorModeIndex = 2; // 24Bit RGB
        break;
    case 0x39: // RAW10
        colorModeIndex = 1; // 30Bit RGB
        break;
    case 0x3A: // RAW12
        colorModeIndex = 0; // 36Bit RGB
        break;
    default:
        colorModeIndex = 1; // 默认30Bit RGB
        break;
    }

    // 设置色彩模式下拉框
    ui.comboBox_2->setCurrentIndex(colorModeIndex);

    // 预创建渲染图像
    m_renderImage = QImage(m_width, m_height, QImage::Format_RGB888);
    m_renderImage.fill(Qt::black); // 初始化为黑色

    LOG_INFO("图像参数已更新");
}

void VideoDisplay::updateVideoFrame(const QByteArray& frameData)
{
    // 保存当前帧数据
    m_currentFrameData = frameData;

    // 如果正在运行，则渲染帧
    if (m_isRunning) {
        renderVideoFrame();
    }
}

void VideoDisplay::renderVideoFrame()
{
    // 如果没有数据或不在运行状态，则退出
    if (m_currentFrameData.isEmpty() || !m_isRunning) {
        return;
    }

    LOG_INFO(QString("渲染视频帧: 数据大小=%1字节").arg(m_currentFrameData.size()));

    // 解码数据创建图像
    m_renderImage = decodeRawData(m_currentFrameData);

    // 触发重绘
    update();
}

QImage VideoDisplay::decodeRawData(const QByteArray& data)
{
    // 创建目标图像
    QImage image(m_width, m_height, QImage::Format_RGB888);

    // 根据不同格式解码
    int colorMode = ui.comboBox_2->currentIndex();

    // 根据色彩模式确定每像素字节数
    int bytesPerPixel = 3; // 默认24位RGB

    switch (colorMode) {
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
    int totalPixels = m_width * m_height;
    int requiredBytes = totalPixels * bytesPerPixel;

    if (data.size() < requiredBytes) {
        LOG_WARN(QString("数据量不足以填充完整图像: 需要%1字节，实际%2字节")
            .arg(requiredBytes).arg(data.size()));

        // 用黑色填充的图像
        image.fill(Qt::black);
        return image;
    }

    // 填充图像数据
    for (int y = 0; y < m_height; y++) {
        for (int x = 0; x < m_width; x++) {
            int pixelOffset = (y * m_width + x) * bytesPerPixel;

            if (pixelOffset + 2 < data.size()) {
                // 根据不同的色彩模式处理
                if (bytesPerPixel >= 3) {
                    // 24位以上的RGB模式
                    uchar r = static_cast<uchar>(data[pixelOffset]);
                    uchar g = static_cast<uchar>(data[pixelOffset + 1]);
                    uchar b = static_cast<uchar>(data[pixelOffset + 2]);

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

void VideoDisplay::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    // 如果不在运行状态，不需要绘制
    if (!m_isRunning) {
        return;
    }

    // 获取视频预览区域
    QRect targetRect = ui.frame->geometry();

    // 创建绘图器
    QPainter painter(this);

    // 计算保持宽高比的绘制区域
    QRect drawRect = targetRect;

    if (m_renderImage.width() > 0 && m_renderImage.height() > 0) {
        // 计算宽高比
        double imgRatio = static_cast<double>(m_renderImage.width()) / m_renderImage.height();
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
        painter.drawImage(drawRect, m_renderImage);
    }
}

void VideoDisplay::updateUIState()
{
    // 根据当前状态更新UI控件状态
    bool isRunning = m_isRunning;

    // 启动/停止按钮状态
    ui.pushButton_2->setEnabled(!isRunning);
    ui.pushButton_3->setEnabled(isRunning);

    // 如果正在运行，禁用参数修改
    ui.lineEdit->setReadOnly(isRunning);
    ui.lineEdit_2->setReadOnly(isRunning);
    ui.comboBox_2->setEnabled(!isRunning);
    ui.comboBox_3->setEnabled(!isRunning);
    ui.comboBox_4->setEnabled(!isRunning);
}

void VideoDisplay::onStartButtonClicked()
{
    LOG_INFO("开始视频显示按钮点击");

    // 获取并验证参数
    bool ok = false;
    int width = ui.lineEdit_2->text().toInt(&ok);
    if (!ok || width <= 0 || width > MAX_RESOLUTION) {
        QMessageBox::warning(this,
            "参数错误",
            QString("无效的视频宽度，请输入1-%1之间的数值").arg(MAX_RESOLUTION));
        return;
    }

    ok = false;
    int height = ui.lineEdit->text().toInt(&ok);
    if (!ok || height <= 0 || height > MAX_RESOLUTION) {
        QMessageBox::warning(this,
            "参数错误",
            QString("无效的视频高度，请输入1-%1之间的数值").arg(MAX_RESOLUTION));
        return;
    }

    // 更新参数
    m_width = width;
    m_height = height;

    // 创建空白图像
    m_renderImage = QImage(m_width, m_height, QImage::Format_RGB888);
    m_renderImage.fill(Qt::black);

    // 设置为运行状态
    m_isRunning = true;

    // 更新UI状态
    updateUIState();

    // 触发重绘
    update();

    // 发送状态变更信号
    emit videoDisplayStatusChanged(true);

    LOG_INFO(QString("视频显示已启动: 分辨率=%1x%2").arg(m_width).arg(m_height));
}

void VideoDisplay::onStopButtonClicked()
{
    LOG_INFO("停止视频显示按钮点击");

    // 设置为非运行状态
    m_isRunning = false;

    // 更新UI状态
    updateUIState();

    // 发送状态变更信号
    emit videoDisplayStatusChanged(false);

    LOG_INFO("视频显示已停止");
}

void VideoDisplay::onExitButtonClicked()
{
    LOG_INFO("退出视频显示按钮点击");

    // 如果正在运行，先停止
    if (m_isRunning) {
        onStopButtonClicked();
    }

    // 关闭窗口
    close();
}

void VideoDisplay::onColorModeChanged(int index)
{
    LOG_INFO(QString("色彩模式已更改为: %1").arg(ui.comboBox_2->currentText()));

    // 如果正在显示，更新渲染
    if (m_isRunning && !m_currentFrameData.isEmpty()) {
        renderVideoFrame();
    }
}