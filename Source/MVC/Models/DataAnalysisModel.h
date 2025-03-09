// Source/MVC/Models/DataAnalysisModel.h
#pragma once

#include <QObject>
#include <memory>
#include <vector>

/**
 * @brief 数据分析项结构
 */
struct DataAnalysisItem {
    int index;                  ///< 数据索引
    QString timeStamp;          ///< 时间戳
    double value;               ///< 数据值
    QString description;        ///< 描述信息
    QVector<double> dataPoints; ///< 数据点列表
    bool isValid;               ///< 是否有效

    // 默认构造函数
    DataAnalysisItem()
        : index(0), value(0.0), isValid(false) {}

    // 构造函数
    DataAnalysisItem(int idx, const QString& ts, double val, const QString& desc, const QVector<double>& points, bool valid = true)
        : index(idx), timeStamp(ts), value(val), description(desc), dataPoints(points), isValid(valid) {}
};

/**
 * @brief 统计信息结构
 */
struct StatisticsInfo {
    double min;                 ///< 最小值
    double max;                 ///< 最大值
    double average;             ///< 平均值
    double median;              ///< 中位数
    double stdDeviation;        ///< 标准差
    int count;                  ///< 数据点数量

    // 默认构造函数
    StatisticsInfo()
        : min(0.0), max(0.0), average(0.0), median(0.0), stdDeviation(0.0), count(0) {}
};

/**
 * @brief 数据分析模型类
 *
 * 负责存储和管理数据分析的数据
 */
class DataAnalysisModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的共享指针
     */
    static DataAnalysisModel* getInstance();

    /**
     * @brief 析构函数
     */
    ~DataAnalysisModel();

    /**
     * @brief 获取数据项列表
     * @return 数据项列表引用
     */
    const std::vector<DataAnalysisItem>& getDataItems() const;

    /**
     * @brief 获取数据项数量
     * @return 数据项数量
     */
    int getDataItemCount() const;

    /**
     * @brief 获取指定索引的数据项
     * @param index 数据项索引
     * @return 数据项引用
     */
    const DataAnalysisItem& getDataItem(int index) const;

    /**
     * @brief 添加数据项
     * @param item 要添加的数据项
     */
    void addDataItem(const DataAnalysisItem& item);

    /**
     * @brief 更新数据项
     * @param index 要更新的数据项索引
     * @param item 新的数据项
     * @return 是否更新成功
     */
    bool updateDataItem(int index, const DataAnalysisItem& item);

    /**
     * @brief 删除数据项
     * @param index 要删除的数据项索引
     * @return 是否删除成功
     */
    bool removeDataItem(int index);

    /**
     * @brief 清除所有数据项
     */
    void clearDataItems();

    /**
     * @brief 获取统计信息
     * @return 统计信息结构
     */
    StatisticsInfo getStatistics() const;

    /**
     * @brief 计算统计信息
     */
    void calculateStatistics();

    /**
     * @brief 导入数据
     * @param filePath 文件路径
     * @return 是否导入成功
     */
    bool importData(const QString& filePath);

    /**
     * @brief 导出数据
     * @param filePath 文件路径
     * @param selectedIndices 选中的索引列表，若为空则导出所有
     * @return 是否导出成功
     */
    bool exportData(const QString& filePath, const QVector<int>& selectedIndices = QVector<int>());

    /**
     * @brief 设置数据
     * @param data 原始数据
     * @param columns 列数
     * @param rows 行数
     * @return 是否设置成功
     */
    bool setRawData(const QByteArray& data, int columns, int rows);

    /**
     * @brief 根据条件筛选数据
     * @param filterExpression 筛选表达式
     * @return 筛选后的数据项索引列表
     */
    QVector<int> filterData(const QString& filterExpression);

signals:
    /**
     * @brief 数据变更信号
     */
    void dataChanged();

    /**
     * @brief 统计信息变更信号
     * @param stats 新的统计信息
     */
    void statisticsChanged(const StatisticsInfo& stats);

    /**
     * @brief 数据导入完成信号
     * @param success 是否成功
     * @param message 消息
     */
    void importCompleted(bool success, const QString& message);

    /**
     * @brief 数据导出完成信号
     * @param success 是否成功
     * @param message 消息
     */
    void exportCompleted(bool success, const QString& message);

private:
    /**
     * @brief 构造函数（私有，用于单例模式）
     */
    DataAnalysisModel();

    /**
     * @brief 排序数据
     * @param column 排序的列
     * @param ascending 是否升序
     */
    void sortData(int column, bool ascending);

private:
    std::vector<DataAnalysisItem> m_dataItems;            ///< 数据项列表
    StatisticsInfo m_statistics;                          ///< 统计信息
    QByteArray m_rawData;                                 ///< 原始数据
    int m_columns;                                        ///< 列数
    int m_rows;                                           ///< 行数
};