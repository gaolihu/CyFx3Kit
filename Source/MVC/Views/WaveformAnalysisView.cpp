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
#include <QShowEvent>
#include <QMessageBox>

WaveformAnalysisView::WaveformAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::WaveformAnalysisClass)
    , m_controller(nullptr)
    , m_isDragging(false)
{
    // 设置UI
    ui->setupUi(this);

    // 设置窗口标志和模态
    setWindowFlags(Qt::Dialog);
    setWindowModality(Qt::ApplicationModal);

    // 开启鼠标追踪
    setMouseTracking(true);

    // 初始化连接
    connectSignals();

    // 初始化界面状态
    initializeUIState();

    LOG_INFO("波形分析视图已创建");
}

WaveformAnalysisView::~WaveformAnalysisView()
{
    delete ui;
    LOG_INFO("波形分析视图已销毁");
}

void WaveformAnalysisView::setController(WaveformAnalysisController* controller)
{
    m_controller = controller;
}

void WaveformAnalysisView::updateMarkerList(const QVector<int>& markers)
{
    // 清空标记列表
    ui->markerList->clear();

    // 添加标记项
    for (int i = 0; i < markers.size(); ++i) {
        int markerPos = markers[i];
        QString label = QString("标记点 %1: 位置 %2").arg(i + 1).arg(markerPos);
        ui->markerList->addItem(label);
    }
}

void WaveformAnalysisView::setAnalysisResult(const QString& text)
{
    ui->analysisResultText->setText(text);
}

void WaveformAnalysisView::setStatusMessage(const QString& message)
{
    ui->waveformStatusBar->showMessage(message);
}

void WaveformAnalysisView::connectSignals()
{
    // 通道复选框
    connect(ui->channel0Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);
    connect(ui->channel1Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);
    connect(ui->channel2Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);
    connect(ui->channel3Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);

    // 工具栏动作
    connect(ui->actionLoadTestData, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onLoadTestDataTriggered);
    connect(ui->actionZoomIn, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onZoomInTriggered);
    connect(ui->actionZoomOut, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onZoomOutTriggered);
    connect(ui->actionZoomReset, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onZoomResetTriggered);
    connect(ui->actionStartAnalysis, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onStartAnalysisTriggered);
    connect(ui->actionStopAnalysis, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onStopAnalysisTriggered);
    connect(ui->actionExportData, &QAction::triggered, this, &WaveformAnalysisView::slot_WA_V_onExportDataTriggered);

    // 按钮
    connect(ui->analyzeButton, &QPushButton::clicked, this, &WaveformAnalysisView::slot_WA_V_onAnalyzeButtonClicked);
    connect(ui->clearMarkersButton, &QPushButton::clicked, this, &WaveformAnalysisView::slot_WA_V_onClearMarkersButtonClicked);

    // 垂直缩放滑块
    connect(ui->verticalScaleSlider, &QSlider::valueChanged, this, &WaveformAnalysisView::slot_WA_V_onVerticalScaleSliderChanged);

    // 通道可见性变更信号连接到模型
    connect(this, &WaveformAnalysisView::signal_WA_V_channelVisibilityChanged,
        WaveformAnalysisModel::getInstance(), &WaveformAnalysisModel::setChannelEnabled);
}

void WaveformAnalysisView::initializeUIState()
{
    // 设置状态栏初始消息
    ui->waveformStatusBar->showMessage("波形分析就绪");

    // 设置分析结果初始文本
    ui->analysisResultText->clear();

    // 清空标记列表
    ui->markerList->clear();
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
                m_controller->slot_WA_C_zoomInAtPoint(event->position().toPoint());
            }
            else {
                // 向下滚动，缩小
                m_controller->slot_WA_C_zoomOutAtPoint(event->position().toPoint());
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

void WaveformAnalysisView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // 通知控制器当前可见
    if (m_controller) {
        m_controller->setTabVisible(true);
        m_controller->slot_WA_C_handleTabActivated();
    }
}

void WaveformAnalysisView::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    // Notify controller the tab is now hidden
    if (m_controller) {
        m_controller->setTabVisible(false);
    }
}

void WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled(int state)
{
    QCheckBox* sender = qobject_cast<QCheckBox*>(QObject::sender());
    if (!sender)
        return;

    int channel = -1;
    if (sender == ui->channel0Check) channel = 0;
    else if (sender == ui->channel1Check) channel = 1;
    else if (sender == ui->channel2Check) channel = 2;
    else if (sender == ui->channel3Check) channel = 3;

    if (channel != -1) {
        bool visible = (state == Qt::Checked);
        emit signal_WA_V_channelVisibilityChanged(channel, visible);
        update(); // 触发重绘
    }
}

void WaveformAnalysisView::slot_WA_V_onLoadTestDataTriggered()
{
    if (m_controller) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("加载测试数据"));
        m_controller->slot_WA_C_loadData("test_data.bin", 0, 1000);
    }
}

void WaveformAnalysisView::slot_WA_V_onZoomInTriggered()
{
    if (m_controller) {
        m_controller->slot_WA_C_zoomIn();
    }
}

void WaveformAnalysisView::slot_WA_V_onZoomOutTriggered()
{
    if (m_controller) {
        m_controller->slot_WA_C_zoomOut();
    }
}

void WaveformAnalysisView::slot_WA_V_onZoomResetTriggered()
{
    if (m_controller) {
        m_controller->slot_WA_C_zoomReset();
    }
}

void WaveformAnalysisView::slot_WA_V_onStartAnalysisTriggered()
{
    if (m_controller) {
        m_controller->slot_WA_C_startAnalysis();

        // 更新UI状态
        ui->actionStartAnalysis->setEnabled(false);
        ui->actionStopAnalysis->setEnabled(true);
        ui->waveformStatusBar->showMessage("波形分析运行中...");
    }
}

void WaveformAnalysisView::slot_WA_V_onStopAnalysisTriggered()
{
    if (m_controller) {
        m_controller->slot_WA_C_stopAnalysis();

        // 更新UI状态
        ui->actionStartAnalysis->setEnabled(true);
        ui->actionStopAnalysis->setEnabled(false);
        ui->waveformStatusBar->showMessage("波形分析已停止");
    }
}

void WaveformAnalysisView::slot_WA_V_onExportDataTriggered()
{
    QMessageBox::information(this, "导出数据", "导出功能即将实现...");
}

void WaveformAnalysisView::slot_WA_V_onAnalyzeButtonClicked()
{
    WaveformAnalysisModel* model = WaveformAnalysisModel::getInstance();
    if (model) {
        model->analyzeData();
        setAnalysisResult(model->getDataAnalysisResult());
    }
}

void WaveformAnalysisView::slot_WA_V_onClearMarkersButtonClicked()
{
    WaveformAnalysisModel* model = WaveformAnalysisModel::getInstance();
    if (model) {
        model->clearMarkerPoints();
        ui->markerList->clear();
    }
}

void WaveformAnalysisView::slot_WA_V_onVerticalScaleSliderChanged(int value)
{
    if (m_controller) {
        // 计算垂直缩放因子 (0.5-2.0)
        double scaleFactor = 0.5 + (value / 10.0) * 1.5;
        emit signal_WA_V_verticalScaleChanged(value);

        // 更新状态栏显示
        ui->waveformStatusBar->showMessage(QString("垂直缩放: %1").arg(scaleFactor, 0, 'f', 1));

        // 触发重绘
        update();
    }
}