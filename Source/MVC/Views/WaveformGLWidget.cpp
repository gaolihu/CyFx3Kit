// Source/MVC/Views/WaveformGLWidget.cpp
#include "WaveformGLWidget.h"
#include "WaveformAnalysisModel.h"
#include "Logger.h"
#include <QMatrix4x4>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <algorithm>

WaveformGLWidget::WaveformGLWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    // 设置OpenGL格式
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4); // 启用多重采样抗锯齿
    setFormat(format);

    // 基本设置
    setAutoFillBackground(false);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 确保控件可见
    setVisible(true);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("创建OpenGL波形控件"));
}

WaveformGLWidget::~WaveformGLWidget()
{
    // 确保当前线程拥有OpenGL上下文
    if (QOpenGLContext::currentContext() != context()) {
        makeCurrent();
    }

    // 按照依赖顺序释放资源
    if (m_vertexBuffer) {
        m_vertexBuffer->destroy();
        delete m_vertexBuffer;
        m_vertexBuffer = nullptr;
    }

    if (m_colorBuffer) {
        m_colorBuffer->destroy();
        delete m_colorBuffer;
        m_colorBuffer = nullptr;
    }

    if (m_program) {
        delete m_program;
        m_program = nullptr;
    }

    // 释放网格绘制相关资源
    if (m_gridBuffer) {
        m_gridBuffer->destroy();
        delete m_gridBuffer;
        m_gridBuffer = nullptr;
    }

    if (m_gridColorBuffer) {
        m_gridColorBuffer->destroy();
        delete m_gridColorBuffer;
        m_gridColorBuffer = nullptr;
    }

    if (m_gridProgram) {
        delete m_gridProgram;
        m_gridProgram = nullptr;
    }

    doneCurrent();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL波形控件已销毁"));
}

void WaveformGLWidget::setModel(WaveformAnalysisModel* model)
{
    m_model = model;
    if (m_model) {
        // 获取初始视图范围
        m_model->getViewRange(m_viewXMin, m_viewXMax);
    }
    requestUpdate();
}

void WaveformGLWidget::requestUpdate()
{
    m_needsUpdate = true;
    update();
}

void WaveformGLWidget::setViewRange(double xMin, double xMax)
{
    if (m_viewXMin != xMin || m_viewXMax != xMax) {
        m_viewXMin = xMin;
        m_viewXMax = xMax;
        m_needsUpdate = true;
        update();
    }
}

void WaveformGLWidget::getViewRange(double& xMin, double& xMax) const
{
    xMin = m_viewXMin;
    xMax = m_viewXMax;
}

void WaveformGLWidget::setVerticalScale(double scale)
{
    if (scale > 0 && m_verticalScale != scale) {
        m_verticalScale = scale;
        m_needsUpdate = true;
        update();
    }
}

int WaveformGLWidget::dataToScreenX(double index) const
{
    // 检查范围有效性
    if (!std::isfinite(m_viewXMin) || !std::isfinite(m_viewXMax) || m_viewXMin >= m_viewXMax) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("视图范围异常: xMin=%1, xMax=%2, 使用默认映射").arg(m_viewXMin).arg(m_viewXMax));
        return static_cast<int>((index)*width() / 100.0);
    }

    // 确保索引在有效范围内
    if (index < m_viewXMin) {
        index = m_viewXMin;
    }
    else if (index > m_viewXMax) {
        index = m_viewXMax;
    }

    // 确保不会除以零
    double range = m_viewXMax - m_viewXMin;
    if (range <= 0) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("视图范围无效: xMin=%1, xMax=%2").arg(m_viewXMin).arg(m_viewXMax));
        return 0;
    }

    // 线性映射数据索引到屏幕坐标
    double relativePosition = (index - m_viewXMin) / range;
    int screenX = static_cast<int>(relativePosition * width());

    // 确保返回合理的屏幕坐标
    screenX = qBound(0, screenX, width() - 1);
    return screenX;
}

