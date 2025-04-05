﻿// Source/MVC/Controllers/WaveformAnalysisController.cpp
#include "WaveformAnalysisController.h"
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformAnalysisModel.h"
#include "DataAccessService.h"
#include "Logger.h"
#include <QMessageBox>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

// 修改WaveformAnalysisController构造函数中的初始化代码
WaveformAnalysisController::WaveformAnalysisController(WaveformAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_model(nullptr)
    , m_dataService(&DataAccessService::getInstance())
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

    // 创建更新定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(100);  // 100ms更新间隔

    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析控制器已创建"));
}

WaveformAnalysisController::~WaveformAnalysisController()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }

    if (m_isRunning) {
        slot_WA_C_stopAnalysis();
    }

    // 清除模型数据引用，避免访问已释放资源
    if (m_model) {
        // m_model->clearAllData();
    }

    // 清除数据服务引用
    m_dataService = nullptr;

    LOG_INFO("波形分析控制器已销毁");
}

bool WaveformAnalysisController::initialize()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始初始化波形分析控制器"));

    if (m_isInitialized) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("控制器已初始化，跳过"));
        return true;
    }

    if (!m_view || !m_ui || !m_model) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化失败: view=%1, ui=%2, model=%3")
            .arg(m_view ? "有效" : "无效")
            .arg(m_ui ? "有效" : "无效")
            .arg(m_model ? "有效" : "无效"));
        return false;
    }

    // 初始化通道状态
    for (int ch = 0; ch < 4; ++ch) {
        m_model->setChannelEnabled(ch, true);
    }

    // 重置视图范围到安全值
    m_model->setViewRange(0.0, 100.0);

    // 连接信号和槽
    connectSignals();

    // 初始状态设置
    m_isInitialized = true;
    m_isRunning = false;
    m_verticalScale = 1.0;  // 设置默认垂直缩放

    LOG_INFO("波形分析控制器已初始化");
    return true;
}

void WaveformAnalysisController::connectSignals()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("连接控制器信号和槽"));

    // 从模型到控制器的信号连接
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_dataLoaded,
        this, &WaveformAnalysisController::slot_WA_C_onDataLoaded);
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_viewRangeChanged,
        this, &WaveformAnalysisController::slot_WA_C_onViewRangeChanged);
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_markersChanged,
        this, &WaveformAnalysisController::slot_WA_C_onMarkersChanged);
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_channelStateChanged,
        this, &WaveformAnalysisController::slot_WA_C_onChannelStateChanged);
}

void WaveformAnalysisController::handlePaintEvent(QPainter* painter, const QRect& chartRect)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("绘制事件触发: 图表区域=[%1,%2,%3,%4]")
        .arg(chartRect.left()).arg(chartRect.top())
        .arg(chartRect.width()).arg(chartRect.height()));

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("增加marker"));

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

bool WaveformAnalysisController::processWaveformData(const QByteArray& data)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形控制器接收数据，大小: %1 字节").arg(data.size()));

    if (data.isEmpty()) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("接收到空数据"));
        return false;
    }

    // 数据是否有效的简单检查
    if (data.size() < 4) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("数据太小，至少需要4字节"));
        return false;
    }

    // 如果我们的波形视图正在显示，解析数据并更新模型
    if (m_view && m_view->isVisible() && m_model) {
        // 尝试将数据解析为波形并加载到模型中
        bool success = m_model->parsePacketData(data);

        if (success) {
            // 更新视图
            if (m_view) {
                m_view->update();
            }
            return true;
        }
        else {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("波形数据解析失败"));
        }
    }

    return false;
}

void WaveformAnalysisController::slot_WA_C_startAnalysis()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始分析"));

    if (m_isRunning)
        return;

    m_isRunning = true;

    // 启动更新定时器
    m_updateTimer->start();

    LOG_INFO("波形分析已启动");
}

void WaveformAnalysisController::slot_WA_C_stopAnalysis()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止分析"));

    if (!m_isRunning)
        return;

    m_isRunning = false;

    // 停止更新定时器
    m_updateTimer->stop();

    LOG_INFO("波形分析已停止");
}

void WaveformAnalysisController::slot_WA_C_zoomIn()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("放大"));

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

