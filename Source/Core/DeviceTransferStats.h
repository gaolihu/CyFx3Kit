// Source/Core/DeviceTransferStats.h
#pragma once

#include <cstdint>
#include <chrono>

/**
 * @brief 设备传输统计数据结构
 *
 * 存储与设备数据传输相关的统计信息
 */
struct DeviceTransferStats {
    uint64_t bytesTransferred;   ///< 总传输字节数
    double transferRate;         ///< 传输速率(MB/s)
    uint32_t errorCount;         ///< 传输错误计数
    uint64_t elapseMs;           ///< 传输时间ms
    std::chrono::steady_clock::time_point startTime;  ///< 传输开始时间点

    /**
     * @brief 获取传输持续时间(秒)
     * @return 传输持续时间
     */
    uint64_t getDurationSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    }
};