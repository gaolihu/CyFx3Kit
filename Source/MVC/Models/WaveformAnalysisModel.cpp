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

    m_dataService = &DataAccessService::getInstance();
#if 1
    // 测试用
    // 初始化索引数据，确保至少有基本数据
    m_indexData.resize(100);
    for (int i = 0; i < 100; i++) {
        m_indexData[i] = i;
    }
#endif
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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始加载波形数据: 文件=%1, 起始=%2, 长度=%3")
        .arg(filename).arg(startIndex).arg(length));

    if (!m_dataService) {
        LOG_ERROR("数据访问服务未设置，尝试获取服务实例");
        m_dataService = &DataAccessService::getInstance();
        if (!m_dataService) {
            LOG_ERROR("获取数据服务实例失败");
            return false;
        }
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

        // 使用DataAccessService获取实际通道数据
        bool allChannelsEmpty = true;
        for (int channel = 0; channel < 4; ++channel) {
            // 调用DataAccessService获取通道数据
            LOG_INFO(LocalQTCompat::fromLocal8Bit("开始获取通道%1数据").arg(channel));
            QVector<double> channelData = m_dataService->getChannelData(
                filename, channel, startIndex, length);

            if (!channelData.isEmpty()) {
                m_channelData[channel] = channelData;
                allChannelsEmpty = false;
                LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1数据加载成功: 大小=%2")
                    .arg(channel).arg(channelData.size()));
            }
            else {
                LOG_WARN(LocalQTCompat::fromLocal8Bit("通道%1数据加载失败或为空，使用模拟数据").arg(channel));

                // 如果获取失败，生成模拟数据作为后备
                QVector<double> simulatedData;
                simulatedData.reserve(length);

                for (int i = 0; i < length; ++i) {
                    // 生成不同模式的模拟数据，确保可视化效果
                    double value = (i % (10 + channel * 5) < (5 + channel * 2)) ? 1.0 : 0.0;
                    simulatedData.append(value);
                }

                m_channelData[channel] = simulatedData;
                allChannelsEmpty = false;
                LOG_INFO(LocalQTCompat::fromLocal8Bit("通道%1使用模拟数据: 大小=%2")
                    .arg(channel).arg(simulatedData.size()));
            }
        }

        // 确保视图范围有效
        if (length > 0) {
            m_xMin = startIndex;
            m_xMax = startIndex + length - 1;
        }
        else {
            // 设置默认视图范围
            m_xMin = 0;
            m_xMax = 100;
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("设置视图范围: [%1, %2]").arg(m_xMin).arg(m_xMax));

        m_isLoading = false;

        // 发送数据加载完成信号
        emit dataLoaded(!allChannelsEmpty);
        emit viewRangeChanged(m_xMin, m_xMax);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("波形数据加载完成: 文件=%1, 起始=%2, 长度=%3")
            .arg(filename).arg(startIndex).arg(length));
        return !allChannelsEmpty;
    }
    catch (const std::exception& e) {
        m_isLoading = false;
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载波形数据异常: %1").arg(e.what()));

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("解析包数据，大小：%1").arg(data.size()));

    try {
        // 清除现有数据
        for (int i = 0; i < 4; ++i) {
            m_channelData[i].clear();
        }
        m_indexData.clear();

        // 解析二进制数据到通道数据
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

                // 根据通道索引提取不同的位
                double value = 0.0;
                switch (channel) {
                case 0: // BYTE0：提取最低2位
                    value = byteValue & 0x03;
                    break;
                case 1: // BYTE1：提取中间2位
                    value = (byteValue >> 2) & 0x03;
                    break;
                case 2: // BYTE2：提取高2位
                    value = (byteValue >> 4) & 0x03;
                    break;
                case 3: // BYTE3：提取最高2位
                    value = (byteValue >> 6) & 0x03;
                    break;
                }

                // 归一化到0-1范围，大于0的值都视为高电平
                value = value > 0 ? 1.0 : 0.0;

                channelData.append(value);
            }

            m_channelData[channel] = channelData;
        }

        // 设置默认视图范围
        m_xMin = 0;
        m_xMax = dataLength - 1;

        // 确保视图范围有效
        if (m_xMax <= m_xMin) {
            m_xMin = 0;
            m_xMax = 100;
        }

        // 发送信号通知数据变化
        emit dataLoaded(true);
        emit viewRangeChanged(m_xMin, m_xMax);

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取 %1 通道数据").arg(channel));

    if (channel >= 0 && channel < m_channelData.size()) {
        return m_channelData[channel];
    }
    return QVector<double>();
}

QVector<double> WaveformAnalysisModel::getIndexData() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取索引数据"));

    return m_indexData;
}

