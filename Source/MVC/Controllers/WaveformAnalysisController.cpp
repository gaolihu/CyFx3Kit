// Source/MVC/Controllers/WaveformAnalysisController.cpp
#include "WaveformAnalysisController.h"
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformConfigModel.h"
#include "Logger.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <cmath>

WaveformAnalysisController::WaveformAnalysisController(WaveformAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_simulationTimer(nullptr)
    , m_isInitialized(false)
    , m_isBatchUpdate(false)
    , m_xMin(0.0)
    , m_xMax(100.0)
    , m_yMin(-5.0)
    , m_yMax(5.0)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取或创建模型实例
    m_model = WaveformConfigModel::getInstance();

    // 初始化模拟数据定时器
    m_simulationTimer = new QTimer(this);
    connect(m_simulationTimer, &QTimer::timeout, this, &WaveformAnalysisController::onUpdateSimulatedData);
}

WaveformAnalysisController::~WaveformAnalysisController()
{
    // 停止模拟数据定时器
    if (m_simulationTimer) {
        m_simulationTimer->stop();
        delete m_simulationTimer;
        m_simulationTimer = nullptr;
    }

    // 如果分析正在运行，停止分析
    if (m_model && m_model->getConfig().isRunning) {
        stopAnalysis();
    }

    LOG_INFO("波形分析控制器已销毁");
}

bool WaveformAnalysisController::initialize()
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
    LOG_INFO("波形分析控制器已初始化");
    return true;
}

void WaveformAnalysisController::handlePaintEvent(QPainter* painter, const QRect& chartRect)
{
    if (!painter || chartRect.isEmpty()) {
        return;
    }

    // 保存绘图器状态
    painter->save();

    // 绘制网格
    if (m_model && m_model->getConfig().gridEnabled) {
        drawGrid(painter, chartRect);
    }

    // 绘制标尺
    drawRulers(painter, chartRect);

    // 绘制波形
    drawWaveform(painter, chartRect);

    // 绘制标记点
    drawMarkers(painter, chartRect);

    // 恢复绘图器状态
    painter->restore();
}

void WaveformAnalysisController::drawGrid(QPainter* painter, const QRect& rect)
{
    // 设置网格样式
    painter->setPen(QPen(QColor(200, 200, 200, 100), 1, Qt::DotLine));

    // 绘制水平线
    int numHLines = 10;
    double hStep = rect.height() / static_cast<double>(numHLines);
    for (int i = 0; i <= numHLines; i++) {
        int y = rect.top() + static_cast<int>(i * hStep);
        painter->drawLine(rect.left(), y, rect.right(), y);
    }

    // 绘制垂直线
    int numVLines = 10;
    double vStep = rect.width() / static_cast<double>(numVLines);
    for (int i = 0; i <= numVLines; i++) {
        int x = rect.left() + static_cast<int>(i * vStep);
        painter->drawLine(x, rect.top(), x, rect.bottom());
    }
}

void WaveformAnalysisController::drawRulers(QPainter* painter, const QRect& rect)
{
    // 设置标尺样式
    painter->setPen(QPen(Qt::black, 1, Qt::SolidLine));

    // 绘制X轴
    painter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());

    // 绘制Y轴
    painter->drawLine(rect.left(), rect.top(), rect.left(), rect.bottom());

    // 绘制X轴刻度
    int numXTicks = 10;
    double xStep = rect.width() / static_cast<double>(numXTicks);
    double xRange = m_xMax - m_xMin;
    double xValueStep = xRange / numXTicks;

    for (int i = 0; i <= numXTicks; i++) {
        int x = rect.left() + static_cast<int>(i * xStep);
        double value = m_xMin + i * xValueStep;

        // 绘制刻度线
        painter->drawLine(x, rect.bottom(), x, rect.bottom() + 5);

        // 绘制刻度值
        QString text = QString::number(value, 'f', 1);
        QRectF textRect(x - 25, rect.bottom() + 6, 50, 20);
        painter->drawText(textRect, Qt::AlignCenter, text);
    }

    // 绘制Y轴刻度
    int numYTicks = 10;
    double yStep = rect.height() / static_cast<double>(numYTicks);
    double yRange = m_yMax - m_yMin;
    double yValueStep = yRange / numYTicks;

    for (int i = 0; i <= numYTicks; i++) {
        int y = rect.bottom() - static_cast<int>(i * yStep);
        double value = m_yMin + i * yValueStep;

        // 绘制刻度线
        painter->drawLine(rect.left(), y, rect.left() - 5, y);

        // 绘制刻度值
        QString text = QString::number(value, 'f', 1);
        QRectF textRect(rect.left() - 50, y - 10, 45, 20);
        painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text);
    }

    // 绘制轴标签
    painter->drawText(rect.left() + rect.width() / 2, rect.bottom() + 25, "时间 (ms)");

    // 创建旋转的Y轴标签
    painter->save();
    painter->translate(rect.left() - 55, rect.top() + rect.height() / 2);
    painter->rotate(-90);
    painter->drawText(-50, 0, 100, 20, Qt::AlignCenter, "电压 (V)");
    painter->restore();
}

