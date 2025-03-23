// Source/Analysis/FeatureExtractor.h
#pragma once

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QVariant>
#include "DataPacket.h"

/**
 * @brief 数据特征提取器
 */
class FeatureExtractor : public QObject {
    Q_OBJECT

public:
    static FeatureExtractor& getInstance();

    /**
     * @brief 从数据包提取特征
     * @param packet 数据包
     * @return 提取的特征映射
     */
    QMap<QString, QVariant> extractFeatures(const DataPacket& packet);

    /**
     * @brief 从原始数据提取特征
     * @param data 原始数据
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     * @return 提取的特征映射
     */
    QMap<QString, QVariant> extractFeaturesFromRaw(const QByteArray& data,
        uint16_t width = 1920,
        uint16_t height = 1080,
        uint8_t format = 0x39);

    /**
     * @brief 添加自定义特征提取器
     * @param name 特征名称
     * @param extractor 提取函数
     */
    void addFeatureExtractor(const QString& name,
        std::function<QVariant(const QByteArray&, uint16_t, uint16_t, uint8_t)> extractor);

    /**
     * @brief 设置是否启用特定特征提取
     * @param featureName 特征名称
     * @param enabled 是否启用
     */
    void setFeatureEnabled(const QString& featureName, bool enabled);

    /**
     * @brief 获取所有可用特征名称
     * @return 特征名称列表
     */
    QStringList getAvailableFeatures() const;

signals:
    /**
     * @brief 特征提取完成信号
     * @param timestamp 数据包时间戳
     * @param features 提取的特征
     */
    void featuresExtracted(uint64_t timestamp, const QMap<QString, QVariant>& features);

private:
    FeatureExtractor(QObject* parent = nullptr);
    ~FeatureExtractor();

    // 内置特征提取器
    QVariant extractAverageValue(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format);
    QVariant extractMaxValue(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format);
    QVariant extractMinValue(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format);
    QVariant extractHistogram(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format);
    QVariant extractEdgeCount(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format);
    QVariant extractNoiseLevel(const QByteArray& data, uint16_t width, uint16_t height, uint8_t format);

    // 特征提取器映射
    QMap<QString, std::function<QVariant(const QByteArray&, uint16_t, uint16_t, uint8_t)>> m_extractors;
    QMap<QString, bool> m_enabledFeatures;
};