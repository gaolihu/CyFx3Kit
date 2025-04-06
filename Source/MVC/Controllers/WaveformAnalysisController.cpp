// Source/MVC/Controllers/WaveformAnalysisController.cpp
#include "WaveformAnalysisController.h"
#include "WaveformGLWidget.h"
#include "WaveformAnalysisView.h"
#include "ui_WaveformAnalysis.h"
#include "WaveformAnalysisModel.h"
#include "DataAccessService.h"
#include "Logger.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QProgressDialog>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

WaveformAnalysisController::WaveformAnalysisController(WaveformAnalysisView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_model(nullptr)
    , m_dataService(&DataAccessService::getInstance())
    , m_glWidget(nullptr)
    , m_updateTimer(nullptr)
    , m_isRunning(false)
    , m_isInitialized(false)
    , m_verticalScale(1.0)
    , m_autoScale(true)
    , m_dataPacketCache(new QByteArray())
    , m_cacheStartIndex(0)
    , m_cacheLength(0)
    , m_isCacheValid(false) {

    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
        m_glWidget = m_view->getGLWidget();
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
    m_model = nullptr;

    // 清除数据服务引用
    m_dataService = nullptr;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析控制器已销毁"));
}

bool WaveformAnalysisController::initialize()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始初始化波形分析控制器"));

    if (m_isInitialized) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("控制器已初始化，跳过"));
        return true;
    }

    if (!m_view || !m_ui || !m_model || !m_glWidget) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化失败: view=%1, ui=%2, model=%3, glWidget=%4")
            .arg(m_view ? "有效" : "无效")
            .arg(m_ui ? "有效" : "无效")
            .arg(m_model ? "有效" : "无效")
            .arg(m_glWidget ? "有效" : "无效"));
        return false;
    }

    // 初始化通道状态
    for (int ch = 0; ch < 4; ++ch) {
        m_model->setChannelEnabled(ch, true);
    }

    // 设置OpenGL控件的模型
    m_glWidget->setModel(m_model);

    // 重置视图范围到安全值
    m_model->setViewRange(0.0, 100.0);

    // 连接信号和槽
    connectSignals();

    // 生成初始测试数据
    generateMockData();

    // 设置视图范围并通知界面更新
    m_model->setViewRange(0, 999);
    if (m_glWidget) {
        m_glWidget->setViewRange(0, 999);
        m_glWidget->requestUpdate();
    }

    // 初始状态设置
    m_isInitialized = true;
    m_isRunning = false;
    m_verticalScale = 1.0;  // 设置默认垂直缩放
    m_glWidget->setVerticalScale(m_verticalScale);

    // 强制更新UI
    if (m_view) {
        m_view->update();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析控制器已初始化"));
    return true;
}

void WaveformAnalysisController::connectSignals()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("连接控制器信号和槽"));

    // 连接OpenGL控件信号到控制器
    connect(m_glWidget, &WaveformGLWidget::viewRangeChanged,
        this, &WaveformAnalysisController::onGLWidgetViewRangeChanged);
    connect(m_glWidget, &WaveformGLWidget::markerAdded,
        this, &WaveformAnalysisController::onGLWidgetMarkerAdded);
    connect(m_glWidget, &WaveformGLWidget::panRequested,
        this, &WaveformAnalysisController::onGLWidgetPanRequested);
    connect(m_glWidget, &WaveformGLWidget::loadDataRequested,
        this, &WaveformAnalysisController::onGLWidgetLoadDataRequested);

    // 从模型到控制器的信号连接
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_dataLoaded,
        this, &WaveformAnalysisController::slot_WA_C_onDataLoaded);
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_viewRangeChanged,
        this, &WaveformAnalysisController::slot_WA_C_onViewRangeChanged);
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_markersChanged,
        this, &WaveformAnalysisController::slot_WA_C_onMarkersChanged);
    connect(m_model, &WaveformAnalysisModel::signal_WA_M_channelStateChanged,
        this, &WaveformAnalysisController::slot_WA_C_onChannelStateChanged);

    // 连接定时器信号
    connect(m_updateTimer, &QTimer::timeout, this, &WaveformAnalysisController::slot_WA_C_onUpdateTimerTriggered);
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
            // 确保通道数据和索引数据长度一致
            ensureDataConsistency();

            // 通知OpenGL控件更新
            m_glWidget->requestUpdate();

            return true;
        }
        else {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("波形数据解析失败"));
        }
    }

    return false;
}

