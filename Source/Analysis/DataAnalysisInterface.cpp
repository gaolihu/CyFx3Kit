// Source/Analysis/DataAnalysisInterface.cpp

#include "DataAnalysisInterface.h"
#include "DataAnalysisModel.h"
#include "Logger.h"
#include <QDateTime>
#include <QDataStream>
#include <algorithm>
#include <numeric>
#include <cmath>

DataAnalysisManager& DataAnalysisManager::getInstance()
{
    static DataAnalysisManager instance;
    return instance;
}

DataAnalysisManager::DataAnalysisManager()
{
    // 注册默认分析器
    registerAnalyzer("basic_statistics", std::make_shared<BasicStatisticsAnalyzer>());
    registerAnalyzer("trend_analysis", std::make_shared<TrendAnalyzer>());
    registerAnalyzer("anomaly_detection", std::make_shared<AnomalyDetectionAnalyzer>());

    LOG_INFO("数据分析管理器已初始化");
}

DataAnalysisManager::~DataAnalysisManager()
{
    LOG_INFO("数据分析管理器已销毁");
}

void DataAnalysisManager::registerAnalyzer(const QString& name,
    std::shared_ptr<IDataAnalyzer> analyzer)
{
    if (!analyzer) {
        LOG_ERROR(QString("无法注册空分析器：%1").arg(name));
        return;
    }

    m_analyzers[name] = analyzer;
    LOG_INFO(QString("已注册分析器：%1").arg(name));
}

QMap<QString, std::shared_ptr<IDataAnalyzer>> DataAnalysisManager::getAnalyzers() const
{
    return m_analyzers;
}

AnalysisResult DataAnalysisManager::analyze(const DataAnalysisItem& item,
    const QString& analyzerName)
{
    if (analyzerName.isEmpty()) {
        // 合并所有适用分析器的结果
        QMap<QString, QVariant> combinedMetrics;
        bool anySuccess = false;

        for (auto it = m_analyzers.begin(); it != m_analyzers.end(); ++it) {
            AnalysisResult result = it.value()->analyze(item);

            if (result.success) {
                anySuccess = true;

                // 合并指标，带上分析器前缀避免冲突
                for (auto metric = result.metrics.begin(); metric != result.metrics.end(); ++metric) {
                    QString prefixedKey = QString("%1.%2").arg(it.key()).arg(metric.key());
                    combinedMetrics[prefixedKey] = metric.value();
                }
            }

            // 发送分析结果信号
            emit analysisResultAvailable(result, it.key());
        }

        if (anySuccess) {
            return AnalysisResult::createSuccess(combinedMetrics);
        }
        else {
            return AnalysisResult::createFailure("所有分析器都失败了");
        }
    }
    else {
        // 使用指定的分析器
        if (!m_analyzers.contains(analyzerName)) {
            return AnalysisResult::createFailure(QString("分析器未找到：%1").arg(analyzerName));
        }

        AnalysisResult result = m_analyzers[analyzerName]->analyze(item);

        // 发送分析结果信号
        emit analysisResultAvailable(result, analyzerName);

        return result;
    }
}

AnalysisResult DataAnalysisManager::analyzeBatch(const std::vector<DataAnalysisItem>& items,
    const QString& analyzerName)
{
    if (items.empty()) {
        return AnalysisResult::createFailure("没有可分析的数据项");
    }

    if (analyzerName.isEmpty()) {
        // 合并所有适用分析器的结果
        QMap<QString, QVariant> combinedMetrics;
        bool anySuccess = false;

        for (auto it = m_analyzers.begin(); it != m_analyzers.end(); ++it) {
            if (it.value()->supportsBatchProcessing()) {
                AnalysisResult result = it.value()->analyzeBatch(items);

                if (result.success) {
                    anySuccess = true;

                    // 合并指标，带上分析器前缀避免冲突
                    for (auto metric = result.metrics.begin(); metric != result.metrics.end(); ++metric) {
                        QString prefixedKey = QString("%1.%2").arg(it.key()).arg(metric.key());
                        combinedMetrics[prefixedKey] = metric.value();
                    }
                }

                // 发送分析结果信号
                emit analysisResultAvailable(result, it.key());
            }
        }

        if (anySuccess) {
            return AnalysisResult::createSuccess(combinedMetrics);
        }
        else {
            return AnalysisResult::createFailure("所有分析器都失败了");
        }
    }
    else {
        // 使用指定的分析器
        if (!m_analyzers.contains(analyzerName)) {
            return AnalysisResult::createFailure(QString("分析器未找到：%1").arg(analyzerName));
        }

        if (!m_analyzers[analyzerName]->supportsBatchProcessing()) {
            return AnalysisResult::createFailure(QString("分析器不支持批处理：%1").arg(analyzerName));
        }

        AnalysisResult result = m_analyzers[analyzerName]->analyzeBatch(items);

        // 发送分析结果信号
        emit analysisResultAvailable(result, analyzerName);

        return result;
    }
}