double WaveformGLWidget::screenToDataX(int x) const
{
    // 防止除以零
    if (width() <= 0) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("绘制区域宽度为零或负值"));
        return m_viewXMin;
    }

    // 确保x在绘制区域内
    x = qBound(0, x, width() - 1);

    // 线性映射屏幕坐标到数据索引
    double relativePosition = static_cast<double>(x) / width();
    double dataIndex = m_viewXMin + relativePosition * (m_viewXMax - m_viewXMin);

    return dataIndex;
}

void WaveformGLWidget::slot_WF_GL_handleMousePress(const QPoint& pos, Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        m_lastMousePos = pos;
        m_isDragging = true;
        setCursor(Qt::ClosedHandCursor);
    }

    // 记录事件处理
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件处理鼠标按下 - 位置: (%1, %2), 按钮: %3")
        .arg(pos.x()).arg(pos.y()).arg(static_cast<int>(button)));
}

void WaveformGLWidget::slot_WF_GL_handleMouseMove(const QPoint& pos, Qt::MouseButtons buttons)
{
    if (m_isDragging) {
        QPoint delta = pos - m_lastMousePos;
        m_lastMousePos = pos;

        if (!delta.isNull()) {
            // 处理平移
            slot_WF_GL_handlePanEvent(delta);
        }
    }

    // 记录事件处理
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件处理鼠标移动 - 位置: (%1, %2), 按钮: %3")
        .arg(pos.x()).arg(pos.y()).arg(static_cast<int>(buttons)));
}

void WaveformGLWidget::slot_WF_GL_handleMouseRelease(const QPoint& pos, Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);
    }

    // 记录事件处理
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件处理鼠标释放 - 位置: (%1, %2), 按钮: %3")
        .arg(pos.x()).arg(pos.y()).arg(static_cast<int>(button)));
}

void WaveformGLWidget::slot_WF_GL_handleMouseDoubleClick(const QPoint& pos, Qt::MouseButton button)
{
    if (button == Qt::LeftButton) {
        // 添加标记点
        addMarker(pos);
    }

    // 记录事件处理
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件处理鼠标双击 - 位置: (%1, %2), 按钮: %3")
        .arg(pos.x()).arg(pos.y()).arg(static_cast<int>(button)));
}

void WaveformGLWidget::slot_WF_GL_handleWheelScroll(const QPoint& pos, const QPoint& angleDelta)
{
    // 内部处理滚轮事件
    slot_WF_GL_handleWheelEvent(pos, angleDelta);

    // 记录事件处理
    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL控件处理滚轮事件 - 位置: (%1, %2), 角度: (%3, %4)")
        .arg(pos.x()).arg(pos.y()).arg(angleDelta.x()).arg(angleDelta.y()));
}

// 添加内部事件处理方法

void WaveformGLWidget::slot_WF_GL_handleWheelEvent(const QPoint& pos, const QPoint& angleDelta)
{
    if (!m_model)
        return;

    // 获取当前视图范围
    double xMin = m_viewXMin;
    double xMax = m_viewXMax;
    double range = xMax - xMin;

    // 鼠标位置对应的数据点
    double clickedIndex = screenToDataX(pos.x());
    double ratio = (clickedIndex - xMin) / range;

    if (angleDelta.y() > 0) {
        // 放大 - 缩小范围为原来的80%
        double newRange = range * 0.8;

        // 保持鼠标所在数据点的相对位置不变
        double newXMin = clickedIndex - ratio * newRange;
        double newXMax = newXMin + newRange;

        // 边界检查
        if (newXMin < 0) {
            newXMin = 0;
            newXMax = newXMin + newRange;
        }

        // 设置新范围
        setViewRange(newXMin, newXMax);

        // 通知视图范围变更
        emit signal_WF_GL_viewRangeChanged(newXMin, newXMax);
    }
    else {
        // 缩小 - 放大范围为原来的125%
        double newRange = range * 1.25;

        // 保持鼠标所在数据点的相对位置不变
        double newXMin = clickedIndex - ratio * newRange;
        double newXMax = newXMin + newRange;

        // 边界检查
        if (newXMin < 0) {
            newXMin = 0;
            newXMax = newXMin + newRange;
        }

        // 设置新范围
        setViewRange(newXMin, newXMax);

        // 通知视图范围变更
        emit signal_WF_GL_viewRangeChanged(newXMin, newXMax);
    }
}