void WaveformAnalysisController::slot_WA_C_zoomOut()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("缩小"));

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

void WaveformAnalysisController::slot_WA_C_zoomReset()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置缩放"));

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

void WaveformAnalysisController::slot_WA_C_zoomInAtPoint(const QPoint& pos)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("进入指定位置放大"));

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

void WaveformAnalysisController::slot_WA_C_zoomOutAtPoint(const QPoint& pos)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("进入指定位置缩小"));

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

bool WaveformAnalysisController::slot_WA_C_loadData(int startIndex, int length)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析控制器加载数据 - 文件: %1, 起始: %2, 长度: %3")
        .arg(startIndex).arg(length));

    if (!m_model || !m_dataService) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("模型或数据服务为空，无法加载数据"));
        return false;
    }

    // 委托给模型加载数据
    bool success = m_model->loadData(startIndex, length);

    if (success) {
        // 加载成功后设置默认视图范围
        double min = startIndex;
        double max = startIndex + length - 1;
        m_model->setViewRange(min, max);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设置初始视图范围: [%1, %2]").arg(min).arg(max));
    }
    else {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("波形数据加载失败"));
    }

    return success;
}

#if 0
bool WaveformAnalysisController::slot_WA_C_loadSimulatedData(int length)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("加载模拟波形数据，长度: %1").arg(length));

    if (!m_model) {
        return false;
    }

    // 创建索引数据
    QVector<double> indexData;
    indexData.reserve(length);
    for (int i = 0; i < length; ++i) {
        indexData.append(i);
    }
    m_model->updateIndexData(indexData);

    // 为每个通道创建模拟数据
    for (int ch = 0; ch < 4; ++ch) {
        QVector<double> channelData;
        channelData.reserve(length);

        // 生成不同的波形模式
        for (int i = 0; i < length; ++i) {
            // 每个通道使用不同周期的方波
            int period = 10 + ch * 5;
            double value = (i % period < period / 2) ? 1.0 : 0.0;
            channelData.append(value);
        }

        m_model->updateChannelData(ch, channelData);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("已生成通道%1模拟数据").arg(ch));
    }

    // 设置视图范围
    m_model->setViewRange(0, length - 1);

    // 触发数据加载成功信号
    emit m_model->dataLoaded(true);
    emit m_model->viewRangeChanged(0, length - 1);

    return true;
}
#endif

void WaveformAnalysisController::slot_WA_C_onDataLoaded(bool success)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据加载结果: %1").arg(success ? "成功" : "失败"));

    if (!success) {
        QMessageBox::warning(m_view, "加载失败", "波形数据加载失败。");
        return;
    }

    // 检查模型中是否有数据
    for (int ch = 0; ch < 4; ++ch) {
        QVector<double> data = m_model->getChannelData(ch);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1数据加载后状态: 大小=%2")
            .arg(ch).arg(data.size()));
    }

    // 获取加载后的索引数据
    QVector<double> indexData = m_model->getIndexData();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引数据加载后状态: 大小=%1, 首值=%2, 尾值=%3")
        .arg(indexData.size())
        .arg(indexData.isEmpty() ? "N/A" : QString::number(indexData.first()))
        .arg(indexData.isEmpty() ? "N/A" : QString::number(indexData.last())));

    // 自动调整视图范围显示所有数据
    LOG_INFO(LocalQTCompat::fromLocal8Bit("执行视图范围重置"));
    slot_WA_C_zoomReset();

    // 更新UI
    LOG_INFO(LocalQTCompat::fromLocal8Bit("触发UI更新"));
    if (m_view) {
        m_view->update();
    }
    else {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无法更新UI: 视图对象为空"));
    }
}

void WaveformAnalysisController::slot_WA_C_onViewRangeChanged(double xMin, double xMax)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("视图更新"));

    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::slot_WA_C_onMarkersChanged()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("Marker改变"));

    // 更新UI
    if (m_view) {
        // 获取当前标记点
        QVector<int> markers = m_model->getMarkerPoints();

        // 更新视图中的标记列表
        m_view->updateMarkerList(markers);

        // 触发重绘
        m_view->update();
    }
}

