// Source/MVC/Views/WaveformAnalysisView.h
#pragma once

#include <QWidget>
#include <memory>

namespace Ui {
    class WaveformAnalysisClass;
}

class WaveformAnalysisController;

/**
 * @brief 波形分析视图类
 *
 * 负责显示波形分析界面及处理用户交互
 */
class WaveformAnalysisView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit WaveformAnalysisView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~WaveformAnalysisView();

    /**
     * @brief 获取UI对象
     * @return UI对象指针
     */
    Ui::WaveformAnalysisClass* getUi() { return ui; }

signals:
    /**
     * @brief 通道可见性改变信号
     * @param channel 通道索引
     * @param visible 是否可见
     */
    void channelVisibilityChanged(int channel, bool visible);

    /**
     * @brief 缩放改变信号
     * @param zoomFactor 缩放因子
     */
    void zoomChanged(double zoomFactor);

    /**
     * @brief 平移信号
     * @param deltaX 水平偏移量
     */
    void panChanged(int deltaX);

protected:
    /**
     * @brief 绘图事件处理
     * @param event 绘图事件
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief 鼠标按下事件
     * @param event 鼠标事件
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标移动事件
     * @param event 鼠标事件
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标释放事件
     * @param event 鼠标事件
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标双击事件
     * @param event 鼠标事件
     */
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /**
     * @brief 鼠标滚轮事件
     * @param event 滚轮事件
     */
    void wheelEvent(QWheelEvent* event) override;

    /**
     * @brief 窗口大小改变事件
     * @param event 大小改变事件
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

private:
    Ui::WaveformAnalysisClass* ui;                          ///< UI对象
    std::unique_ptr<WaveformAnalysisController> m_controller; ///< 控制器对象
    QRect m_chartRect;                                      ///< 图表区域
    bool m_isDragging;                                      ///< 是否正在拖动
    QPoint m_lastMousePos;                                  ///< 上次鼠标位置
};