void WaveformGLWidget::slot_WF_GL_handlePanEvent(const QPoint& delta)
{
    // 通知控制器处理平移
    emit signal_WF_GL_panRequested(delta.x());

    LOG_INFO(LocalQTCompat::fromLocal8Bit("发送平移请求 - 水平偏移: %1")
        .arg(delta.x()));
}

void WaveformGLWidget::addMarker(const QPoint& position)
{
    // 将屏幕坐标转换为数据索引
    double dataIndex = screenToDataX(position.x());

    // 四舍五入到最近的整数
    int index = qRound(dataIndex);

    // 发出标记添加请求
    emit signal_WF_GL_markerAdded(index);
}

void WaveformGLWidget::initializeGL()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("初始化OpenGL环境"));
    initializeOpenGLFunctions();

    // 确保初始化成功
    if (!isValid()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("OpenGL初始化失败，上下文无效"));
        return;
    }

    // 设置白色背景
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // 启用深度测试和抗锯齿
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    // 创建波形着色器程序
    m_program = new QOpenGLShaderProgram();

    // 着色器源码
    const char* vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec2 position;\n"
        "layout (location = 1) in vec3 color;\n"
        "out vec3 vertexColor;\n"
        "uniform mat4 mvp;\n"
        "void main() {\n"
        "    gl_Position = mvp * vec4(position, 0.0, 1.0);\n"
        "    vertexColor = color;\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 330 core\n"
        "in vec3 vertexColor;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(vertexColor, 1.0);\n"
        "}\n";

    // 编译和链接着色器
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_program->link();

    // 创建顶点和颜色缓冲区
    m_vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_vertexBuffer->create();
    m_vertexBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_colorBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_colorBuffer->create();
    m_colorBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // 创建网格着色器程序
    m_gridProgram = new QOpenGLShaderProgram();
    m_gridProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    m_gridProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    m_gridProgram->link();

    // 创建网格缓冲区
    m_gridBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_gridBuffer->create();
    m_gridBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    m_gridColorBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_gridColorBuffer->create();
    m_gridColorBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);

    // 初始化默认网格
    createDefaultGrid();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("OpenGL环境初始化完成"));
}

void WaveformGLWidget::createDefaultGrid()
{
    // 清空现有数据
    m_gridVertices.clear();
    m_gridColors.clear();

    // 设置默认网格颜色
    QVector3D gridColor(0.8f, 0.8f, 0.9f);

    // 添加垂直网格线 - 10条等分线
    for (int i = 0; i <= 10; i++) {
        float x = -1.0f + i * 0.2f;
        m_gridVertices.append(QVector2D(x, -1.0f));
        m_gridVertices.append(QVector2D(x, 1.0f));

        m_gridColors.append(gridColor);
        m_gridColors.append(gridColor);
    }

    // 添加水平网格线 - 4个通道分割线
    for (int i = 0; i <= 4; i++) {
        float y = -1.0f + i * 0.5f;
        m_gridVertices.append(QVector2D(-1.0f, y));
        m_gridVertices.append(QVector2D(1.0f, y));

        m_gridColors.append(gridColor);
        m_gridColors.append(gridColor);
    }
}

void WaveformGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    m_viewportWidth = w;
    m_viewportHeight = h;
    m_needsUpdate = true;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("调整OpenGL视图大小: %1 x %2").arg(w).arg(h));
}