AnalysisResult DataAnalysisManager::analyzeRawData(const QByteArray& data,
    const QString& analyzerName)
{
    if (data.isEmpty()) {
        return AnalysisResult::createFailure("没有可分析的数据");
    }

    if (analyzerName.isEmpty()) {
        // 合并所有适用分析器的结果
        QMap<QString, QVariant> combinedMetrics;
        bool anySuccess = false;

        for (auto it = m_analyzers.begin(); it != m_analyzers.end(); ++it) {
            AnalysisResult result = it.value()->analyzeRawData(data);

            if (result.success) {
                anySuccess = true;

                // 合并指标，带上分析器前缀避免冲突
                for (auto metric = result.metrics.begin(); metric != result.metrics.end(); ++metric) {
                    QString prefixedKey = QString("%1.%2").arg(it.key()).arg(metric.key());
                    combinedMetrics[prefixedKey] = metric.value();
                }
            }

            // 发送分析结果信号
            emit analysisResultAvailable(result, it.key());
        }

        if (anySuccess) {
            return AnalysisResult::createSuccess(combinedMetrics);
        }
        else {
            return AnalysisResult::createFailure("所有分析器都失败了");
        }
    }
    else {
        // 使用指定的分析器
        if (!m_analyzers.contains(analyzerName)) {
            return AnalysisResult::createFailure(QString("分析器未找到：%1").arg(analyzerName));
        }

        AnalysisResult result = m_analyzers[analyzerName]->analyzeRawData(data);

        // 发送分析结果信号
        emit analysisResultAvailable(result, analyzerName);

        return result;
    }
}

// BasicStatisticsAnalyzer实现
BasicStatisticsAnalyzer::BasicStatisticsAnalyzer()
{
}

QString BasicStatisticsAnalyzer::getName() const
{
    return "基本统计分析";
}

QString BasicStatisticsAnalyzer::getDescription() const
{
    return "计算数据的基本统计指标，包括平均值、中位数、标准差、最大值、最小值等";
}

AnalysisResult BasicStatisticsAnalyzer::analyze(const DataAnalysisItem& item)
{
    // 收集所有数值
    QVector<double> values;
    values.append(item.value);

    for (const auto& point : item.dataPoints) {
        values.append(point);
    }

    if (values.isEmpty()) {
        return AnalysisResult::createFailure("没有可分析的数值");
    }

    // 计算统计指标
    QMap<QString, QVariant> metrics = calculateStatistics(values);

    return AnalysisResult::createSuccess(metrics,
        QString("分析了 %1 个数据点").arg(values.size()));
}

AnalysisResult BasicStatisticsAnalyzer::analyzeBatch(const std::vector<DataAnalysisItem>& items)
{
    if (items.empty()) {
        return AnalysisResult::createFailure("没有可分析的数据项");
    }

    // 收集所有数值
    QVector<double> allValues;
    QVector<double> mainValues; // 仅主值

    for (const auto& item : items) {
        mainValues.append(item.value);
        allValues.append(item.value);

        for (const auto& point : item.dataPoints) {
            allValues.append(point);
        }
    }

    if (allValues.isEmpty()) {
        return AnalysisResult::createFailure("没有可分析的数值");
    }

    // 计算所有数据点的统计指标
    QMap<QString, QVariant> allMetrics = calculateStatistics(allValues);

    // 计算仅主值的统计指标
    QMap<QString, QVariant> mainMetrics = calculateStatistics(mainValues);

    // 合并结果，为主值指标添加前缀
    QMap<QString, QVariant> combinedMetrics;

    for (auto it = allMetrics.begin(); it != allMetrics.end(); ++it) {
        combinedMetrics[QString("all.%1").arg(it.key())] = it.value();
    }

    for (auto it = mainMetrics.begin(); it != mainMetrics.end(); ++it) {
        combinedMetrics[QString("main.%1").arg(it.key())] = it.value();
    }

    // 添加项目数量
    combinedMetrics["item_count"] = static_cast<int>(items.size());
    combinedMetrics["data_point_count"] = allValues.size();

    return AnalysisResult::createSuccess(combinedMetrics,
        QString("分析了 %1 个数据项，共 %2 个数据点")
        .arg(items.size())
        .arg(allValues.size()));
}

