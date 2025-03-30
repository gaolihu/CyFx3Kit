// Source/MVC/Controllers/VideoDisplayController.h
#pragma once

#include <QObject>
#include <QImage>
#include <QPainter>
#include <memory>
#include <QTimer>
#include "DataAccessService.h"
#include "IIndexAccess.h"

class VideoDisplayView;
namespace Ui { class VideoDisplayClass; }
class VideoDisplayModel;
struct VideoConfig;

/**
 * @brief 视频显示控制器类
 *
 * 处理视频显示的业务逻辑
 */
class VideoDisplayController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param view 视图对象指针
     */
    explicit VideoDisplayController(VideoDisplayView* view);

    /**
     * @brief 析构函数
     */
    ~VideoDisplayController();

    /**
     * @brief 初始化控制器
     * 连接信号和槽，设置初始状态
     */
    bool initialize();

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式，如RAW8(0x38)、RAW10(0x39)、RAW12(0x3A)等
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 更新视频帧
     * @param frameData 新的帧数据
     */
    void updateVideoFrame(const QByteArray& frameData);

    /**
     * @brief 处理绘图事件
     * @param painter 绘图对象
     */
    void handlePaintEvent(QPainter* painter);

    /**
     * @brief 加载指定命令类型的帧数据
     * @param commandType 命令类型
     * @param limit 最大加载数量 (-1表示不限制)
     * @return 加载的帧数量
     */
    int loadFramesByCommandType(uint8_t commandType, int limit = -1);

    /**
     * @brief 加载指定时间范围的帧数据
     * @param startTime 开始时间戳
     * @param endTime 结束时间戳
     * @return 加载的帧数量
     */
    int loadFramesByTimeRange(uint64_t startTime, uint64_t endTime);

    /**
     * @brief 设置当前帧索引
     * @param index 帧索引
     * @return 是否成功
     */
    bool setCurrentFrame(int index);

    /**
     * @brief 移动到下一帧
     * @return 是否成功
     */
    bool moveToNextFrame();

    /**
     * @brief 移动到上一帧
     * @return 是否成功
     */
    bool moveToPreviousFrame();

    /**
     * @brief 设置自动播放
     * @param enable 是否启用自动播放
     * @param interval 播放间隔(毫秒)
     */
    void setAutoPlay(bool enable, int interval = 33);

public slots:
    /**
     * @brief 开始按钮点击事件处理
     */
    void onStartButtonClicked();

    /**
     * @brief 停止按钮点击事件处理
     */
    void onStopButtonClicked();

    /**
     * @brief 退出按钮点击事件处理
     */
    void onExitButtonClicked();

    /**
     * @brief 色彩模式改变事件处理
     * @param index 当前选中的色彩模式索引
     */
    void onColorModeChanged(int index);

    /**
     * @brief 数据模式改变事件处理
     * @param index 当前选中的数据模式索引
     */
    void onDataModeChanged(int index);

    /**
     * @brief 色彩排布改变事件处理
     * @param index 当前选中的色彩排布索引
     */
    void onColorArrangementChanged(int index);

    /**
     * @brief 虚拟通道改变事件处理
     * @param index 当前选中的虚拟通道索引
     */
    void onVirtualChannelChanged(int index);

    /**
     * @brief 视频高度改变事件处理
     * @param text 高度文本
     */
    void onVideoHeightChanged(const QString& text);

    /**
     * @brief 视频宽度改变事件处理
     * @param text 宽度文本
     */
    void onVideoWidthChanged(const QString& text);

    /**
     * @brief 命令类型改变事件处理
     * @param index 当前选中的命令类型索引
     */
    void onCommandTypeChanged(int index);

    /**
     * @brief 开始时间戳改变事件处理
     * @param text 时间戳文本
     */
    void onStartTimeChanged(const QString& text);

    /**
     * @brief 结束时间戳改变事件处理
     * @param text 时间戳文本
     */
    void onEndTimeChanged(const QString& text);

    /**
     * @brief 播放控制按钮点击事件处理
     */
    void onPlayButtonClicked();

    /**
     * @brief 暂停按钮点击事件处理
     */
    void onPauseButtonClicked();

    /**
     * @brief 下一帧按钮点击事件处理
     */
    void onNextFrameButtonClicked();

    /**
     * @brief 上一帧按钮点击事件处理
     */
    void onPrevFrameButtonClicked();

    /**
     * @brief 速度改变事件处理
     * @param value 速度值
     */
    void onSpeedChanged(int value);

    /**
     * @brief 自动播放定时器超时处理
     */
    void onPlaybackTimerTimeout();

private slots:
    /**
     * @brief 配置变更处理函数
     * @param config 新的视频配置
     */
    void onConfigChanged(const VideoConfig& config);

    /**
     * @brief 帧数据变更处理函数
     * @param data 新的帧数据
     */
    void onFrameDataChanged(const QByteArray& data);

    /**
     * @brief 渲染图像变更处理函数
     * @param image 新的渲染图像
     */
    void onRenderImageChanged(const QImage& image);

    /**
     * @brief 当前帧索引变更处理函数
     * @param index 新的帧索引
     * @param total 总帧数
     */
    void onCurrentFrameChanged(int index, int total);

    /**
     * @brief 当前索引条目变更处理函数
     * @param entry 新的索引条目
     */
    void onCurrentEntryChanged(const PacketIndexEntry& entry);

private:
    /**
     * @brief 连接UI组件的信号与槽
     */
    void connectSignals();

    /**
     * @brief 更新UI状态
     * 根据当前配置更新组件状态
     */
    void updateUIState();

    /**
     * @brief 验证配置参数
     * @return 验证结果，true表示验证通过
     */
    bool validateConfig();

    /**
     * @brief 应用模型数据到UI
     */
    void applyModelToUI();

    /**
     * @brief 应用UI数据到模型
     */
    void applyUIToModel();

    /**
     * @brief 渲染视频帧
     */
    void renderVideoFrame();

    /**
     * @brief 解码RAW数据为QImage
     * @param data RAW格式的图像数据
     * @return 解码后的图像
     */
    QImage decodeRawData(const QByteArray& data);

    /**
     * @brief 加载当前帧数据
     * @return 是否成功加载
     */
    bool loadCurrentFrameData();

    /**
     * @brief 更新播放控制UI
     */
    void updatePlaybackControls();

    /**
     * @brief 填充命令类型下拉列表
     */
    void populateCommandTypeComboBox();

private:
    VideoDisplayView* m_view;                            ///< 视图对象
    Ui::VideoDisplayClass* m_ui;                         ///< UI对象
    VideoDisplayModel* m_model;                          ///< 模型对象

    bool m_isInitialized;                                ///< 初始化标志
    bool m_isBatchUpdate;                                ///< 批量更新标志
    bool m_isPlaying;                                    ///< 是否正在播放

    QTimer* m_playbackTimer;                             ///< 播放定时器

    // 常量定义
    static const int MAX_RESOLUTION = 4096;              ///< 最大分辨率限制
    static const int DEFAULT_WIDTH = 1920;               ///< 默认宽度
    static const int DEFAULT_HEIGHT = 1080;              ///< 默认高度
    static const uint8_t DEFAULT_FORMAT = 0x39;          ///< 默认RAW10格式

    // 命令类型映射
    struct CommandTypeInfo {
        uint8_t code;
        QString name;
    };

    QVector<CommandTypeInfo> m_commandTypes;             ///< 命令类型列表
};