void WaveformGLWidget::paintGL()
{
    // 清除背景
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 使用 QPainter 代替直接 OpenGL 调用
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制背景和网格
    painter.fillRect(rect(), QColor(240, 240, 250));

    // 绘制网格线
    painter.setPen(QPen(QColor(200, 200, 220), 1));
    for (int i = 0; i <= 10; i++) {
        int x = width() * i / 10;
        painter.drawLine(x, 0, x, height());
    }

    for (int i = 0; i <= 4; i++) {
        int y = height() * i / 4;
        painter.drawLine(0, y, width(), y);
    }

    // 绘制通道数据（将原来的 OpenGL 绘制转换为 QPainter 绘制）
    if (m_model) {
        for (int ch = 0; ch < 4; ++ch) {
            if (!m_model->isChannelEnabled(ch))
                continue;

            QVector<double> data = m_model->getChannelData(ch);
            QVector<double> indexData = m_model->getIndexData();

            if (data.isEmpty() || indexData.isEmpty())
                continue;

            // 设置通道颜色
            QColor channelColor;
            switch (ch) {
            case 0: channelColor = Qt::red; break;
            case 1: channelColor = Qt::blue; break;
            case 2: channelColor = Qt::green; break;
            case 3: channelColor = Qt::magenta; break;
            }

            painter.setPen(QPen(channelColor, 2));

            // 计算区域和比例
            int channelHeight = height() / 4;
            int midY = ch * channelHeight + channelHeight / 2;

            // 绘制波形数据
            QPainterPath path;
            bool started = false;

            for (int i = 0; i < data.size() && i < indexData.size(); ++i) {
                double index = indexData[i];
                double value = data[i];

                // 映射到屏幕坐标
                double xRatio = (index - m_viewXMin) / (m_viewXMax - m_viewXMin);
                int x = qRound(xRatio * width());
                int y = midY - (value > 0.5 ? channelHeight / 4 : -channelHeight / 4);

                if (!started) {
                    path.moveTo(x, y);
                    started = true;
                }
                else {
                    path.lineTo(x, y);
                }
            }

            painter.drawPath(path);
        }
    }
    else {
        // 显示提示信息
        painter.setPen(Qt::darkGray);
        painter.drawText(rect(), Qt::AlignCenter, "无波形数据或模型未初始化");
    }

    // 绘制调试信息
    painter.setPen(Qt::black);
    painter.drawText(10, 20, QString("尺寸: %1 x %2").arg(width()).arg(height()));
}

void WaveformGLWidget::updateGridVertices()
{
    if (!m_model)
        return;

    m_gridVertices.clear();
    m_gridColors.clear();

    // 获取网格颜色
    QColor gridColor = m_model->getGridColor();
    float r = gridColor.redF();
    float g = gridColor.greenF();
    float b = gridColor.blueF();
    QVector3D colorVec(r, g, b);

    // 计算合适的网格间距
    double dataRange = m_viewXMax - m_viewXMin;
    double gridStep = calculateGridStep(dataRange);

    // 计算第一条垂直网格线位置
    double firstGrid = std::ceil(m_viewXMin / gridStep) * gridStep;

    // 添加垂直网格线
    for (double x = firstGrid; x <= m_viewXMax; x += gridStep) {
        float normalizedX = normalizeX(x);

        m_gridVertices.append(QVector2D(normalizedX, -1.0f));
        m_gridVertices.append(QVector2D(normalizedX, 1.0f));

        m_gridColors.append(colorVec);
        m_gridColors.append(colorVec);
    }

    // 添加水平网格线（通道分隔线）
    for (int i = 1; i < 4; i++) {
        float y = -1.0f + 2.0f * i / 4.0f;

        m_gridVertices.append(QVector2D(-1.0f, y));
        m_gridVertices.append(QVector2D(1.0f, y));

        m_gridColors.append(colorVec);
        m_gridColors.append(colorVec);
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新网格顶点，共 %1 个顶点").arg(m_gridVertices.size()));
}

void WaveformGLWidget::drawGrid()
{
    if (m_gridVertices.isEmpty() || !m_gridProgram || !m_gridBuffer || !m_gridColorBuffer)
        return;

    // 设置线宽
    glLineWidth(1.0f);

    // 使用网格着色器程序
    m_gridProgram->bind();

    // 设置MVP矩阵
    QMatrix4x4 projection;
    projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    m_gridProgram->setUniformValue("mvp", projection);

    // 更新顶点缓冲
    m_gridBuffer->bind();
    m_gridBuffer->allocate(m_gridVertices.constData(), static_cast<int>(m_gridVertices.size() * sizeof(QVector2D)));

    // 设置顶点属性
    m_gridProgram->enableAttributeArray(0);
    m_gridProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2);

    // 更新并设置颜色缓冲
    m_gridColorBuffer->bind();
    m_gridColorBuffer->allocate(m_gridColors.constData(), static_cast<int>(m_gridColors.size() * sizeof(QVector3D)));

    m_gridProgram->enableAttributeArray(1);
    m_gridProgram->setAttributeBuffer(1, GL_FLOAT, 0, 3);

    // 绘制网格线
    glDrawArrays(GL_LINES, 0, m_gridVertices.size());

    // 禁用顶点属性
    m_gridProgram->disableAttributeArray(0);
    m_gridProgram->disableAttributeArray(1);

    // 解绑着色器程序
    m_gridProgram->release();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("绘制网格线，共 %1 条线段").arg(m_gridVertices.size() / 2));
}