void WaveformAnalysisController::setTabVisible(bool visible)
{
    bool wasVisible = m_isCurrentlyVisible;
    m_isCurrentlyVisible = visible;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置TAB可见性: %1, 之前状态: %2")
        .arg(visible ? "可见" : "不可见")
        .arg(wasVisible ? "可见" : "不可见"));

    // 如果变为可见状态，刷新数据
    if (visible && !wasVisible) {
        // 检查是否有活动的数据采集
        bool isAcquiring = false;
        if (m_dataService) {
            // isAcquiring = m_dataService->isDataAcquisitionActive();
            // 此处应检查数据服务的状态
        }

        // 如果数据采集活跃，加载新数据
        if (isAcquiring) {
            slot_WA_C_loadDataRange(m_currentPosition, m_viewWidth);
        }

        // 更新OpenGL控件
        if (m_glWidget) {
            m_glWidget->requestUpdate();
        }
    }
}

void WaveformAnalysisController::slot_WA_C_startAnalysis()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始分析"));

    if (m_isRunning)
        return;

    m_isRunning = true;

    // 启动更新定时器
    m_updateTimer->start();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析已启动，定时器开始"));
}

void WaveformAnalysisController::slot_WA_C_stopAnalysis()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止分析"));

    if (!m_isRunning)
        return;

    m_isRunning = false;

    // 停止更新定时器
    m_updateTimer->stop();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析已停止，定时器关闭"));
}

void WaveformAnalysisController::slot_WA_C_zoomIn()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("放大操作"));

    if (!m_model || !m_glWidget) return;

    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算中心点
    double center = (xMin + xMax) / 2.0;

    // 缩小范围为原来的80%
    double newWidth = (xMax - xMin) * 0.8;

    // 设置新范围
    double newMin = center - newWidth / 2.0;
    double newMax = center + newWidth / 2.0;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("放大: 视图范围 [%1, %2] -> [%3, %4]")
        .arg(xMin).arg(xMax).arg(newMin).arg(newMax));

    m_model->setViewRange(newMin, newMax);

    // 同步到OpenGL控件
    m_glWidget->setViewRange(newMin, newMax);
}

void WaveformAnalysisController::slot_WA_C_zoomOut()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("缩小操作"));

    if (!m_model || !m_glWidget) return;

    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算中心点
    double center = (xMin + xMax) / 2.0;

    // 放大范围为原来的125%
    double newWidth = (xMax - xMin) * 1.25;

    // 设置新范围
    double newMin = center - newWidth / 2.0;
    double newMax = center + newWidth / 2.0;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("缩小: 视图范围 [%1, %2] -> [%3, %4]")
        .arg(xMin).arg(xMax).arg(newMin).arg(newMax));

    m_model->setViewRange(newMin, newMax);

    // 同步到OpenGL控件
    m_glWidget->setViewRange(newMin, newMax);
}

void WaveformAnalysisController::slot_WA_C_zoomReset()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置缩放"));

    if (!m_model || !m_glWidget) return;

    // 重置为显示所有数据
    QVector<double> indexData = m_model->getIndexData();

    if (!indexData.isEmpty()) {
        double min = indexData.first();
        double max = indexData.last();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("重置视图范围到 [%1, %2]").arg(min).arg(max));
        m_model->setViewRange(min, max);
        m_glWidget->setViewRange(min, max);
    }
    else {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("索引数据为空，设置默认范围 [0, 100]"));
        m_model->setViewRange(0, 100);
        m_glWidget->setViewRange(0, 100);
    }
}

void WaveformAnalysisController::slot_WA_C_setVerticalScale(double scale)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置垂直缩放: %1").arg(scale));

    if (scale > 0) {
        m_verticalScale = scale;

        // 更新OpenGL控件的垂直缩放
        if (m_glWidget) {
            m_glWidget->setVerticalScale(scale);
        }
    }
}

bool WaveformAnalysisController::slot_WA_C_loadData(int startIndex, int length)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("波形分析控制器加载数据 - 起始: %1, 长度: %2")
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

        // 同步到OpenGL控件
        m_glWidget->setViewRange(min, max);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("设置初始视图范围: [%1, %2]").arg(min).arg(max));

        // 确保通道数据和索引数据长度一致
        ensureDataConsistency();
    }
    else {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("波形数据加载失败"));
    }

    return success;
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

        // 更新OpenGL控件
        if (m_glWidget) {
            m_glWidget->requestUpdate();
        }
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

