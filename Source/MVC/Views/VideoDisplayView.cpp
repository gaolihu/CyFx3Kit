// Source/MVC/Views/VideoDisplayView.cpp
#include "VideoDisplayView.h"
#include "ui_VideoDisplay.h"
#include "VideoDisplayController.h"
#include "Logger.h"

#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QWheelEvent>

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
    setMinimumSize(800, 600);

    // 设置焦点策略
    setFocusPolicy(Qt::StrongFocus);
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

int VideoDisplayView::loadFramesByCommandType(uint8_t commandType, int limit)
{
    if (m_controller) {
        return m_controller->loadFramesByCommandType(commandType, limit);
    }
    return 0;
}

int VideoDisplayView::loadFramesByTimeRange(uint64_t startTime, uint64_t endTime)
{
    if (m_controller) {
        return m_controller->loadFramesByTimeRange(startTime, endTime);
    }
    return 0;
}

bool VideoDisplayView::setCurrentFrame(int index)
{
    if (m_controller) {
        return m_controller->setCurrentFrame(index);
    }
    return false;
}

void VideoDisplayView::setAutoPlay(bool enable, int interval)
{
    if (m_controller) {
        m_controller->setAutoPlay(enable, interval);
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

void VideoDisplayView::keyPressEvent(QKeyEvent* event)
{
    // 处理键盘事件
    if (m_controller) {
        switch (event->key()) {
        case Qt::Key_Right:
        case Qt::Key_Down:
        case Qt::Key_Space:
        case Qt::Key_PageDown:
            // 下一帧
            m_controller->moveToNextFrame();
            event->accept();
            break;

        case Qt::Key_Left:
        case Qt::Key_Up:
        case Qt::Key_PageUp:
            // 上一帧
            m_controller->moveToPreviousFrame();
            event->accept();
            break;

        case Qt::Key_P:
            // 播放/暂停
            if (ui->btnPlay->isEnabled()) {
                ui->btnPlay->click();
            }
            else if (ui->btnPause->isEnabled()) {
                ui->btnPause->click();
            }
            event->accept();
            break;

        case Qt::Key_Home:
            // 第一帧
            setCurrentFrame(0);
            event->accept();
            break;

        case Qt::Key_End:
            // 最后一帧
            if (m_controller) {
                if (ui->lblFrameCounter) {
                    QString text = ui->lblFrameCounter->text();
                    int slashPos = text.indexOf('/');
                    if (slashPos > 0) {
                        bool ok = false;
                        int total = text.mid(slashPos + 1).toInt(&ok);
                        if (ok && total > 0) {
                            setCurrentFrame(total - 1);
                        }
                    }
                }
            }
            event->accept();
            break;

        case Qt::Key_Escape:
            // 退出视频显示
            ui->pushButton->click();
            event->accept();
            break;

        default:
            // 其他按键交给基类处理
            QWidget::keyPressEvent(event);
            break;
        }
    }
    else {
        QWidget::keyPressEvent(event);
    }
}

void VideoDisplayView::wheelEvent(QWheelEvent* event)
{
    // 处理鼠标滚轮事件
    if (m_controller) {
        // 向上滚动 = 上一帧，向下滚动 = 下一帧
        if (event->angleDelta().y() > 0) {
            m_controller->moveToPreviousFrame();
        }
        else if (event->angleDelta().y() < 0) {
            m_controller->moveToNextFrame();
        }
        event->accept();
    }
    else {
        QWidget::wheelEvent(event);
    }
}