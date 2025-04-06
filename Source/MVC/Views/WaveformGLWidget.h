// Source/MVC/Views/WaveformGLWidget.h
#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QMap>

class WaveformAnalysisModel;

/**
 * @brief OpenGL波形绘制控件
 *
 * 专注于波形渲染，处理与OpenGL相关的所有操作
 */
class WaveformGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口
     */
    explicit WaveformGLWidget(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~WaveformGLWidget();

    /**
     * @brief 设置模型
     * @param model 波形分析模型指针
     */
    void setModel(WaveformAnalysisModel* model);

    /**
     * @brief 请求更新波形
     */
    void requestUpdate();

    /**
     * @brief 设置数据范围
     * @param xMin 最小X值
     * @param xMax 最大X值
     */
    void setViewRange(double xMin, double xMax);

    /**
     * @brief 设置垂直缩放因子
     * @param scale 缩放因子
     */
    void setVerticalScale(double scale);

    /**
     * @brief 获取当前数据范围
     * @param xMin 最小X值的引用
     * @param xMax 最大X值的引用
     */
    void getViewRange(double& xMin, double& xMax) const;

    /**
     * @brief 添加标记点
     * @param position 位置
     */
    void addMarker(const QPoint& position);

    /**
     * @brief 将数据坐标转换为屏幕坐标
     * @param index 数据索引
     * @return 屏幕坐标
     */
    int dataToScreenX(double index) const;

    /**
     * @brief 将屏幕坐标转换为数据坐标
     * @param x 屏幕X坐标
     * @return 数据索引
     */
    double screenToDataX(int x) const;

signals:
    /**
     * @brief 视图范围变化信号
     * @param xMin 最小X值
     * @param xMax 最大X值
     */
    void viewRangeChanged(double xMin, double xMax);

    /**
     * @brief 请求添加标记点信号
     * @param index 数据索引
     */
    void markerAdded(int index);

    /**
     * @brief 请求平移信号
     * @param deltaX 水平偏移量
     */
    void panRequested(int deltaX);

    /**
     * @brief 请求加载数据范围
     * @param startIndex 起始索引
     * @param length 数据长度
     */
    void loadDataRequested(int startIndex, int length);

protected:
    /**
     * @brief 初始化OpenGL
     */
    void initializeGL() override;

    /**
     * @brief 调整大小事件
     * @param w 宽度
     * @param h 高度
     */
    void resizeGL(int w, int h) override;

    /**
     * @brief 绘制OpenGL场景
     */
    void paintGL() override;

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

private:
    /**
     * @brief 更新网格顶点数据
     */
    void updateGridVertices();

    /**
     * @brief 绘制网格
     */
    void drawGrid();

    /**
     * @brief 创建默认网格
     */
    void createDefaultGrid();

    /**
     * @brief 更新波形数据到OpenGL
     */
    void updateWaveformData();

    /**
     * @brief 使用OpenGL绘制波形
     */
    void drawWaveformsGL();

    /**
     * @brief 绘制标记点
     */
    void drawMarkers();

    /**
     * @brief 将数据坐标转换为归一化OpenGL坐标
     */
    float normalizeX(double x) const;

    /**
     * @brief 将屏幕Y坐标转换为归一化OpenGL坐标
     */
    float normalizeY(int y) const;

    /**
     * @brief 计算合适的网格间距
     * @param range 数据范围
     * @return 网格间距
     */
    double calculateGridStep(double range) const;

private:
    QOpenGLShaderProgram* m_program = nullptr;        ///< OpenGL着色器程序
    QOpenGLBuffer* m_vertexBuffer = nullptr;          ///< 顶点缓冲区
    QOpenGLBuffer* m_colorBuffer = nullptr;           ///< 颜色缓冲区

    QMap<int, QVector<QVector2D>> m_vertexData;       ///< 通道顶点数据
    QMap<int, QVector<QVector3D>> m_colorData;        ///< 通道颜色数据

    WaveformAnalysisModel* m_model = nullptr;         ///< 波形模型指针

    bool m_needsUpdate = true;                        ///< 是否需要更新
    bool m_isDragging = false;                        ///< 是否正在拖动
    QPoint m_lastMousePos;                            ///< 上次鼠标位置
    int m_frameCount = 0;                             ///< 帧计数器
    double m_verticalScale = 1.0;                     ///< 垂直缩放因子
    double m_viewXMin = 0.0;                          ///< 当前视图最小X值
    double m_viewXMax = 100.0;                        ///< 当前视图最大X值

    // 网格相关成员
    QOpenGLShaderProgram* m_gridProgram = nullptr;    ///< 网格着色器程序
    QOpenGLBuffer* m_gridBuffer = nullptr;            ///< 网格顶点缓冲区
    QOpenGLBuffer* m_gridColorBuffer = nullptr;       ///< 网格颜色缓冲区
    QVector<QVector2D> m_gridVertices;                ///< 网格顶点数据
    QVector<QVector3D> m_gridColors;                  ///< 网格颜色数据

    // 视口和坐标转换
    int m_viewportWidth = 0;                          ///< 视口宽度
    int m_viewportHeight = 0;                         ///< 视口高度
};