void WaveformAnalysisController::onGLWidgetViewRangeChanged(double xMin, double xMax)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件视图范围变更: [%1, %2]").arg(xMin).arg(xMax));

    // 同步到模型
    m_model->setViewRange(xMin, xMax);

    // 检查是否需要加载更多数据
    if (shouldLoadMoreData(xMin, xMax)) {
        // 计算加载范围，比当前视图范围略大一些，减少未来加载需求
        double viewRange = xMax - xMin;
        int loadStart = std::max(0, static_cast<int>(std::floor(xMin - viewRange * 0.5)));
        int loadLength = static_cast<int>(std::ceil(viewRange * 2.0));

        // 限制单次加载的最大数据量
        const int MAX_LOAD_LENGTH = 500000;
        if (loadLength > MAX_LOAD_LENGTH) {
            double center = (xMin + xMax) / 2.0;
            loadStart = static_cast<int>(std::floor(center - MAX_LOAD_LENGTH / 2.0));
            loadStart = std::max(0, loadStart);
            loadLength = MAX_LOAD_LENGTH;
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("加载额外数据 - 起始: %1, 长度: %2").arg(loadStart).arg(loadLength));
        slot_WA_C_loadDataRange(loadStart, loadLength);
    }
}

void WaveformAnalysisController::onGLWidgetMarkerAdded(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件请求添加标记点: %1").arg(index));

    // 添加标记点到模型
    m_model->addMarkerPoint(index);
}

void WaveformAnalysisController::onGLWidgetPanRequested(int deltaX)
{
    if (!m_model || !m_glWidget) return;

    double xMin, xMax;
    m_model->getViewRange(xMin, xMax);

    // 计算移动距离对应的数据范围
    double dataRange = xMax - xMin;
    double dataDelta = (m_glWidget->width() > 0) ?
        deltaX * dataRange / m_glWidget->width() : 0;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("处理平移 - 像素偏移: %1, 数据偏移: %2")
        .arg(deltaX).arg(dataDelta));

    // 获取实际数据范围
    double minDataIndex = 0;
    double maxDataIndex = 0;

    // 获取实际通道数据的范围
    for (int ch = 0; ch < 4; ++ch) {
        if (m_model->isChannelEnabled(ch)) {
            QVector<double> data = m_model->getChannelData(ch);
            if (!data.isEmpty()) {
                maxDataIndex = std::max(maxDataIndex, static_cast<double>(data.size() - 1));
            }
        }
    }

    // 计算新的视图范围
    double newMin = xMin - dataDelta;
    double newMax = xMax - dataDelta;

    // 边界检查 - 确保不会平移到数据范围之外
    if (newMin < minDataIndex) {
        // 将整个视图移回有效范围
        double shift = minDataIndex - newMin;
        newMin = minDataIndex;
        newMax += shift;
    }

    if (newMax > maxDataIndex && maxDataIndex > 0) {
        // 将整个视图移回有效范围
        double shift = newMax - maxDataIndex;
        newMax = maxDataIndex;
        newMin = std::max(minDataIndex, newMin - shift);
    }

    // 确保视图范围至少有一定宽度
    double minWidth = 1.0;
    if (newMax - newMin < minWidth) {
        newMax = std::min(maxDataIndex, newMin + minWidth);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置新视图范围: [%1, %2]").arg(newMin).arg(newMax));

    // 仅在范围确实变化时更新
    if (newMin != xMin || newMax != xMax) {
        m_model->setViewRange(newMin, newMax);
        m_glWidget->setViewRange(newMin, newMax);
    }
}

void WaveformAnalysisController::onGLWidgetLoadDataRequested(int startIndex, int length)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件请求加载数据 - 起始: %1, 长度: %2")
        .arg(startIndex).arg(length));

    // 加载请求的数据范围
    slot_WA_C_loadDataRange(startIndex, length);
}