void WaveformAnalysisController::drawWaveform(QPainter* painter, const QRect& rect)
{
    if (!m_model) {
        return;
    }

    // 获取波形数据
    QVector<double> xData = m_model->getXData();
    QVector<double> yData = m_model->getYData();

    // 检查数据是否为空
    if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
        return;
    }

    // 计算波形绘制点集
    m_displayPoints.clear();
    calculateWaveformPoints(rect, xData, yData, m_displayPoints);

    // 设置波形样式
    QPen wavePen;
    wavePen.setWidth(2);

    // 根据波形模式选择颜色和样式
    const WaveformConfig& config = m_model->getConfig();
    switch (config.waveformMode) {
    case WaveformMode::ANALOG:
        wavePen.setColor(QColor(0, 120, 215));
        wavePen.setStyle(Qt::SolidLine);
        break;
    case WaveformMode::DIGITAL:
        wavePen.setColor(QColor(0, 153, 0));
        wavePen.setStyle(Qt::SolidLine);
        break;
    case WaveformMode::MIXED:
        wavePen.setColor(QColor(153, 0, 153));
        wavePen.setStyle(Qt::SolidLine);
        break;
    }

    painter->setPen(wavePen);
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 绘制波形
    if (config.waveformMode == WaveformMode::DIGITAL) {
        // 数字波形使用阶梯线
        QPointF prevPoint = m_displayPoints.first();
        for (int i = 1; i < m_displayPoints.size(); ++i) {
            QPointF currPoint = m_displayPoints[i];
            // 绘制水平线
            painter->drawLine(prevPoint, QPointF(currPoint.x(), prevPoint.y()));
            // 绘制垂直线
            painter->drawLine(QPointF(currPoint.x(), prevPoint.y()), currPoint);
            prevPoint = currPoint;
        }
    }
    else {
        // 模拟和混合波形使用连续线
        for (int i = 1; i < m_displayPoints.size(); ++i) {
            painter->drawLine(m_displayPoints[i - 1], m_displayPoints[i]);
        }
    }
}

void WaveformAnalysisController::drawMarkers(QPainter* painter, const QRect& rect)
{
    if (!m_model) {
        return;
    }

    // 获取标记点数据
    QVector<double> markerXData = m_model->getMarkerXData();
    QVector<double> markerYData = m_model->getMarkerYData();

    // 检查数据是否为空
    if (markerXData.isEmpty() || markerYData.isEmpty() || markerXData.size() != markerYData.size()) {
        return;
    }

    // 计算标记点绘制点集
    m_markerPoints.clear();
    calculateWaveformPoints(rect, markerXData, markerYData, m_markerPoints);

    // 设置标记点样式
    painter->setPen(QPen(Qt::red, 1, Qt::SolidLine));
    painter->setBrush(QBrush(Qt::red));

    // 绘制标记点
    const int markerSize = 6;
    for (const QPointF& point : m_markerPoints) {
        painter->drawEllipse(point, markerSize, markerSize);
    }
}