void WaveformGLWidget::updateWaveformData()
{
    if (!m_model)
        return;

    // 清除之前的数据
    for (int ch = 0; ch < 4; ++ch) {
        m_vertexData[ch].clear();
        m_colorData[ch].clear();
    }

    // 为每个通道准备数据
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_model->isChannelEnabled(ch))
            continue;

        QVector<double> data = m_model->getChannelData(ch);
        QVector<double> indexData = m_model->getIndexData();

        if (data.isEmpty() || indexData.isEmpty()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("通道：%1没有数据").arg(ch));
            continue;
        }

        // 确保索引数据和通道数据长度一致
        if (indexData.size() != data.size()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("警告：通道：%1索引数据和通道数据长度不一致，进行调整").arg(ch));
            // 重新生成索引数据
            indexData.clear();
            for (int i = 0; i < data.size(); ++i) {
                indexData.append(i);
            }
        }

        // 计算可见数据点的范围
        int startIdx = std::max(0, static_cast<int>(std::ceil(m_viewXMin)));
        int endIdx = std::min(static_cast<int>(std::floor(m_viewXMax)), static_cast<int>(data.size()) - 1);

        if (startIdx >= endIdx || startIdx < 0 || endIdx >= data.size()) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("通道：%1可见范围无效，%2 到 %3")
                .arg(ch).arg(startIdx).arg(endIdx));
            continue;
        }

        // 获取通道颜色
        QColor chColor = m_model->getChannelColor(ch);
        float r = chColor.redF();
        float g = chColor.greenF();
        float b = chColor.blueF();
        QVector3D colorVec(r, g, b);

        // 计算通道区域
        int channelHeight = height() / 4;
        int channelTop = channelHeight * ch;
        int midY = channelTop + channelHeight / 2;
        int deltaY = static_cast<int>((channelHeight / 4) * m_verticalScale);
        int highY = midY - deltaY;
        int lowY = midY + deltaY;

        // 准备顶点数据 - 为数字信号创建阶梯波形
        QVector<QVector2D> vertices;
        QVector<QVector3D> colors;

        // 添加第一个点
        if (startIdx < data.size()) {
            float x = normalizeX(indexData[startIdx]);
            float y = data[startIdx] > 0.5 ? normalizeY(highY) : normalizeY(lowY);

            vertices.append(QVector2D(x, y));
            colors.append(colorVec);
        }

        // 创建数字信号的阶梯波形
        for (int i = startIdx; i < endIdx; ++i) {
            if (i >= data.size() || i >= indexData.size() || i + 1 >= data.size() || i + 1 >= indexData.size())
                continue;

            float currentX = normalizeX(indexData[i]);
            float nextX = normalizeX(indexData[i + 1]);

            float currentY = data[i] > 0.5 ? normalizeY(highY) : normalizeY(lowY);
            float nextY = data[i + 1] > 0.5 ? normalizeY(highY) : normalizeY(lowY);

            // 添加水平线段到下一个点
            vertices.append(QVector2D(nextX, currentY));
            colors.append(colorVec);

            // 如果有电平跳变，添加垂直线段
            if (currentY != nextY) {
                vertices.append(QVector2D(nextX, currentY));
                colors.append(colorVec);

                vertices.append(QVector2D(nextX, nextY));
                colors.append(colorVec);
            }
        }

        // 保存顶点数据
        m_vertexData[ch] = vertices;
        m_colorData[ch] = colors;
    }
}