void WaveformAnalysisController::slot_WA_C_onDataLoaded(bool success)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("数据加载结果: %1").arg(success ? "成功" : "失败"));

    if (!m_view || !m_model || !m_glWidget) return;

    if (!success) {
        // 没有数据是正常的，没有采集当然没有数据
        return;
    }

    // 检查模型中是否有数据
    bool hasData = false;
    int maxDataSize = 0;
    for (int ch = 0; ch < 4; ++ch) {
        QVector<double> data = m_model->getChannelData(ch);
        int dataSize = data.size();
        if (dataSize > 0) {
            hasData = true;
            maxDataSize = qMax(maxDataSize, dataSize);
            LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1数据加载后状态: 大小=%2")
                .arg(ch).arg(dataSize));
        }
    }

    if (!hasData) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("所有通道均无数据"));
        return;
    }

    // 获取加载后的索引数据
    QVector<double> indexData = m_model->getIndexData();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("索引数据加载后状态: 大小=%1, 首值=%2, 尾值=%3")
        .arg(indexData.size())
        .arg(indexData.isEmpty() ? "N/A" : QString::number(indexData.first()))
        .arg(indexData.isEmpty() ? "N/A" : QString::number(indexData.last())));

    // 确保通道数据和索引数据长度一致
    ensureDataConsistency();

    // 调整视图范围，确保设置了合理的范围
    if (!indexData.isEmpty()) {
        double startIdx = indexData.first();
        double endIdx = indexData.last();

        // 强制应用新的范围
        LOG_INFO(LocalQTCompat::fromLocal8Bit("重置视图范围: %1 到 %2").arg(startIdx).arg(endIdx));
        m_model->setViewRange(startIdx, endIdx);
        m_glWidget->setViewRange(startIdx, endIdx);
    }
    else if (maxDataSize > 0) {
        // 如果索引数据为空但有通道数据，使用通道数据索引
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用通道数据长度作为范围: 0 到 %1").arg(maxDataSize - 1));
        m_model->setViewRange(0, maxDataSize - 1);
        m_glWidget->setViewRange(0, maxDataSize - 1);
    }
    else {
        // 兜底默认范围
        LOG_INFO(LocalQTCompat::fromLocal8Bit("使用默认范围: 0 到 100"));
        m_model->setViewRange(0, 100);
        m_glWidget->setViewRange(0, 100);
    }

    // 更新OpenGL控件
    m_glWidget->requestUpdate();
}

void WaveformAnalysisController::slot_WA_C_onViewRangeChanged(double xMin, double xMax)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("模型视图范围变化: [%1, %2]").arg(xMin).arg(xMax));

    // 同步到OpenGL控件
    if (m_glWidget) {
        m_glWidget->setViewRange(xMin, xMax);
    }

    // 检查并加载需要的数据
    if (shouldLoadMoreData(xMin, xMax)) {
        // 计算加载范围，比当前视图范围略大一些
        double viewRange = xMax - xMin;
        int loadStart = std::max(0, static_cast<int>(std::floor(xMin - viewRange * 0.5)));
        int loadLength = static_cast<int>(std::ceil(viewRange * 2.0));

        // 限制单次加载的最大数据量
        const int MAX_LOAD_LENGTH = 500000;
        if (loadLength > MAX_LOAD_LENGTH) {
            double center = (xMin + xMax) / 2.0;
            loadStart = static_cast<int>(std::floor(center - MAX_LOAD_LENGTH / 2.0));
            loadStart = std::max(0, loadStart);
            loadLength = MAX_LOAD_LENGTH;
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("加载额外数据 - 起始: %1, 长度: %2").arg(loadStart).arg(loadLength));
        slot_WA_C_loadDataRange(loadStart, loadLength);
    }
}

void WaveformAnalysisController::slot_WA_C_onMarkersChanged()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("Marker状态改变"));

    if (!m_view || !m_model) return;

    // 更新UI
    if (m_view) {
        // 获取当前标记点
        QVector<int> markers = m_model->getMarkerPoints();

        // 更新视图中的标记列表
        m_view->updateMarkerList(markers);

        // 更新OpenGL控件
        if (m_glWidget) {
            m_glWidget->requestUpdate();
        }
    }
}

void WaveformAnalysisController::slot_WA_C_onChannelStateChanged(int channel, bool enabled)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道 %1 状态变化: %2")
        .arg(channel).arg(enabled ? "启用" : "禁用"));

    // 更新OpenGL控件
    if (m_glWidget) {
        m_glWidget->requestUpdate();
    }
}

void WaveformAnalysisController::slot_WA_C_onUpdateTimerTriggered()
{
    // 更新OpenGL控件
    if (m_glWidget) {
        m_glWidget->requestUpdate();
    }
}

