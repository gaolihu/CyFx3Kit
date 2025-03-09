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

signals:
    /**
     * @brief 视频显示状态改变信号
     * @param isRunning 当前是否正在显示视频
     */
    void videoDisplayStatusChanged(bool isRunning);

protected:
    /**
     * @brief 绘图事件处理
     * @param event 绘图事件
     */
    void paintEvent(QPaintEvent* event) override;

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

private:
    Ui::VideoDisplayClass* ui;                            ///< UI对象
    std::unique_ptr<VideoDisplayController> m_controller; ///< 控制器对象
};