AnalysisResult BasicStatisticsAnalyzer::analyzeRawData(const QByteArray& data)
{
    if (data.isEmpty()) {
        return AnalysisResult::createFailure("没有可分析的数据");
    }

    // 尝试将二进制数据解析为双精度值序列
    QVector<double> values;
    QDataStream stream(data);

    while (!stream.atEnd()) {
        double value;
        stream >> value;

        if (stream.status() == QDataStream::Ok) {
            values.append(value);
        }
        else {
            // 如果读取失败，尝试按字节解析
            break;
        }
    }

    if (values.isEmpty()) {
        // 如果无法解析为双精度值，尝试按字节处理
        const char* rawData = data.constData();
        for (int i = 0; i < data.size(); ++i) {
            values.append(static_cast<double>(static_cast<unsigned char>(rawData[i])));
        }
    }

    if (values.isEmpty()) {
        return AnalysisResult::createFailure("无法从原始数据提取数值");
    }

    // 计算统计指标
    QMap<QString, QVariant> metrics = calculateStatistics(values);

    return AnalysisResult::createSuccess(metrics,
        QString("分析了 %1 个数据点").arg(values.size()));
}

bool BasicStatisticsAnalyzer::supportsBatchProcessing() const
{
    return true;
}

QStringList BasicStatisticsAnalyzer::getSupportedMetrics() const
{
    return QStringList() << "min" << "max" << "mean" << "median" << "std_dev"
        << "variance" << "range" << "sum" << "count";
}

QMap<QString, QVariant> BasicStatisticsAnalyzer::calculateStatistics(const QVector<double>& values)
{
    QMap<QString, QVariant> metrics;

    if (values.isEmpty()) {
        return metrics;
    }

    // 数据点数量
    metrics["count"] = values.size();

    // 最小值和最大值
    auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    metrics["min"] = *minIt;
    metrics["max"] = *maxIt;
    metrics["range"] = *maxIt - *minIt;

    // 求和
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    metrics["sum"] = sum;

    // 平均值
    double mean = sum / values.size();
    metrics["mean"] = mean;

    // 中位数（需要排序）
    QVector<double> sortedValues = values;
    std::sort(sortedValues.begin(), sortedValues.end());

    if (sortedValues.size() % 2 == 0) {
        // 偶数个数据点，取中间两个数的平均值
        int mid = sortedValues.size() / 2;
        metrics["median"] = (sortedValues[mid - 1] + sortedValues[mid]) / 2.0;
    }
    else {
        // 奇数个数据点，取中间的数
        metrics["median"] = sortedValues[sortedValues.size() / 2];
    }

    // 方差和标准差
    double variance = 0.0;
    for (const auto& value : values) {
        variance += std::pow(value - mean, 2);
    }
    variance /= values.size();
    metrics["variance"] = variance;
    metrics["std_dev"] = std::sqrt(variance);

    return metrics;
}

// TrendAnalyzer实现
TrendAnalyzer::TrendAnalyzer()
{
}

QString TrendAnalyzer::getName() const
{
    return "趋势分析";
}

QString TrendAnalyzer::getDescription() const
{
    return "分析数据的趋势和模式，包括线性回归、移动平均、斜率等";
}

AnalysisResult TrendAnalyzer::analyze(const DataAnalysisItem& item)
{
    // 单个项目无法进行趋势分析
    return AnalysisResult::createFailure("趋势分析需要多个数据项");
}

AnalysisResult TrendAnalyzer::analyzeBatch(const std::vector<DataAnalysisItem>& items)
{
    if (items.size() < 2) {
        return AnalysisResult::createFailure("趋势分析需要至少两个数据项");
    }

    // 提取数值和时间戳
    QVector<double> values;
    QVector<QDateTime> timestamps;

    for (const auto& item : items) {
        values.append(item.value);
        timestamps.append(QDateTime::fromString(item.timeStamp, Qt::ISODate));
    }

    if (values.size() < 2) {
        return AnalysisResult::createFailure("没有足够的数值进行趋势分析");
    }

    // 计算趋势指标
    QMap<QString, QVariant> metrics = calculateTrend(values, timestamps);

    return AnalysisResult::createSuccess(metrics,
        QString("分析了 %1 个数据点的趋势").arg(values.size()));
}

