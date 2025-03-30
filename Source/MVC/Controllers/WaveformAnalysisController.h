// Source/MVC/Controllers/WaveformAnalysisController.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QPainter>
#include <memory>

class WaveformAnalysisView;
namespace Ui { class WaveformAnalysisClass; }
class WaveformAnalysisModel;
class DataAccessService;

/**
 * @brief 波形分析控制器类
 *
 * 负责处理波形分析的业务逻辑和视图更新
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
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 处理绘图事件
     * @param painter 绘图对象
     * @param chartRect 图表区域矩形
     */
    void handlePaintEvent(QPainter* painter, const QRect& chartRect);

    /**
     * @brief 处理水平平移操作
     * @param deltaX 水平偏移像素
     */
    void handlePanDelta(int deltaX);

    /**
     * @brief 在指定位置添加标记点
     * @param pos 鼠标位置
     */
    void addMarkerAtPosition(const QPoint& pos);

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
    // void performMeasurement();

    /**
     * @brief 导出数据
     */
    // void exportData();

    /**
     * @brief 放大
     */
    void zoomIn();

    /**
     * @brief 缩小
     */
    void zoomOut();

    /**
     * @brief 重置缩放
     */
    void zoomReset();

    /**
     * @brief 在指定点放大
     * @param pos 鼠标位置
     */
    void zoomInAtPoint(const QPoint& pos);

    /**
     * @brief 在指定点缩小
     * @param pos 鼠标位置
     */
    void zoomOutAtPoint(const QPoint& pos);

    /**
     * @brief 设置水平缩放
     * @param value 滑块值
     */
    // void setHorizontalScale(int value);

    /**
     * @brief 设置垂直缩放
     * @param value 滑块值
     */
    // void setVerticalScale(int value);

    /**
     * @brief 设置自动缩放
     * @param enabled 是否启用
     */
    // void setAutoScale(bool enabled);

    /**
     * @brief 加载数据
     * @param filename 文件名
     * @param startIndex 起始索引
     * @param length 数据长度
     */
    void loadData(const QString& filename, int startIndex, int length);

    /**
     * @brief 加载数据包
     * @param packetIndex 数据包索引
     */
    // void loadDataPacket(uint64_t packetIndex);

private slots:
    /**
     * @brief 模型数据加载完成
     * @param success 是否成功
     */
    void onDataLoaded(bool success);

    /**
     * @brief 视图范围改变
     * @param xMin 最小索引
     * @param xMax 最大索引
     */
    void onViewRangeChanged(double xMin, double xMax);

    /**
     * @brief 标记点改变
     */
    void onMarkersChanged();

    /**
     * @brief 通道状态改变
     * @param channel 通道索引
     * @param enabled 是否启用
     */
    void onChannelStateChanged(int channel, bool enabled);

    /**
     * @brief 数据分析完成
     * @param result 分析结果
     */
    // void onDataAnalysisCompleted(const QString& result);

    /**
     * @brief 更新定时器触发
     */
    void onUpdateTimerTriggered();

private:
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 绘制背景和框架
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawBackground(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制网格
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawGrid(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制标尺
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawRulers(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制通道标签
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawChannelLabels(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制波形数据
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawWaveforms(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制标记点
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawMarkers(QPainter* painter, const QRect& rect);

    /**
     * @brief 绘制光标和辅助信息
     * @param painter 绘图对象
     * @param rect 区域矩形
     */
    void drawCursor(QPainter* painter, const QRect& rect);

    /**
     * @brief 将数据索引转换为屏幕坐标
     * @param index 数据索引
     * @param rect 绘制区域
     * @return 屏幕X坐标
     */
    int dataToScreenX(double index, const QRect& rect);

    /**
     * @brief 将屏幕坐标转换为数据索引
     * @param x 屏幕X坐标
     * @param rect 绘制区域
     * @return 数据索引
     */
    double screenToDataX(int x, const QRect& rect);

    /**
     * @brief 更新UI状态
     */
    void updateUIState();

private:
    WaveformAnalysisView* m_view;                 ///< 视图对象
    Ui::WaveformAnalysisClass* m_ui;              ///< UI对象
    WaveformAnalysisModel* m_model;               ///< 模型对象
    DataAccessService* m_dataService;             ///< 数据访问服务

    QTimer* m_updateTimer;                        ///< 更新定时器
    bool m_isRunning;                             ///< 是否正在运行
    bool m_isInitialized;                         ///< 是否已初始化
    double m_verticalScale;                       ///< 垂直缩放因子
    bool m_autoScale;                             ///< 自动缩放
    QPoint m_cursorPosition;                      ///< 光标位置
    bool m_isCursorVisible;                       ///< 光标是否可见

    QRect m_lastChartRect;                        ///< 上次图表区域
};