bool WaveformAnalysisController::loadWaveformDataFromService(int startIndex, int length)
{
    // 检查数据服务是否可用
    if (!m_dataService) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("数据访问服务未设置，无法加载波形数据"));
        return false;
    }

    // 检查范围是否有效
    if (startIndex < 0 || length <= 0) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的数据范围"));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始加载波形数据 - 起始: %1, 长度: %2").arg(startIndex).arg(length));

    // 创建一个进度对话框，但先不显示
    QProgressDialog* progressDialog = nullptr;
    QTimer* progressTimer = new QTimer(this);

    // 设置500ms后才显示进度对话框，避免短时操作闪烁
    connect(progressTimer, &QTimer::timeout, this, [&progressDialog, this]() {
        if (!progressDialog) {
            progressDialog = new QProgressDialog("正在加载数据...", "取消", 0, 100,
                qobject_cast<QWidget*>(parent()));
            progressDialog->setWindowModality(Qt::WindowModal);
            progressDialog->setMinimumDuration(0);
            progressDialog->setValue(10);
            progressDialog->show();
        }
        });
    progressTimer->setSingleShot(true);
    progressTimer->start(500);

    // 创建加载任务并放入后台线程
    auto future = QtConcurrent::run([this, startIndex, length, progressDialog, progressTimer]() {
        bool success = false;

        try {
            // 先生成索引数据
            QVector<double> indexData;
            indexData.reserve(length);
            for (int i = 0; i < length; ++i) {
                indexData.append(startIndex + i);
            }

            // 准备通道数据容器
            QVector<QVector<double>> channelDataResults(4);

            // 读取原始数据
            QByteArray rawData = m_dataService->readRawData(startIndex, length);

            if (!rawData.isEmpty()) {
                // 成功读取原始数据
                // 更新缓存
                m_dataPacketCache = QSharedPointer<QByteArray>(new QByteArray(rawData));
                m_cacheStartIndex = startIndex;
                m_cacheLength = rawData.size();
                m_isCacheValid = true;

                // 提取每个通道的数据
                for (int ch = 0; ch < 4; ++ch) {
                    channelDataResults[ch] = m_dataService->extractChannelData(rawData, ch);
                }

                success = true;
            }

            // 在主线程中更新UI
            QMetaObject::invokeMethod(this, [this, success, indexData, channelDataResults,
                startIndex, length, progressTimer, progressDialog]() {
                    // 取消进度对话框计时器
                    progressTimer->stop();
                    delete progressTimer;

                    // 关闭进度对话框
                    if (progressDialog) {
                        progressDialog->close();
                        progressDialog->deleteLater();
                    }

                    if (success && m_model) {
                        // 更新模型数据
                        m_model->updateIndexData(indexData);

                        for (int ch = 0; ch < 4; ++ch) {
                            if (!channelDataResults[ch].isEmpty()) {
                                m_model->updateChannelData(ch, channelDataResults[ch]);
                            }
                        }

                        // 确保通道数据和索引数据长度一致
                        ensureDataConsistency();

                        // 设置视图范围
                        m_model->setViewRange(startIndex, startIndex + length - 1);

                        // 同步到OpenGL控件
                        if (m_glWidget) {
                            m_glWidget->setViewRange(startIndex, startIndex + length - 1);
                            m_glWidget->requestUpdate();
                        }

                        // 发送数据加载成功信号
                        emit m_model->signal_WA_M_dataLoaded(true);
                    }
                    else if (m_model) {
                        emit m_model->signal_WA_M_dataLoaded(false);
                    }
                }, Qt::QueuedConnection);

        }
        catch (const std::exception& e) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载波形数据异常: %1").arg(e.what()));

            // 在主线程中处理错误
            QMetaObject::invokeMethod(this, [progressTimer, progressDialog]() {
                progressTimer->stop();
                delete progressTimer;

                if (progressDialog) {
                    progressDialog->close();
                    progressDialog->deleteLater();
                }
                }, Qt::QueuedConnection);
        }
        });

    return true; // 返回true表示加载操作已开始
}

bool WaveformAnalysisController::shouldLoadMoreData(double xMin, double xMax)
{
    // 如果缓存无效，需要加载
    if (!m_isCacheValid || !m_dataPacketCache || m_dataPacketCache->isEmpty()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("缓存无效，需要加载更多数据"));
        return true;
    }

    // 如果请求的范围完全在缓存中，不需要加载
    if (xMin >= m_cacheStartIndex && xMax <= m_cacheStartIndex + m_cacheLength) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("请求范围完全在缓存中，无需加载"));
        return false;
    }

    // 如果超出缓存太多，需要加载
    double overlap = 0.0;
    double requestLength = xMax - xMin;

    // 计算与缓存区域的重叠部分
    if (xMin < m_cacheStartIndex && xMax > m_cacheStartIndex) {
        overlap = xMax - m_cacheStartIndex;
    }
    else if (xMin < m_cacheStartIndex + m_cacheLength && xMax > m_cacheStartIndex + m_cacheLength) {
        overlap = m_cacheStartIndex + m_cacheLength - xMin;
    }
    else if (xMin >= m_cacheStartIndex && xMax <= m_cacheStartIndex + m_cacheLength) {
        overlap = requestLength;
    }

    // 计算重叠比例
    double overlapRatio = requestLength > 0 ? (overlap / requestLength) : 0;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("与缓存重叠比例: %1").arg(overlapRatio));

    // 如果重叠不足50%，需要加载新数据
    return overlapRatio < 0.5;
}

