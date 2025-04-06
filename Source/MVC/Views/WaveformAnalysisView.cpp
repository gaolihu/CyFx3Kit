// Source/MVC/Views/WaveformAnalysisView.cpp
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformAnalysisController.h"
#include "WaveformGLWidget.h"
#include "WaveformAnalysisModel.h"
#include "Logger.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMessageBox>
#include <QTimer>

WaveformAnalysisView::WaveformAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::WaveformAnalysisClass)
    , m_controller(nullptr)
    , m_isDragging(false)
    , m_glWidget(nullptr)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始创建波形分析视图"));

    // 设置UI
    ui->setupUi(this);

    // 基本设置
    setWindowFlags(Qt::Dialog);
    setWindowModality(Qt::ApplicationModal);
    setMouseTracking(true);

    // 修改m_chartView的背景以便于调试
    ui->m_chartView->setStyleSheet("background-color: rgba(240, 240, 240, 0.5);"); // 半透明背景便于观察

    // 清理可能存在的布局
    if (ui->m_chartView->layout()) {
        QLayout* oldLayout = ui->m_chartView->layout();
        while (QLayoutItem* item = oldLayout->takeAt(0)) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        delete oldLayout;
    }

    // 创建新的无边距布局
    QVBoxLayout* layout = new QVBoxLayout(ui->m_chartView);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    ui->m_chartView->setLayout(layout);

    // 创建OpenGL波形显示控件
    m_glWidget = new WaveformGLWidget(ui->m_chartView);
    m_glWidget->setMinimumSize(100, 100);

    // 设置关键属性
    m_glWidget->setAttribute(Qt::WA_TranslucentBackground, false);
    m_glWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
    m_glWidget->setAttribute(Qt::WA_NoSystemBackground, true);

    // 重要修改：允许鼠标事件直接传递到GLWidget
    // 不要设置WA_TransparentForMouseEvents，让GLWidget接收鼠标事件
    m_glWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);

    // 确保正确显示
    m_glWidget->setAutoFillBackground(false);
    m_glWidget->setVisible(true);

    // 添加到布局
    layout->addWidget(m_glWidget);

    // 确保在视图层次中位于最上层
    m_glWidget->raise();

    // 连接信号和初始化界面
    connectSignals();
    initializeUIState();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析视图创建完成"));
}

WaveformAnalysisView::~WaveformAnalysisView()
{
    // OpenGL Widget 会由 Qt 自动清理，不需要手动删除
    delete ui;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析视图已销毁"));
}

void WaveformAnalysisView::setController(WaveformAnalysisController* controller)
{
    m_controller = controller;

    // 将控制器设置到OpenGL控件
    if (m_glWidget && m_controller) {
        // m_glWidget->setController(m_controller);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("已将控制器设置到OpenGL控件"));
    }
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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始连接信号和槽"));

    // 通道复选框
    connect(ui->channel0Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);
    connect(ui->channel1Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);
    connect(ui->channel2Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);
    connect(ui->channel3Check, &QCheckBox::stateChanged, this, &WaveformAnalysisView::slot_WA_V_onChannelCheckboxToggled);

    // 工具栏动作
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

    if (m_glWidget) {
        connect(this, &WaveformAnalysisView::signal_WA_V_glMousePressed,
            m_glWidget, &WaveformGLWidget::slot_WF_GL_handleMousePress);
        connect(this, &WaveformAnalysisView::signal_WA_V_glMouseMoved,
            m_glWidget, &WaveformGLWidget::slot_WF_GL_handleMouseMove);
        connect(this, &WaveformAnalysisView::signal_WA_V_glMouseReleased,
            m_glWidget, &WaveformGLWidget::slot_WF_GL_handleMouseRelease);
        connect(this, &WaveformAnalysisView::signal_WA_V_glMouseDoubleClicked,
            m_glWidget, &WaveformGLWidget::slot_WF_GL_handleMouseDoubleClick);
        connect(this, &WaveformAnalysisView::signal_WA_V_glWheelScrolled,
            m_glWidget, &WaveformGLWidget::slot_WF_GL_handleWheelScroll);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("信号和槽连接完成"));
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
    // 首先让基类处理绘制
    QWidget::paintEvent(event);

    // 仅在DEBUG构建中显示详细日志
#ifdef QT_DEBUG
    LOG_INFO(LocalQTCompat::fromLocal8Bit("paintEvent - 事件矩形: (%1, %2, %3, %4)")
        .arg(event->rect().x())
        .arg(event->rect().y())
        .arg(event->rect().width())
        .arg(event->rect().height()));
#endif

    // 获取当前图表区域
    QRect chartRect = ui->m_chartView->geometry();

    // 仅在OpenGL控件外部区域绘制
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 避免在m_chartView区域绘制任何东西
    if (!chartRect.isValid()) {
        chartRect = QRect(0, 0, 0, 0);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("警告: m_chartView几何区域无效"));
    }

    // 只在非图表区域进行普通绘制
    if (m_controller && !chartRect.contains(event->rect())) {
        // m_controller->handlePaintEvent(&painter, chartRect);
    }
}

