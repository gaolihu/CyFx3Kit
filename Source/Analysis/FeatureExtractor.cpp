// Source/Analysis/FeatureExtractor.cpp

#include <QFuture>
#include <QtConcurrent>
#include "FeatureExtractor.h"
#include "Logger.h"
#include <algorithm>
#include <numeric>
#include <cmath>

FeatureExtractor& FeatureExtractor::getInstance()
{
    static FeatureExtractor instance;
    return instance;
}

FeatureExtractor::FeatureExtractor(QObject* parent)
    : QObject(parent)
{
    // 注册内置特征提取器
    m_extractors["average"] = [this](const QByteArray& data, uint16_t width, uint16_t height, uint8_t format) {
        return extractAverageValue(data, width, height, format);
    };

    m_extractors["max"] = [this](const QByteArray& data, uint16_t width, uint16_t height, uint8_t format) {
        return extractMaxValue(data, width, height, format);
    };

    m_extractors["min"] = [this](const QByteArray& data, uint16_t width, uint16_t height, uint8_t format) {
        return extractMinValue(data, width, height, format);
    };

    m_extractors["histogram"] = [this](const QByteArray& data, uint16_t width, uint16_t height, uint8_t format) {
        return extractHistogram(data, width, height, format);
    };

    m_extractors["edge_count"] = [this](const QByteArray& data, uint16_t width, uint16_t height, uint8_t format) {
        return extractEdgeCount(data, width, height, format);
    };

    m_extractors["noise_level"] = [this](const QByteArray& data, uint16_t width, uint16_t height, uint8_t format) {
        return extractNoiseLevel(data, width, height, format);
    };

    // 默认启用所有特征
    for (auto it = m_extractors.begin(); it != m_extractors.end(); ++it) {
        m_enabledFeatures[it.key()] = true;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("特征提取器已初始化，支持 %1 种特征").arg(m_extractors.size()));
}

FeatureExtractor::~FeatureExtractor()
{
}

QMap<QString, QVariant> FeatureExtractor::extractFeatures(const DataPacket& packet)
{
    // 从数据包获取数据
    const uint8_t* data = packet.getData();
    size_t size = packet.getSize();

    // 转换为QByteArray
    QByteArray byteArray(reinterpret_cast<const char*>(data), static_cast<int>(size));

    // 默认使用1920x1080的RAW10格式
    uint16_t width = 1920;
    uint16_t height = 1080;
    uint8_t format = 0x39; // RAW10

    return extractFeaturesFromRaw(byteArray, width, height, format);
}

QMap<QString, QVariant> FeatureExtractor::extractFeaturesFromRaw(
    const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    QMap<QString, QVariant> features;
    QElapsedTimer timer;
    timer.start();

    // 创建任务列表
    QVector<QPair<QString, std::function<QVariant()>>> tasks;

    for (auto it = m_extractors.begin(); it != m_extractors.end(); ++it) {
        if (m_enabledFeatures.value(it.key(), false)) {
            QString featureName = it.key();
            auto extractorFunc = it.value();

            tasks.append(qMakePair(featureName, [=]() -> QVariant {
                try {
                    return extractorFunc(data, width, height, format);
                }
                catch (const std::exception& e) {
                    LOG_ERROR(LocalQTCompat::fromLocal8Bit("提取特征 %1 失败: %2")
                        .arg(featureName)
                        .arg(e.what()));
                    return QVariant();
                }
                }));
        }
    }

    // 使用QtConcurrent并行执行
    QFuture<void> future = QtConcurrent::map(tasks, [&features](const QPair<QString, std::function<QVariant()>>& task) {
        QVariant result = task.second();
        if (result.isValid()) {
            // 需要互斥锁保护features的访问
            static QMutex mutex;
            QMutexLocker locker(&mutex);
            features[task.first] = result;
        }
        });

    // 等待所有任务完成
    future.waitForFinished();

    // 计算总的提取时间...
    qint64 elapsed = timer.elapsed();
    features["extraction_time_ms"] = static_cast<int>(elapsed);

    LOG_DEBUG(LocalQTCompat::fromLocal8Bit("提取了 %1 个特征，耗时 %2 ms")
        .arg(features.size())
        .arg(elapsed));

    // 发送特征提取完成信号
    emit featuresExtracted(0, features);

    return features;
}

