// Source/Core/DataPacket.h
#pragma once

#include <vector>

// 数据包结构
struct DataPacket {
    std::vector<uint8_t> data;
    size_t size;
    uint64_t timestamp;
};