void WaveformGLWidget::drawWaveformsGL()
{
    if (!m_model)
        return;

    // 设置线宽
    glLineWidth(m_model->getWaveformLineWidth());

    // 绑定着色器程序
    m_program->bind();

    // 设置MVP矩阵
    QMatrix4x4 projection;
    projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    m_program->setUniformValue("mvp", projection);

    // 标记是否有数据被绘制
    bool hasDrawnAnyData = false;

    // 为每个通道绘制数据
    for (int ch = 0; ch < 4; ++ch) {
        if (!m_model->isChannelEnabled(ch) || m_vertexData[ch].isEmpty())
            continue;

        // 确认顶点数量
        int vertexCount = m_vertexData[ch].size();

        if (vertexCount < 2) {
            continue;
        }

        // 更新顶点缓冲区
        m_vertexBuffer->bind();
        m_vertexBuffer->allocate(m_vertexData[ch].constData(),
            vertexCount * sizeof(QVector2D));

        // 更新颜色缓冲区
        m_colorBuffer->bind();
        m_colorBuffer->allocate(m_colorData[ch].constData(),
            vertexCount * sizeof(QVector3D));

        // 设置顶点属性
        m_program->enableAttributeArray(0);
        m_vertexBuffer->bind();
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2);

        // 设置颜色属性
        m_program->enableAttributeArray(1);
        m_colorBuffer->bind();
        m_program->setAttributeBuffer(1, GL_FLOAT, 0, 3);

        // 使用GL_LINE_STRIP绘制连续线段
        glDrawArrays(GL_LINE_STRIP, 0, vertexCount);

        // 禁用顶点属性
        m_program->disableAttributeArray(0);
        m_program->disableAttributeArray(1);

        hasDrawnAnyData = true;
    }

    // 如果没有绘制任何数据，绘制一个提示文本或标志
    if (!hasDrawnAnyData) {
        // 这里只是设置一个标记，实际的提示文本绘制可以在paintGL中完成
        // 或者在这里绘制一个简单的"无数据"标志
        LOG_WARN(LocalQTCompat::fromLocal8Bit("没有有效的通道数据用于绘制"));
    }

    // 解绑着色器程序
    m_program->release();
}

void WaveformGLWidget::drawMarkers()
{
    if (!m_model)
        return;

    // 获取标记点
    QVector<int> markers = m_model->getMarkerPoints();
    if (markers.isEmpty())
        return;

    // 设置标记线样式
    glLineWidth(1.5f);
    m_program->bind();

    // 设置MVP矩阵
    QMatrix4x4 projection;
    projection.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    m_program->setUniformValue("mvp", projection);

    // 准备标记线顶点
    QVector<QVector2D> markerVertices;
    QVector<QVector3D> markerColors;
    QVector3D markerColor(1.0f, 0.0f, 0.0f); // 红色

    for (int markerPos : markers) {
        // 检查标记点是否在可见范围内
        if (markerPos < m_viewXMin || markerPos > m_viewXMax)
            continue;

        // 计算标记线位置
        float x = normalizeX(markerPos);
        markerVertices.append(QVector2D(x, -1.0f));
        markerVertices.append(QVector2D(x, 1.0f));
        markerColors.append(markerColor);
        markerColors.append(markerColor);
    }

    if (!markerVertices.isEmpty()) {
        // 更新顶点缓冲区
        m_vertexBuffer->bind();
        m_vertexBuffer->allocate(markerVertices.constData(),
            static_cast<int>(markerVertices.size() * sizeof(QVector2D)));

        // 更新颜色缓冲区
        m_colorBuffer->bind();
        m_colorBuffer->allocate(markerColors.constData(),
            static_cast<int>(markerColors.size() * sizeof(QVector3D)));

        // 设置顶点属性
        m_program->enableAttributeArray(0);
        m_program->setAttributeBuffer(0, GL_FLOAT, 0, 2);

        // 设置颜色属性
        m_program->enableAttributeArray(1);
        m_program->setAttributeBuffer(1, GL_FLOAT, 0, 3);

        // 绘制标记线
        glDrawArrays(GL_LINES, 0, markerVertices.size());

        // 禁用顶点属性
        m_program->disableAttributeArray(0);
        m_program->disableAttributeArray(1);
    }

    m_program->release();
}

