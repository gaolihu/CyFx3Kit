// Source/MVC/Models/VideoDisplayModel.h
#pragma once

#include <QObject>
#include <memory>
#include <QImage>
#include <QVector>
#include <QPair>
#include "IndexGenerator.h"

/**
 * @brief 视频配置数据结构
 */
struct VideoConfig {
    uint16_t width = 1920;               ///< 视频宽度
    uint16_t height = 1080;              ///< 视频高度
    uint8_t format = 0x39;               ///< 视频格式，如RAW8(0x38)、RAW10(0x39)、RAW12(0x3A)等
    int colorMode = 1;                   ///< 色彩模式索引
    int dataMode = 0;                    ///< 数据模式索引
    int colorArrangement = 0;            ///< 色彩排布索引
    int virtualChannel = 0;              ///< 虚拟通道
    double fps = 0.0;                    ///< 帧率 (PPS)
    bool isRunning = false;              ///< 是否正在运行
    uint8_t commandType = 0;             ///< 命令类型过滤
    uint64_t startTimestamp = 0;         ///< 开始时间戳
    uint64_t endTimestamp = 0;           ///< 结束时间戳
    bool autoAdvance = false;            ///< 自动播放下一帧
    int playbackSpeed = 1;               ///< 播放速度倍率
};

/**
 * @brief 视频配置模型类
 *
 * 负责存储和管理视频显示的配置数据
 */
class VideoDisplayModel : public QObject
{
    Q_OBJECT

public:
    static VideoDisplayModel* getInstance();

    /**
     * @brief 获取当前配置
     * @return 当前视频配置
     */
    VideoConfig getConfig() const;

    /**
     * @brief 设置配置
     * @param config 新的视频配置
     */
    void setConfig(const VideoConfig& config);

    /**
     * @brief 获取当前帧数据
     * @return 当前视频帧数据
     */
    QByteArray getFrameData() const;

    /**
     * @brief 设置当前帧数据
     * @param data 新的视频帧数据
     */
    void setFrameData(const QByteArray& data);

    /**
     * @brief 获取当前索引条目
     * @return 当前帧的索引条目
     */
    PacketIndexEntry getCurrentEntry() const;

    /**
     * @brief 设置当前索引条目
     * @param entry 新的索引条目
     */
    void setCurrentEntry(const PacketIndexEntry& entry);

    /**
     * @brief 获取渲染图像
     * @return 当前渲染的图像
     */
    QImage getRenderImage() const;

    /**
     * @brief 设置渲染图像
     * @param image 新的渲染图像
     */
    void setRenderImage(const QImage& image);

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

    /**
     * @brief 设置已加载的帧列表
     * @param entries 索引条目列表
     */
    void setLoadedFrames(const QVector<PacketIndexEntry>& entries);

    /**
     * @brief 获取已加载的帧列表
     * @return 索引条目列表
     */
    QVector<PacketIndexEntry> getLoadedFrames() const;

    /**
     * @brief 获取当前帧索引
     * @return 当前帧在已加载列表中的索引
     */
    int getCurrentFrameIndex() const;

    /**
     * @brief 设置当前帧索引
     * @param index 新的帧索引
     * @return 是否设置成功
     */
    bool setCurrentFrameIndex(int index);

    /**
     * @brief 获取总帧数
     * @return 已加载的总帧数
     */
    int getTotalFrames() const;

    /**
     * @brief 移动到下一帧
     * @return 是否成功移动到下一帧
     */
    bool moveToNextFrame();

    /**
     * @brief 移动到上一帧
     * @return 是否成功移动到上一帧
     */
    bool moveToPreviousFrame();

signals:
    /**
     * @brief 配置变更信号
     * @param config 新的视频配置
     */
    void configChanged(const VideoConfig& config);

    /**
     * @brief 帧数据变更信号
     * @param data 新的帧数据
     */
    void frameDataChanged(const QByteArray& data);

    /**
     * @brief 渲染图像变更信号
     * @param image 新的渲染图像
     */
    void renderImageChanged(const QImage& image);

    /**
     * @brief 当前帧索引变更信号
     * @param index 新的帧索引
     * @param total 总帧数
     */
    void currentFrameChanged(int index, int total);

    /**
     * @brief 当前索引条目变更信号
     * @param entry 新的索引条目
     */
    void currentEntryChanged(const PacketIndexEntry& entry);

private:
    /**
     * @brief 构造函数（私有，用于单例模式）
     */
    VideoDisplayModel();

    /**
     * @brief 析构函数
     */
    ~VideoDisplayModel();

    /**
     * @brief 创建默认配置
     * @return 默认视频配置
     */
    VideoConfig createDefaultConfig();

private:
    VideoConfig m_config;                                ///< 当前配置
    QByteArray m_frameData;                              ///< 当前帧数据
    QImage m_renderImage;                                ///< 当前渲染图像
    QVector<PacketIndexEntry> m_loadedFrames;            ///< 已加载的帧列表
    int m_currentFrameIndex = -1;                        ///< 当前帧索引
    PacketIndexEntry m_currentEntry;                     ///< 当前帧的索引条目
};