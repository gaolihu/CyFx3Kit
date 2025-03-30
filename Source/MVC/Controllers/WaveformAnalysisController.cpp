// Source/MVC/Controllers/WaveformAnalysisController.cpp
#include "WaveformAnalysisController.h"
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformAnalysisModel.h"
#include "DataAccessService.h"
#include "Logger.h"
#include <QMessageBox>
#include <algorithm>
#include <cmath>

WaveformAnalysisController::WaveformAnalysisController(WaveformAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_model(nullptr)
    , m_dataService(nullptr)
    , m_updateTimer(nullptr)
    , m_isRunning(false)
    , m_isInitialized(false)
    , m_verticalScale(1.0)
    , m_autoScale(true)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取模型实例
    m_model = WaveformAnalysisModel::getInstance();

    // 获取数据访问服务实例
    m_dataService = &DataAccessService::getInstance();

    // 创建更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(100);  // 100ms更新间隔

    // 连接定时器信号
    connect(m_updateTimer, &QTimer::timeout,
        this, &WaveformAnalysisController::onUpdateTimerTriggered);

    LOG_INFO("波形分析控制器已创建");
}

WaveformAnalysisController::~WaveformAnalysisController()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }

    if (m_isRunning) {
        stopAnalysis();
    }

    LOG_INFO("波形分析控制器已销毁");
}

bool WaveformAnalysisController::initialize()
{
    if (m_isInitialized)
        return true;

    if (!m_view || !m_ui || !m_model)
        return false;

    // 连接信号和槽
    connectSignals();

    // 初始状态设置
    m_isInitialized = true;
    m_isRunning = false;

    LOG_INFO("波形分析控制器已初始化");
    return true;
}

void WaveformAnalysisController::connectSignals()
{
    // 从模型到控制器的信号连接
    connect(m_model, &WaveformAnalysisModel::dataLoaded,
        this, &WaveformAnalysisController::onDataLoaded);
    connect(m_model, &WaveformAnalysisModel::viewRangeChanged,
        this, &WaveformAnalysisController::onViewRangeChanged);
    connect(m_model, &WaveformAnalysisModel::markersChanged,
        this, &WaveformAnalysisController::onMarkersChanged);
    connect(m_model, &WaveformAnalysisModel::channelStateChanged,
        this, &WaveformAnalysisController::onChannelStateChanged);
}

void WaveformAnalysisController::handlePaintEvent(QPainter* painter, const QRect& chartRect)
{
    if (!m_isInitialized || !painter)
        return;

    // 保存图表区域
    m_lastChartRect = chartRect;

    // 绘制背景
    painter->fillRect(chartRect, Qt::white);

    // 绘制网格
    drawGrid(painter, chartRect);

    // 绘制通道标签
    drawChannelLabels(painter, chartRect);

    // 绘制波形
    drawWaveforms(painter, chartRect);

    // 绘制标记点
    drawMarkers(painter, chartRect);
}