AnalysisResult TrendAnalyzer::analyzeRawData(const QByteArray& data)
{
    // 原始数据没有时间信息，无法进行趋势分析
    QVector<double> values;
    QDataStream stream(data);

    while (!stream.atEnd()) {
        double value;
        stream >> value;

        if (stream.status() == QDataStream::Ok) {
            values.append(value);
        }
        else {
            break;
        }
    }

    if (values.size() < 2) {
        return AnalysisResult::createFailure("没有足够的数值进行趋势分析");
    }

    // 创建假时间戳（索引作为时间）
    QVector<QDateTime> fakeTimestamps;
    QDateTime baseTime = QDateTime::currentDateTime();

    for (int i = 0; i < values.size(); ++i) {
        fakeTimestamps.append(baseTime.addSecs(i));
    }

    // 计算趋势指标
    QMap<QString, QVariant> metrics = calculateTrend(values, fakeTimestamps);

    return AnalysisResult::createSuccess(metrics,
        QString("分析了 %1 个数据点的趋势（使用索引作为时间）")
        .arg(values.size()));
}

bool TrendAnalyzer::supportsBatchProcessing() const
{
    return true;
}

QStringList TrendAnalyzer::getSupportedMetrics() const
{
    return QStringList() << "slope" << "intercept" << "r_squared"
        << "trend_direction" << "moving_average"
        << "trend_strength";
}

QMap<QString, QVariant> TrendAnalyzer::calculateTrend(const QVector<double>& values,
    const QVector<QDateTime>& timestamps)
{
    QMap<QString, QVariant> metrics;

    if (values.size() != timestamps.size() || values.isEmpty()) {
        return metrics;
    }

    // 计算时间序列索引（以秒为单位）
    QVector<double> timeIndices;
    qint64 baseTime = timestamps.first().toSecsSinceEpoch();

    for (const auto& time : timestamps) {
        timeIndices.append(time.toSecsSinceEpoch() - baseTime);
    }

    // 计算线性回归
    auto [slope, intercept] = calculateLinearRegression(timeIndices, values);
    metrics["slope"] = slope;
    metrics["intercept"] = intercept;

    // 计算R平方（拟合优度）
    double meanY = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double totalSumSquares = 0.0;
    double residualSumSquares = 0.0;

    for (int i = 0; i < values.size(); ++i) {
        double predicted = slope * timeIndices[i] + intercept;
        residualSumSquares += std::pow(values[i] - predicted, 2);
        totalSumSquares += std::pow(values[i] - meanY, 2);
    }

    double rSquared = 1.0 - (residualSumSquares / totalSumSquares);
    metrics["r_squared"] = rSquared;

    // 趋势方向
    QString trendDirection = "flat";
    if (slope > 0.0001) {
        trendDirection = "upward";
    }
    else if (slope < -0.0001) {
        trendDirection = "downward";
    }
    metrics["trend_direction"] = trendDirection;

    // 趋势强度（基于R平方和斜率）
    double trendStrength = std::abs(slope) * rSquared;
    metrics["trend_strength"] = trendStrength;

    // 计算移动平均
    QVector<double> movingAvg = calculateMovingAverage(values,
        std::min<int>(5, static_cast<int>(values.size())));

    // 转换为QVariantList
    QVariantList movingAvgList;
    for (double val : movingAvg) {
        movingAvgList.append(val);
    }
    metrics["moving_average"] = movingAvgList;

    return metrics;
}

QVector<double> TrendAnalyzer::calculateMovingAverage(const QVector<double>& values, int window)
{
    QVector<double> result;

    if (values.isEmpty() || window <= 0) {
        return result;
    }

    // 确保窗口大小不超过数据长度
    window = std::min(window, static_cast<int>(values.size()));

    // 计算移动平均
    for (int i = 0; i < values.size(); ++i) {
        int start = std::max(0, i - window / 2);
        int end = std::min<int>(static_cast<int>(values.size()) - 1, i + window / 2);

        double sum = 0.0;
        for (int j = start; j <= end; ++j) {
            sum += values[j];
        }

        result.append(sum / (end - start + 1));
    }

    return result;
}

