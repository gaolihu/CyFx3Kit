// Source/File/IIndexAccess.h
#pragma once

#include <QVector>
#include <QString>
#include <QVariant>
#include "IndexGenerator.h"

/**
 * @brief 索引访问接口
 */
class IIndexAccess {
public:
    virtual ~IIndexAccess() {}

    /**
     * @brief 根据时间戳查找最接近的数据包
     */
    virtual PacketIndexEntry findClosestPacket(uint64_t timestamp) const = 0;

    /**
     * @brief 获取所有索引条目
     */
    virtual QVector<PacketIndexEntry> getAllIndexEntries() const = 0;

    /**
     * @brief 获取索引条目数量
     */
    virtual int getIndexCount() const = 0;
};

/**
 * @brief 基于IndexGenerator的索引访问实现
 */
class IndexGeneratorAccess : public IIndexAccess {
public:
    PacketIndexEntry findClosestPacket(uint64_t timestamp) const override {
        return IndexGenerator::getInstance().findClosestPacket(timestamp);
    }

    QVector<PacketIndexEntry> getAllIndexEntries() const override {
        return IndexGenerator::getInstance().getAllIndexEntries();
    }

    int getIndexCount() const override {
        return IndexGenerator::getInstance().getIndexCount();
    }
};