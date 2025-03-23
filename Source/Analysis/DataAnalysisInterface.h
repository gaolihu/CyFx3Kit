// Source/Analysis/DataAnalysisInterface.h
#pragma once

#include <QObject>
#include <QVariant>
#include <QString>
#include <QMap>
#include <QVector>
#include <functional>
#include <memory>

// 前向声明
struct DataAnalysisItem;
class QByteArray;

/**
 * @brief 分析结果接口
 */
struct AnalysisResult {
    bool success;                      // 分析是否成功
    QMap<QString, QVariant> metrics;   // 计算的指标
    QString description;               // 结果描述
    QString error;                     // 错误信息（如果有）

    // 默认构造函数
    AnalysisResult() : success(false) {}

    // 成功构造函数
    static AnalysisResult createSuccess(const QMap<QString, QVariant>& metrics,
        const QString& description = QString()) {
        AnalysisResult result;
        result.success = true;
        result.metrics = metrics;
        result.description = description;
        return result;
    }

    // 失败构造函数
    static AnalysisResult createFailure(const QString& error) {
        AnalysisResult result;
        result.success = false;
        result.error = error;
        return result;
    }
};

/**
 * @brief 数据分析处理器接口
 */
class IDataAnalyzer {
public:
    virtual ~IDataAnalyzer() = default;

    /**
     * @brief 获取分析器名称
     * @return 分析器名称
     */
    virtual QString getName() const = 0;

    /**
     * @brief 获取分析器描述
     * @return 分析器描述
     */
    virtual QString getDescription() const = 0;

    /**
     * @brief 分析单个数据项
     * @param item 数据项
     * @return 分析结果
     */
    virtual AnalysisResult analyze(const DataAnalysisItem& item) = 0;

    /**
     * @brief 分析多个数据项
     * @param items 数据项列表
     * @return 分析结果
     */
    virtual AnalysisResult analyzeBatch(const std::vector<DataAnalysisItem>& items) = 0;

    /**
     * @brief 分析原始数据
     * @param data 原始二进制数据
     * @return 分析结果
     */
    virtual AnalysisResult analyzeRawData(const QByteArray& data) = 0;

    /**
     * @brief 是否支持批处理
     * @return 是否支持批处理
     */
    virtual bool supportsBatchProcessing() const = 0;

    /**
     * @brief 获取分析器支持的指标
     * @return 支持的指标列表
     */
    virtual QStringList getSupportedMetrics() const = 0;
};

/**
 * @brief 数据分析管理器
 *
 * 集中管理所有分析处理器，提供统一的分析接口
 */
class DataAnalysisManager : public QObject {
    Q_OBJECT

public:
    static DataAnalysisManager& getInstance();

    /**
     * @brief 注册分析器
     * @param name 分析器名称
     * @param analyzer 分析器实例
     */
    void registerAnalyzer(const QString& name, std::shared_ptr<IDataAnalyzer> analyzer);

    /**
     * @brief 获取所有注册的分析器
     * @return 分析器映射
     */
    QMap<QString, std::shared_ptr<IDataAnalyzer>> getAnalyzers() const;

    /**
     * @brief 分析单个数据项
     * @param item 数据项
     * @param analyzerName 分析器名称，为空则使用所有适用的分析器
     * @return 分析结果
     */
    AnalysisResult analyze(const DataAnalysisItem& item,
        const QString& analyzerName = QString());

    /**
     * @brief 分析多个数据项
     * @param items 数据项列表
     * @param analyzerName 分析器名称，为空则使用所有适用的分析器
     * @return 分析结果
     */
    AnalysisResult analyzeBatch(const std::vector<DataAnalysisItem>& items,
        const QString& analyzerName = QString());

    /**
     * @brief 分析原始数据
     * @param data 原始二进制数据
     * @param analyzerName 分析器名称，为空则使用所有适用的分析器
     * @return 分析结果
     */
    AnalysisResult analyzeRawData(const QByteArray& data,
        const QString& analyzerName = QString());

signals:
    /**
     * @brief 分析结果可用信号
     * @param result 分析结果
     * @param analyzerName 分析器名称
     */
    void analysisResultAvailable(const AnalysisResult& result, const QString& analyzerName);

private:
    DataAnalysisManager();
    ~DataAnalysisManager();

    QMap<QString, std::shared_ptr<IDataAnalyzer>> m_analyzers;
};

/**
 * @brief 基本统计分析器
 *
 * 提供标准统计分析功能
 */
class BasicStatisticsAnalyzer : public IDataAnalyzer {
public:
    BasicStatisticsAnalyzer();

    QString getName() const override;
    QString getDescription() const override;

    AnalysisResult analyze(const DataAnalysisItem& item) override;
    AnalysisResult analyzeBatch(const std::vector<DataAnalysisItem>& items) override;
    AnalysisResult analyzeRawData(const QByteArray& data) override;

    bool supportsBatchProcessing() const override;
    QStringList getSupportedMetrics() const override;

private:
    // 计算基本统计量
    QMap<QString, QVariant> calculateStatistics(const QVector<double>& values);
};

/**
 * @brief 趋势分析器
 *
 * 分析数据的趋势和模式
 */
class TrendAnalyzer : public IDataAnalyzer {
public:
    TrendAnalyzer();

    QString getName() const override;
    QString getDescription() const override;

    AnalysisResult analyze(const DataAnalysisItem& item) override;
    AnalysisResult analyzeBatch(const std::vector<DataAnalysisItem>& items) override;
    AnalysisResult analyzeRawData(const QByteArray& data) override;

    bool supportsBatchProcessing() const override;
    QStringList getSupportedMetrics() const override;

private:
    // 计算趋势指标
    QMap<QString, QVariant> calculateTrend(const QVector<double>& values,
        const QVector<QDateTime>& timestamps);

    // 计算移动平均
    QVector<double> calculateMovingAverage(const QVector<double>& values, int window);

    // 计算线性回归
    QPair<double, double> calculateLinearRegression(const QVector<double>& x,
        const QVector<double>& y);
};

/**
 * @brief 异常检测分析器
 *
 * 检测数据中的异常值和模式
 */
class AnomalyDetectionAnalyzer : public IDataAnalyzer {
public:
    AnomalyDetectionAnalyzer();

    QString getName() const override;
    QString getDescription() const override;

    AnalysisResult analyze(const DataAnalysisItem& item) override;
    AnalysisResult analyzeBatch(const std::vector<DataAnalysisItem>& items) override;
    AnalysisResult analyzeRawData(const QByteArray& data) override;

    bool supportsBatchProcessing() const override;
    QStringList getSupportedMetrics() const override;

private:
    // 使用Z-score检测异常值
    QVector<int> detectAnomaliesByZScore(const QVector<double>& values, double threshold = 3.0);

    // 使用移动平均检测异常
    QVector<int> detectAnomaliesByMovingAverage(const QVector<double>& values,
        int window = 5,
        double threshold = 2.0);
};

// 分析器工厂，用于创建各种分析器实例
class AnalyzerFactory {
public:
    static std::shared_ptr<IDataAnalyzer> createAnalyzer(const QString& type);
    static QStringList getSupportedAnalyzerTypes();
};