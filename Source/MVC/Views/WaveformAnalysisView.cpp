// Source/MVC/Views/WaveformAnalysisView.cpp
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformAnalysisController.h"
#include "Logger.h"
#include <QPainter>
#include <QResizeEvent>

WaveformAnalysisView::WaveformAnalysisView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::WaveformAnalysisClass)
{
    ui->setupUi(this);

    initializeUI();

    // 创建控制器
    m_controller = std::make_unique<WaveformAnalysisController>(this);

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

    // 确保控件正确初始化
    if (ui->m_resultsTextEdit) {
        ui->m_resultsTextEdit->setReadOnly(true);
        ui->m_resultsTextEdit->setFont(QFont("Courier New", 10));
    }

    // 初始化滑块默认值
    if (ui->m_horizontalScaleSlider) {
        ui->m_horizontalScaleSlider->setValue(50);
    }

    if (ui->m_verticalScaleSlider) {
        ui->m_verticalScaleSlider->setValue(50);
    }
}

void WaveformAnalysisView::setWaveformData(const QVector<double>& xData, const QVector<double>& yData)
{
    if (m_controller) {
        m_controller->setWaveformData(xData, yData);
    }
}

void WaveformAnalysisView::addDataPoint(double x, double y)
{
    if (m_controller) {
        m_controller->addDataPoint(x, y);
    }
}

void WaveformAnalysisView::clearData()
{
    if (m_controller) {
        m_controller->clearData();
    }
}

void WaveformAnalysisView::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    // 创建绘图器
    QPainter painter(this);

    // 获取图表区域
    QRect chartRect;
    if (ui->m_chartView) {
        chartRect = ui->m_chartView->rect();
        // 将坐标系转换为全局坐标
        chartRect.moveTopLeft(ui->m_chartView->mapTo(this, QPoint(0, 0)));
    }
    else {
        // 如果没有专门的图表视图控件，使用整个窗口的中央区域
        chartRect = rect();
        chartRect.adjust(100, 50, -100, -50); // 添加边距
    }

    // 委托给控制器处理绘制
    if (m_controller) {
        m_controller->handlePaintEvent(&painter, chartRect);
    }
}