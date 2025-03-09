// Source/MVC/Models/DataAnalysisModel.cpp
#include "DataAnalysisModel.h"
#include "Logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <algorithm>
#include <numeric>
#include <cmath>

DataAnalysisModel* DataAnalysisModel::getInstance()
{
    static  DataAnalysisModel s;
    return &s;
}

DataAnalysisModel::DataAnalysisModel()
    : QObject(nullptr)
    , m_columns(0)
    , m_rows(0)
{
    LOG_INFO("数据分析模型已创建");
}

DataAnalysisModel::~DataAnalysisModel()
{
    LOG_INFO("数据分析模型已销毁");
}

const std::vector<DataAnalysisItem>& DataAnalysisModel::getDataItems() const
{
    return m_dataItems;
}

int DataAnalysisModel::getDataItemCount() const
{
    return static_cast<int>(m_dataItems.size());
}

const DataAnalysisItem& DataAnalysisModel::getDataItem(int index) const
{
    static DataAnalysisItem emptyItem;

    if (index >= 0 && index < static_cast<int>(m_dataItems.size())) {
        return m_dataItems[index];
    }

    return emptyItem;
}

void DataAnalysisModel::addDataItem(const DataAnalysisItem& item)
{
    m_dataItems.push_back(item);
    calculateStatistics();
    emit dataChanged();
}

bool DataAnalysisModel::updateDataItem(int index, const DataAnalysisItem& item)
{
    if (index >= 0 && index < static_cast<int>(m_dataItems.size())) {
        m_dataItems[index] = item;
        calculateStatistics();
        emit dataChanged();
        return true;
    }

    return false;
}

bool DataAnalysisModel::removeDataItem(int index)
{
    if (index >= 0 && index < static_cast<int>(m_dataItems.size())) {
        m_dataItems.erase(m_dataItems.begin() + index);
        calculateStatistics();
        emit dataChanged();
        return true;
    }

    return false;
}

void DataAnalysisModel::clearDataItems()
{
    m_dataItems.clear();
    m_statistics = StatisticsInfo();
    emit dataChanged();
    emit statisticsChanged(m_statistics);
}

StatisticsInfo DataAnalysisModel::getStatistics() const
{
    return m_statistics;
}

void DataAnalysisModel::calculateStatistics()
{
    if (m_dataItems.empty()) {
        m_statistics = StatisticsInfo();
        emit statisticsChanged(m_statistics);
        return;
    }

    // 用于统计的有效数据点
    std::vector<double> validValues;

    // 收集所有有效的数据点
    for (const auto& item : m_dataItems) {
        if (item.isValid) {
            validValues.push_back(item.value);

            // 添加数据点列表中的所有值（如果有的话）
            for (const auto& point : item.dataPoints) {
                validValues.push_back(point);
            }
        }
    }

    // 如果没有有效数据，返回空统计
    if (validValues.empty()) {
        m_statistics = StatisticsInfo();
        emit statisticsChanged(m_statistics);
        return;
    }

    // 计算最小值和最大值
    auto [minIt, maxIt] = std::minmax_element(validValues.begin(), validValues.end());
    m_statistics.min = *minIt;
    m_statistics.max = *maxIt;

    // 计算平均值
    double sum = std::accumulate(validValues.begin(), validValues.end(), 0.0);
    m_statistics.count = static_cast<int>(validValues.size());
    m_statistics.average = sum / m_statistics.count;

    // 计算中位数（需要排序后计算）
    std::vector<double> sortedValues = validValues;
    std::sort(sortedValues.begin(), sortedValues.end());

    if (sortedValues.size() % 2 == 0) {
        // 偶数个数据点，取中间两个数的平均值
        size_t mid = sortedValues.size() / 2;
        m_statistics.median = (sortedValues[mid - 1] + sortedValues[mid]) / 2.0;
    }
    else {
        // 奇数个数据点，取中间的数
        m_statistics.median = sortedValues[sortedValues.size() / 2];
    }

    // 计算标准差
    double variance = 0.0;
    for (const auto& value : validValues) {
        variance += std::pow(value - m_statistics.average, 2);
    }
    variance /= m_statistics.count;
    m_statistics.stdDeviation = std::sqrt(variance);

    emit statisticsChanged(m_statistics);
}