void WaveformAnalysisView::mousePressEvent(QMouseEvent* event)
{
    // 获取当前图表区域
    QRect chartRect = ui->m_chartView->geometry();

    // 在非图表区域处理鼠标事件
    if (!chartRect.contains(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("鼠标按下在图表区域内 - 位置: (%1, %2)")
        .arg(event->pos().x())
        .arg(event->pos().y()));

    // 记录拖动起点
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->pos();
    }

    // 使用信号通知GL控件，而不是直接转发事件
    if (m_glWidget) {
        QPoint localPos = event->pos() - chartRect.topLeft();
        emit signal_WA_V_glMousePressed(localPos, event->button());
    }

    // 事件已处理
    event->accept();
}

void WaveformAnalysisView::mouseMoveEvent(QMouseEvent* event)
{
    // 获取当前图表区域
    QRect chartRect = ui->m_chartView->geometry();

    // 在非图表区域处理鼠标事件
    if (!chartRect.contains(event->pos())) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // 处理拖动逻辑
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        if (!delta.isNull()) {
            // 记录新位置
            m_lastMousePos = event->pos();

            // 发送信号通知拖动
            emit signal_WA_V_panChanged(delta.x());

            // 请求更新显示
            updateWaveform();
        }
    }

    // 使用信号通知GL控件，而不是直接转发事件
    if (m_glWidget) {
        QPoint localPos = event->pos() - chartRect.topLeft();
        emit signal_WA_V_glMouseMoved(localPos, event->buttons());
    }

    // 事件已处理
    event->accept();
}

void WaveformAnalysisView::mouseReleaseEvent(QMouseEvent* event)
{
    // 结束拖动状态
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
    }

    // 获取当前图表区域
    QRect chartRect = ui->m_chartView->geometry();

    // 非图表区域正常处理
    if (!chartRect.contains(event->pos())) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    // 使用信号通知GL控件，而不是直接转发事件
    if (m_glWidget) {
        QPoint localPos = event->pos() - chartRect.topLeft();
        emit signal_WA_V_glMouseReleased(localPos, event->button());
    }

    // 事件已处理
    event->accept();
}

void WaveformAnalysisView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // 获取当前图表区域
    QRect chartRect = ui->m_chartView->geometry();

    // 在非图表区域处理鼠标双击事件
    if (!chartRect.contains(event->pos())) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("鼠标双击在图表区域内 - 位置: (%1, %2)")
        .arg(event->pos().x())
        .arg(event->pos().y()));

    // 使用信号通知GL控件，而不是直接转发事件
    if (m_glWidget) {
        QPoint localPos = event->pos() - chartRect.topLeft();
        emit signal_WA_V_glMouseDoubleClicked(localPos, event->button());
    }

    // 事件已处理
    event->accept();
}

void WaveformAnalysisView::wheelEvent(QWheelEvent* event)
{
    // 获取当前图表区域
    QRect chartRect = ui->m_chartView->geometry();

    // 在非图表区域处理滚轮事件
    if (!chartRect.contains(event->position().toPoint())) {
        QWidget::wheelEvent(event);
        return;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("滚轮事件在图表区域内 - 位置: (%1, %2)")
        .arg(event->position().toPoint().x())
        .arg(event->position().toPoint().y()));

    // 直接处理缩放逻辑
    if (m_controller) {
        if (event->angleDelta().y() > 0) {
            m_controller->slot_WA_C_zoomIn();
        }
        else {
            m_controller->slot_WA_C_zoomOut();
        }

        // 更新波形显示
        updateWaveform();
    }

    // 使用信号通知GL控件
    if (m_glWidget) {
        QPoint localPos = event->position().toPoint() - chartRect.topLeft();
        emit signal_WA_V_glWheelScrolled(localPos, event->angleDelta());
    }

    // 事件已处理
    event->accept();
}

void WaveformAnalysisView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("resizeEvent - 旧尺寸: (%1 x %2), 新尺寸: (%3 x %4)")
        .arg(event->oldSize().width()).arg(event->oldSize().height())
        .arg(event->size().width()).arg(event->size().height()));

    if (m_glWidget) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("resizeEvent - OpenGL控件几何区域: (%1, %2, %3, %4)")
            .arg(m_glWidget->geometry().x())
            .arg(m_glWidget->geometry().y())
            .arg(m_glWidget->geometry().width())
            .arg(m_glWidget->geometry().height()));

        // 确保OpenGL控件填满m_chartView
        QSize newSize = ui->m_chartView->size();
        if (m_glWidget->size() != newSize) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("调整OpenGL控件大小以匹配m_chartView"));
            m_glWidget->setGeometry(0, 0, newSize.width(), newSize.height());

            // 强制布局更新
            if (ui->m_chartView->layout()) {
                ui->m_chartView->layout()->update();
                ui->m_chartView->layout()->activate();
            }
        }
    }

    // 触发重绘
    update();
}

