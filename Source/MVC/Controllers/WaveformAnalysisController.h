// Source/MVC/Controllers/WaveformAnalysisController.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QPainter>
#include <memory>

class WaveformAnalysisView;
namespace Ui { class WaveformAnalysisClass; }
class WaveformConfigModel;
struct WaveformConfig;
struct MeasurementResult;

/**
 * @brief 波形分析控制器类
 *
 * 负责处理波形分析的业务逻辑
 */
class WaveformAnalysisController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param view 关联的视图对象
     */
    explicit WaveformAnalysisController(WaveformAnalysisView* view);

    /**
     * @brief 析构函数
     */
    ~WaveformAnalysisController();

    /**
     * @brief 初始化控制器
     * 连接信号和槽，设置初始状态
     */
    bool initialize();

    /**
     * @brief 处理绘图事件
     * @param painter 绘图对象
     * @param chartRect 图表区域矩形
     */
    void handlePaintEvent(QPainter* painter, const QRect& chartRect);

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

public slots:
    /**
     * @brief 开始分析
     */
    void startAnalysis();

    /**
     * @brief 停止分析
     */
    void stopAnalysis();

    /**
     * @brief 执行测量
     */
    void performMeasurement();

    /**
     * @brief 导出数据
     */
    void exportData();

    /**
     * @brief 波形类型改变处理
     * @param index 组合框索引
     */
    void onWaveformTypeChanged(int index);

    /**
     * @brief 触发模式改变处理
     * @param index 组合框索引
     */
    void onTriggerModeChanged(int index);

    /**
     * @brief 缩放控制 - 放大
     */
    void onZoomInButtonClicked();

    /**
     * @brief 缩放控制 - 缩小
     */
    void onZoomOutButtonClicked();

    /**
     * @brief 缩放控制 - 重置
     */
    void onZoomResetButtonClicked();

    /**
     * @brief 自动缩放状态改变
     * @param checked 是否选中
     */
    void onAutoScaleChanged(bool checked);

    /**
     * @brief 水平滑块值改变
     * @param value 滑块值
     */
    void onHorizontalScaleChanged(int value);

    /**
     * @brief 垂直滑块值改变
     * @param value 滑块值
     */
    void onVerticalScaleChanged(int value);

    /**
     * @brief 采样率改变
     * @param value 采样率值
     */
    void onSampleRateChanged(int value);

    /**
     * @brief 窗口大小改变
     * @param value 窗口大小值
     */
    void onWindowSizeChanged(int value);

    /**
     * @brief 窗口类型改变
     * @param index 窗口类型索引
     */
    void onWindowTypeChanged(int index);

    /**
     * @brief 峰值检测状态改变
     * @param checked 是否选中
     */
    void onPeakDetectionChanged(bool checked);

    /**
     * @brief 噪声滤波状态改变
     * @param checked 是否选中
     */
    void onNoiseFilterChanged(bool checked);

    /**
     * @brief 自相关状态改变
     * @param checked 是否选中
     */
    void onAutoCorrelationChanged(bool checked);

    /**
     * @brief 显示网格状态改变
     * @param checked 是否选中
     */
    void onGridEnabledChanged(bool checked);

    /**
     * @brief 颜色主题改变
     * @param index 颜色主题索引
     */
    void onColorThemeChanged(int index);

    /**
     * @brief 刷新率改变
     * @param value 刷新率值
     */
    void onRefreshRateChanged(int value);

private slots:
    /**
     * @brief 配置变更处理
     * @param config 新的波形配置
     */
    void onConfigChanged(const WaveformConfig& config);

    /**
     * @brief 波形数据变更处理
     * @param xData 新的X轴数据
     * @param yData 新的Y轴数据
     */
    void onWaveformDataChanged(const QVector<double>& xData, const QVector<double>& yData);

    /**
     * @brief 测量结果变更处理
     * @param result 新的测量结果
     */
    void onMeasurementResultChanged(const MeasurementResult& result);

    /**
     * @brief 标记点变更处理
     * @param xData 标记点X坐标
     * @param yData 标记点Y坐标
     */
    void onMarkersChanged(const QVector<double>& xData, const QVector<double>& yData);

    /**
     * @brief 更新模拟数据
     * 用于演示和测试目的
     */
    void onUpdateSimulatedData();

private:
    /**
     * @brief 连接UI控件的信号与槽
     */
    void connectSignals();

    /**
     * @brief 更新UI状态
     * 根据当前配置更新组件状态
     */
    void updateUIState();

    /**
     * @brief 应用模型数据到UI
     */
    void applyModelToUI();

    /**
     * @brief 应用UI数据到模型
     */
    void applyUIToModel();

    /**
     * @brief 生成模拟波形数据
     * 用于演示和测试目的
     */
    void generateSimulatedData();

    /**
     * @brief 绘制波形
     * @param painter 绘图对象
     * @param rect 绘制区域矩形
     */
    void drawWaveform(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制网格
     * @param painter 绘图对象
     * @param rect 绘制区域矩形
     */
    void drawGrid(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制标尺
     * @param painter 绘图对象
     * @param rect 绘制区域矩形
     */
    void drawRulers(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制标记点
     * @param painter 绘图对象
     * @param rect 绘制区域矩形
     */
    void drawMarkers(QPainter* painter, const QRect& rect);

    /**
     * @brief 计算波形数据的显示转换
     * @param rect 显示区域矩形
     * @param xData X轴数据
     * @param yData Y轴数据
     * @param points 转换后的屏幕坐标点集
     */
    void calculateWaveformPoints(const QRect& rect,
        const QVector<double>& xData,
        const QVector<double>& yData,
        QVector<QPointF>& points);

    /**
     * @brief 更新测量结果文本显示
     * @param result 测量结果
     */
    void updateMeasurementDisplay(const MeasurementResult& result);

    /**
     * @brief 数据转换为波形颜色
     * 根据数据值和配置计算显示颜色
     * @param value 数据值
     * @return 对应的显示颜色
     */
    QColor getWaveformColor(double value);

private:
    WaveformAnalysisView* m_view;                                ///< 视图对象
    Ui::WaveformAnalysisClass* m_ui;                         ///< UI对象
    WaveformConfigModel* m_model;                            ///< 模型对象

    QTimer* m_simulationTimer;                               ///< 模拟数据定时器

    bool m_isInitialized;                                    ///< 初始化标志
    bool m_isBatchUpdate;                                    ///< 批量更新标志

    // 绘图相关变量
    double m_xMin;                                           ///< X轴最小值
    double m_xMax;                                           ///< X轴最大值
    double m_yMin;                                           ///< Y轴最小值
    double m_yMax;                                           ///< Y轴最大值
    QVector<QPointF> m_displayPoints;                        ///< 显示点集合
    QVector<QPointF> m_markerPoints;                         ///< 标记点集合
};