// Source/MVC/Views/WaveformAnalysisView.cpp
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformAnalysisController.h"
#include "WaveformAnalysisModel.h"
#include "Logger.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>

WaveformAnalysisView::WaveformAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::WaveformAnalysisClass)
    , m_isDragging(false)
{
    ui->setupUi(this);

    // 初始化UI
    initializeUI();

    // 创建控制器
    m_controller = std::make_unique<WaveformAnalysisController>(this);

    // 连接信号和槽
    connectSignals();

    // 初始化控制器
    m_controller->initialize();

    LOG_INFO("波形分析视图已创建");
}

WaveformAnalysisView::~WaveformAnalysisView()
{
    delete ui;
    LOG_INFO("波形分析视图已销毁");
}

void WaveformAnalysisView::initializeUI()
{
    // 设置窗口标题
    setWindowTitle("波形分析");

    // 设置窗口标志
    setWindowFlags(Qt::Dialog);

    // 设置窗口模态
    setWindowModality(Qt::ApplicationModal);

    // 设置窗口大小
    resize(1000, 700);

    // 设置鼠标追踪，以便接收鼠标移动事件
    setMouseTracking(true);
}

void WaveformAnalysisView::connectSignals()
{
    // 连接通道可见性改变信号
    connect(this, &WaveformAnalysisView::channelVisibilityChanged,
        WaveformAnalysisModel::getInstance(), &WaveformAnalysisModel::setChannelEnabled);
}

void WaveformAnalysisView::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    // 创建绘图器
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 获取图表区域
    m_chartRect = ui->m_chartView->rect();
    m_chartRect.moveTopLeft(ui->m_chartView->mapTo(this, QPoint(0, 0)));

    // 委托给控制器处理绘制
    if (m_controller) {
        m_controller->handlePaintEvent(&painter, m_chartRect);
    }
}

void WaveformAnalysisView::mousePressEvent(QMouseEvent* event)
{
    // 只处理图表区域内的鼠标事件
    if (event->button() == Qt::LeftButton && m_chartRect.contains(event->pos())) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }

    QWidget::mousePressEvent(event);
}

void WaveformAnalysisView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        // 通知控制器处理平移
        if (m_controller) {
            m_controller->handlePanDelta(delta.x());
        }
    }

    QWidget::mouseMoveEvent(event);
}

void WaveformAnalysisView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);
    }

    QWidget::mouseReleaseEvent(event);
}

void WaveformAnalysisView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // 双击图表区域添加标记点
    if (event->button() == Qt::LeftButton && m_chartRect.contains(event->pos())) {
        if (m_controller) {
            m_controller->addMarkerAtPosition(event->pos());
        }
    }

    QWidget::mouseDoubleClickEvent(event);
}

void WaveformAnalysisView::wheelEvent(QWheelEvent* event)
{
    // 仅处理图表区域内的滚轮事件
    if (m_chartRect.contains(event->position().toPoint())) {
        if (m_controller) {
            if (event->angleDelta().y() > 0) {
                // 向上滚动，放大
                m_controller->zoomInAtPoint(event->position().toPoint());
            }
            else {
                // 向下滚动，缩小
                m_controller->zoomOutAtPoint(event->position().toPoint());
            }
        }

        event->accept();
    }
    else {
        QWidget::wheelEvent(event);
    }
}

void WaveformAnalysisView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    // 更新图表区域
    m_chartRect = ui->m_chartView->rect();
    m_chartRect.moveTopLeft(ui->m_chartView->mapTo(this, QPoint(0, 0)));

    // 触发重绘
    update();
}