void WaveformAnalysisModel::getViewRange(double& xMin, double& xMax) const
{
    // 检查范围值是否有效并记录日志
    if (!std::isfinite(m_xMin) || !std::isfinite(m_xMax) || m_xMin >= m_xMax) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("视图范围异常: %1 ~ %2，返回默认范围")
            .arg(m_xMin)
            .arg(m_xMax));

        // 返回安全的默认值
        xMin = 0.0;
        xMax = 100.0;
    }
    else {
        xMin = m_xMin;
        xMax = m_xMax;

        LOG_INFO(LocalQTCompat::fromLocal8Bit("获取视图范围：%1 ~ %2")
            .arg(xMin)
            .arg(xMax));
    }
}

void WaveformAnalysisModel::setViewRange(double xMin, double xMax)
{
    // 验证参数有效性
    if (!std::isfinite(xMin) || !std::isfinite(xMax)) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的视图范围参数: xMin=%1, xMax=%2").arg(xMin).arg(xMax));
        return;
    }

    if (xMin >= xMax) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的视图范围: xMin(%1) >= xMax(%2)").arg(xMin).arg(xMax));
        return;
    }

    // 防止极端值
    const double MAX_RANGE = 1.0e6;
    if (std::abs(xMin) > MAX_RANGE || std::abs(xMax) > MAX_RANGE) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("视图范围过大: xMin=%1, xMax=%2").arg(xMin).arg(xMax));
        return;
    }

    m_xMin = xMin;
    m_xMax = xMax;

    emit viewRangeChanged(m_xMin, m_xMax);
}

QVector<int> WaveformAnalysisModel::getMarkerPoints() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取Marker点"));

    return m_markerPoints;
}

void WaveformAnalysisModel::addMarkerPoint(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("增加Marker点"));

    if (!m_markerPoints.contains(index)) {
        m_markerPoints.append(index);
        emit markersChanged();
    }
}

void WaveformAnalysisModel::removeMarkerPoint(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("删除Marker点"));

    int pos = m_markerPoints.indexOf(index);
    if (pos >= 0) {
        m_markerPoints.removeAt(pos);
        emit markersChanged();
    }
}

void WaveformAnalysisModel::clearMarkerPoints()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("清理Marker点"));

    if (!m_markerPoints.isEmpty()) {
        m_markerPoints.clear();
        emit markersChanged();
    }
}

double WaveformAnalysisModel::getZoomLevel() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取缩放等级"));

    return m_zoomLevel;
}

void WaveformAnalysisModel::setZoomLevel(double level)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置缩放等级：%1").arg(level));

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("使能%1通道").arg(channel));

    if (channel >= 0 && channel < 4) {
        if (m_channelEnabled[channel] != enabled) {
            m_channelEnabled[channel] = enabled;
            emit channelStateChanged(channel, enabled);
        }
    }
}

QColor WaveformAnalysisModel::getChannelColor(int channel) const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取%1通道色彩").arg(channel));

    return m_channelColors.value(channel, QColor(Qt::black));
}

void WaveformAnalysisModel::setChannelColor(int channel, const QColor& color)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置%1通道色彩").arg(channel));

    if (channel >= 0 && channel < 4) {
        m_channelColors[channel] = color;
    }
}

QColor WaveformAnalysisModel::getGridColor() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取栅格色彩"));

    return m_gridColor;
}

void WaveformAnalysisModel::setGridColor(const QColor& color)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置栅格色彩"));

    m_gridColor = color;
}

QColor WaveformAnalysisModel::getBackgroundColor() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取背景色彩"));

    return m_backgroundColor;
}

void WaveformAnalysisModel::setBackgroundColor(const QColor& color)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置背景色彩"));

    m_backgroundColor = color;
}

float WaveformAnalysisModel::getWaveformLineWidth() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取波形线宽"));

    return m_waveformLineWidth;
}

void WaveformAnalysisModel::setWaveformLineWidth(float width)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置波形线宽：%1").arg(width));

    if (width > 0) {
        m_waveformLineWidth = width;
    }
}

int WaveformAnalysisModel::getWaveformRenderMode() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取波形渲染模式"));

    return m_waveformRenderMode;
}

void WaveformAnalysisModel::setWaveformRenderMode(int mode)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置波形渲染模式：%1").arg(mode));

    if (mode >= 0 && mode <= 1) {
        m_waveformRenderMode = mode;
    }
}

QString WaveformAnalysisModel::getDataAnalysisResult() const
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取数据分析结果"));

    return m_dataAnalysisResult;
}

void WaveformAnalysisModel::analyzeData()
{
    QString result;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("分析数据"));

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
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新通道：%1 数据").arg(channel));

    if (channel >= 0 && channel < 4) {
        m_channelData[channel] = data;
    }
}

void WaveformAnalysisModel::updateIndexData(const QVector<double>& data)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新索引数据：%1").arg(data.size()));

    m_indexData = data;
}