void WaveformAnalysisController::updateDataCache(int startIndex, int length)
{
    if (!m_dataService) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("数据访问服务未设置，无法更新缓存"));
        return;
    }

    // 读取原始数据
    QByteArray rawData = m_dataService->readRawData(startIndex, length);

    if (!rawData.isEmpty()) {
        m_dataPacketCache = QSharedPointer<QByteArray>(new QByteArray(rawData));
        m_cacheStartIndex = startIndex;
        m_cacheLength = rawData.size();
        m_isCacheValid = true;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("更新数据缓存成功 - 起始: %1, 长度: %2")
            .arg(startIndex).arg(m_cacheLength));
    }
    else {
        m_isCacheValid = false;
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("更新数据缓存失败"));
    }
}

void WaveformAnalysisController::ensureDataConsistency()
{
    if (!m_model) return;

    // 检查所有通道数据长度
    int maxLength = 0;
    for (int ch = 0; ch < 4; ++ch) {
        QVector<double> data = m_model->getChannelData(ch);
        if (!data.isEmpty()) {
            maxLength = qMax(maxLength, data.size());
        }
    }

    if (maxLength == 0) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("没有有效的通道数据"));
        return;
    }

    // 获取索引数据
    QVector<double> indexData = m_model->getIndexData();

    // 如果索引数据长度与通道数据不一致，调整索引数据
    if (indexData.size() != maxLength) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("索引数据长度(%1)与通道数据长度(%2)不一致，调整索引数据")
            .arg(indexData.size()).arg(maxLength));

        QVector<double> newIndexData;
        newIndexData.reserve(maxLength);
        for (int i = 0; i < maxLength; ++i) {
            newIndexData.append(i);
        }

        m_model->updateIndexData(newIndexData);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("已生成 %1 个新索引数据点").arg(newIndexData.size()));
    }
}

void WaveformAnalysisController::generateMockData()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("生成测试波形数据"));

    // 生成测试用的索引数据
    const int dataPoints = 1000;
    QVector<double> indexData;
    indexData.reserve(dataPoints);
    for (int i = 0; i < dataPoints; ++i) {
        indexData.append(i);
    }

    // 更新索引数据到模型
    m_model->updateIndexData(indexData);

    // 为每个通道生成不同模式的测试数据
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_model->isChannelEnabled(ch)) continue;

        QVector<double> channelData;
        channelData.reserve(dataPoints);

        // 为不同通道生成不同特征的数据
        switch (ch) {
        case 0: // 通道0：方波 (0和1交替)
            for (int i = 0; i < dataPoints; ++i) {
                channelData.append((i / 25) % 2 ? 1.0 : 0.0);
            }
            break;

        case 1: // 通道1：方波 (0和1交替，但频率不同)
            for (int i = 0; i < dataPoints; ++i) {
                channelData.append((i / 50) % 2 ? 1.0 : 0.0);
            }
            break;

        case 2: // 通道2：脉冲波形
            for (int i = 0; i < dataPoints; ++i) {
                channelData.append(i % 100 < 10 ? 1.0 : 0.0);
            }
            break;

        case 3: // 通道3：复杂的脉冲序列
            for (int i = 0; i < dataPoints; ++i) {
                if (i % 120 < 10) {
                    channelData.append(1.0);
                }
                else if ((i + 40) % 120 < 5) {
                    channelData.append(1.0);
                }
                else {
                    channelData.append(0.0);
                }
            }
            break;
        }

        // 更新通道数据到模型
        m_model->updateChannelData(ch, channelData);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("已为通道 %1 生成 %2 个测试数据点")
            .arg(ch).arg(channelData.size()));
    }

    // 设置初始视图范围
    m_model->setViewRange(0, dataPoints - 1);

    // 发送数据加载成功信号，触发界面更新
    emit m_model->signal_WA_M_dataLoaded(true);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("测试数据生成完成"));
}