QPair<double, double> TrendAnalyzer::calculateLinearRegression(const QVector<double>& x,
    const QVector<double>& y)
{
    if (x.size() != y.size() || x.isEmpty()) {
        return qMakePair(0.0, 0.0);
    }

    double sumX = 0.0;
    double sumY = 0.0;
    double sumXY = 0.0;
    double sumX2 = 0.0;
    int n = x.size();

    for (int i = 0; i < n; ++i) {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumX2 += x[i] * x[i];
    }

    // 计算斜率和截距
    double xMean = sumX / n;
    double yMean = sumY / n;

    double numerator = sumXY - (sumX * sumY) / n;
    double denominator = sumX2 - (sumX * sumX) / n;

    if (std::abs(denominator) < 1e-10) {
        // 避免除以零
        return qMakePair(0.0, yMean);
    }

    double slope = numerator / denominator;
    double intercept = yMean - slope * xMean;

    return qMakePair(slope, intercept);
}

// AnomalyDetectionAnalyzer实现
AnomalyDetectionAnalyzer::AnomalyDetectionAnalyzer()
{
}

QString AnomalyDetectionAnalyzer::getName() const
{
    return "异常检测";
}

QString AnomalyDetectionAnalyzer::getDescription() const
{
    return "检测数据中的异常值和异常模式";
}

AnalysisResult AnomalyDetectionAnalyzer::analyze(const DataAnalysisItem& item)
{
    // 单个数据项无法进行异常检测
    return AnalysisResult::createFailure("异常检测需要多个数据项");
}

AnalysisResult AnomalyDetectionAnalyzer::analyzeBatch(const std::vector<DataAnalysisItem>& items)
{
    if (items.size() < 4) {
        return AnalysisResult::createFailure("异常检测需要至少4个数据项");
    }

    // 提取数值
    QVector<double> values;

    for (const auto& item : items) {
        values.append(item.value);
    }

    // 进行异常检测
    QVector<int> zscoreAnomalies = detectAnomaliesByZScore(values);
    QVector<int> maAnomalies = detectAnomaliesByMovingAverage(values);

    // 合并结果
    QSet<int> allAnomalies;
    for (int idx : zscoreAnomalies) {
        allAnomalies.insert(idx);
    }

    for (int idx : maAnomalies) {
        allAnomalies.insert(idx);
    }

    // 构建结果指标
    QMap<QString, QVariant> metrics;

    // 异常索引列表
    QVariantList anomalyIndices;
    for (int idx : allAnomalies) {
        anomalyIndices.append(idx);
    }
    metrics["anomaly_indices"] = anomalyIndices;

    // 异常值列表
    QVariantList anomalyValues;
    for (int idx : allAnomalies) {
        if (idx >= 0 && idx < values.size()) {
            anomalyValues.append(values[idx]);
        }
    }
    metrics["anomaly_values"] = anomalyValues;

    // 异常比例
    metrics["anomaly_count"] = allAnomalies.size();
    metrics["anomaly_percentage"] = (static_cast<double>(allAnomalies.size()) / values.size()) * 100.0;

    // Z-score检测到的异常
    metrics["zscore_anomaly_count"] = zscoreAnomalies.size();

    // 移动平均检测到的异常
    metrics["ma_anomaly_count"] = maAnomalies.size();

    return AnalysisResult::createSuccess(metrics,
        QString("在 %1 个数据点中检测到 %2 个异常")
        .arg(values.size())
        .arg(allAnomalies.size()));
}

AnalysisResult AnomalyDetectionAnalyzer::analyzeRawData(const QByteArray& data)
{
    // 解析原始数据为数值
    QVector<double> values;
    QDataStream stream(data);

    while (!stream.atEnd()) {
        double value;
        stream >> value;

        if (stream.status() == QDataStream::Ok) {
            values.append(value);
        }
        else {
            break;
        }
    }

    if (values.size() < 4) {
        return AnalysisResult::createFailure("异常检测需要至少4个数据点");
    }

    // 进行异常检测
    QVector<int> zscoreAnomalies = detectAnomaliesByZScore(values);
    QVector<int> maAnomalies = detectAnomaliesByMovingAverage(values);

    // 合并结果
    QSet<int> allAnomalies;
    for (int idx : zscoreAnomalies) {
        allAnomalies.insert(idx);
    }

    for (int idx : maAnomalies) {
        allAnomalies.insert(idx);
    }

    // 构建结果指标
    QMap<QString, QVariant> metrics;

    // 异常索引列表
    QVariantList anomalyIndices;
    for (int idx : allAnomalies) {
        anomalyIndices.append(idx);
    }
    metrics["anomaly_indices"] = anomalyIndices;

    // 异常值列表
    QVariantList anomalyValues;
    for (int idx : allAnomalies) {
        if (idx >= 0 && idx < values.size()) {
            anomalyValues.append(values[idx]);
        }
    }
    metrics["anomaly_values"] = anomalyValues;

    // 异常比例
    metrics["anomaly_count"] = allAnomalies.size();
    metrics["anomaly_percentage"] = (static_cast<double>(allAnomalies.size()) / values.size()) * 100.0;

    // Z-score检测到的异常
    metrics["zscore_anomaly_count"] = zscoreAnomalies.size();

    // 移动平均检测到的异常
    metrics["ma_anomaly_count"] = maAnomalies.size();

    return AnalysisResult::createSuccess(metrics,
        QString("在 %1 个数据点中检测到 %2 个异常")
        .arg(values.size())
        .arg(allAnomalies.size()));
}

