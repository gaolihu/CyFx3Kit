// Source/MVC/Models/WaveformAnalysisModel.cpp
#include "WaveformAnalysisModel.h"
#include "DataAccessService.h"
#include "Logger.h"
#include "DataPacket.h"

WaveformAnalysisModel* WaveformAnalysisModel::getInstance()
{
    static WaveformAnalysisModel instance;
    return &instance;
}

WaveformAnalysisModel::WaveformAnalysisModel()
    : QObject(nullptr)
    , m_dataService(nullptr)
    , m_xMin(0.0)
    , m_xMax(100.0)
    , m_zoomLevel(1.0)
    , m_isLoading(false)
{
    // 初始化默认设置
    initializeDefaults();

    LOG_INFO("波形分析模型已创建");
}

WaveformAnalysisModel::~WaveformAnalysisModel()
{
    LOG_INFO("波形分析模型已销毁");
}

void WaveformAnalysisModel::initializeDefaults()
{
    // 设置默认通道状态
    for (int i = 0; i < 4; ++i) {
        m_channelEnabled[i] = true;
    }

    // 初始化通道数据容器
    m_channelData.resize(4);

    // 设置默认通道颜色
    m_channelColors[0] = QColor(Qt::red);         // BYTE0 - 红色
    m_channelColors[1] = QColor(Qt::blue);        // BYTE1 - 蓝色
    m_channelColors[2] = QColor(Qt::green);       // BYTE2 - 绿色
    m_channelColors[3] = QColor(Qt::magenta);     // BYTE3 - 洋红色

    // 设置默认样式
    m_gridColor = QColor(230, 230, 230);
    m_backgroundColor = QColor(255, 255, 255);
    m_waveformLineWidth = 2.0f;
    m_waveformRenderMode = 0; // 线条模式
}

void WaveformAnalysisModel::setDataAccessService(DataAccessService* service)
{
    m_dataService = service;

    // 连接数据读取完成信号
    if (m_dataService) {
        connect(m_dataService, &DataAccessService::dataReadComplete,
            this, &WaveformAnalysisModel::processReceivedData);
    }
}

bool WaveformAnalysisModel::loadData(const QString& filename, int startIndex, int length)
{
    if (!m_dataService) {
        LOG_ERROR("数据访问服务未设置");
        return false;
    }

    if (m_isLoading) {
        LOG_WARN("已有数据正在加载中");
        return false;
    }

    try {
        m_isLoading = true;

        // 清除现有数据
        for (int i = 0; i < 4; ++i) {
            m_channelData[i].clear();
        }
        m_indexData.clear();

        // 生成索引数据
        m_indexData.reserve(length);
        for (int i = 0; i < length; ++i) {
            m_indexData.append(startIndex + i);
        }

        // 模拟生成通道数据（实际应用中应从文件中读取）
        // 在实际应用中，这里应该调用DataAccessService读取相关数据
        for (int channel = 0; channel < 4; ++channel) {
            QVector<double> data;
            data.reserve(length);

            for (int i = 0; i < length; ++i) {
                // 生成一些示例数据，实际应用中应替换为真实数据
                // 这里生成0和1的序列，模拟数字信号
                double value = (i % (10 + channel * 5) < (5 + channel * 2)) ? 1.0 : 0.0;
                data.append(value);
            }

            m_channelData[channel] = data;
        }

        // 设置默认视图范围
        m_xMin = startIndex;
        m_xMax = startIndex + length - 1;

        m_isLoading = false;

        // 发送数据加载完成信号
        emit dataLoaded(true);
        emit viewRangeChanged(m_xMin, m_xMax);

        LOG_INFO(QString("已加载波形数据, 文件: %1, 起始: %2, 长度: %3")
            .arg(filename)
            .arg(startIndex)
            .arg(length));
        return true;
    }
    catch (const std::exception& e) {
        m_isLoading = false;
        LOG_ERROR(QString("加载波形数据失败: %1").arg(e.what()));

        // 发送数据加载失败信号
        emit dataLoaded(false);
        return false;
    }
}

