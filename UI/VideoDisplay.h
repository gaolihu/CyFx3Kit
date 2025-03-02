#pragma once

#include <QWidget>
#include <QMessageBox>
#include <QImage>
#include <QPainter>
#include "ui_VideoDisplay.h"

/**
 * @brief 视频显示窗口类
 *
 * 负责解码和显示视频数据，支持不同格式和分辨率的视频流
 */
class VideoDisplay : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit VideoDisplay(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~VideoDisplay();

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

private slots:
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

private:
    Ui::VideoDisplayClass ui;

    // 图像参数
    uint16_t m_width;         ///< 图像宽度
    uint16_t m_height;        ///< 图像高度
    uint8_t m_format;         ///< 图像格式

    // 当前视频帧数据
    QByteArray m_currentFrameData;

    // 渲染的图像
    QImage m_renderImage;

    // 视频显示状态
    bool m_isRunning;

    /**
     * @brief 初始化UI界面
     */
    void initializeUI();

    /**
     * @brief 连接信号与槽
     */
    void connectSignals();

    /**
     * @brief 更新UI状态
     */
    void updateUIState();

    /**
     * @brief 渲染视频帧
     */
    void renderVideoFrame();

    /**
     * @brief 绘图事件处理
     * @param event 绘图事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 解码RAW数据为QImage
     * @param data RAW格式的图像数据
     * @return 解码后的图像
     */
    QImage decodeRawData(const QByteArray& data);
};