void WaveformAnalysisController::calculateWaveformPoints(const QRect& rect,
    const QVector<double>& xData,
    const QVector<double>& yData,
    QVector<QPointF>& points)
{
    // 确保数据一致
    if (xData.size() != yData.size()) {
        return;
    }

    // 获取坐标范围
    if (m_model && m_model->getConfig().autoScale && !xData.isEmpty()) {
        // 自动缩放
        m_xMin = *std::min_element(xData.begin(), xData.end());
        m_xMax = *std::max_element(xData.begin(), xData.end());
        m_yMin = *std::min_element(yData.begin(), yData.end());
        m_yMax = *std::max_element(yData.begin(), yData.end());

        // 添加一点边距
        double xMargin = (m_xMax - m_xMin) * 0.05;
        double yMargin = (m_yMax - m_yMin) * 0.05;

        // 确保有最小范围
        if (m_xMax - m_xMin < 0.001) {
            double mid = (m_xMax + m_xMin) / 2;
            m_xMin = mid - 0.5;
            m_xMax = mid + 0.5;
        }
        if (m_yMax - m_yMin < 0.001) {
            double mid = (m_yMax + m_yMin) / 2;
            m_yMin = mid - 0.5;
            m_yMax = mid + 0.5;
        }

        m_xMin -= xMargin;
        m_xMax += xMargin;
        m_yMin -= yMargin;
        m_yMax += yMargin;
    }

    // 坐标转换因子
    double xScale = rect.width() / (m_xMax - m_xMin);
    double yScale = rect.height() / (m_yMax - m_yMin);

    // 计算屏幕坐标点
    points.reserve(xData.size());
    for (int i = 0; i < xData.size(); ++i) {
        double x = rect.left() + (xData[i] - m_xMin) * xScale;
        double y = rect.bottom() - (yData[i] - m_yMin) * yScale;
        points.append(QPointF(x, y));
    }
}

void WaveformAnalysisController::setWaveformData(const QVector<double>& xData, const QVector<double>& yData)
{
    if (m_model) {
        m_model->setWaveformData(xData, yData);
    }
}

void WaveformAnalysisController::addDataPoint(double x, double y)
{
    if (m_model) {
        m_model->addDataPoint(x, y);
    }
}

void WaveformAnalysisController::clearData()
{
    if (m_model) {
        m_model->clearData();
    }
}

void WaveformAnalysisController::startAnalysis()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("开始波形分析");

    // 如果没有数据，生成模拟数据
    if (m_model->getXData().isEmpty() || m_model->getYData().isEmpty()) {
        generateSimulatedData();
    }

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.isRunning = true;
    m_model->setConfig(config);

    // 更新UI状态
    updateUIState();

    // 启动模拟数据更新定时器
    int refreshInterval = 1000 / config.refreshRate;
    m_simulationTimer->start(refreshInterval);

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::stopAnalysis()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("停止波形分析");

    // 停止模拟数据更新定时器
    m_simulationTimer->stop();

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.isRunning = false;
    m_model->setConfig(config);

    // 更新UI状态
    updateUIState();
}

void WaveformAnalysisController::performMeasurement()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("执行波形测量");

    // 如果没有数据，生成模拟数据
    if (m_model->getXData().isEmpty() || m_model->getYData().isEmpty()) {
        generateSimulatedData();
    }

    // 获取测量结果（模型会自动更新测量结果）
    MeasurementResult result = m_model->getMeasurementResult();

    // 更新测量结果显示
    updateMeasurementDisplay(result);

    // 切换到测量结果标签页
    if (m_ui && m_ui->m_rightTabWidget) {
        m_ui->m_rightTabWidget->setCurrentIndex(1);
    }

    // 请求重绘以显示标记点
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::exportData()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("导出波形数据");

    // 检查是否有数据
    QVector<double> xData = m_model->getXData();
    QVector<double> yData = m_model->getYData();

    if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
        QMessageBox::warning(m_view, "导出失败", "无数据可导出");
        return;
    }

    // 显示文件保存对话框
    QString fileName = QFileDialog::getSaveFileName(m_view,
        "导出波形数据",
        QDir::homePath() + "/waveform_data.csv",
        "CSV文件 (*.csv);;文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    // 打开文件
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(m_view, "导出失败", "无法打开文件进行写入");
        return;
    }

    // 写入表头
    QTextStream out(&file);
    out << "时间(ms),电压(V)\n";

    // 写入数据
    for (int i = 0; i < xData.size() && i < yData.size(); ++i) {
        out << QString::number(xData[i], 'f', 4) << ","
            << QString::number(yData[i], 'f', 4) << "\n";
    }

    file.close();

    LOG_INFO(QString("波形数据已导出到: %1").arg(fileName));
    QMessageBox::information(m_view, "导出成功", QString("波形数据已导出到:\n%1").arg(fileName));

    // 发送导出请求信号
    if (m_view) {
        emit m_view->exportRequested();
    }
}

