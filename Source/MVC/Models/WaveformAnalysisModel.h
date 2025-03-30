// Source/MVC/Models/WaveformAnalysisModel.h
#pragma once

#include <QObject>
#include <QVector>
#include <QMap>
#include <QColor>
#include <memory>

// 添加必要的前向声明
class DataAccessService;

/**
 * @brief 波形分析模型类
 *
 * 负责存储和管理波形数据和配置
 */
class WaveformAnalysisModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的指针
     */
    static WaveformAnalysisModel* getInstance();

    /**
     * @brief 设置数据访问服务
     * @param service 数据访问服务指针
     */
    void setDataAccessService(DataAccessService* service);

    /**
     * @brief 加载指定文件的数据
     * @param filename 文件名
     * @param startIndex 起始索引
     * @param length 数据长度
     * @return 是否加载成功
     */
    bool loadData(const QString& filename, int startIndex, int length);

    /**
     * @brief 异步加载波形数据
     * @param packetIndex 数据包索引
     * @return 是否开始加载
     */
    bool loadDataAsync(uint64_t packetIndex);

    /**
     * @brief 获取通道数据
     * @param channel 通道索引(0-3)
     * @return 通道数据向量
     */
    QVector<double> getChannelData(int channel) const;

    /**
     * @brief 获取索引数据
     * @return 索引数据向量
     */
    QVector<double> getIndexData() const;

    /**
     * @brief 获取可见数据范围
     * @param xMin 输出最小索引
     * @param xMax 输出最大索引
     */
    void getViewRange(double& xMin, double& xMax) const;

    /**
     * @brief 设置可见数据范围
     * @param xMin 最小索引
     * @param xMax 最大索引
     */
    void setViewRange(double xMin, double xMax);

    /**
     * @brief 获取标记点数据
     * @return 标记点索引向量
     */
    QVector<int> getMarkerPoints() const;

    /**
     * @brief 添加标记点
     * @param index 数据索引
     */
    void addMarkerPoint(int index);

    /**
     * @brief 移除标记点
     * @param index 数据索引
     */
    void removeMarkerPoint(int index);

    /**
     * @brief 清除标记点
     */
    void clearMarkerPoints();

    /**
     * @brief 获取当前显示的缩放级别
     * @return 缩放级别
     */
    double getZoomLevel() const;

    /**
     * @brief 设置缩放级别
     * @param level 新的缩放级别
     */
    void setZoomLevel(double level);

    /**
     * @brief 检查通道是否启用
     * @param channel 通道索引(0-3)
     * @return 是否启用
     */
    bool isChannelEnabled(int channel) const;

    /**
     * @brief 设置通道启用状态
     * @param channel 通道索引(0-3)
     * @param enabled 是否启用
     */
    void setChannelEnabled(int channel, bool enabled);

    /**
     * @brief 获取通道颜色
     * @param channel 通道索引(0-3)
     * @return 通道颜色
     */
    QColor getChannelColor(int channel) const;

    /**
     * @brief 设置通道颜色
     * @param channel 通道索引(0-3)
     * @param color 通道颜色
     */
    void setChannelColor(int channel, const QColor& color);

    /**
     * @brief 获取网格颜色
     * @return 网格颜色
     */
    QColor getGridColor() const;

    /**
     * @brief 设置网格颜色
     * @param color 网格颜色
     */
    void setGridColor(const QColor& color);

    /**
     * @brief 获取背景颜色
     * @return 背景颜色
     */
    QColor getBackgroundColor() const;

    /**
     * @brief 设置背景颜色
     * @param color 背景颜色
     */
    void setBackgroundColor(const QColor& color);

    /**
     * @brief 获取波形线宽
     * @return 波形线宽
     */
    float getWaveformLineWidth() const;

    /**
     * @brief 设置波形线宽
     * @param width 波形线宽
     */
    void setWaveformLineWidth(float width);

    /**
     * @brief 获取波形渲染模式
     * @return 渲染模式（0: 线条模式, 1: 填充模式）
     */
    int getWaveformRenderMode() const;

    /**
     * @brief 设置波形渲染模式
     * @param mode 渲染模式（0: 线条模式, 1: 填充模式）
     */
    void setWaveformRenderMode(int mode);

    /**
     * @brief 解析数据包成波形数据
     * @param data 原始二进制数据
     * @return 是否解析成功
     */
    bool parsePacketData(const QByteArray& data);

    /**
     * @brief 获取数据分析结果
     * @return 分析结果字符串
     */
    QString getDataAnalysisResult() const;

    /**
     * @brief 分析当前数据
     */
    void analyzeData();

    /**
     * @brief 更新通道数据
     * @param channel 通道索引
     * @param data 通道数据
     */
    void updateChannelData(int channel, const QVector<double>& data);

    /**
     * @brief 更新索引数据
     * @param data 索引数据
     */
    void updateIndexData(const QVector<double>& data);

signals:
    /**
     * @brief 数据加载完成信号
     * @param success 是否成功
     */
    void signal_WA_M_dataLoaded(bool success);

    /**
     * @brief 视图范围改变信号
     * @param xMin 最小索引
     * @param xMax 最大索引
     */
    void signal_WA_M_viewRangeChanged(double xMin, double xMax);

    /**
     * @brief 标记点改变信号
     */
    void signal_WA_M_markersChanged();

    /**
     * @brief 通道状态改变信号
     * @param channel 通道索引
     * @param enabled 是否启用
     */
    void signal_WA_M_channelStateChanged(int channel, bool enabled);

    /**
     * @brief 数据分析完成信号
     * @param result 分析结果
     */
    void signal_WA_M_dataAnalysisCompleted(const QString& result);

private:
    /**
     * @brief 构造函数（私有，用于单例模式）
     */
    WaveformAnalysisModel();

    /**
     * @brief 析构函数
     */
    ~WaveformAnalysisModel();

    // 禁用拷贝构造和赋值
    WaveformAnalysisModel(const WaveformAnalysisModel&) = delete;
    WaveformAnalysisModel& operator=(const WaveformAnalysisModel&) = delete;

    /**
     * @brief 初始化默认设置
     */
    void initializeDefaults();

    /**
     * @brief 处理从DataAccessService接收的数据
     * @param timestamp 时间戳
     * @param data 数据
     */
    void processReceivedData(uint64_t timestamp, const QByteArray& data);

    DataAccessService* m_dataService;          // 数据访问服务
    QVector<QVector<double>> m_channelData;    // 多通道数据
    QVector<double> m_indexData;               // 索引数据
    QVector<int> m_markerPoints;               // 标记点
    double m_xMin;                             // 视图最小索引
    double m_xMax;                             // 视图最大索引
    double m_zoomLevel;                        // 缩放级别
    QMap<int, bool> m_channelEnabled;          // 通道启用状态
    QMap<int, QColor> m_channelColors;         // 通道颜色
    QColor m_gridColor;                        // 网格颜色
    QColor m_backgroundColor;                  // 背景颜色
    float m_waveformLineWidth;                 // 波形线宽
    int m_waveformRenderMode;                  // 波形渲染模式
    QString m_dataAnalysisResult;              // 数据分析结果
    bool m_isLoading;                          // 是否正在加载数据
};