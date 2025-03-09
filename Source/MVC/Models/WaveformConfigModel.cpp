// WaveformConfigModel.cpp
#include "WaveformConfigModel.h"
#include "Logger.h"
#include <QSettings>
#include <QDateTime>
#include <cmath>
#include <algorithm>
#include <numeric>

WaveformConfigModel* WaveformConfigModel::getInstance()
{
    static WaveformConfigModel s;
    return &s;
}

WaveformConfigModel::WaveformConfigModel()
    : QObject(nullptr)
    , m_config(createDefaultConfig())
{
    LOG_INFO("波形配置模型已创建");
}

WaveformConfigModel::~WaveformConfigModel()
{
    LOG_INFO("波形配置模型已销毁");
}

WaveformConfig WaveformConfigModel::getConfig() const
{
    return m_config;
}

void WaveformConfigModel::setConfig(const WaveformConfig& config)
{
    m_config = config;
    emit configChanged(m_config);
    LOG_INFO("波形配置已更新");
}

QVector<double> WaveformConfigModel::getXData() const
{
    return m_xData;
}

QVector<double> WaveformConfigModel::getYData() const
{
    return m_yData;
}

void WaveformConfigModel::setWaveformData(const QVector<double>& xData, const QVector<double>& yData)
{
    // 检查数据有效性
    if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
        LOG_WARN("设置波形数据失败: 数据为空或X/Y长度不匹配");
        return;
    }

    // 保存数据
    m_xData = xData;
    m_yData = yData;

    // 更新测量结果和标记点
    updateMeasurementResult();
    updateMarkers();

    // 发送数据变更信号
    emit waveformDataChanged(m_xData, m_yData);

    LOG_INFO(QString("波形数据已更新, 数据点数: %1").arg(m_xData.size()));
}

void WaveformConfigModel::addDataPoint(double x, double y)
{
    // 添加数据点
    m_xData.append(x);
    m_yData.append(y);

    // 限制数据点数量，避免内存占用过大
    const int MAX_POINTS = 10000;
    while (m_xData.size() > MAX_POINTS) {
        m_xData.removeFirst();
        m_yData.removeFirst();
    }

    // 更新测量结果和标记点
    updateMeasurementResult();
    updateMarkers();

    // 发送数据变更信号
    emit waveformDataChanged(m_xData, m_yData);
}

void WaveformConfigModel::clearData()
{
    m_xData.clear();
    m_yData.clear();
    m_markerXData.clear();
    m_markerYData.clear();

    // 重置测量结果
    m_measurementResult = MeasurementResult();

    // 发送数据变更信号
    emit waveformDataChanged(m_xData, m_yData);
    emit markersChanged(m_markerXData, m_markerYData);
    emit measurementResultChanged(m_measurementResult);

    LOG_INFO("波形数据已清空");
}

MeasurementResult WaveformConfigModel::getMeasurementResult() const
{
    return m_measurementResult;
}

QVector<double> WaveformConfigModel::getMarkerXData() const
{
    return m_markerXData;
}

QVector<double> WaveformConfigModel::getMarkerYData() const
{
    return m_markerYData;
}

bool WaveformConfigModel::saveConfig()
{
    try {
        QSettings settings("FX3Tool", "WaveformConfig");

        // 保存波形模式和触发模式
        settings.setValue("waveformMode", static_cast<int>(m_config.waveformMode));
        settings.setValue("triggerMode", static_cast<int>(m_config.triggerMode));

        // 保存参数设置
        settings.setValue("sampleRate", m_config.sampleRate);
        settings.setValue("triggerLevel", m_config.triggerLevel);
        settings.setValue("triggerSlope", m_config.triggerSlope);
        settings.setValue("preTriggerPercent", m_config.preTriggerPercent);
        settings.setValue("windowSize", m_config.windowSize);
        settings.setValue("windowType", m_config.windowType);

        // 保存显示设置
        settings.setValue("zoomLevel", m_config.zoomLevel);
        settings.setValue("timeBase", m_config.timeBase);
        settings.setValue("voltageScale", m_config.voltageScale);
        settings.setValue("autoScale", m_config.autoScale);
        settings.setValue("gridEnabled", m_config.gridEnabled);
        settings.setValue("refreshRate", m_config.refreshRate);
        settings.setValue("colorTheme", m_config.colorTheme);

        // 保存分析设置
        settings.setValue("peakDetection", m_config.peakDetection);
        settings.setValue("noiseFilter", m_config.noiseFilter);
        settings.setValue("autoCorrelation", m_config.autoCorrelation);

        LOG_INFO("波形配置已保存到存储");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存波形配置失败: %1").arg(e.what()));
        return false;
    }
}