void WaveformAnalysisController::onWaveformTypeChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("波形类型已更改为: %1").arg(m_ui->m_waveformTypeCombo->currentText()));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();

    switch (index) {
    case 0:
        config.waveformMode = WaveformMode::ANALOG;
        break;
    case 1:
        config.waveformMode = WaveformMode::DIGITAL;
        break;
    case 2:
        config.waveformMode = WaveformMode::MIXED;
        break;
    default:
        config.waveformMode = WaveformMode::ANALOG;
        break;
    }

    m_model->setConfig(config);

    // 重新生成模拟数据
    if (m_model->getXData().isEmpty() || config.isRunning) {
        generateSimulatedData();
    }

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onTriggerModeChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("触发模式已更改为: %1").arg(m_ui->m_triggerModeCombo->currentText()));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();

    switch (index) {
    case 0:
        config.triggerMode = TriggerMode::AUTO;
        break;
    case 1:
        config.triggerMode = TriggerMode::NORMAL;
        break;
    case 2:
        config.triggerMode = TriggerMode::SINGLE;
        break;
    default:
        config.triggerMode = TriggerMode::AUTO;
        break;
    }

    m_model->setConfig(config);
}

void WaveformAnalysisController::onZoomInButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("缩放放大");

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.zoomLevel *= 1.25;
    m_model->setConfig(config);

    // 更新显示范围
    double xCenter = (m_xMin + m_xMax) / 2.0;
    double yCenter = (m_yMin + m_yMax) / 2.0;
    double xHalfRange = (m_xMax - m_xMin) / (2.0 * 1.25);
    double yHalfRange = (m_yMax - m_yMin) / (2.0 * 1.25);

    m_xMin = xCenter - xHalfRange;
    m_xMax = xCenter + xHalfRange;
    m_yMin = yCenter - yHalfRange;
    m_yMax = yCenter + yHalfRange;

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onZoomOutButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("缩放缩小");

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.zoomLevel /= 1.25;
    m_model->setConfig(config);

    // 更新显示范围
    double xCenter = (m_xMin + m_xMax) / 2.0;
    double yCenter = (m_yMin + m_yMax) / 2.0;
    double xHalfRange = (m_xMax - m_xMin) / 2.0 * 1.25;
    double yHalfRange = (m_yMax - m_yMin) / 2.0 * 1.25;

    m_xMin = xCenter - xHalfRange;
    m_xMax = xCenter + xHalfRange;
    m_yMin = yCenter - yHalfRange;
    m_yMax = yCenter + yHalfRange;

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onZoomResetButtonClicked()
{
    if (!m_model) {
        return;
    }

    LOG_INFO("缩放重置");

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.zoomLevel = 1.0;
    m_model->setConfig(config);

    // 更新显示范围
    if (config.autoScale) {
        // 自动缩放会在绘制时更新范围
        if (m_view) {
            m_view->update();
        }
    }
    else {
        // 使用默认范围
        m_xMin = 0.0;
        m_xMax = 100.0;
        m_yMin = -5.0;
        m_yMax = 5.0;

        // 请求重绘
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::onAutoScaleChanged(bool checked)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("自动缩放已%1").arg(checked ? "启用" : "禁用"));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.autoScale = checked;
    m_model->setConfig(config);

    // 如果启用自动缩放，立即更新显示
    if (checked) {
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::onHorizontalScaleChanged(int value)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("水平缩放已更改为: %1").arg(value));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.timeBase = static_cast<double>(value) / 50.0;
    m_model->setConfig(config);

    // 更新显示范围
    if (!config.autoScale) {
        double xCenter = (m_xMin + m_xMax) / 2.0;
        double xHalfRange = 50.0 / value;

        m_xMin = xCenter - xHalfRange;
        m_xMax = xCenter + xHalfRange;

        // 请求重绘
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::onVerticalScaleChanged(int value)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("垂直缩放已更改为: %1").arg(value));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.voltageScale = static_cast<double>(value) / 50.0;
    m_model->setConfig(config);

    // 更新显示范围
    if (!config.autoScale) {
        double yCenter = (m_yMin + m_yMax) / 2.0;
        double yHalfRange = 5.0 / (value / 50.0);

        m_yMin = yCenter - yHalfRange;
        m_yMax = yCenter + yHalfRange;

        // 请求重绘
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::onSampleRateChanged(int value)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("采样率已更改为: %1 Hz").arg(value));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.sampleRate = static_cast<double>(value);
    m_model->setConfig(config);
}