void WaveformAnalysisController::slot_WA_C_onChannelStateChanged(int channel, bool enabled)
{
    // 更新UI
    if (m_view) {
        m_view->update();
    }
}

void WaveformAnalysisController::slot_WA_C_onUpdateTimerTriggered()
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

    // 防止无效视图范围
    if (!std::isfinite(xMin) || !std::isfinite(xMax) || xMin >= xMax) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("视图范围无效: [%1, %2]，重置为默认值").arg(xMin).arg(xMax));

        // 重置为默认值
        QVector<double> indexData = m_model->getIndexData();
        if (!indexData.isEmpty()) {
            xMin = indexData.first();
            xMax = indexData.last();
        }
        else {
            xMin = 0;
            xMax = 100;
        }

        m_model->setViewRange(xMin, xMax);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("绘制波形: 视图范围=[%1,%2], 图表区域=[%3,%4,%5,%6]")
        .arg(xMin).arg(xMax)
        .arg(rect.left()).arg(rect.top())
        .arg(rect.width()).arg(rect.height()));

    // 为每个通道绘制波形
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_model->isChannelEnabled(ch)) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1已禁用，跳过绘制").arg(ch));
            continue;
        }

        QVector<double> data = m_model->getChannelData(ch);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1数据: 大小=%2")
            .arg(ch).arg(data.size()));

        if (data.isEmpty()) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("通道%1数据为空").arg(ch));
            continue;
        }

        // 计算通道区域 - 将整个矩形区域平均分成4个通道区域
        int channelHeight = rect.height() / 4;
        int channelTop = rect.top() + channelHeight * ch;

        // 通道的高、低电平位置
        int highY = channelTop + channelHeight / 4;     // 高电平位置
        int lowY = channelTop + channelHeight * 3 / 4;  // 低电平位置

        // 应用垂直缩放 - 假设m_verticalScale存储缩放因子
        int midY = channelTop + channelHeight / 2;
        int deltaY = static_cast<int>((channelHeight / 4) * m_verticalScale);
        highY = midY - deltaY;
        lowY = midY + deltaY;

        // 设置波形颜色
        QColor color = m_model->getChannelColor(ch);
        QPen pen(color);
        pen.setWidth(2);
        painter->setPen(pen);

        // 初始化上次电平状态和位置
        uint8_t lastLevel = 255;  // 无效的初始值
        int lastX = -1;

        // 计算可见数据范围
        int startIdx = std::max(0, static_cast<int>(std::ceil(xMin)));
        int endIdx = std::min(static_cast<int>(std::floor(xMax)), static_cast<int>(data.size() - 1));

        // 确保范围有效
        if (startIdx < 0 || startIdx >= data.size() || endIdx < 0 || endIdx >= data.size() || startIdx > endIdx) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("通道%1数据范围无效: [%2, %3]，数据大小: %4")
                .arg(ch).arg(startIdx).arg(endIdx).arg(data.size()));
            continue;
        }

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

        LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1波形绘制完成").arg(ch));
    }
}

void WaveformAnalysisController::slot_WA_C_setVerticalScale(double scale)
{
    if (scale > 0) {
        m_verticalScale = scale;

        // 更新UI
        if (m_view) {
            m_view->update();
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

    // 防止除以零或无效值
    if (!std::isfinite(xMin) || !std::isfinite(xMax) || qFuzzyCompare(xMax, xMin)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("视图范围异常: xMin=%1, xMax=%2, 使用默认映射").arg(xMin).arg(xMax));
        return rect.left() + static_cast<int>((index - 0) * rect.width() / 100.0);
    }

    // 检查index是否在合理范围内
    if (index < xMin - (xMax - xMin) || index > xMax + (xMax - xMin)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("数据索引超出合理范围: %1, 范围=[%2,%3]")
            .arg(index).arg(xMin).arg(xMax));

        // 限制在合理范围内
        index = std::max(xMin, std::min(index, xMax));
    }

    // 线性映射数据索引到屏幕坐标
    int screenX = rect.left() + static_cast<int>((index - xMin) * rect.width() / (xMax - xMin));

    // 确保返回合理的屏幕坐标
    screenX = std::max(rect.left(), std::min(screenX, rect.right()));

    // 添加周期性采样日志，避免日志过多
    if (static_cast<int>(index) % 100 == 0 || index == xMin || index == xMax) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("坐标转换: 数据索引=%1 -> 屏幕X=%2")
            .arg(index).arg(screenX));
    }

    return screenX;
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

