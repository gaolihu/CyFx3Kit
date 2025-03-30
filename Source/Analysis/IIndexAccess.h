// Source/Analysis/IIndexAccess.h
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

    /**
     * @brief 获取特定时间范围内的所有数据包
     * @param startTime 开始时间戳
     * @param endTime 结束时间戳
     * @return 范围内的索引条目列表
     */
    virtual QVector<PacketIndexEntry> getPacketsInRange(uint64_t startTime, uint64_t endTime) const = 0;

    /**
     * @brief 根据查询条件检索数据包索引
     * @param query 查询条件
     * @return 匹配的索引条目列表
     */
    virtual QVector<PacketIndexEntry> queryIndex(const IndexQuery& query) const = 0;

    /**
     * @brief 查找特定命令类型的数据包
     * @param commandType 命令类型
     * @param limit 最大结果数量，-1表示不限制
     * @return 匹配的索引条目列表
     */
    virtual QVector<PacketIndexEntry> findPacketsByCommandType(uint8_t commandType, int limit = -1) const = 0;
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

    QVector<PacketIndexEntry> getPacketsInRange(uint64_t startTime, uint64_t endTime) const override {
        return IndexGenerator::getInstance().getPacketsInRange(startTime, endTime);
    }

    QVector<PacketIndexEntry> queryIndex(const IndexQuery& query) const override {
        return IndexGenerator::getInstance().queryIndex(query);
    }

    QVector<PacketIndexEntry> findPacketsByCommandType(uint8_t commandType, int limit = -1) const override {
        IndexQuery query;
        query.featureFilters.append(QString("commandType=%1").arg(commandType));
        query.limit = limit;
        return IndexGenerator::getInstance().queryIndex(query);
    }
};