bool DataAnalysisModel::importData(const QString& filePath)
{
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            LOG_ERROR(QString("无法打开文件 %1: %2").arg(filePath).arg(file.errorString()));
            emit importCompleted(false, QString("无法打开文件: %1").arg(file.errorString()));
            return false;
        }

        // 清除现有数据
        clearDataItems();

        QString fileExtension = QFileInfo(filePath).suffix().toLower();

        if (fileExtension == "csv") {
            QTextStream in(&file);
            int index = 0;

            // 读取第一行作为标题（如果有）
            QString headerLine = in.readLine();
            QStringList headers = headerLine.split(',');

            // 处理数据行
            while (!in.atEnd()) {
                QString line = in.readLine();
                QStringList fields = line.split(',');

                if (fields.size() < 2) {
                    continue; // 跳过无效行
                }

                QString timestamp = fields.size() > 0 ? fields[0] : QString();
                double value = fields.size() > 1 ? fields[1].toDouble() : 0.0;
                QString description = fields.size() > 2 ? fields[2] : QString();

                // 后续列作为数据点
                QVector<double> dataPoints;
                for (int i = 3; i < fields.size(); ++i) {
                    bool ok;
                    double point = fields[i].toDouble(&ok);
                    if (ok) {
                        dataPoints.append(point);
                    }
                }

                DataAnalysisItem item(index++, timestamp, value, description, dataPoints);
                addDataItem(item);
            }
        }
        else if (fileExtension == "json") {
            QByteArray jsonData = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);

            if (doc.isArray()) {
                QJsonArray array = doc.array();
                int index = 0;

                for (const QJsonValue& value : array) {
                    if (value.isObject()) {
                        QJsonObject obj = value.toObject();

                        QString timestamp = obj["timestamp"].toString();
                        double val = obj["value"].toDouble();
                        QString description = obj["description"].toString();

                        QVector<double> dataPoints;
                        if (obj.contains("dataPoints") && obj["dataPoints"].isArray()) {
                            QJsonArray pointsArray = obj["dataPoints"].toArray();
                            for (const QJsonValue& point : pointsArray) {
                                if (point.isDouble()) {
                                    dataPoints.append(point.toDouble());
                                }
                            }
                        }

                        DataAnalysisItem item(index++, timestamp, val, description, dataPoints);
                        addDataItem(item);
                    }
                }
            }
        }
        else {
            // 二进制文件或其他格式，直接读取为二进制数据
            m_rawData = file.readAll();
            // 在这里可以添加处理二进制数据的逻辑
            file.close();

            emit importCompleted(true, QString("文件 %1 导入成功，二进制数据大小: %2 字节")
                .arg(filePath).arg(m_rawData.size()));
            return true;
        }

        file.close();
        calculateStatistics();

        LOG_INFO(QString("从文件 %1 导入了 %2 条数据").arg(filePath).arg(m_dataItems.size()));
        emit importCompleted(true, QString("成功导入 %1 条数据").arg(m_dataItems.size()));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("导入数据时发生异常: %1").arg(e.what()));
        emit importCompleted(false, QString("导入数据时发生异常: %1").arg(e.what()));
        return false;
    }
}

