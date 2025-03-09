// Source/MVC/Models/WaveformConfigModel.h
#pragma once

#include <QObject>
#include <memory>
#include <QVector>
#include <QColor>

/**
 * @brief 波形模式枚举
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
 * @brief 测量结果数据结构
 */
struct MeasurementResult {
    double minValue = 0.0;           ///< 最小值
    double maxValue = 0.0;           ///< 最大值
    double avgValue = 0.0;           ///< 平均值
    double peakToPeak = 0.0;         ///< 峰峰值
    double frequency = 0.0;          ///< 估计频率
    double period = 0.0;             ///< 估计周期
    double rmsValue = 0.0;           ///< 均方根值
    double stdDeviation = 0.0;       ///< 标准差
    int zeroCrossings = 0;           ///< 过零点数
    int maxIndex = 0;                ///< 最大值索引
    int minIndex = 0;                ///< 最小值索引
    QString analysisTime;            ///< 分析时间戳
};

/**
 * @brief 波形配置数据结构
 */
struct WaveformConfig {
    WaveformMode waveformMode = WaveformMode::ANALOG;   ///< 波形模式
    TriggerMode triggerMode = TriggerMode::AUTO;        ///< 触发模式
    double sampleRate = 10000.0;                        ///< 采样率（Hz）
    double triggerLevel = 0.0;                          ///< 触发电平
    int triggerSlope = 0;                               ///< 触发边沿（0=上升，1=下降，2=双向）
    int preTriggerPercent = 20;                         ///< 预触发百分比
    int windowSize = 1024;                              ///< 窗口大小
    int windowType = 0;                                 ///< 窗口类型
    double zoomLevel = 1.0;                             ///< 缩放级别
    double timeBase = 1.0;                              ///< 时基
    double voltageScale = 1.0;                          ///< 电压刻度
    bool autoScale = true;                              ///< 自动缩放
    bool gridEnabled = true;                            ///< 显示网格
    int refreshRate = 10;                               ///< 刷新率（Hz）
    int colorTheme = 0;                                 ///< 颜色主题
    bool peakDetection = false;                         ///< 峰值检测
    bool noiseFilter = false;                           ///< 噪声滤波
    bool autoCorrelation = false;                       ///< 自相关
    bool isRunning = false;                             ///< 是否在运行中
};

/**
 * @brief 波形配置模型类
 *
 * 负责存储和管理波形数据和配置
 */
class WaveformConfigModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的共享指针
     */
    static WaveformConfigModel* getInstance();

    /**
     * @brief 获取波形配置
     * @return 当前波形配置
     */
    WaveformConfig getConfig() const;

    /**
     * @brief 设置波形配置
     * @param config 新的波形配置
     */
    void setConfig(const WaveformConfig& config);

    /**
     * @brief 获取波形数据（X值）
     * @return X轴数据向量
     */
    QVector<double> getXData() const;

    /**
     * @brief 获取波形数据（Y值）
     * @return Y轴数据向量
     */
    QVector<double> getYData() const;

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
     * @brief 清除波形数据
     */
    void clearData();

    /**
     * @brief 获取测量结果
     * @return 当前测量结果
     */
    MeasurementResult getMeasurementResult() const;

    /**
     * @brief 获取标记点（X坐标）
     * @return 标记点X坐标向量
     */
    QVector<double> getMarkerXData() const;

    /**
     * @brief 获取标记点（Y坐标）
     * @return 标记点Y坐标向量
     */
    QVector<double> getMarkerYData() const;

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
     * @param config 新的波形配置
     */
    void configChanged(const WaveformConfig& config);

    /**
     * @brief 波形数据变更信号
     * @param xData 新的X轴数据
     * @param yData 新的Y轴数据
     */
    void waveformDataChanged(const QVector<double>& xData, const QVector<double>& yData);

    /**
     * @brief 测量结果变更信号
     * @param result 新的测量结果
     */
    void measurementResultChanged(const MeasurementResult& result);

    /**
     * @brief 标记点变更信号
     * @param xData 标记点X坐标
     * @param yData 标记点Y坐标
     */
    void markersChanged(const QVector<double>& xData, const QVector<double>& yData);

private:
    /**
     * @brief 构造函数（私有，用于单例模式）
     */
    WaveformConfigModel();

    /**
     * @brief 析构函数
     */
    ~WaveformConfigModel();

    /**
     * @brief 创建默认配置
     * @return 默认波形配置
     */
    WaveformConfig createDefaultConfig();

    /**
     * @brief 更新测量结果
     * 根据当前波形数据计算并更新测量结果
     */
    void updateMeasurementResult();

    /**
     * @brief 更新标记点
     * 根据当前波形数据和设置更新标记点
     */
    void updateMarkers();

private:
    WaveformConfig m_config;                                ///< 当前配置
    QVector<double> m_xData;                                ///< X轴数据
    QVector<double> m_yData;                                ///< Y轴数据
    MeasurementResult m_measurementResult;                  ///< 测量结果
    QVector<double> m_markerXData;                          ///< 标记点X坐标
    QVector<double> m_markerYData;                          ///< 标记点Y坐标
};