void WaveformAnalysisController::onWindowSizeChanged(int value)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("窗口大小已更改为: %1").arg(value));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.windowSize = value;
    m_model->setConfig(config);
}

void WaveformAnalysisController::onWindowTypeChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("窗口类型已更改为索引: %1").arg(index));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.windowType = index;
    m_model->setConfig(config);
}

void WaveformAnalysisController::onPeakDetectionChanged(bool checked)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("峰值检测已%1").arg(checked ? "启用" : "禁用"));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.peakDetection = checked;
    m_model->setConfig(config);

    // 更新标记点
    if (m_model->getXData().size() > 0) {
        // 模型会自动更新标记点
        // 请求重绘
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::onNoiseFilterChanged(bool checked)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("噪声滤波已%1").arg(checked ? "启用" : "禁用"));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.noiseFilter = checked;
    m_model->setConfig(config);

    // 如果有数据并且已启用，对数据进行滤波
    if (checked && m_model->getXData().size() > 0) {
        // 简单实现：重新生成数据，实际应用中应该对现有数据进行滤波
        generateSimulatedData();

        // 请求重绘
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::onAutoCorrelationChanged(bool checked)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("自相关已%1").arg(checked ? "启用" : "禁用"));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.autoCorrelation = checked;
    m_model->setConfig(config);
}

void WaveformAnalysisController::onGridEnabledChanged(bool checked)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("网格显示已%1").arg(checked ? "启用" : "禁用"));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.gridEnabled = checked;
    m_model->setConfig(config);

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onColorThemeChanged(int index)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("颜色主题已更改为索引: %1").arg(index));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.colorTheme = index;
    m_model->setConfig(config);

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onRefreshRateChanged(int value)
{
    if (m_isBatchUpdate || !m_model) {
        return;
    }

    LOG_INFO(QString("刷新率已更改为: %1 Hz").arg(value));

    // 更新模型状态
    WaveformConfig config = m_model->getConfig();
    config.refreshRate = value;
    m_model->setConfig(config);

    // 如果定时器正在运行，更新其间隔
    if (m_simulationTimer && m_simulationTimer->isActive() && config.isRunning) {
        int refreshInterval = 1000 / value; // 转换为毫秒
        m_simulationTimer->setInterval(refreshInterval);
    }
}

