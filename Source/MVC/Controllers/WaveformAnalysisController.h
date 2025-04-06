// Source/MVC/Controllers/WaveformAnalysisController.h
#pragma once

#include <QObject>
#include <QTimer>
#include <QRect>
#include <QPixmap>
#include <QPainter>
#include <QSharedPointer>
#include <QByteArray>
#include <memory>

class WaveformAnalysisView;
namespace Ui { class WaveformAnalysisClass; }
class WaveformAnalysisModel;
class DataAccessService;
class FileOperationController;
class WaveformGLWidget;

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
     * @return 初始化是否成功
     */
    bool initialize();

    /**
     * @brief 处理波形数据
     * @param data 原始二进制数据
     * @return 处理是否成功
     */
    bool processWaveformData(const QByteArray& data);

    /**
     * @brief 设置标签页可见性
     * @param visible 是否可见
     */
    void setTabVisible(bool visible);

    /**
     * @brief 设置文件操作控制器
     * @param controller 文件操作控制器
     */
    void setFileOperationController(FileOperationController* controller) {
        m_fileOperationController = controller;
    }

    /**
     * @brief 获取模型指针
     * @return 模型指针
     */
    WaveformAnalysisModel* getModel() const {
        return m_model;
    }

    /**
     * @brief 获取垂直缩放因子
     * @return 垂直缩放因子
     */
    double getVerticalScale() const {
        return m_verticalScale;
    }

public slots:
    /**
     * @brief 开始分析
     */
    void slot_WA_C_startAnalysis();

    /**
     * @brief 停止分析
     */
    void slot_WA_C_stopAnalysis();

    /**
     * @brief 放大
     */
    void slot_WA_C_zoomIn();

    /**
     * @brief 缩小
     */
    void slot_WA_C_zoomOut();

    /**
     * @brief 重置缩放
     */
    void slot_WA_C_zoomReset();

    /**
     * @brief 设置垂直缩放
     * @param scale 缩放因子
     */
    void slot_WA_C_setVerticalScale(double scale);

    /**
     * @brief 加载数据
     * @param startIndex 起始索引
     * @param length 数据长度
     * @return 加载是否成功
     */
    bool slot_WA_C_loadData(int startIndex, int length);

    /**
     * @brief 处理标签页激活
     */
    void slot_WA_C_handleTabActivated();

    /**
     * @brief 按需加载数据范围
     * @param startPos 起始位置
     * @param length 数据长度
     * @return 加载是否成功
     */
    bool slot_WA_C_loadDataRange(int startPos, int length);

private slots:
    /**
     * @brief 处理OpenGL控件视图范围变化
     * @param xMin 最小X值
     * @param xMax 最大X值
     */
    void onGLWidgetViewRangeChanged(double xMin, double xMax);

    /**
     * @brief 处理OpenGL控件添加标记点
     * @param index 数据索引
     */
    void onGLWidgetMarkerAdded(int index);

    /**
     * @brief 处理OpenGL控件平移请求
     * @param deltaX 水平偏移量
     */
    void onGLWidgetPanRequested(int deltaX);

    /**
     * @brief 处理OpenGL控件加载数据请求
     * @param startIndex 起始索引
     * @param length 数据长度
     */
    void onGLWidgetLoadDataRequested(int startIndex, int length);

    /**
     * @brief 模型数据加载完成
     * @param success 是否成功
     */
    void slot_WA_C_onDataLoaded(bool success);

    /**
     * @brief 视图范围改变
     * @param xMin 最小索引
     * @param xMax 最大索引
     */
    void slot_WA_C_onViewRangeChanged(double xMin, double xMax);

    /**
     * @brief 标记点改变
     */
    void slot_WA_C_onMarkersChanged();

    /**
     * @brief 通道状态改变
     * @param channel 通道索引
     * @param enabled 是否启用
     */
    void slot_WA_C_onChannelStateChanged(int channel, bool enabled);

    /**
     * @brief 更新定时器触发
     */
    void slot_WA_C_onUpdateTimerTriggered();

private:
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 从服务中加载波形数据
     * @param startIndex 起始索引
     * @param length 数据长度
     * @return 加载是否成功
     */
    bool loadWaveformDataFromService(int startIndex, int length);

    /**
     * @brief 判断是否需要加载更多数据
     * @param xMin 最小索引
     * @param xMax 最大索引
     * @return 是否需要加载
     */
    bool shouldLoadMoreData(double xMin, double xMax);

    /**
     * @brief 更新数据缓存
     * @param startIndex 起始索引
     * @param length 数据长度
     */
    void updateDataCache(int startIndex, int length);

    /**
     * @brief 确保通道数据和索引数据长度匹配
     */
    void ensureDataConsistency();

    /**
     * @brief 生成模拟测试数据
     */
    void generateMockData();

private:
    WaveformAnalysisView* m_view;                   ///< 视图对象
    Ui::WaveformAnalysisClass* m_ui;                ///< UI对象
    WaveformAnalysisModel* m_model;                 ///< 模型对象
    DataAccessService* m_dataService;               ///< 数据访问服务
    FileOperationController* m_fileOperationController{ nullptr }; ///< 文件操作控制器
    WaveformGLWidget* m_glWidget;                   ///< OpenGL波形控件

    QTimer* m_updateTimer;                          ///< 更新定时器
    bool m_isRunning;                               ///< 是否正在运行
    bool m_isInitialized;                           ///< 是否已初始化
    double m_verticalScale;                         ///< 垂直缩放因子
    bool m_autoScale;                               ///< 自动缩放

    bool m_isActive{ false };                       ///< 当前标签页是否激活
    bool m_isCurrentlyVisible{ false };             ///< 当前标签页是否可见
    int m_viewWidth{ 1000 };                        ///< 当前视图宽度(数据点)
    int m_currentPosition{ 0 };                     ///< 当前视图起始位置

    // 缓存相关
    QSharedPointer<QByteArray> m_dataPacketCache;   ///< 数据包缓存
    int m_cacheStartIndex;                          ///< 缓存起始索引
    int m_cacheLength;                              ///< 缓存长度
    bool m_isCacheValid;                            ///< 缓存是否有效
};