bool WaveformConfigModel::loadConfig()
{
    try {
        QSettings settings("FX3Tool", "WaveformConfig");

        // 加载波形模式和触发模式
        m_config.waveformMode = static_cast<WaveformMode>(
            settings.value("waveformMode", static_cast<int>(WaveformMode::ANALOG)).toInt());
        m_config.triggerMode = static_cast<TriggerMode>(
            settings.value("triggerMode", static_cast<int>(TriggerMode::AUTO)).toInt());

        // 加载参数设置
        m_config.sampleRate = settings.value("sampleRate", 10000.0).toDouble();
        m_config.triggerLevel = settings.value("triggerLevel", 0.0).toDouble();
        m_config.triggerSlope = settings.value("triggerSlope", 0).toInt();
        m_config.preTriggerPercent = settings.value("preTriggerPercent", 20).toInt();
        m_config.windowSize = settings.value("windowSize", 1024).toInt();
        m_config.windowType = settings.value("windowType", 0).toInt();

        // 加载显示设置
        m_config.zoomLevel = settings.value("zoomLevel", 1.0).toDouble();
        m_config.timeBase = settings.value("timeBase", 1.0).toDouble();
        m_config.voltageScale = settings.value("voltageScale", 1.0).toDouble();
        m_config.autoScale = settings.value("autoScale", true).toBool();
        m_config.gridEnabled = settings.value("gridEnabled", true).toBool();
        m_config.refreshRate = settings.value("refreshRate", 10).toInt();
        m_config.colorTheme = settings.value("colorTheme", 0).toInt();

        // 加载分析设置
        m_config.peakDetection = settings.value("peakDetection", false).toBool();
        m_config.noiseFilter = settings.value("noiseFilter", false).toBool();
        m_config.autoCorrelation = settings.value("autoCorrelation", false).toBool();

        // 重置运行状态
        m_config.isRunning = false;

        // 发送配置变更信号
        emit configChanged(m_config);

        LOG_INFO("波形配置已从存储加载");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载波形配置失败: %1").arg(e.what()));
        // 加载失败时使用默认配置
        resetToDefault();
        return false;
    }
}

void WaveformConfigModel::resetToDefault()
{
    m_config = createDefaultConfig();
    emit configChanged(m_config);
    LOG_INFO("波形配置已重置为默认值");
}

WaveformConfig WaveformConfigModel::createDefaultConfig()
{
    WaveformConfig config;

    // 设置默认值
    config.waveformMode = WaveformMode::ANALOG;
    config.triggerMode = TriggerMode::AUTO;
    config.sampleRate = 10000.0;
    config.triggerLevel = 0.0;
    config.triggerSlope = 0;
    config.preTriggerPercent = 20;
    config.windowSize = 1024;
    config.windowType = 0;
    config.zoomLevel = 1.0;
    config.timeBase = 1.0;
    config.voltageScale = 1.0;
    config.autoScale = true;
    config.gridEnabled = true;
    config.refreshRate = 10;
    config.colorTheme = 0;
    config.peakDetection = false;
    config.noiseFilter = false;
    config.autoCorrelation = false;
    config.isRunning = false;

    return config;
}