void WaveformAnalysisController::onConfigChanged(const WaveformConfig& config)
{
    // 更新UI
    m_isBatchUpdate = true;
    applyModelToUI();
    m_isBatchUpdate = false;

    // 更新UI状态
    updateUIState();

    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onWaveformDataChanged(const QVector<double>& xData, const QVector<double>& yData)
{
    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onMeasurementResultChanged(const MeasurementResult& result)
{
    // 更新测量结果显示
    updateMeasurementDisplay(result);
}

void WaveformAnalysisController::onMarkersChanged(const QVector<double>& xData, const QVector<double>& yData)
{
    // 请求重绘
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onUpdateSimulatedData()
{
    if (!m_model || !m_model->getConfig().isRunning) {
        return;
    }

    // 获取现有数据
    QVector<double> xData = m_model->getXData();
    QVector<double> yData = m_model->getYData();

    // 为模拟效果，移除一些旧数据点并添加新数据点
    const int newPointsCount = 10;

    if (xData.size() > newPointsCount) {
        // 移除旧数据点
        for (int i = 0; i < newPointsCount; ++i) {
            xData.removeFirst();
            yData.removeFirst();
        }

        // 添加新数据点
        double lastX = xData.last();
        double timeStep = 0.1; // ms

        for (int i = 1; i <= newPointsCount; ++i) {
            double x = lastX + i * timeStep;

            // 根据波形模式生成不同类型的波形
            double y = 0.0;
            WaveformConfig config = m_model->getConfig();

            switch (config.waveformMode) {
            case WaveformMode::ANALOG:
                // 生成正弦波 + 噪声
                y = 3.0 * std::sin(2.0 * M_PI * 0.01 * x) +
                    1.0 * std::sin(2.0 * M_PI * 0.05 * x);

                // 如果未启用噪声滤波，添加噪声
                if (!config.noiseFilter) {
                    y += 0.2 * ((double)rand() / RAND_MAX - 0.5);
                }
                break;

            case WaveformMode::DIGITAL:
                // 生成数字方波
                y = (std::sin(2.0 * M_PI * 0.02 * x) > 0) ? 3.3 : 0.0;
                // 添加一些上升/下降时间
                if (!yData.isEmpty() && std::abs(yData.last() - y) > 1.0) {
                    y = yData.last() + (y - yData.last()) * 0.2;
                }
                break;

            case WaveformMode::MIXED:
                // 混合模拟和数字信号
                y = 2.0 * std::sin(2.0 * M_PI * 0.01 * x);
                // 添加数字脉冲
                if (std::fmod(x, 20.0) < 2.0) {
                    y += 2.0;
                }
                // 如果未启用噪声滤波，添加噪声
                if (!config.noiseFilter) {
                    y += 0.1 * ((double)rand() / RAND_MAX - 0.5);
                }
                break;
            }

            xData.append(x);
            yData.append(y);
        }

        // 更新数据
        m_model->setWaveformData(xData, yData);
    }
}

void WaveformAnalysisController::connectSignals()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 连接UI控件信号

    // 波形类型
    connect(m_ui->m_waveformTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &WaveformAnalysisController::onWaveformTypeChanged);

    // 触发模式
    connect(m_ui->m_triggerModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &WaveformAnalysisController::onTriggerModeChanged);

    // 采样率
    connect(m_ui->m_sampleRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
        this, &WaveformAnalysisController::onSampleRateChanged);

    // 自动缩放
    connect(m_ui->m_autoscaleCheck, &QCheckBox::toggled,
        this, &WaveformAnalysisController::onAutoScaleChanged);

    // 缩放按钮
    connect(m_ui->m_zoomInButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::onZoomInButtonClicked);
    connect(m_ui->m_zoomOutButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::onZoomOutButtonClicked);
    connect(m_ui->m_zoomResetButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::onZoomResetButtonClicked);

    // 水平和垂直滑块
    connect(m_ui->m_horizontalScaleSlider, &QSlider::valueChanged,
        this, &WaveformAnalysisController::onHorizontalScaleChanged);
    connect(m_ui->m_verticalScaleSlider, &QSlider::valueChanged,
        this, &WaveformAnalysisController::onVerticalScaleChanged);

    // 控制按钮
    connect(m_ui->m_startButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::startAnalysis);
    connect(m_ui->m_stopButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::stopAnalysis);
    connect(m_ui->m_exportButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::exportData);
    connect(m_ui->m_measureButton, &QPushButton::clicked,
        this, &WaveformAnalysisController::performMeasurement);

    // 连接模型信号
    connect(m_model, &WaveformConfigModel::configChanged,
        this, &WaveformAnalysisController::onConfigChanged);
    connect(m_model, &WaveformConfigModel::waveformDataChanged,
        this, &WaveformAnalysisController::onWaveformDataChanged);
    connect(m_model, &WaveformConfigModel::measurementResultChanged,
        this, &WaveformAnalysisController::onMeasurementResultChanged);
    connect(m_model, &WaveformConfigModel::markersChanged,
        this, &WaveformAnalysisController::onMarkersChanged);
}

void WaveformAnalysisController::updateUIState()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 获取当前配置
    WaveformConfig config = m_model->getConfig();

    // 更新按钮状态
    m_ui->m_startButton->setEnabled(!config.isRunning);
    m_ui->m_stopButton->setEnabled(config.isRunning);

    // 更新自动缩放复选框
    m_ui->m_autoscaleCheck->setChecked(config.autoScale);

    // 更新缩放滑块状态
    m_ui->m_horizontalScaleSlider->setEnabled(!config.autoScale);
    m_ui->m_verticalScaleSlider->setEnabled(!config.autoScale);
}

void WaveformAnalysisController::applyModelToUI()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 获取当前配置
    WaveformConfig config = m_model->getConfig();

    // 设置波形类型
    switch (config.waveformMode) {
    case WaveformMode::ANALOG:
        m_ui->m_waveformTypeCombo->setCurrentIndex(0);
        break;
    case WaveformMode::DIGITAL:
        m_ui->m_waveformTypeCombo->setCurrentIndex(1);
        break;
    case WaveformMode::MIXED:
        m_ui->m_waveformTypeCombo->setCurrentIndex(2);
        break;
    }

    // 设置触发模式
    switch (config.triggerMode) {
    case TriggerMode::AUTO:
        m_ui->m_triggerModeCombo->setCurrentIndex(0);
        break;
    case TriggerMode::NORMAL:
        m_ui->m_triggerModeCombo->setCurrentIndex(1);
        break;
    case TriggerMode::SINGLE:
        m_ui->m_triggerModeCombo->setCurrentIndex(2);
        break;
    }

    // 设置采样率
    m_ui->m_sampleRateSpin->setValue(static_cast<int>(config.sampleRate));

    // 设置自动缩放
    m_ui->m_autoscaleCheck->setChecked(config.autoScale);
}