bool WaveformAnalysisModel::loadDataAsync(uint64_t packetIndex)
{
    if (!m_dataService) {
        LOG_ERROR("数据访问服务未设置");
        return false;
    }

    if (m_isLoading) {
        LOG_WARN("已有数据正在加载中");
        return false;
    }

    m_isLoading = true;

    try {
        // 清除现有数据
        for (int i = 0; i < 4; ++i) {
            m_channelData[i].clear();
        }
        m_indexData.clear();

        // 使用DataAccessService异步读取数据包
        // 如果DataAccessService没有直接提供按索引读取的方法，那么需要增加该接口
        // 这里假设有一个findPacketByIndex方法，实际使用时可能需要调整
        // m_dataService->readPacketByIndex(packetIndex);

        // 或者按时间戳读取
        // 这里我们假设有一个对应的索引到时间戳的转换
        uint64_t timestamp = packetIndex; // 实际应用中需要转换逻辑
        m_dataService->readPacketByTimestamp(timestamp);

        LOG_INFO(QString("开始异步加载数据包, 索引: %1").arg(packetIndex));
        return true;
    }
    catch (const std::exception& e) {
        m_isLoading = false;
        LOG_ERROR(QString("异步加载数据包失败: %1").arg(e.what()));
        emit dataLoaded(false);
        return false;
    }
}

void WaveformAnalysisModel::processReceivedData(uint64_t timestamp, const QByteArray& data)
{
    LOG_INFO(QString("收到数据包, 时间戳: %1, 大小: %2 字节").arg(timestamp).arg(data.size()));

    if (parsePacketData(data)) {
        m_isLoading = false;
        emit dataLoaded(true);
        emit viewRangeChanged(m_xMin, m_xMax);
    }
    else {
        m_isLoading = false;
        emit dataLoaded(false);
    }
}

bool WaveformAnalysisModel::parsePacketData(const QByteArray& data)
{
    try {
        // 清除现有数据
        for (int i = 0; i < 4; ++i) {
            m_channelData[i].clear();
        }
        m_indexData.clear();

        // 解析二进制数据到通道数据
        // 这里仅是一个简单示例，实际应用中需要按照数据格式进行正确解析
        int dataLength = data.size();

        // 确保至少有足够的数据点
        if (dataLength < 4) {
            LOG_ERROR("数据包太小，无法解析");
            return false;
        }

        // 生成索引数据
        m_indexData.reserve(dataLength);
        for (int i = 0; i < dataLength; ++i) {
            m_indexData.append(i);
        }

        // 解析为4个通道的数据
        for (int channel = 0; channel < 4; ++channel) {
            QVector<double> channelData;
            channelData.reserve(dataLength);

            for (int i = 0; i < dataLength; ++i) {
                // 从每个字节中提取对应通道的位
                // 这里假设每个字节包含4个通道的数据，每个通道占用2位
                uint8_t byteValue = static_cast<uint8_t>(data.at(i));
                double value = (byteValue >> (channel * 2)) & 0x03;

                // 归一化到0-1范围
                value = value > 0 ? 1.0 : 0.0;

                channelData.append(value);
            }

            m_channelData[channel] = channelData;
        }

        // 设置默认视图范围
        m_xMin = 0;
        m_xMax = dataLength - 1;

        LOG_INFO(QString("成功解析数据包，数据点数: %1").arg(dataLength));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("解析数据包失败: %1").arg(e.what()));
        return false;
    }
}

QVector<double> WaveformAnalysisModel::getChannelData(int channel) const
{
    if (channel >= 0 && channel < m_channelData.size()) {
        return m_channelData[channel];
    }
    return QVector<double>();
}

QVector<double> WaveformAnalysisModel::getIndexData() const
{
    return m_indexData;
}

void WaveformAnalysisModel::getViewRange(double& xMin, double& xMax) const
{
    xMin = m_xMin;
    xMax = m_xMax;
}

void WaveformAnalysisModel::setViewRange(double xMin, double xMax)
{
    if (xMin >= xMax) {
        return;
    }

    m_xMin = xMin;
    m_xMax = xMax;

    emit viewRangeChanged(m_xMin, m_xMax);
}

QVector<int> WaveformAnalysisModel::getMarkerPoints() const
{
    return m_markerPoints;
}

void WaveformAnalysisModel::addMarkerPoint(int index)
{
    if (!m_markerPoints.contains(index)) {
        m_markerPoints.append(index);
        emit markersChanged();
    }
}

void WaveformAnalysisModel::removeMarkerPoint(int index)
{
    int pos = m_markerPoints.indexOf(index);
    if (pos >= 0) {
        m_markerPoints.removeAt(pos);
        emit markersChanged();
    }
}

void WaveformAnalysisModel::clearMarkerPoints()
{
    if (!m_markerPoints.isEmpty()) {
        m_markerPoints.clear();
        emit markersChanged();
    }
}