void WaveformConfigModel::updateMeasurementResult()
{
    // 检查数据是否为空
    if (m_yData.isEmpty()) {
        m_measurementResult = MeasurementResult();
        emit measurementResultChanged(m_measurementResult);
        return;
    }

    // 计算基本统计值
    double sum = 0.0;
    double sumSquares = 0.0;
    double minVal = m_yData[0];
    double maxVal = m_yData[0];
    int minIndex = 0;
    int maxIndex = 0;

    for (int i = 0; i < m_yData.size(); ++i) {
        double value = m_yData[i];
        sum += value;
        sumSquares += value * value;

        if (value < minVal) {
            minVal = value;
            minIndex = i;
        }

        if (value > maxVal) {
            maxVal = value;
            maxIndex = i;
        }
    }

    // 计算均值和标准差
    double average = sum / m_yData.size();
    double variance = (sumSquares / m_yData.size()) - (average * average);
    double stdDev = std::sqrt(variance);
    double rmsValue = std::sqrt(sumSquares / m_yData.size());
    double peakToPeak = maxVal - minVal;

    // 估计频率和周期（如果有时间数据）
    double frequency = 0.0;
    double period = 0.0;
    int zeroCrossings = 0;

    if (m_xData.size() > 1) {
        // 简单的过零点检测来估计周期
        for (int i = 1; i < m_yData.size(); ++i) {
            if ((m_yData[i - 1] < average && m_yData[i] > average) ||
                (m_yData[i - 1] > average && m_yData[i] < average)) {
                zeroCrossings++;
            }
        }

        double timeSpan = m_xData.last() - m_xData.first();

        if (zeroCrossings > 0) {
            period = (2.0 * timeSpan) / zeroCrossings;
            frequency = 1.0 / period;
        }
    }

    // 更新测量结果
    m_measurementResult.minValue = minVal;
    m_measurementResult.maxValue = maxVal;
    m_measurementResult.avgValue = average;
    m_measurementResult.peakToPeak = peakToPeak;
    m_measurementResult.frequency = frequency;
    m_measurementResult.period = period;
    m_measurementResult.rmsValue = rmsValue;
    m_measurementResult.stdDeviation = stdDev;
    m_measurementResult.zeroCrossings = zeroCrossings;
    m_measurementResult.maxIndex = maxIndex;
    m_measurementResult.minIndex = minIndex;
    m_measurementResult.analysisTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // 发送测量结果变更信号
    emit measurementResultChanged(m_measurementResult);
}

void WaveformConfigModel::updateMarkers()
{
    // 清空现有标记
    m_markerXData.clear();
    m_markerYData.clear();

    // 检查数据是否为空
    if (m_xData.isEmpty() || m_yData.isEmpty()) {
        emit markersChanged(m_markerXData, m_markerYData);
        return;
    }

    // 峰值检测
    if (m_config.peakDetection) {
        // 为最大值和最小值添加标记
        if (m_measurementResult.maxIndex >= 0 && m_measurementResult.maxIndex < m_xData.size()) {
            m_markerXData.append(m_xData[m_measurementResult.maxIndex]);
            m_markerYData.append(m_yData[m_measurementResult.maxIndex]);
        }

        if (m_measurementResult.minIndex >= 0 && m_measurementResult.minIndex < m_xData.size() &&
            m_measurementResult.minIndex != m_measurementResult.maxIndex) {
            m_markerXData.append(m_xData[m_measurementResult.minIndex]);
            m_markerYData.append(m_yData[m_measurementResult.minIndex]);
        }

        // 添加过零点标记
        double avg = m_measurementResult.avgValue;
        for (int i = 1; i < m_yData.size(); ++i) {
            if ((m_yData[i - 1] < avg && m_yData[i] > avg) ||
                (m_yData[i - 1] > avg && m_yData[i] < avg)) {
                // 使用线性插值计算更精确的过零点
                double t = (avg - m_yData[i - 1]) / (m_yData[i] - m_yData[i - 1]);
                double x = m_xData[i - 1] + t * (m_xData[i] - m_xData[i - 1]);

                m_markerXData.append(x);
                m_markerYData.append(avg);
            }
        }
    }

    // 触发点标记
    if (m_config.triggerMode != TriggerMode::AUTO) {
        double triggerLevel = m_config.triggerLevel;
        for (int i = 1; i < m_yData.size(); ++i) {
            bool triggered = false;

            // 根据触发边沿检查触发条件
            if (m_config.triggerSlope == 0) { // 上升沿
                triggered = (m_yData[i - 1] < triggerLevel && m_yData[i] >= triggerLevel);
            }
            else if (m_config.triggerSlope == 1) { // 下降沿
                triggered = (m_yData[i - 1] > triggerLevel && m_yData[i] <= triggerLevel);
            }
            else { // 双向
                triggered = (m_yData[i - 1] < triggerLevel && m_yData[i] >= triggerLevel) ||
                    (m_yData[i - 1] > triggerLevel && m_yData[i] <= triggerLevel);
            }

            if (triggered) {
                // 使用线性插值计算更精确的触发点
                double t = (triggerLevel - m_yData[i - 1]) / (m_yData[i] - m_yData[i - 1]);
                double x = m_xData[i - 1] + t * (m_xData[i] - m_xData[i - 1]);

                m_markerXData.append(x);
                m_markerYData.append(triggerLevel);
            }
        }
    }

    // 发送标记点变更信号
    emit markersChanged(m_markerXData, m_markerYData);
}