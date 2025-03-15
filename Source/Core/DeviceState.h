// Source/Core/DeviceState.h
#pragma once

/**
 * @brief 设备状态枚举
 */
enum class DeviceState {
    DEV_DISCONNECTED,   // 设备未连接
    DEV_CONNECTING,     // 设备正在连接
    DEV_CONNECTED,      // 设备已连接但未传输
    DEV_TRANSFERRING,   // 设备正在传输数据
    DEV_ERROR           // 设备错误状态
};