double WaveformAnalysisModel::getZoomLevel() const
{
    return m_zoomLevel;
}

void WaveformAnalysisModel::setZoomLevel(double level)
{
    if (level > 0) {
        m_zoomLevel = level;
    }
}

bool WaveformAnalysisModel::isChannelEnabled(int channel) const
{
    return m_channelEnabled.value(channel, false);
}

void WaveformAnalysisModel::setChannelEnabled(int channel, bool enabled)
{
    if (channel >= 0 && channel < 4) {
        if (m_channelEnabled[channel] != enabled) {
            m_channelEnabled[channel] = enabled;
            emit channelStateChanged(channel, enabled);
        }
    }
}

QColor WaveformAnalysisModel::getChannelColor(int channel) const
{
    return m_channelColors.value(channel, QColor(Qt::black));
}

void WaveformAnalysisModel::setChannelColor(int channel, const QColor& color)
{
    if (channel >= 0 && channel < 4) {
        m_channelColors[channel] = color;
    }
}

QColor WaveformAnalysisModel::getGridColor() const
{
    return m_gridColor;
}

void WaveformAnalysisModel::setGridColor(const QColor& color)
{
    m_gridColor = color;
}

QColor WaveformAnalysisModel::getBackgroundColor() const
{
    return m_backgroundColor;
}

void WaveformAnalysisModel::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
}

float WaveformAnalysisModel::getWaveformLineWidth() const
{
    return m_waveformLineWidth;
}

void WaveformAnalysisModel::setWaveformLineWidth(float width)
{
    if (width > 0) {
        m_waveformLineWidth = width;
    }
}

int WaveformAnalysisModel::getWaveformRenderMode() const
{
    return m_waveformRenderMode;
}

void WaveformAnalysisModel::setWaveformRenderMode(int mode)
{
    if (mode >= 0 && mode <= 1) {
        m_waveformRenderMode = mode;
    }
}

QString WaveformAnalysisModel::getDataAnalysisResult() const
{
    return m_dataAnalysisResult;
}

void WaveformAnalysisModel::analyzeData()
{
    QString result;

    // 分析每个通道的数据
    for (int ch = 0; ch < 4; ++ch) {
        if (!isChannelEnabled(ch)) {
            continue;
        }

        const QVector<double>& data = m_channelData[ch];
        if (data.isEmpty()) {
            continue;
        }

        // 计算可见区域的范围
        int startIdx = qMax(0, qCeil(m_xMin));
        int endIdx = qMin(data.size() - 1, qFloor(m_xMax));

        // 统计高低电平
        int highLevelCount = 0;
        int lowLevelCount = 0;
        int transitionCount = 0;
        double lastValue = -1;

        for (int i = startIdx; i <= endIdx; ++i) {
            double value = data[i];

            if (value > 0.5) {
                highLevelCount++;
            }
            else {
                lowLevelCount++;
            }

            if (lastValue >= 0 && lastValue != value) {
                transitionCount++;
            }

            lastValue = value;
        }

        // 计算统计数据
        int totalPoints = endIdx - startIdx + 1;
        double highPercent = totalPoints > 0 ? (highLevelCount * 100.0 / totalPoints) : 0;
        double lowPercent = totalPoints > 0 ? (lowLevelCount * 100.0 / totalPoints) : 0;
        double avgPeriod = transitionCount > 0 ? (totalPoints * 2.0 / transitionCount) : 0;

        // 添加到结果
        result += QString("通道 BYTE%1:\n").arg(ch);
        result += QString("  数据点数: %1\n").arg(totalPoints);
        result += QString("  高电平次数: %1 (%2%)\n").arg(highLevelCount).arg(highPercent, 0, 'f', 1);
        result += QString("  低电平次数: %1 (%2%)\n").arg(lowLevelCount).arg(lowPercent, 0, 'f', 1);
        result += QString("  电平跳变次数: %1\n").arg(transitionCount);
        result += QString("  平均周期: %1 个采样点\n").arg(avgPeriod, 0, 'f', 2);
        result += "\n";
    }

    m_dataAnalysisResult = result;
    emit dataAnalysisCompleted(result);
}

void WaveformAnalysisModel::updateChannelData(int channel, const QVector<double>& data)
{
    if (channel >= 0 && channel < 4) {
        m_channelData[channel] = data;
    }
}

void WaveformAnalysisModel::updateIndexData(const QVector<double>& data)
{
    m_indexData = data;
}