float WaveformGLWidget::normalizeX(double x) const
{
    double range = m_viewXMax - m_viewXMin;
    if (range <= 0.0001) {
        range = 1.0; // 防止除以零
    }

    // 转换到[0,1]范围
    double normalized = (x - m_viewXMin) / range;

    // 限制在[0,1]范围内
    normalized = qBound(0.0, normalized, 1.0);

    // 转换到[-1,1]范围(OpenGL坐标空间)
    return static_cast<float>(normalized * 2.0 - 1.0);
}

float WaveformGLWidget::normalizeY(int y) const
{
    // 确保高度不为零
    int h = height();
    if (h <= 0) h = 1;

    // 屏幕坐标系Y轴向下，OpenGL坐标系Y轴向上，需要反转
    double normalized = 1.0 - (static_cast<double>(y) / h);

    // 限制在[0,1]范围内
    normalized = qBound(0.0, normalized, 1.0);

    // 转换到[-1,1]范围
    return static_cast<float>(normalized * 2.0 - 1.0);
}

double WaveformGLWidget::calculateGridStep(double range) const
{
    // 目标网格线数量
    const int TARGET_GRID_COUNT = 10;

    // 计算粗略步长
    double rawStep = range / TARGET_GRID_COUNT;

    // 找到合适的步长 (1, 2, 5, 10, 20, 50, ...)
    double power = std::pow(10, std::floor(std::log10(rawStep)));
    double normalizedStep = rawStep / power;

    if (normalizedStep < 1.5)
        return power;
    else if (normalizedStep < 3.5)
        return 2 * power;
    else if (normalizedStep < 7.5)
        return 5 * power;
    else
        return 10 * power;
}

void WaveformGLWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_lastMousePos = event->pos();
        m_isDragging = true;
        setCursor(Qt::ClosedHandCursor);
    }

    QOpenGLWidget::mousePressEvent(event);
}

void WaveformGLWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        // 通知控制器处理平移
        emit signal_WF_GL_panRequested(delta.x());
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void WaveformGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);
    }

    QOpenGLWidget::mouseReleaseEvent(event);
}

void WaveformGLWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // 添加标记点
        addMarker(event->pos());
    }

    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void WaveformGLWidget::wheelEvent(QWheelEvent* event)
{
    if (!m_model)
        return;

    // 获取当前视图范围
    double xMin = m_viewXMin;
    double xMax = m_viewXMax;
    double range = xMax - xMin;

    // 鼠标位置对应的数据点
    double clickedIndex = screenToDataX(event->position().toPoint().x());
    double ratio = (clickedIndex - xMin) / range;

    if (event->angleDelta().y() > 0) {
        // 放大 - 缩小范围为原来的80%
        double newRange = range * 0.8;

        // 保持鼠标所在数据点的相对位置不变
        double newXMin = clickedIndex - ratio * newRange;
        double newXMax = newXMin + newRange;

        // 边界检查
        if (newXMin < 0) {
            newXMin = 0;
            newXMax = newXMin + newRange;
        }

        // 设置新范围
        setViewRange(newXMin, newXMax);

        // 通知视图范围变更
        emit signal_WF_GL_viewRangeChanged(newXMin, newXMax);
    }
    else {
        // 缩小 - 放大范围为原来的125%
        double newRange = range * 1.25;

        // 保持鼠标所在数据点的相对位置不变
        double newXMin = clickedIndex - ratio * newRange;
        double newXMax = newXMin + newRange;

        // 边界检查
        if (newXMin < 0) {
            newXMin = 0;
            newXMax = newXMin + newRange;
        }

        // 设置新范围
        setViewRange(newXMin, newXMax);

        // 通知视图范围变更
        emit signal_WF_GL_viewRangeChanged(newXMin, newXMax);
    }

    // 阻止事件传播
    event->accept();
}