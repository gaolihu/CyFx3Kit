// WaveformAnalysis.h
#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QSplitter>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QFileDialog>
#include <QTextEdit>
#include <QTimer>

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QScatterSeries>
#endif

/**
 * @brief 波形分析模式枚举
 */
enum class WaveformMode {
    ANALOG,     ///< 模拟波形
    DIGITAL,    ///< 数字波形
    MIXED       ///< 混合波形
};

/**
 * @brief 触发模式枚举
 */
enum class TriggerMode {
    AUTO,       ///< 自动触发
    NORMAL,     ///< 普通触发
    SINGLE      ///< 单次触发
};

/**
 * @brief 波形分析窗口类
 *
 * 负责信号波形显示和分析功能，支持多种分析模式和参数设置
 */
class WaveformAnalysis : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit WaveformAnalysis(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~WaveformAnalysis();

    /**
     * @brief 设置波形数据
     * @param xData X轴数据
     * @param yData Y轴数据
     */
    void setWaveformData(const QVector<double>& xData, const QVector<double>& yData);

    /**
     * @brief 添加波形数据点
     * @param x X轴数据点
     * @param y Y轴数据点
     */
    void addDataPoint(double x, double y);

    /**
     * @brief 清除波形数据
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

public slots:
    /**
     * @brief 开始分析
     */
    void startAnalysis();

    /**
     * @brief 停止分析
     */
    void stopAnalysis();

private slots:
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
     * @brief 导出按钮点击处理
     */
    void onExportButtonClicked();

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
     * @brief 测量按钮点击处理
     */
    void onMeasureButtonClicked();

    /**
     * @brief 设置按钮点击处理
     */
    void onSettingsButtonClicked();

    /**
     * @brief 自动缩放状态改变
     * @param checked 是否选中
     */
    void onAutoScaleChanged(bool checked);

    /**
     * @brief 更新模拟数据（测试用）
     */
    void onUpdateSimulatedData();

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

    /**
     * @brief 创建工具栏
     * @return 工具栏控件
     */
    QWidget* createToolBar();

    /**
     * @brief 创建波形显示区域
     * @return 波形显示区域控件
     */
    QWidget* createWaveformDisplay();

    /**
     * @brief 创建分析控制面板
     * @return 分析控制面板控件
     */
    QWidget* createAnalysisPanel();

    /**
     * @brief 创建测量结果面板
     * @return 测量结果面板控件
     */
    QWidget* createMeasurementsPanel();

    /**
     * @brief 更新波形显示
     */
    void updateWaveformDisplay();

    /**
     * @brief 更新标记
     */
    void updateMarkers();

    /**
     * @brief 计算波形统计信息
     */
    void calculateStatistics();

    /**
     * @brief 生成模拟数据（测试用）
     */
    void generateSimulatedData();

private:
    // UI组件
    QSplitter* m_mainSplitter;         ///< 主分割器
    QTabWidget* m_rightTabWidget;      ///< 右侧标签页控件

    QComboBox* m_waveformTypeCombo;    ///< 波形类型组合框
    QComboBox* m_triggerModeCombo;     ///< 触发模式组合框
    QSpinBox* m_sampleRateSpin;        ///< 采样率设置
    QCheckBox* m_autoscaleCheck;       ///< 自动缩放复选框

    QPushButton* m_startButton;        ///< 开始按钮
    QPushButton* m_stopButton;         ///< 停止按钮
    QPushButton* m_exportButton;       ///< 导出按钮
    QPushButton* m_measureButton;      ///< 测量按钮
    QPushButton* m_settingsButton;     ///< 设置按钮

    QPushButton* m_zoomInButton;       ///< 放大按钮
    QPushButton* m_zoomOutButton;      ///< 缩小按钮
    QPushButton* m_zoomResetButton;    ///< 重置缩放按钮

    QSlider* m_horizontalScaleSlider;  ///< 水平缩放滑块
    QSlider* m_verticalScaleSlider;    ///< 垂直缩放滑块

    QTextEdit* m_resultsTextEdit;      ///< 结果文本框

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    QChartView* m_chartView;           ///< 图表视图
    QChart* m_chart;                   ///< 图表对象
    QLineSeries* m_mainSeries;         ///< 主数据系列
    QScatterSeries* m_markerSeries;    ///< 标记点系列
    QValueAxis* m_axisX;               ///< X轴
    QValueAxis* m_axisY;               ///< Y轴
#else
    QLabel* m_noChartLabel;            ///< 无图表支持提示标签
#endif

    // 数据相关
    QVector<double> m_xData;           ///< X轴数据
    QVector<double> m_yData;           ///< Y轴数据
    double m_sampleRate;               ///< 当前采样率

    // 状态相关
    bool m_isRunning;                  ///< 是否在运行
    double m_zoomLevel;                ///< 当前缩放级别
    WaveformMode m_waveformMode;       ///< 当前波形模式
    TriggerMode m_triggerMode;         ///< 当前触发模式

    // 定时器（用于模拟数据生成）
    QTimer* m_simulationTimer;         ///< 模拟数据定时器
};