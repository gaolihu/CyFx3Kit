// Source/MVC/Views/VideoDisplayView.h
#pragma once

#include <QWidget>
#include <memory>

namespace Ui {
    class VideoDisplayClass;
}

class VideoDisplayController;

/**
 * @brief 视频显示视图类
 *
 * 负责显示视频界面
 */
class VideoDisplayView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit VideoDisplayView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~VideoDisplayView();

    /**
     * @brief 获取UI对象
     * @return UI对象指针
     */
    Ui::VideoDisplayClass* getUi() { return ui; }

    void setVidioDisplayController(VideoDisplayController* controller) {
        m_controller = controller;
    }

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
     * @brief 开始/停止自动播放
     * @param enable 是否启用自动播放
     * @param interval 播放间隔(毫秒)
     */
    void setAutoPlay(bool enable, int interval = 33);

signals:
    /**
     * @brief 视频显示状态改变信号
     * @param isRunning 当前是否正在显示视频
     */
    void signal_VD_V_videoDisplayStatusChanged(bool isRunning);

    /**
     * @brief 帧索引变更信号
     * @param index 新的帧索引
     * @param total 总帧数
     */
    void signal_VD_V_frameIndexChanged(int index, int total);

protected:
    /**
     * @brief 绘图事件处理
     * @param event 绘图事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 键盘事件处理
     * @param event 键盘事件
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief 鼠标滚轮事件处理
     * @param event 鼠标滚轮事件
     */
    void wheelEvent(QWheelEvent* event) override;

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

private:
    Ui::VideoDisplayClass* ui;                  ///< UI对象
    VideoDisplayController* m_controller;       ///< 控制器对象
};