bool AnomalyDetectionAnalyzer::supportsBatchProcessing() const
{
    return true;
}

QStringList AnomalyDetectionAnalyzer::getSupportedMetrics() const
{
    return QStringList() << "anomaly_indices" << "anomaly_values"
        << "anomaly_count" << "anomaly_percentage"
        << "zscore_anomaly_count" << "ma_anomaly_count";
}

QVector<int> AnomalyDetectionAnalyzer::detectAnomaliesByZScore(const QVector<double>& values,
    double threshold)
{
    QVector<int> anomalies;

    if (values.size() < 2) {
        return anomalies;
    }

    // 计算均值和标准差
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();

    double variance = 0.0;
    for (const auto& value : values) {
        variance += std::pow(value - mean, 2);
    }
    variance /= values.size();

    double stdDev = std::sqrt(variance);

    // 如果标准差接近于0，无法使用Z-score
    if (stdDev < 1e-10) {
        return anomalies;
    }

    // 检测异常值
    for (int i = 0; i < values.size(); ++i) {
        double zScore = std::abs(values[i] - mean) / stdDev;

        if (zScore > threshold) {
            anomalies.append(i);
        }
    }

    return anomalies;
}

QVector<int> AnomalyDetectionAnalyzer::detectAnomaliesByMovingAverage(const QVector<double>& values,
    int window,
    double threshold)
{
    QVector<int> anomalies;

    if (values.size() < window) {
        return anomalies;
    }

    // 计算移动平均
    QVector<double> movingAvg;
    QVector<double> movingStdDev;

    for (int i = 0; i < values.size(); ++i) {
        int start = std::max(0, i - window / 2);
        int end = std::min(static_cast<int>(values.size()) - 1, i + window / 2);

        // 窗口内的值
        QVector<double> windowValues;
        for (int j = start; j <= end; ++j) {
            if (j != i) { // 排除当前点
                windowValues.append(values[j]);
            }
        }

        if (windowValues.isEmpty()) {
            movingAvg.append(values[i]);
            movingStdDev.append(0);
            continue;
        }

        // 计算窗口均值
        double windowMean = std::accumulate(windowValues.begin(), windowValues.end(), 0.0)
            / windowValues.size();
        movingAvg.append(windowMean);

        // 计算窗口标准差
        double windowVariance = 0.0;
        for (const auto& val : windowValues) {
            windowVariance += std::pow(val - windowMean, 2);
        }
        windowVariance /= windowValues.size();
        movingStdDev.append(std::sqrt(windowVariance));
    }

    // 检测异常值
    for (int i = 0; i < values.size(); ++i) {
        // 如果标准差接近于0，使用绝对差异
        if (movingStdDev[i] < 1e-10) {
            double absDiff = std::abs(values[i] - movingAvg[i]);
            if (absDiff > threshold) {
                anomalies.append(i);
            }
        }
        else {
            // 使用Z-score
            double zScore = std::abs(values[i] - movingAvg[i]) / movingStdDev[i];
            if (zScore > threshold) {
                anomalies.append(i);
            }
        }
    }

    return anomalies;
}

// AnalyzerFactory实现
std::shared_ptr<IDataAnalyzer> AnalyzerFactory::createAnalyzer(const QString& type)
{
    if (type == "basic_statistics") {
        return std::make_shared<BasicStatisticsAnalyzer>();
    }
    else if (type == "trend_analysis") {
        return std::make_shared<TrendAnalyzer>();
    }
    else if (type == "anomaly_detection") {
        return std::make_shared<AnomalyDetectionAnalyzer>();
    }

    return nullptr;
}

QStringList AnalyzerFactory::getSupportedAnalyzerTypes()
{
    return QStringList() << "basic_statistics" << "trend_analysis" << "anomaly_detection";
}