void WaveformAnalysisController::slot_WA_C_handleTabActivated()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析标签页被激活"));

    if (!m_isActive) {
        m_isActive = true;

        // 确保数据服务已初始化
        if (!m_dataService) {
            m_dataService = &DataAccessService::getInstance();
            LOG_INFO(LocalQTCompat::fromLocal8Bit("标签页激活时初始化数据服务"));
        }
    }

    if (m_isCurrentlyVisible) {
        // 首次激活时，加载初始数据
        LOG_INFO(LocalQTCompat::fromLocal8Bit("当前TAB可见，加载初始数据"));

        // 尝试加载实际数据
        bool success = slot_WA_C_loadDataRange(m_currentPosition, m_viewWidth);

        // 如果加载失败，使用模拟数据
        if (!success) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("加载实际数据失败"));
        }

        // 更新视图
        if (m_view) {
            m_view->update();
        }
    }
}

void WaveformAnalysisController::slot_WA_C_updateVisibleRange(int startPos, int viewWidth)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新可视范围 - 起始: %1, 长度: %2")
        .arg(startPos).arg(viewWidth));

    // 仅当活动且范围有显著变化时加载数据
    if (m_isActive && (abs(startPos - m_currentPosition) > viewWidth / 4 ||
        abs(viewWidth - m_viewWidth) > viewWidth / 4)) {

        m_currentPosition = startPos;
        m_viewWidth = viewWidth;

        // 加载新范围的数据，加上一些填充
        slot_WA_C_loadDataRange(startPos - viewWidth / 2, viewWidth * 2);
    }
}

bool WaveformAnalysisController::slot_WA_C_loadDataRange(int startPos, int length)
{
    // 确保非负值
    startPos = std::max(0, startPos);
    length = std::max(100, length);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("按需加载波形数据 - 起始: %1, 长度: %2")
        .arg(startPos).arg(length));

    return loadWaveformDataFromService(startPos, length);
}

bool WaveformAnalysisController::loadWaveformDataFromService(int startIndex, int length)
{
    if (!m_dataService) {
        LOG_ERROR("数据访问服务未设置，无法加载波形数据");
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始加载波形数据 - 起始: %1, 长度: %2")
        .arg(startIndex).arg(length));

    // 清除现有数据
    QVector<double> indexData;
    indexData.reserve(length);
    for (int i = 0; i < length; ++i) {
        indexData.append(startIndex + i);
    }
    m_model->updateIndexData(indexData);

    bool hasData = false;

    // 为每个通道加载数据
    for (int ch = 0; ch < 4; ++ch) {
        QVector<double> channelData = m_dataService->getChannelData(
            ch, startIndex, length);

        if (!channelData.isEmpty()) {
            m_model->updateChannelData(ch, channelData);
            hasData = true;
            LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1数据加载成功: 大小=%2")
                .arg(ch).arg(channelData.size()));
        }
        else {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("通道%1数据加载失败或为空").arg(ch));
        }
    }

    // 设置视图范围
    if (hasData) {
        m_model->setViewRange(startIndex, startIndex + length - 1);
    }

    return hasData;
}

void WaveformAnalysisController::setTabVisible(bool visible)
{
    bool wasVisible = m_isCurrentlyVisible;
    m_isCurrentlyVisible = visible;

    // If becoming visible and we have active data acquisition, refresh the view
    if (visible && !wasVisible) {
        // Check if we have an active data acquisition
        bool isAcquiring = false;
        if (m_dataService) {
            // isAcquiring = m_dataService->isDataAcquisitionActive();
            // WTF
        }

        // Load fresh data if acquisition is active
        if (isAcquiring) {
            slot_WA_C_loadDataRange(m_currentPosition, m_viewWidth);
        }

        // Update the UI when becoming visible
        if (m_view) {
            m_view->update();
        }
    }
}