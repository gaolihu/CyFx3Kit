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

    /**
     * @brief 设置控制器
     * @param controller 控制器指针
     */
    void setController(WaveformAnalysisController* controller);

    /**
     * @brief 更新标记列表
     * @param markers 标记点列表
     */
    void updateMarkerList(const QVector<int>& markers);

    /**
     * @brief 设置分析结果文本
     * @param text 分析结果文本
     */
    void setAnalysisResult(const QString& text);

    /**
     * @brief 设置状态栏消息
     * @param message 状态消息
     */
    void setStatusMessage(const QString& message);

signals:
    /**
     * @brief 通道可见性改变信号
     * @param channel 通道索引
     * @param visible 是否可见
     */
    void signal_WA_V_channelVisibilityChanged(int channel, bool visible);

    /**
     * @brief 缩放改变信号
     * @param zoomFactor 缩放因子
     */
    void signal_WA_V_zoomChanged(double zoomFactor);

    /**
     * @brief 平移信号
     * @param deltaX 水平偏移量
     */
    void signal_WA_V_panChanged(int deltaX);

    /**
     * @brief 垂直缩放改变信号
     * @param value 缩放值
     */
    void signal_WA_V_verticalScaleChanged(int value);

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

    /**
     * @brief 显示事件
     * @param event 显示事件
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief 显示事件
     * @param event 显示事件
     */
    void hideEvent(QHideEvent* event) override;

private slots:
    /**
     * @brief 处理通道选择变更
     * @param state 复选框状态
     */
    void slot_WA_V_onChannelCheckboxToggled(int state);

    /**
     * @brief 处理加载测试数据动作
     */
    void slot_WA_V_onLoadTestDataTriggered();

    /**
     * @brief 处理放大动作
     */
    void slot_WA_V_onZoomInTriggered();

    /**
     * @brief 处理缩小动作
     */
    void slot_WA_V_onZoomOutTriggered();

    /**
     * @brief 处理重置视图动作
     */
    void slot_WA_V_onZoomResetTriggered();

    /**
     * @brief 处理开始分析动作
     */
    void slot_WA_V_onStartAnalysisTriggered();

    /**
     * @brief 处理停止分析动作
     */
    void slot_WA_V_onStopAnalysisTriggered();

    /**
     * @brief 处理导出数据动作
     */
    void slot_WA_V_onExportDataTriggered();

    /**
     * @brief 处理分析按钮点击
     */
    void slot_WA_V_onAnalyzeButtonClicked();

    /**
     * @brief 处理清除标记按钮点击
     */
    void slot_WA_V_onClearMarkersButtonClicked();

    /**
     * @brief 处理垂直缩放滑块变更
     * @param value 滑块值
     */
    void slot_WA_V_onVerticalScaleSliderChanged(int value);

private:
    /**
     * @brief 初始化连接
     */
    void connectSignals();

    /**
     * @brief 初始化界面状态
     */
    void initializeUIState();

private:
    Ui::WaveformAnalysisClass* ui;              ///< UI对象
    WaveformAnalysisController* m_controller;   ///< 控制器对象
    QRect m_chartRect;                          ///< 图表区域
    bool m_isDragging;                          ///< 是否正在拖动
    QPoint m_lastMousePos;                      ///< 上次鼠标位置
};