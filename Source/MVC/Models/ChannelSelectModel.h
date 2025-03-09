// Source/MVC/Models/ChannelSelectModel.h
#pragma once

#include <QObject>
#include <memory>
#include <array>

/**
 * @brief 通道配置数据结构
 */
struct ChannelConfig {
    int captureType = 0;                           ///< 抓取类型
    bool clockPNSwap = false;                      ///< 时钟PN交换
    std::array<bool, 4> channelEnabled = { true, true, true, true }; ///< 通道使能
    std::array<bool, 4> channelPNSwap = { false, false, false, false }; ///< 通道PN交换
    std::array<int, 4> channelSwap = { 0, 1, 2, 3 }; ///< 通道交换
    bool testModeEnabled = false;                  ///< 测试模式使能
    int testModeType = 0;                          ///< 测试模式类型
    int videoWidth = 1920;                         ///< 视频宽度
    int videoHeight = 1080;                        ///< 视频高度
    double teValue = 60.0;                         ///< TE值
};

/**
 * @brief 通道配置模型类
 *
 * 负责存储和管理通道配置数据
 */
class ChannelSelectModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ChannelSelectModel* getInstance();

    /**
     * @brief 获取当前配置
     * @return 当前通道配置
     */
    ChannelConfig getConfig() const;

    /**
     * @brief 设置配置
     * @param config 新的通道配置
     */
    void setConfig(const ChannelConfig& config);

    /**
     * @brief 保存配置到存储
     * @return 保存结果，true表示成功
     */
    bool saveConfig();

    /**
     * @brief 加载配置从存储
     * @return 加载结果，true表示成功
     */
    bool loadConfig();

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

signals:
    /**
     * @brief 配置变更信号
     * @param config 新的通道配置
     */
    void configChanged(const ChannelConfig& config);

private:
    /**
     * @brief 构造函数（私有，用于单例模式）
     */
    ChannelSelectModel();

    /**
     * @brief 析构函数
     */
    ~ChannelSelectModel();

    /**
     * @brief 创建默认配置
     * @return 默认通道配置
     */
    ChannelConfig createDefaultConfig();

private:
    ChannelConfig m_config;                                ///< 当前配置
};