bool DataAnalysisModel::exportData(const QString& filePath, const QVector<int>& selectedIndices)
{
    try {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            LOG_ERROR(QString("无法打开文件 %1: %2").arg(filePath).arg(file.errorString()));
            emit exportCompleted(false, QString("无法打开文件: %1").arg(file.errorString()));
            return false;
        }

        QString fileExtension = QFileInfo(filePath).suffix().toLower();

        if (fileExtension == "csv") {
            QTextStream out(&file);

            // 写入头部
            out << "Index,Timestamp,Value,Description,DataPoints...\n";

            // 写入数据
            const std::vector<DataAnalysisItem>& items = getDataItems();

            if (selectedIndices.isEmpty()) {
                // 导出所有数据
                for (const auto& item : items) {
                    if (!item.isValid) continue; // 跳过无效项

                    out << item.index << "," << item.timeStamp << "," << item.value << ","
                        << item.description;

                    // 写入所有数据点
                    for (const auto& point : item.dataPoints) {
                        out << "," << point;
                    }

                    out << "\n";
                }
            }
            else {
                // 导出选定的索引
                for (int index : selectedIndices) {
                    if (index >= 0 && index < static_cast<int>(items.size())) {
                        const DataAnalysisItem& item = items[index];
                        if (!item.isValid) continue; // 跳过无效项

                        out << item.index << "," << item.timeStamp << "," << item.value << ","
                            << item.description;

                        // 写入所有数据点
                        for (const auto& point : item.dataPoints) {
                            out << "," << point;
                        }

                        out << "\n";
                    }
                }
            }
        }
        else if (fileExtension == "json") {
            QJsonArray array;
            const std::vector<DataAnalysisItem>& items = getDataItems();

            if (selectedIndices.isEmpty()) {
                // 导出所有数据
                for (const auto& item : items) {
                    if (!item.isValid) continue; // 跳过无效项

                    QJsonObject obj;
                    obj["index"] = item.index;
                    obj["timestamp"] = item.timeStamp;
                    obj["value"] = item.value;
                    obj["description"] = item.description;

                    QJsonArray pointsArray;
                    for (const auto& point : item.dataPoints) {
                        pointsArray.append(point);
                    }
                    obj["dataPoints"] = pointsArray;

                    array.append(obj);
                }
            }
            else {
                // 导出选定的索引
                for (int index : selectedIndices) {
                    if (index >= 0 && index < static_cast<int>(items.size())) {
                        const DataAnalysisItem& item = items[index];
                        if (!item.isValid) continue; // 跳过无效项

                        QJsonObject obj;
                        obj["index"] = item.index;
                        obj["timestamp"] = item.timeStamp;
                        obj["value"] = item.value;
                        obj["description"] = item.description;

                        QJsonArray pointsArray;
                        for (const auto& point : item.dataPoints) {
                            pointsArray.append(point);
                        }
                        obj["dataPoints"] = pointsArray;

                        array.append(obj);
                    }
                }
            }

            QJsonDocument doc(array);
            file.write(doc.toJson());
        }
        else {
            // 二进制或其他格式，直接写入原始数据
            if (!m_rawData.isEmpty()) {
                file.write(m_rawData);
            }
            else {
                // 没有原始数据，尝试将对象序列化为二进制
                QByteArray binaryData;
                QDataStream stream(&binaryData, QIODevice::WriteOnly);

                // 写入项目数量
                const std::vector<DataAnalysisItem>& items = getDataItems();
                stream << static_cast<qint32>(items.size());

                // 写入每个项目
                for (const auto& item : items) {
                    if (!item.isValid) continue; // 跳过无效项

                    stream << item.index << item.timeStamp << item.value << item.description;

                    // 写入数据点数量和数据点
                    stream << static_cast<qint32>(item.dataPoints.size());
                    for (const auto& point : item.dataPoints) {
                        stream << point;
                    }
                }

                file.write(binaryData);
            }
        }

        file.close();
        LOG_INFO(QString("已将数据导出到文件 %1").arg(filePath));
        emit exportCompleted(true, QString("数据已成功导出到 %1").arg(filePath));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("导出数据时发生异常: %1").arg(e.what()));
        emit exportCompleted(false, QString("导出数据时发生异常: %1").arg(e.what()));
        return false;
    }
}

