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
 * 负责显示波形分析界面
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
     * @brief 设置波形数据
     * @param xData X轴数据
     * @param yData Y轴数据
     */
    void setWaveformData(const QVector<double>& xData, const QVector<double>& yData);

    /**
     * @brief 添加数据点
     * @param x X轴数据点
     * @param y Y轴数据点
     */
    void addDataPoint(double x, double y);

    /**
     * @brief 清除数据
     */
    void clearData();

signals:
    /**
     * @brief 分析完成信号
     * @param result 分析结果
     */
    void analysisCompleted(const QString& result);

    /**
     * @brief 导出请求信号
     */
    void exportRequested();

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
    Ui::WaveformAnalysisClass* ui;                          ///< UI对象
    std::unique_ptr<WaveformAnalysisController> m_controller; ///< 控制器对象
};