void WaveformAnalysisController::applyUIToModel()
{
    if (!m_ui || !m_model) {
        return;
    }

    // 创建新的配置对象
    WaveformConfig config = m_model->getConfig();

    // 获取波形类型
    switch (m_ui->m_waveformTypeCombo->currentIndex()) {
    case 0:
        config.waveformMode = WaveformMode::ANALOG;
        break;
    case 1:
        config.waveformMode = WaveformMode::DIGITAL;
        break;
    case 2:
        config.waveformMode = WaveformMode::MIXED;
        break;
    default:
        config.waveformMode = WaveformMode::ANALOG;
        break;
    }

    // 获取触发模式
    switch (m_ui->m_triggerModeCombo->currentIndex()) {
    case 0:
        config.triggerMode = TriggerMode::AUTO;
        break;
    case 1:
        config.triggerMode = TriggerMode::NORMAL;
        break;
    case 2:
        config.triggerMode = TriggerMode::SINGLE;
        break;
    default:
        config.triggerMode = TriggerMode::AUTO;
        break;
    }

    // 获取采样率
    config.sampleRate = static_cast<double>(m_ui->m_sampleRateSpin->value());

    // 获取自动缩放
    config.autoScale = m_ui->m_autoscaleCheck->isChecked();

    // 更新模型
    m_model->setConfig(config);
}

