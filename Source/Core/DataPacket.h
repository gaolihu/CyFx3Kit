// Source/Core/DataPacket.h
#pragma once

#include <vector>
#include <cstdint>
#include <memory>

/**
 * @brief 数据包结构体
 * 包含批处理支持，减少信号触发频率
 */
struct DataPacket {
    std::shared_ptr<std::vector<uint8_t>> data;  // 使用共享指针减少数据复制
    uint64_t timestamp;                          // 时间戳

    // 批处理支持字段
    bool isBatchComplete = true;                 // 是否是批次的最后一个包
    uint32_t batchId = 0;                        // 批次ID
    size_t packetsInBatch = 1;                   // 批次中包的总数

    // 便捷访问方法
    const uint8_t* getData() const { return data ? data->data() : nullptr; }
    size_t getSize() const { return data ? data->size() : 0; }
};

/**
 * @brief 数据包批次
 * 用于高效批量处理数据包
 */
using DataPacketBatch = std::vector<DataPacket>;