void WaveformAnalysisView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("showEvent - 视图显示"));

    // 检查OpenGL控件
    if (m_glWidget) {
        // 确保OpenGL控件大小正确
        m_glWidget->setGeometry(0, 0, ui->m_chartView->width(), ui->m_chartView->height());

        // 确保OpenGL控件属性正确
        m_glWidget->setAttribute(Qt::WA_OpaquePaintEvent, true);
        m_glWidget->setAutoFillBackground(false);
        m_glWidget->setVisible(true);
        m_glWidget->raise(); // 确保在顶层

        // 强制布局更新
        if (ui->m_chartView->layout()) {
            ui->m_chartView->layout()->update();
            ui->m_chartView->layout()->activate();
        }

        // 强制OpenGL控件立即更新
        m_glWidget->update();
        m_glWidget->requestUpdate();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("已强制设置OpenGL控件属性以优化渲染"));
    }

    // 通知控制器当前可见
    if (m_controller) {
        m_controller->setTabVisible(true);
        m_controller->slot_WA_C_handleTabActivated();
    }

    // 请求更新
    update();

    // 延迟请求OpenGL控件更新，确保数据已加载
    QTimer::singleShot(50, this, [this]() {
        if (m_glWidget) {
            m_glWidget->update();
            m_glWidget->requestUpdate();
        }
        });
}

void WaveformAnalysisView::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("hideEvent - 视图隐藏"));

    // 通知控制器当前不可见
    if (m_controller) {
        m_controller->setTabVisible(false);
    }
}

void WaveformAnalysisView::updateWaveform()
{
    // 请求OpenGL控件更新波形
    if (m_glWidget && m_glWidget->isVisible()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("请求更新波形"));
        m_glWidget->requestUpdate();
    }
    else if (!m_glWidget) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("错误: 尝试更新波形但OpenGL控件为空"));
    }
    else {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("警告: OpenGL控件不可见，无法更新波形"));
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
        LOG_INFO(LocalQTCompat::fromLocal8Bit("通道 %1 可见性更改为: %2")
            .arg(channel)
            .arg(visible ? "可见" : "不可见"));

        emit signal_WA_V_channelVisibilityChanged(channel, visible);

        // 请求更新OpenGL控件
        updateWaveform();
    }
}

void WaveformAnalysisView::slot_WA_V_onZoomInTriggered()
{
    if (m_controller) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("触发放大操作"));
        m_controller->slot_WA_C_zoomIn();

        // 发出缩放改变信号 - 使用固定值代替不存在的getZoomFactor()
        emit signal_WA_V_zoomChanged(0);

        // 请求更新OpenGL控件
        updateWaveform();
    }
}

void WaveformAnalysisView::slot_WA_V_onZoomOutTriggered()
{
    if (m_controller) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("触发缩小操作"));
        m_controller->slot_WA_C_zoomOut();

        // 发出缩放改变信号 - 使用固定值代替不存在的getZoomFactor()
        emit signal_WA_V_zoomChanged(0);

        // 请求更新OpenGL控件
        updateWaveform();
    }
}

void WaveformAnalysisView::slot_WA_V_onZoomResetTriggered()
{
    if (m_controller) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("触发重置缩放操作"));
        m_controller->slot_WA_C_zoomReset();

        // 发出缩放改变信号 - 使用固定值代替不存在的getZoomFactor()
        emit signal_WA_V_zoomChanged(0);

        // 请求更新OpenGL控件
        updateWaveform();
    }
}

void WaveformAnalysisView::slot_WA_V_onStartAnalysisTriggered()
{
    if (m_controller) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("开始波形分析"));
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
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止波形分析"));
        m_controller->slot_WA_C_stopAnalysis();

        // 更新UI状态
        ui->actionStartAnalysis->setEnabled(true);
        ui->actionStopAnalysis->setEnabled(false);
        ui->waveformStatusBar->showMessage("波形分析已停止");
    }
}

void WaveformAnalysisView::slot_WA_V_onExportDataTriggered()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("尝试导出数据"));
    QMessageBox::information(this, "导出数据", "导出功能即将实现...");
}

void WaveformAnalysisView::slot_WA_V_onAnalyzeButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("分析按钮被点击"));
    WaveformAnalysisModel* model = WaveformAnalysisModel::getInstance();
    if (model) {
        model->analyzeData();
        setAnalysisResult(model->getDataAnalysisResult());
    }
}

void WaveformAnalysisView::slot_WA_V_onClearMarkersButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("清除标记按钮被点击"));
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
        LOG_INFO(LocalQTCompat::fromLocal8Bit("垂直缩放滑块改变为: %1, 缩放因子: %2")
            .arg(value)
            .arg(scaleFactor, 0, 'f', 1));

        m_controller->slot_WA_C_setVerticalScale(scaleFactor);
        emit signal_WA_V_verticalScaleChanged(value);

        // 更新状态栏显示
        ui->waveformStatusBar->showMessage(QString("垂直缩放: %1").arg(scaleFactor, 0, 'f', 1));

        // 请求更新OpenGL控件
        updateWaveform();
    }
}