void WaveformAnalysisController::handlePanDelta(int deltaX)
{
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算移动距离对应的数据范围
    double dataRange = xMax - xMin;
    double dataDelta = deltaX * dataRange / m_lastChartRect.width();

    // 更新视图范围
    m_model->setViewRange(xMin - dataDelta, xMax - dataDelta);

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::addMarkerAtPosition(const QPoint& pos)
{
    if (!m_lastChartRect.contains(pos))
        return;

    // 将屏幕坐标转换为数据索引
    double dataIndex = screenToDataX(pos.x(), m_lastChartRect);

    // 四舍五入到最近的整数索引
    int index = qRound(dataIndex);

    // 添加标记点
    m_model->addMarkerPoint(index);

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::startAnalysis()
{
    if (m_isRunning)
        return;

    m_isRunning = true;

    // 启动更新定时器
    m_updateTimer->start();

    LOG_INFO("波形分析已启动");
}

void WaveformAnalysisController::stopAnalysis()
{
    if (!m_isRunning)
        return;

    m_isRunning = false;

    // 停止更新定时器
    m_updateTimer->stop();

    LOG_INFO("波形分析已停止");
}

void WaveformAnalysisController::zoomIn()
{
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算中心点
    double center = (xMin + xMax) / 2.0;

    // 缩小范围为原来的80%
    double newWidth = (xMax - xMin) * 0.8;

    // 设置新范围
    m_model->setViewRange(center - newWidth / 2.0, center + newWidth / 2.0);

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::zoomOut()
{
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算中心点
    double center = (xMin + xMax) / 2.0;

    // 放大范围为原来的125%
    double newWidth = (xMax - xMin) * 1.25;

    // 设置新范围
    m_model->setViewRange(center - newWidth / 2.0, center + newWidth / 2.0);

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::zoomReset()
{
    // 重置为显示所有数据
    QVector<double> indexData = m_model->getIndexData();

    if (!indexData.isEmpty()) {
        m_model->setViewRange(indexData.first(), indexData.last());
    }
    else {
        m_model->setViewRange(0, 100);
    }

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::zoomInAtPoint(const QPoint& pos)
{
    if (!m_lastChartRect.contains(pos))
        return;

    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 将点击位置转换为数据索引
    double clickedIndex = screenToDataX(pos.x(), m_lastChartRect);

    // 计算新范围（缩小到80%）
    double newWidth = (xMax - xMin) * 0.8;

    // 计算新的xMin和xMax，保持点击位置的相对位置不变
    double ratio = (clickedIndex - xMin) / (xMax - xMin);
    double newXMin = clickedIndex - newWidth * ratio;
    double newXMax = newXMin + newWidth;

    // 设置新范围
    m_model->setViewRange(newXMin, newXMax);

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::zoomOutAtPoint(const QPoint& pos)
{
    if (!m_lastChartRect.contains(pos))
        return;

    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 将点击位置转换为数据索引
    double clickedIndex = screenToDataX(pos.x(), m_lastChartRect);

    // 计算新范围（放大到125%）
    double newWidth = (xMax - xMin) * 1.25;

    // 计算新的xMin和xMax，保持点击位置的相对位置不变
    double ratio = (clickedIndex - xMin) / (xMax - xMin);
    double newXMin = clickedIndex - newWidth * ratio;
    double newXMax = newXMin + newWidth;

    // 设置新范围
    m_model->setViewRange(newXMin, newXMax);

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::loadData(const QString& filename, int startIndex, int length)
{
    // 委托给模型加载数据
    m_model->loadData(filename, startIndex, length);
}

void WaveformAnalysisController::onDataLoaded(bool success)
{
    if (!success) {
        QMessageBox::warning(m_view, "加载失败", "波形数据加载失败。");
        return;
    }

    // 自动调整视图范围显示所有数据
    zoomReset();

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onViewRangeChanged(double xMin, double xMax)
{
    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onMarkersChanged()
{
    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onChannelStateChanged(int channel, bool enabled)
{
    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::onUpdateTimerTriggered()
{
    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::drawGrid(QPainter* painter, const QRect& rect)
{
    // 设置网格线画笔
    QPen pen(QColor(230, 230, 230));
    pen.setWidth(1);
    painter->setPen(pen);

    // 水平网格线（通道分隔线）
    for (int i = 1; i < 4; ++i) {
        int y = rect.top() + rect.height() * i / 4;
        painter->drawLine(rect.left(), y, rect.right(), y);
    }

    // 垂直网格线
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算合适的网格间距
    double dataRange = xMax - xMin;
    double gridStep = 1.0;

    // 根据数据范围自动调整网格间距
    if (dataRange > 1000) gridStep = 100.0;
    else if (dataRange > 500) gridStep = 50.0;
    else if (dataRange > 100) gridStep = 10.0;
    else if (dataRange > 50) gridStep = 5.0;
    else if (dataRange > 20) gridStep = 2.0;

    // 计算第一条网格线位置
    double firstGrid = std::ceil(xMin / gridStep) * gridStep;

    // 绘制垂直网格线
    for (double x = firstGrid; x <= xMax; x += gridStep) {
        int screenX = dataToScreenX(x, rect);
        painter->drawLine(screenX, rect.top(), screenX, rect.bottom());
    }
}

void WaveformAnalysisController::drawChannelLabels(QPainter* painter, const QRect& rect)
{
    // 设置标签画笔
    painter->setFont(QFont("Arial", 9, QFont::Bold));

    // 为每个通道绘制标签
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_model->isChannelEnabled(ch))
            continue;

        // 计算标签位置
        int yTop = rect.top() + rect.height() * ch / 4;

        // 设置通道颜色
        QColor color;
        switch (ch) {
        case 0: color = Qt::red; break;
        case 1: color = Qt::blue; break;
        case 2: color = Qt::green; break;
        case 3: color = Qt::magenta; break;
        }

        painter->setPen(color);

        // 绘制标签
        QString label = QString("BYTE%1").arg(ch);
        painter->drawText(rect.left(), yTop + 15, label);
    }
}

void WaveformAnalysisController::drawWaveforms(QPainter* painter, const QRect& rect)
{
    // 获取当前视图范围
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 为每个通道绘制波形
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_model->isChannelEnabled(ch))
            continue;

        QVector<double> data = m_model->getChannelData(ch);
        if (data.isEmpty())
            continue;

        // 计算通道区域
        int channelHeight = rect.height() / 4;
        int channelTop = rect.top() + rect.height() * ch / 4;
        int highY = channelTop + channelHeight / 4;     // 高电平位置
        int lowY = channelTop + channelHeight * 3 / 4;  // 低电平位置

        // 设置波形颜色
        QColor color;
        switch (ch) {
        case 0: color = Qt::red; break;
        case 1: color = Qt::blue; break;
        case 2: color = Qt::green; break;
        case 3: color = Qt::magenta; break;
        }

        QPen pen(color);
        pen.setWidth(2);
        painter->setPen(pen);

        // 初始化上次电平状态和位置
        uint8_t lastLevel = 255;  // 无效的初始值
        int lastX = -1;

        // 计算可见数据范围
        int startIdx = std::max(0, static_cast<int>(std::ceil(xMin)));
        int endIdx = std::min(static_cast<int>(std::floor(xMax)), static_cast<int>(data.size() - 1));

        // 绘制可见范围内的数据
        for (int i = startIdx; i <= endIdx; ++i) {
            // 计算屏幕坐标
            int x = dataToScreenX(i, rect);

            // 判断电平
            uint8_t level = (data[i] > 0) ? 1 : 0;

            if (lastLevel == 255) {
                // 第一个数据点
                lastLevel = level;
                lastX = x;
                continue;
            }

            // 绘制水平线段
            if (lastLevel == 1) {
                painter->drawLine(lastX, highY, x, highY);
            }
            else {
                painter->drawLine(lastX, lowY, x, lowY);
            }

            // 如果电平发生变化，绘制垂直过渡线
            if (lastLevel != level) {
                painter->drawLine(x, highY, x, lowY);
            }

            // 更新上次状态
            lastLevel = level;
            lastX = x;
        }

        // 绘制最后一段到边界的线段
        if (lastLevel != 255 && lastX != -1) {
            int rightEdge = dataToScreenX(xMax, rect);
            if (lastLevel == 1) {
                painter->drawLine(lastX, highY, rightEdge, highY);
            }
            else {
                painter->drawLine(lastX, lowY, rightEdge, lowY);
            }
        }
    }
}

void WaveformAnalysisController::drawMarkers(QPainter* painter, const QRect& rect)
{
    // 获取标记点
    QVector<int> markers = m_model->getMarkerPoints();
    if (markers.isEmpty())
        return;

    // 获取当前视图范围
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 设置标记线样式
    QPen pen(Qt::red);
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    painter->setPen(pen);

    // 设置标记文本样式
    painter->setFont(QFont("Arial", 8, QFont::Bold));

    // 绘制每个标记点
    for (int i = 0; i < markers.size(); ++i) {
        int markerPos = markers[i];

        // 检查标记点是否在可见范围内
        if (markerPos < xMin || markerPos > xMax)
            continue;

        // 计算屏幕坐标
        int x = dataToScreenX(markerPos, rect);

        // 绘制标记竖线
        painter->drawLine(x, rect.top(), x, rect.bottom());

        // 绘制标记标签
        painter->setPen(Qt::black);
        QString label = QString("M%1").arg(i + 1);
        QRect labelRect(x - 10, rect.top() - 15, 20, 12);

        painter->fillRect(labelRect, QColor(255, 255, 200));
        painter->drawRect(labelRect);
        painter->drawText(labelRect, Qt::AlignCenter, label);

        // 恢复标记线画笔
        pen.setColor(Qt::red);
        pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
    }
}

int WaveformAnalysisController::dataToScreenX(double index, const QRect& rect)
{
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 防止除以零
    if (qFuzzyCompare(xMax, xMin))
        return rect.left();

    // 线性映射数据索引到屏幕坐标
    return rect.left() + static_cast<int>((index - xMin) * rect.width() / (xMax - xMin));
}

double WaveformAnalysisController::screenToDataX(int x, const QRect& rect)
{
    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 防止除以零
    if (rect.width() <= 0)
        return xMin;

    // 线性映射屏幕坐标到数据索引
    return xMin + (x - rect.left()) * (xMax - xMin) / rect.width();
}