bool DataAnalysisModel::setRawData(const QByteArray& data, int columns, int rows)
{
    m_rawData = data;
    m_columns = columns;
    m_rows = rows;

    if (data.isEmpty()) {
        return false;
    }

    // 清除现有数据
    clearDataItems();

    // 解析原始数据填充数据项的示例逻辑
    // 这里仅为示例，需要根据实际数据格式调整
    try {
        // 示例：假设原始数据是二进制格式，每列占4字节浮点数
        if (columns > 0 && rows > 0) {
            int bytesPerRow = columns * sizeof(float);
            int index = 0;

            for (int row = 0; row < rows; ++row) {
                if ((row + 1) * bytesPerRow <= data.size()) {
                    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

                    // 取第一列作为主值
                    float mainValue = 0.0f;
                    memcpy(&mainValue, data.constData() + row * bytesPerRow, sizeof(float));

                    // 描述
                    QString description = QString("Row %1").arg(row);

                    // 其余列作为数据点
                    QVector<double> dataPoints;
                    for (int col = 1; col < columns; ++col) {
                        float value = 0.0f;
                        memcpy(&value, data.constData() + row * bytesPerRow + col * sizeof(float), sizeof(float));
                        dataPoints.append(static_cast<double>(value));
                    }

                    DataAnalysisItem item(index++, timestamp, mainValue, description, dataPoints);
                    addDataItem(item);
                }
            }

            calculateStatistics();
            return true;
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("解析原始数据时发生异常: %1").arg(e.what()));
        return false;
    }

    return false;
}

QVector<int> DataAnalysisModel::filterData(const QString& filterExpression)
{
    QVector<int> result;

    if (filterExpression.isEmpty()) {
        // 如果过滤表达式为空，返回所有有效项的索引
        for (size_t i = 0; i < m_dataItems.size(); ++i) {
            if (m_dataItems[i].isValid) {
                result.append(static_cast<int>(i));
            }
        }
        return result;
    }

    // 简单的过滤逻辑示例
    // 实际项目中可能需要更复杂的表达式解析和计算
    for (size_t i = 0; i < m_dataItems.size(); ++i) {
        const DataAnalysisItem& item = m_dataItems[i];

        if (!item.isValid) {
            continue;
        }

        // 检查描述中是否包含过滤表达式
        if (item.description.contains(filterExpression, Qt::CaseInsensitive)) {
            result.append(static_cast<int>(i));
            continue;
        }

        // 检查值是否匹配表达式
        // 这里只是示例，实际应用可能需要更复杂的表达式处理
        bool matchesValue = false;

        if (filterExpression.contains(">")) {
            double threshold = filterExpression.section('>', 1).trimmed().toDouble();
            if (item.value > threshold) {
                matchesValue = true;
            }
        }
        else if (filterExpression.contains("<")) {
            double threshold = filterExpression.section('<', 1).trimmed().toDouble();
            if (item.value < threshold) {
                matchesValue = true;
            }
        }
        else if (filterExpression.contains("=")) {
            double threshold = filterExpression.section('=', 1).trimmed().toDouble();
            if (qFuzzyCompare(item.value, threshold)) {
                matchesValue = true;
            }
        }

        if (matchesValue) {
            result.append(static_cast<int>(i));
        }
    }

    return result;
}

void DataAnalysisModel::sortData(int column, bool ascending)
{
    // 根据指定列对数据进行排序
    auto comparator = [column, ascending](const DataAnalysisItem& a, const DataAnalysisItem& b) {
        // 如果无效项，则将其排在后面
        if (!a.isValid && !b.isValid) return false;
        if (!a.isValid) return false;
        if (!b.isValid) return true;

        bool result = false;

        switch (column) {
        case 0: // 索引
            result = a.index < b.index;
            break;
        case 1: // 时间戳
            result = a.timeStamp < b.timeStamp;
            break;
        case 2: // 值
            result = a.value < b.value;
            break;
        case 3: // 描述
            result = a.description < b.description;
            break;
        default: // 数据点
            if (column - 4 < a.dataPoints.size() && column - 4 < b.dataPoints.size()) {
                result = a.dataPoints[column - 4] < b.dataPoints[column - 4];
            }
            else if (column - 4 < a.dataPoints.size()) {
                result = true;
            }
            else {
                result = false;
            }
            break;
        }

        return ascending ? result : !result;
    };

    std::sort(m_dataItems.begin(), m_dataItems.end(), comparator);
    emit dataChanged();
}