void FeatureExtractor::addFeatureExtractor(const QString& name,
    std::function<QVariant(const QByteArray&, uint16_t, uint16_t, uint8_t)> extractor)
{
    m_extractors[name] = extractor;
    m_enabledFeatures[name] = true;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("添加特征提取器: %1").arg(name));
}

void FeatureExtractor::setFeatureEnabled(const QString& featureName, bool enabled)
{
    if (m_extractors.contains(featureName)) {
        m_enabledFeatures[featureName] = enabled;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("特征 %1 已%2")
            .arg(featureName)
            .arg(enabled ? "启用" : "禁用"));
    }
    else {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("尝试设置不存在的特征: %1").arg(featureName));
    }
}

QStringList FeatureExtractor::getAvailableFeatures() const
{
    return m_extractors.keys();
}

// 内置特征提取器实现
QVariant FeatureExtractor::extractAverageValue(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    // 简单实现：计算RAW8格式的平均像素值
    if (format == 0x38) { // RAW8
        if (data.size() < static_cast<int>(static_cast<uint32_t>(width) * height)) {
            throw std::runtime_error("数据大小不足");
        }

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        uint64_t sum = 0;

        // 使用uint32_t而不是int，避免有符号/无符号比较警告
        for (uint32_t i = 0; i < static_cast<uint32_t>(width) * height; ++i) {
            sum += rawData[i];
        }

        return sum / static_cast<double>(width * height);
    }
    else if (format == 0x39) { // RAW10
        // RAW10格式需要特殊处理
        // 每5个字节包含4个10位像素
        size_t bytesPerRow = (static_cast<size_t>(width) * 10 + 7) / 8;
        if (static_cast<size_t>(data.size()) < bytesPerRow * height) {
            throw std::runtime_error("数据大小不足");
        }

        // 简化实现：仅使用前8位
        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        uint64_t sum = 0;
        size_t count = 0;

        for (uint16_t y = 0; y < height; ++y) {
            for (uint16_t x = 0; x < width; ++x) {
                // 计算像素在数据中的偏移
                size_t pixelOffset = (static_cast<size_t>(y) * bytesPerRow) + (static_cast<size_t>(x) * 10 / 8);
                if (pixelOffset < static_cast<size_t>(data.size())) {
                    sum += rawData[pixelOffset];
                    count++;
                }
            }
        }

        return (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
    }

    return 0.0; // 不支持的格式
}

QVariant FeatureExtractor::extractMaxValue(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    // 简单实现：查找RAW8数据的最大值
    if (format == 0x38) { // RAW8
        if (data.isEmpty()) {
            return 0;
        }

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        uint8_t maxVal = 0;

        // 明确指定std::min的模板参数类型，并统一使用size_t
        size_t maxElements = std::min<size_t>(
            static_cast<size_t>(data.size()),
            static_cast<size_t>(width) * static_cast<size_t>(height)
            );

        for (size_t i = 0; i < maxElements; ++i) {
            maxVal = std::max(maxVal, rawData[i]);
        }

        return static_cast<int>(maxVal);
    }

    return 0; // 不支持的格式
}

QVariant FeatureExtractor::extractMinValue(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    // 简单实现：查找RAW8数据的最小值
    if (format == 0x38) { // RAW8
        if (data.isEmpty()) {
            return 0;
        }

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        uint8_t minVal = 255;

        // 明确指定std::min的模板参数类型
        size_t maxElements = std::min<size_t>(
            static_cast<size_t>(data.size()),
            static_cast<size_t>(width) * static_cast<size_t>(height)
            );

        for (size_t i = 0; i < maxElements; ++i) {
            minVal = std::min(minVal, rawData[i]);
        }

        return static_cast<int>(minVal);
    }

    return 0; // 不支持的格式
}

QVariant FeatureExtractor::extractHistogram(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    // 简单实现：计算RAW8图像的直方图（降采样到16个区间）
    if (format == 0x38) { // RAW8
        if (data.isEmpty()) {
            return QVariantList();
        }

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        QVector<int> histogram(16, 0);

        // 修复std::min使用问题
        size_t maxElements = std::min<size_t>(
            static_cast<size_t>(data.size()),
            static_cast<size_t>(width) * static_cast<size_t>(height)
            );

        for (size_t i = 0; i < maxElements; ++i) {
            int bin = rawData[i] / 16; // 将0-255映射到0-15区间
            histogram[bin]++;
        }

        // 转换为QVariantList
        QVariantList result;
        for (int val : histogram) {
            result.append(val);
        }

        return result;
    }

    return QVariantList(); // 不支持的格式
}

QVariant FeatureExtractor::extractEdgeCount(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    // 简单实现：计算RAW8图像的简单边缘数量
    if (format == 0x38 && width > 1 && height > 1) { // RAW8
        if (static_cast<size_t>(data.size()) < static_cast<size_t>(width) * height) {
            return 0;
        }

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        int edgeCount = 0;
        int threshold = 30; // 边缘检测阈值

        // 水平方向检测边缘
        for (uint16_t y = 0; y < height; ++y) {
            for (uint16_t x = 1; x < width; ++x) {
                size_t idx1 = static_cast<size_t>(y) * width + x - 1;
                size_t idx2 = static_cast<size_t>(y) * width + x;

                if (std::abs(static_cast<int>(rawData[idx1]) - static_cast<int>(rawData[idx2])) > threshold) {
                    edgeCount++;
                }
            }
        }

        // 垂直方向检测边缘
        for (uint16_t y = 1; y < height; ++y) {
            for (uint16_t x = 0; x < width; ++x) {
                size_t idx1 = static_cast<size_t>(y - 1) * width + x;
                size_t idx2 = static_cast<size_t>(y) * width + x;

                if (std::abs(static_cast<int>(rawData[idx1]) - static_cast<int>(rawData[idx2])) > threshold) {
                    edgeCount++;
                }
            }
        }

        return edgeCount;
    }

    return 0; // 不支持的格式
}

QVariant FeatureExtractor::extractNoiseLevel(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format)
{
    // 简单实现：计算RAW8图像的局部噪声水平（标准差）
    if (format == 0x38 && width > 2 && height > 2) { // RAW8
        if (static_cast<size_t>(data.size()) < static_cast<size_t>(width) * height) {
            return 0.0;
        }

        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.constData());
        double sumVariance = 0.0;
        int blockSize = 8; // 局部块大小
        int blockCount = 0;

        // 在图像中采样局部块
        for (uint16_t y = 0; y <= height - blockSize; y += blockSize) {
            for (uint16_t x = 0; x <= width - blockSize; x += blockSize) {
                // 计算局部块的均值
                double blockSum = 0.0;
                for (int by = 0; by < blockSize; ++by) {
                    for (int bx = 0; bx < blockSize; ++bx) {
                        blockSum += rawData[(static_cast<size_t>(y) + by) * width + (static_cast<size_t>(x) + bx)];
                    }
                }
                double blockMean = blockSum / (blockSize * blockSize);

                // 计算局部块的方差
                double blockVariance = 0.0;
                for (int by = 0; by < blockSize; ++by) {
                    for (int bx = 0; bx < blockSize; ++bx) {
                        double diff = rawData[(static_cast<size_t>(y) + by) * width + (static_cast<size_t>(x) + bx)] - blockMean;
                        blockVariance += diff * diff;
                    }
                }
                blockVariance /= (blockSize * blockSize - 1);

                sumVariance += blockVariance;
                blockCount++;
            }
        }

        // 计算平均噪声水平
        double averageNoiseLevel = (blockCount > 0) ? std::sqrt(sumVariance / blockCount) : 0.0;
        return averageNoiseLevel;
    }

    return 0.0; // 不支持的格式
}