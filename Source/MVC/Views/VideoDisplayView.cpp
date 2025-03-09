// Source/MVC/Views/VideoDisplayView.cpp
#include "VideoDisplayView.h"
#include "ui_VideoDisplay.h"
#include "VideoDisplayController.h"
#include "Logger.h"

#include <QPainter>
#include <QPaintEvent>

VideoDisplayView::VideoDisplayView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::VideoDisplayClass)
{
    ui->setupUi(this);

    initializeUI();

    // 创建控制器
    m_controller = std::make_unique<VideoDisplayController>(this);

    // 初始化控制器
    m_controller->initialize();

    LOG_INFO("视频显示视图已创建");
}

VideoDisplayView::~VideoDisplayView()
{
    // m_controller 会在 unique_ptr 析构时自动释放，
    // 但确保在UI释放前释放控制器
    m_controller.reset();

    delete ui;
    LOG_INFO("视频显示视图已销毁");
}

void VideoDisplayView::initializeUI()
{
    // 设置窗口标题
    setWindowTitle("视频显示");

    // 设置窗口标志
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinMaxButtonsHint);

    // 设置窗口大小策略
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    // 设置最小大小
    setMinimumSize(640, 480);
}

void VideoDisplayView::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    if (m_controller) {
        m_controller->setImageParameters(width, height, format);
    }
}

void VideoDisplayView::updateVideoFrame(const QByteArray& frameData)
{
    if (m_controller) {
        m_controller->updateVideoFrame(frameData);
    }
}

void VideoDisplayView::paintEvent(QPaintEvent* event)
{
    // 调用基类方法
    QWidget::paintEvent(event);

    // 绘制视频帧
    QPainter painter(this);

    if (m_controller) {
        m_controller->handlePaintEvent(&painter);
    }
}