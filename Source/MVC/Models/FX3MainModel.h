// MVC/Models/FX3MainModel.h
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>
#include <atomic>
#include <mutex>
#include "AppStateMachine.h"

class ChannelSelectModel;

/**
 * @brief FX3主模型类 - 应用程序的核心数据模型
 *
 * 管理应用程序状态、设备配置和数据处理配置
 * 实现为单例模式，确保整个应用中只有一个数据源
 */
class FX3MainModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例指针
     */
    static FX3MainModel* getInstance();

    //--- 应用状态管理 ---//

    /**
     * @brief 获取当前应用程序状态
     * @return 应用程序状态
     */
    AppState getAppState() const;

    /**
     * @brief 获取设备连接状态
     * @return 设备是否连接
     */
    bool isDeviceConnected() const;

    /**
     * @brief 获取数据传输状态
     * @return 是否正在传输数据
     */
    bool isTransferring() const;

    /**
     * @brief 获取应用程序是否正在关闭
     * @return 是否正在关闭
     */
    bool isClosing() const;

    /**
     * @brief 设置设备连接状态
     * @param connected 设备是否已连接
     */
    void setDeviceConnected(bool connected);

    /**
     * @brief 设置数据传输状态
     * @param transferring 是否正在传输数据
     */
    void setTransferring(bool transferring);

    /**
     * @brief 设置应用程序关闭状态
     * @param closing 是否正在关闭
     */
    void setClosing(bool closing);

    //--- 视频配置管理 ---//

    /**
     * @brief 获取视频配置
     * @param width 输出参数，视频宽度
     * @param height 输出参数，视频高度
     * @param format 输出参数，视频格式
     */
    void getVideoConfig(uint16_t& width, uint16_t& height, uint8_t& format) const;

    /**
     * @brief 设置视频配置
     * @param width 视频宽度
     * @param height 视频高度
     * @param format 视频格式
     */
    void setVideoConfig(uint16_t width, uint16_t height, uint8_t format);

    //--- 设备信息管理 ---//

    /**
     * @brief 获取设备信息
     * @param deviceName 输出参数，设备名称
     * @param firmwareVersion 输出参数，固件版本
     * @param serialNumber 输出参数，序列号
     */
    void getDeviceInfo(QString& deviceName, QString& firmwareVersion, QString& serialNumber) const;

    /**
     * @brief 设置设备信息
     * @param deviceName 设备名称
     * @param firmwareVersion 固件版本
     * @param serialNumber 序列号
     */
    void setDeviceInfo(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber);

    //--- 传输统计管理 ---//

    /**
     * @brief 更新传输统计信息
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率 (bytes/sec)
     * @param errorCount 错误计数
     */
    void updateTransferStats(uint64_t bytesTransferred, double transferRate, uint32_t errorCount);

    /**
     * @brief 获取传输统计信息
     * @param bytesTransferred 输出参数，已传输字节数
     * @param transferRate 输出参数，传输速率 (bytes/sec)
     * @param errorCount 输出参数，错误计数
     */
    void getTransferStats(uint64_t& bytesTransferred, double& transferRate, uint32_t& errorCount) const;

    /**
     * @brief 重置传输统计信息
     */
    void resetTransferStats();

    //--- 命令目录管理 ---//

    /**
     * @brief 获取命令文件目录
     * @return 命令文件目录
     */
    QString getCommandDirectory() const;

    /**
     * @brief 设置命令文件目录
     * @param dir 命令文件目录
     */
    void setCommandDirectory(const QString& dir);

signals:
    /**
     * @brief 设备连接状态变更信号
     * @param connected 设备是否已连接
     */
    void deviceConnectionChanged(bool connected);

    /**
     * @brief 数据传输状态变更信号
     * @param transferring 是否正在传输数据
     */
    void transferStateChanged(bool transferring);

    /**
     * @brief 传输统计信息更新信号
     * @param bytesTransferred 已传输字节数
     * @param transferRate 传输速率 (bytes/sec)
     * @param errorCount 错误计数
     */
    void transferStatsUpdated(uint64_t bytesTransferred, double transferRate, uint32_t errorCount);

    /**
     * @brief 视频配置变更信号
     * @param width 视频宽度
     * @param height 视频高度
     * @param format 视频格式
     */
    void videoConfigChanged(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 设备信息变更信号
     * @param deviceName 设备名称
     * @param firmwareVersion 固件版本
     * @param serialNumber 序列号
     */
    void deviceInfoChanged(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber);

    /**
     * @brief 应用程序状态变更信号
     * @param state 新状态
     * @param oldState 旧状态
     * @param reason 变更原因
     */
    void appStateChanged(AppState state, AppState oldState, const QString& reason);

    /**
     * @brief 命令目录变更信号
     * @param dir 新命令目录
     */
    void commandDirectoryChanged(const QString& dir);

    /**
     * @brief 关闭状态变更信号
     * @param closing 是否正在关闭
     */
    void closingStateChanged(bool closing);

private:
    /**
     * @brief 构造函数 (私有，用于单例模式)
     */
    FX3MainModel();

    /**
     * @brief 析构函数
     */
    ~FX3MainModel();

    // 禁用拷贝构造和赋值操作
    FX3MainModel(const FX3MainModel&) = delete;
    FX3MainModel& operator=(const FX3MainModel&) = delete;

    /**
     * @brief 初始化模型
     */
    void initialize();

    /**
     * @brief 连接信号
     */
    void connectSignals();

private:
    // 应用程序状态
    std::atomic<bool> m_deviceConnected;          ///< 设备连接状态
    std::atomic<bool> m_transferring;             ///< 数据传输状态
    std::atomic<bool> m_closing;                  ///< 应用程序是否正在关闭

    // 通道和视频配置
    ChannelSelectModel* m_channelConfigModel;     ///< 通道配置模型
    uint16_t m_videoWidth;                        ///< 视频宽度
    uint16_t m_videoHeight;                       ///< 视频高度
    uint8_t m_videoFormat;                        ///< 视频格式
    mutable std::mutex m_videoConfigMutex;        ///< 视频配置互斥锁

    // 设备信息
    QString m_deviceName;                         ///< 设备名称
    QString m_firmwareVersion;                    ///< 固件版本
    QString m_serialNumber;                       ///< 序列号
    mutable std::mutex m_deviceInfoMutex;         ///< 设备信息互斥锁

    // 传输统计
    uint64_t m_bytesTransferred;                  ///< 已传输字节数
    double m_transferRate;                        ///< 传输速率 (bytes/sec)
    uint32_t m_errorCount;                        ///< 错误计数
    mutable std::mutex m_statsMutex;              ///< 统计信息互斥锁

    // 配置
    QString m_commandDir;                         ///< 命令文件目录
    mutable std::mutex m_configMutex;             ///< 配置互斥锁
};