void WaveformAnalysisController::generateSimulatedData()
{
    // 清空现有数据
    QVector<double> xData;
    QVector<double> yData;

    // 生成模拟数据
    const int numPoints = 1000;
    const double timeStep = 0.1; // ms

    for (int i = 0; i < numPoints; ++i) {
        double x = i * timeStep;

        // 根据波形模式生成不同类型的波形
        double y = 0.0;
        WaveformConfig config = m_model->getConfig();

        switch (config.waveformMode) {
        case WaveformMode::ANALOG:
            // 生成正弦波 + 噪声
            y = 3.0 * std::sin(2.0 * M_PI * 0.01 * x) +
                1.0 * std::sin(2.0 * M_PI * 0.05 * x);

            // 如果未启用噪声滤波，添加噪声
            if (!config.noiseFilter) {
                y += 0.2 * ((double)rand() / RAND_MAX - 0.5);
            }
            break;

        case WaveformMode::DIGITAL:
            // 生成数字方波
            y = (std::sin(2.0 * M_PI * 0.02 * x) > 0) ? 3.3 : 0.0;
            // 添加一些上升/下降时间
            for (int j = 0; j < 5; ++j) {
                if (i > 0 && std::abs(yData[i - 1] - y) > 1.0) {
                    y = yData[i - 1] + (y - yData[i - 1]) * 0.2;
                    break;
                }
            }
            break;

        case WaveformMode::MIXED:
            // 混合模拟和数字信号
            y = 2.0 * std::sin(2.0 * M_PI * 0.01 * x);
            // 添加数字脉冲
            if (std::fmod(x, 20.0) < 2.0) {
                y += 2.0;
            }
            // 如果未启用噪声滤波，添加噪声
            if (!config.noiseFilter) {
                y += 0.1 * ((double)rand() / RAND_MAX - 0.5);
            }
            break;
        }

        xData.append(x);
        yData.append(y);
    }

    // 更新模型
    m_model->setWaveformData(xData, yData);

    LOG_INFO(QString("已生成模拟波形数据，类型: %1, 点数: %2").arg(
        m_ui->m_waveformTypeCombo->currentText()).arg(numPoints));
}

void WaveformAnalysisController::updateMeasurementDisplay(const MeasurementResult& result)
{
    if (!m_ui || !m_ui->m_resultsTextEdit) {
        return;
    }

    // 生成报告
    QString report;
    report += "===== 波形统计信息 =====\n\n";
    report += QString("数据点数: %1\n").arg(m_model->getXData().size());
    report += QString("最小值: %1 V (位置: %2)\n").arg(result.minValue, 0, 'f', 4).arg(result.minIndex);
    report += QString("最大值: %1 V (位置: %2)\n").arg(result.maxValue, 0, 'f', 4).arg(result.maxIndex);
    report += QString("平均值: %1 V\n").arg(result.avgValue, 0, 'f', 4);
    report += QString("峰峰值: %1 V\n").arg(result.peakToPeak, 0, 'f', 4);
    report += QString("标准差: %1 V\n").arg(result.stdDeviation, 0, 'f', 4);
    report += QString("均方根: %1 V\n").arg(result.rmsValue, 0, 'f', 4);

    if (result.period > 0) {
        report += QString("估计周期: %1 ms\n").arg(result.period, 0, 'f', 4);
        report += QString("估计频率: %1 Hz\n").arg(result.frequency, 0, 'f', 4);
    }
    else {
        report += "无法估计周期/频率\n";
    }

    report += QString("过零点数: %1\n").arg(result.zeroCrossings);

    if (!m_model->getXData().isEmpty()) {
        report += QString("\n时间范围: %1 - %2 ms\n").arg(m_model->getXData().first(), 0, 'f', 4).arg(m_model->getXData().last(), 0, 'f', 4);
    }

    report += QString("采样率: %1 Hz\n").arg(m_model->getConfig().sampleRate, 0, 'f', 1);
    report += QString("\n分析时间: %1\n").arg(result.analysisTime);

    // 显示报告
    m_ui->m_resultsTextEdit->setText(report);
}

QColor WaveformAnalysisController::getWaveformColor(double value)
{
    if (!m_model) {
        return QColor(0, 120, 215); // 默认蓝色
    }

    // 根据颜色主题选择颜色
    switch (m_model->getConfig().colorTheme) {
    case 0: // 默认配色
        return QColor(0, 120, 215); // 蓝色

    case 1: // 暗黑主题
        return QColor(0, 255, 0); // 绿色

    case 2: // 高对比度
        // 根据值的范围设置颜色
        if (value > 0) {
            return QColor(255, 0, 0); // 正值为红色
        }
        else {
            return QColor(0, 0, 255); // 负值为蓝色
        }

    default:
        return QColor(0, 120, 215); // 默认蓝色
    }
}