// Source/MVC/Models/VideoDisplayModel.h
#pragma once

#include <QObject>
#include <memory>
#include <QImage>

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
};