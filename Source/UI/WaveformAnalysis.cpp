
// WaveformAnalysis.cpp
#include "WaveformAnalysis.h"
#include "Logger.h"
#include <cmath>
#include <QDateTime>
#include <QFormLayout>

WaveformAnalysis::WaveformAnalysis(QWidget* parent)
    : QWidget(parent)
    , m_isRunning(false)
    , m_zoomLevel(1.0)
    , m_waveformMode(WaveformMode::ANALOG)
    , m_triggerMode(TriggerMode::AUTO)
    , m_sampleRate(10000.0)
    , m_simulationTimer(nullptr)
{
    initializeUI();

    // 初始化模拟数据定时器
    m_simulationTimer = new QTimer(this);
    connect(m_simulationTimer, &QTimer::timeout, this, &WaveformAnalysis::onUpdateSimulatedData);

    LOG_INFO("波形分析窗口已创建");
}

WaveformAnalysis::~WaveformAnalysis()
{
    if (m_isRunning) {
        stopAnalysis();
    }

    if (m_simulationTimer) {
        m_simulationTimer->stop();
    }

    LOG_INFO("波形分析窗口被销毁");
}

void WaveformAnalysis::initializeUI()
{
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 添加工具栏
    QWidget* toolbar = createToolBar();
    mainLayout->addWidget(toolbar);

    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // 左侧波形显示区域
    QWidget* waveformWidget = createWaveformDisplay();
    m_mainSplitter->addWidget(waveformWidget);

    // 右侧控制和结果区域
    m_rightTabWidget = new QTabWidget(this);
    m_rightTabWidget->addTab(createAnalysisPanel(), "分析设置");
    m_rightTabWidget->addTab(createMeasurementsPanel(), "测量结果");
    m_mainSplitter->addWidget(m_rightTabWidget);

    // 设置分割比例
    m_mainSplitter->setStretchFactor(0, 3); // 波形显示区域占较大空间
    m_mainSplitter->setStretchFactor(1, 1);

    // 添加到主布局
    mainLayout->addWidget(m_mainSplitter);

    // 设置窗口大小
    resize(1000, 700);
}

QWidget* WaveformAnalysis::createToolBar()
{
    QWidget* toolbarWidget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(toolbarWidget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // 波形类型选择
    QLabel* typeLabel = new QLabel("波形类型:", this);
    m_waveformTypeCombo = new QComboBox(this);
    m_waveformTypeCombo->addItem("模拟波形");
    m_waveformTypeCombo->addItem("数字波形");
    m_waveformTypeCombo->addItem("混合波形");
    connect(m_waveformTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &WaveformAnalysis::onWaveformTypeChanged);

    // 触发模式选择
    QLabel* triggerLabel = new QLabel("触发模式:", this);
    m_triggerModeCombo = new QComboBox(this);
    m_triggerModeCombo->addItem("自动");
    m_triggerModeCombo->addItem("普通");
    m_triggerModeCombo->addItem("单次");
    connect(m_triggerModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &WaveformAnalysis::onTriggerModeChanged);

    // 采样率设置
    QLabel* rateLabel = new QLabel("采样率(Hz):", this);
    m_sampleRateSpin = new QSpinBox(this);
    m_sampleRateSpin->setRange(100, 1000000);
    m_sampleRateSpin->setValue(10000);
    m_sampleRateSpin->setSingleStep(1000);
    m_sampleRateSpin->setFixedWidth(100);

    // 自动缩放选项
    m_autoscaleCheck = new QCheckBox("自动缩放", this);
    m_autoscaleCheck->setChecked(true);
    connect(m_autoscaleCheck, &QCheckBox::toggled, this, &WaveformAnalysis::onAutoScaleChanged);

    // 控制按钮
    m_startButton = new QPushButton("开始", this);
    m_stopButton = new QPushButton("停止", this);
    m_exportButton = new QPushButton("导出", this);
    m_measureButton = new QPushButton("测量", this);
    m_settingsButton = new QPushButton("设置", this);

    // 初始状态
    m_stopButton->setEnabled(false);

    // 连接按钮信号
    connect(m_startButton, &QPushButton::clicked, this, &WaveformAnalysis::startAnalysis);
    connect(m_stopButton, &QPushButton::clicked, this, &WaveformAnalysis::stopAnalysis);
    connect(m_exportButton, &QPushButton::clicked, this, &WaveformAnalysis::onExportButtonClicked);
    connect(m_measureButton, &QPushButton::clicked, this, &WaveformAnalysis::onMeasureButtonClicked);
    connect(m_settingsButton, &QPushButton::clicked, this, &WaveformAnalysis::onSettingsButtonClicked);

    // 添加到工具栏布局
    layout->addWidget(typeLabel);
    layout->addWidget(m_waveformTypeCombo);
    layout->addSpacing(10);
    layout->addWidget(triggerLabel);
    layout->addWidget(m_triggerModeCombo);
    layout->addSpacing(10);
    layout->addWidget(rateLabel);
    layout->addWidget(m_sampleRateSpin);
    layout->addSpacing(10);
    layout->addWidget(m_autoscaleCheck);
    layout->addStretch();
    layout->addWidget(m_startButton);
    layout->addWidget(m_stopButton);
    layout->addWidget(m_measureButton);
    layout->addWidget(m_exportButton);
    layout->addWidget(m_settingsButton);

    return toolbarWidget;
}

QWidget* WaveformAnalysis::createWaveformDisplay()
{
    QWidget* waveformWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(waveformWidget);
    layout->setContentsMargins(0, 0, 0, 0);

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 创建图表
    m_chart = new QChart();
    m_chart->setTitle("波形显示");
    m_chart->legend()->hide();

    // 创建数据系列
    m_mainSeries = new QLineSeries();
    m_mainSeries->setName("主波形");

    m_markerSeries = new QScatterSeries();
    m_markerSeries->setName("标记点");
    m_markerSeries->setMarkerSize(8);

    // 添加到图表
    m_chart->addSeries(m_mainSeries);
    m_chart->addSeries(m_markerSeries);

    // 创建轴
    m_axisX = new QValueAxis();
    m_axisX->setTitleText("时间 (ms)");
    m_axisX->setRange(0, 100);

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("电压 (V)");
    m_axisY->setRange(-5, 5);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_mainSeries->attachAxis(m_axisX);
    m_mainSeries->attachAxis(m_axisY);
    m_markerSeries->attachAxis(m_axisX);
    m_markerSeries->attachAxis(m_axisY);

    // 创建图表视图
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 添加缩放控制
    QHBoxLayout* zoomLayout = new QHBoxLayout();
    m_zoomInButton = new QPushButton("+", this);
    m_zoomOutButton = new QPushButton("-", this);
    m_zoomResetButton = new QPushButton("重置", this);

    connect(m_zoomInButton, &QPushButton::clicked, this, &WaveformAnalysis::onZoomInButtonClicked);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &WaveformAnalysis::onZoomOutButtonClicked);
    connect(m_zoomResetButton, &QPushButton::clicked, this, &WaveformAnalysis::onZoomResetButtonClicked);

    // 水平和垂直滑块
    m_horizontalScaleSlider = new QSlider(Qt::Horizontal, this);
    m_horizontalScaleSlider->setRange(1, 100);
    m_horizontalScaleSlider->setValue(50);

    m_verticalScaleSlider = new QSlider(Qt::Vertical, this);
    m_verticalScaleSlider->setRange(1, 100);
    m_verticalScaleSlider->setValue(50);

    // 添加缩放控件
    zoomLayout->addWidget(m_zoomInButton);
    zoomLayout->addWidget(m_zoomResetButton);
    zoomLayout->addWidget(m_zoomOutButton);

    // 将组件添加到布局
    QGridLayout* chartLayout = new QGridLayout();
    chartLayout->addWidget(m_chartView, 0, 0);
    chartLayout->addWidget(m_verticalScaleSlider, 0, 1);
    chartLayout->addLayout(zoomLayout, 1, 0);
    chartLayout->addWidget(m_horizontalScaleSlider, 2, 0);

    layout->addLayout(chartLayout);
#else
    // 如果没有图表支持，则显示提示信息
    m_noChartLabel = new QLabel("Qt图表库不可用，无法显示波形图表", this);
    m_noChartLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_noChartLabel);
#endif

    return waveformWidget;
}

QWidget* WaveformAnalysis::createAnalysisPanel()
{
    QWidget* analysisWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(analysisWidget);

    // 分析选项分组
    QGroupBox* analysisGroup = new QGroupBox("分析选项", this);
    QFormLayout* analysisLayout = new QFormLayout(analysisGroup);

    // 添加分析参数
    QComboBox* analysisTypeCombo = new QComboBox(this);
    analysisTypeCombo->addItem("频谱分析");
    analysisTypeCombo->addItem("周期分析");
    analysisTypeCombo->addItem("幅值分析");
    analysisTypeCombo->addItem("噪声分析");

    QSpinBox* windowSizeSpin = new QSpinBox(this);
    windowSizeSpin->setRange(64, 8192);
    windowSizeSpin->setValue(1024);
    windowSizeSpin->setSingleStep(64);

    QComboBox* windowTypeCombo = new QComboBox(this);
    windowTypeCombo->addItem("矩形窗");
    windowTypeCombo->addItem("汉宁窗");
    windowTypeCombo->addItem("汉明窗");
    windowTypeCombo->addItem("布莱克曼窗");

    // 添加到布局
    analysisLayout->addRow("分析类型:", analysisTypeCombo);
    analysisLayout->addRow("窗口大小:", windowSizeSpin);
    analysisLayout->addRow("窗口类型:", windowTypeCombo);

    // 添加分析选项
    QCheckBox* peakDetectCheck = new QCheckBox("峰值检测", this);
    QCheckBox* noiseFilterCheck = new QCheckBox("噪声滤波", this);
    QCheckBox* autoCorrelateCheck = new QCheckBox("自相关", this);

    analysisLayout->addRow("", peakDetectCheck);
    analysisLayout->addRow("", noiseFilterCheck);
    analysisLayout->addRow("", autoCorrelateCheck);

    layout->addWidget(analysisGroup);

    // 触发设置分组
    QGroupBox* triggerGroup = new QGroupBox("触发设置", this);
    QFormLayout* triggerLayout = new QFormLayout(triggerGroup);

    QDoubleSpinBox* triggerLevelSpin = new QDoubleSpinBox(this);
    triggerLevelSpin->setRange(-10.0, 10.0);
    triggerLevelSpin->setValue(0.0);
    triggerLevelSpin->setSingleStep(0.1);

    QComboBox* triggerSlopeCombo = new QComboBox(this);
    triggerSlopeCombo->addItem("上升沿");
    triggerSlopeCombo->addItem("下降沿");
    triggerSlopeCombo->addItem("双向");

    QSpinBox* preTriggerSpin = new QSpinBox(this);
    preTriggerSpin->setRange(0, 100);
    preTriggerSpin->setValue(20);
    preTriggerSpin->setSuffix("%");

    triggerLayout->addRow("触发电平:", triggerLevelSpin);
    triggerLayout->addRow("触发边沿:", triggerSlopeCombo);
    triggerLayout->addRow("预触发量:", preTriggerSpin);

    layout->addWidget(triggerGroup);

    // 显示设置分组
    QGroupBox* displayGroup = new QGroupBox("显示设置", this);
    QFormLayout* displayLayout = new QFormLayout(displayGroup);

    QCheckBox* gridCheck = new QCheckBox("显示网格", this);
    gridCheck->setChecked(true);

    QComboBox* colorThemeCombo = new QComboBox(this);
    colorThemeCombo->addItem("默认配色");
    colorThemeCombo->addItem("暗黑主题");
    colorThemeCombo->addItem("高对比度");

    QSpinBox* refreshRateSpin = new QSpinBox(this);
    refreshRateSpin->setRange(1, 60);
    refreshRateSpin->setValue(10);
    refreshRateSpin->setSuffix(" Hz");

    displayLayout->addRow("", gridCheck);
    displayLayout->addRow("配色方案:", colorThemeCombo);
    displayLayout->addRow("刷新率:", refreshRateSpin);

    layout->addWidget(displayGroup);

    // 添加弹性空间
    layout->addStretch();

    return analysisWidget;
}

QWidget* WaveformAnalysis::createMeasurementsPanel()
{
    QWidget* measureWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(measureWidget);

    // 结果显示区域
    m_resultsTextEdit = new QTextEdit(this);
    m_resultsTextEdit->setReadOnly(true);
    m_resultsTextEdit->setFont(QFont("Courier New", 10));

    layout->addWidget(m_resultsTextEdit);

    // 添加测量按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    QPushButton* frequencyBtn = new QPushButton("频率", this);
    QPushButton* amplitudeBtn = new QPushButton("幅值", this);
    QPushButton* periodBtn = new QPushButton("周期", this);
    QPushButton* rmsBtn = new QPushButton("均方根", this);
    QPushButton* statisticsBtn = new QPushButton("统计", this);

    buttonLayout->addWidget(frequencyBtn);
    buttonLayout->addWidget(amplitudeBtn);
    buttonLayout->addWidget(periodBtn);
    buttonLayout->addWidget(rmsBtn);
    buttonLayout->addWidget(statisticsBtn);

    layout->addLayout(buttonLayout);

    return measureWidget;
}

void WaveformAnalysis::setWaveformData(const QVector<double>& xData, const QVector<double>& yData)
{
    // 检查数据有效性
    if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
        LOG_WARN("设置波形数据失败: 数据为空或X/Y长度不匹配");
        return;
    }

    // 保存数据
    m_xData = xData;
    m_yData = yData;

    // 更新显示
    updateWaveformDisplay();

    LOG_INFO(QString("波形数据已更新, 数据点数: %1").arg(m_xData.size()));
}

void WaveformAnalysis::addDataPoint(double x, double y)
{
    // 添加数据点
    m_xData.append(x);
    m_yData.append(y);

    // 限制数据点数量
    const int MAX_POINTS = 10000;
    while (m_xData.size() > MAX_POINTS) {
        m_xData.removeFirst();
        m_yData.removeFirst();
    }

    // 如果正在运行，则更新显示
    if (m_isRunning) {
        updateWaveformDisplay();
    }
}

void WaveformAnalysis::clearData()
{
    m_xData.clear();
    m_yData.clear();

    // 清空图表
#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    m_mainSeries->clear();
    m_markerSeries->clear();
#endif

    LOG_INFO("波形数据已清空");
}

void WaveformAnalysis::startAnalysis()
{
    if (m_isRunning) {
        return;
    }

    LOG_INFO("开始波形分析");

    // 更新UI状态
    m_isRunning = true;
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);

    // 如果没有数据，生成模拟数据
    if (m_xData.isEmpty() || m_yData.isEmpty()) {
        generateSimulatedData();
    }

    // 更新波形显示
    updateWaveformDisplay();

    // 启动模拟数据更新定时器
    m_simulationTimer->start(100); // 10Hz刷新率
}

void WaveformAnalysis::stopAnalysis()
{
    if (!m_isRunning) {
        return;
    }

    LOG_INFO("停止波形分析");

    // 停止模拟数据更新定时器
    m_simulationTimer->stop();

    // 更新UI状态
    m_isRunning = false;
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
}

void WaveformAnalysis::updateWaveformDisplay()
{
#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 检查数据
    if (m_xData.isEmpty() || m_yData.isEmpty()) {
        return;
    }

    // 清空现有数据
    m_mainSeries->clear();

    // 添加数据点
    for (int i = 0; i < m_xData.size(); ++i) {
        m_mainSeries->append(m_xData[i], m_yData[i]);
    }

    // 如果启用了自动缩放
    if (m_autoscaleCheck->isChecked()) {
        // 计算X轴范围
        double xMin = *std::min_element(m_xData.begin(), m_xData.end());
        double xMax = *std::max_element(m_xData.begin(), m_xData.end());
        double xRange = xMax - xMin;

        // 计算Y轴范围
        double yMin = *std::min_element(m_yData.begin(), m_yData.end());
        double yMax = *std::max_element(m_yData.begin(), m_yData.end());
        double yRange = yMax - yMin;

        // 添加一点边距
        double xMargin = xRange * 0.05;
        double yMargin = yRange * 0.05;

        // 至少有一定的范围，避免单点时显示异常
        if (xRange < 0.001) {
            xRange = 1.0;
            xMargin = 0.5;
        }
        if (yRange < 0.001) {
            yRange = 1.0;
            yMargin = 0.5;
        }

        // 设置轴范围
        m_axisX->setRange(xMin - xMargin, xMax + xMargin);
        m_axisY->setRange(yMin - yMargin, yMax + yMargin);
    }

    // 更新标记点（如果有）
    updateMarkers();

    // 刷新图表
    m_chart->update();
    m_chartView->update();
#endif
}

void WaveformAnalysis::updateMarkers()
{
#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 清空现有标记
    m_markerSeries->clear();

    // 根据当前分析模式添加标记点
    // 这里仅作为示例，为最大值和最小值添加标记
    if (!m_yData.isEmpty()) {
        int maxIndex = std::max_element(m_yData.begin(), m_yData.end()) - m_yData.begin();
        int minIndex = std::min_element(m_yData.begin(), m_yData.end()) - m_yData.begin();

        // 添加最大值标记
        if (maxIndex >= 0 && maxIndex < m_xData.size()) {
            m_markerSeries->append(m_xData[maxIndex], m_yData[maxIndex]);
        }

        // 添加最小值标记
        if (minIndex >= 0 && minIndex < m_xData.size() && minIndex != maxIndex) {
            m_markerSeries->append(m_xData[minIndex], m_yData[minIndex]);
        }
    }
#endif
}

void WaveformAnalysis::calculateStatistics()
{
    // 检查数据
    if (m_yData.isEmpty()) {
        m_resultsTextEdit->setText("无数据可分析");
        return;
    }

    // 计算统计信息
    double sum = 0.0;
    double sumSquares = 0.0;
    double minVal = m_yData[0];
    double maxVal = m_yData[0];
    int minIndex = 0;
    int maxIndex = 0;

    for (int i = 0; i < m_yData.size(); ++i) {
        double value = m_yData[i];
        sum += value;
        sumSquares += value * value;

        if (value < minVal) {
            minVal = value;
            minIndex = i;
        }

        if (value > maxVal) {
            maxVal = value;
            maxIndex = i;
        }
    }

    // 计算平均值和标准差
    double average = sum / m_yData.size();
    double variance = (sumSquares / m_yData.size()) - (average * average);
    double stdDev = std::sqrt(variance);
    double peakToPeak = maxVal - minVal;

    // 估计频率和周期（如果有时间数据）
    double frequency = 0.0;
    double period = 0.0;

    if (m_xData.size() > 1) {
        // 简单的过零点检测来估计周期
        int zeroCrossings = 0;
        for (int i = 1; i < m_yData.size(); ++i) {
            if ((m_yData[i - 1] < average && m_yData[i] > average) ||
                (m_yData[i - 1] > average && m_yData[i] < average)) {
                zeroCrossings++;
            }
        }

        double timeSpan = m_xData.last() - m_xData.first();

        if (zeroCrossings > 0) {
            period = (2.0 * timeSpan) / zeroCrossings;
            frequency = 1.0 / period;
        }
    }

    // 生成报告
    QString report;
    report += "===== 波形统计信息 =====\n\n";
    report += QString("数据点数: %1\n").arg(m_yData.size());
    report += QString("最小值: %1 V (位置: %2)\n").arg(minVal, 0, 'f', 4).arg(minIndex);
    report += QString("最大值: %1 V (位置: %2)\n").arg(maxVal, 0, 'f', 4).arg(maxIndex);
    report += QString("平均值: %1 V\n").arg(average, 0, 'f', 4);
    report += QString("峰峰值: %1 V\n").arg(peakToPeak, 0, 'f', 4);
    report += QString("标准差: %1 V\n").arg(stdDev, 0, 'f', 4);

    if (period > 0) {
        report += QString("估计周期: %1 ms\n").arg(period, 0, 'f', 4);
        report += QString("估计频率: %1 Hz\n").arg(frequency, 0, 'f', 4);
    }
    else {
        report += "无法估计周期/频率\n";
    }

    report += QString("\n时间范围: %1 - %2 ms\n").arg(m_xData.first(), 0, 'f', 4).arg(m_xData.last(), 0, 'f', 4);
    report += QString("采样率: %1 Hz\n").arg(m_sampleRate, 0, 'f', 1);
    report += QString("\n分析时间: %1\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

    // 显示报告
    m_resultsTextEdit->setText(report);

    // 打印调试信息
    LOG_INFO("波形统计分析完成");
}

void WaveformAnalysis::generateSimulatedData()
{
    // 清空现有数据
    m_xData.clear();
    m_yData.clear();

    // 生成模拟数据
    const int numPoints = 1000;
    const double timeStep = 0.1; // ms

    for (int i = 0; i < numPoints; ++i) {
        double x = i * timeStep;

        // 根据波形模式生成不同类型的波形
        double y = 0.0;

        switch (m_waveformMode) {
        case WaveformMode::ANALOG:
            // 生成正弦波 + 噪声
            y = 3.0 * std::sin(2.0 * M_PI * 0.01 * x) +
                1.0 * std::sin(2.0 * M_PI * 0.05 * x) +
                0.2 * ((double)rand() / RAND_MAX - 0.5);
            break;

        case WaveformMode::DIGITAL:
            // 生成数字方波
            y = (std::sin(2.0 * M_PI * 0.02 * x) > 0) ? 3.3 : 0.0;
            // 添加一些上升/下降时间
            for (int j = 0; j < 5; ++j) {
                if (i > 0 && std::abs(m_yData[i - 1] - y) > 1.0) {
                    y = m_yData[i - 1] + (y - m_yData[i - 1]) * 0.2;
                    break;
                }
            }
            break;

        case WaveformMode::MIXED:
            // 混合模拟和数字信号
            y = 2.0 * std::sin(2.0 * M_PI * 0.01 * x);
            // 添加数字脉冲
            if (std::fmod(x, 20.0) < 2.0) {
                y += 2.0;
            }
            // 添加一些噪声
            y += 0.1 * ((double)rand() / RAND_MAX - 0.5);
            break;
        }

        m_xData.append(x);
        m_yData.append(y);
    }

    LOG_INFO(QString("已生成模拟波形数据，类型: %1, 点数: %2").arg(
        m_waveformTypeCombo->currentText()).arg(numPoints));
}

void WaveformAnalysis::onUpdateSimulatedData()
{
    if (!m_isRunning) {
        return;
    }

    // 为模拟效果，移除一些旧数据点并添加新数据点
    const int newPointsCount = 10;

    if (m_xData.size() > newPointsCount) {
        // 移除旧数据点
        for (int i = 0; i < newPointsCount; ++i) {
            m_xData.removeFirst();
            m_yData.removeFirst();
        }

        // 添加新数据点
        double lastX = m_xData.last();
        double timeStep = 0.1; // ms

        for (int i = 1; i <= newPointsCount; ++i) {
            double x = lastX + i * timeStep;

            // 根据波形模式生成不同类型的波形
            double y = 0.0;

            switch (m_waveformMode) {
            case WaveformMode::ANALOG:
                // 生成正弦波 + 噪声
                y = 3.0 * std::sin(2.0 * M_PI * 0.01 * x) +
                    1.0 * std::sin(2.0 * M_PI * 0.05 * x) +
                    0.2 * ((double)rand() / RAND_MAX - 0.5);
                break;

            case WaveformMode::DIGITAL:
                // 生成数字方波
                y = (std::sin(2.0 * M_PI * 0.02 * x) > 0) ? 3.3 : 0.0;
                // 添加一些上升/下降时间
                if (!m_yData.isEmpty() && std::abs(m_yData.last() - y) > 1.0) {
                    y = m_yData.last() + (y - m_yData.last()) * 0.2;
                }
                break;

            case WaveformMode::MIXED:
                // 混合模拟和数字信号
                y = 2.0 * std::sin(2.0 * M_PI * 0.01 * x);
                // 添加数字脉冲
                if (std::fmod(x, 20.0) < 2.0) {
                    y += 2.0;
                }
                // 添加一些噪声
                y += 0.1 * ((double)rand() / RAND_MAX - 0.5);
                break;
            }

            m_xData.append(x);
            m_yData.append(y);
        }

        // 更新显示
        updateWaveformDisplay();
    }
}

void WaveformAnalysis::onWaveformTypeChanged(int index)
{
    // 根据索引更新波形模式
    switch (index) {
    case 0:
        m_waveformMode = WaveformMode::ANALOG;
        break;
    case 1:
        m_waveformMode = WaveformMode::DIGITAL;
        break;
    case 2:
        m_waveformMode = WaveformMode::MIXED;
        break;
    }

    LOG_INFO(QString("波形类型已变更为: %1").arg(m_waveformTypeCombo->currentText()));

    // 重新生成模拟数据
    if (m_xData.isEmpty() || m_isRunning) {
        generateSimulatedData();
        updateWaveformDisplay();
    }
}

void WaveformAnalysis::onTriggerModeChanged(int index)
{
    // 根据索引更新触发模式
    switch (index) {
    case 0:
        m_triggerMode = TriggerMode::AUTO;
        break;
    case 1:
        m_triggerMode = TriggerMode::NORMAL;
        break;
    case 2:
        m_triggerMode = TriggerMode::SINGLE;
        break;
    }

    LOG_INFO(QString("触发模式已变更为: %1").arg(m_triggerModeCombo->currentText()));
}

void WaveformAnalysis::onExportButtonClicked()
{
    LOG_INFO("导出按钮点击");

    if (m_xData.isEmpty() || m_yData.isEmpty()) {
        QMessageBox::warning(this, "导出失败", "无数据可导出");
        return;
    }

    // 显示文件保存对话框
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出波形数据",
        QDir::homePath() + "/waveform_data.csv",
        "CSV文件 (*.csv);;文本文件 (*.txt);;所有文件 (*.*)");

    if (fileName.isEmpty()) {
        return;
    }

    // 打开文件
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "导出失败", "无法打开文件进行写入");
        return;
    }

    // 写入表头
    QTextStream out(&file);
    out << "时间(ms),电压(V)\n";

    // 写入数据
    for (int i = 0; i < m_xData.size() && i < m_yData.size(); ++i) {
        out << QString::number(m_xData[i], 'f', 4) << ","
            << QString::number(m_yData[i], 'f', 4) << "\n";
    }

    file.close();

    LOG_INFO(QString("波形数据已导出到: %1").arg(fileName));
    QMessageBox::information(this, "导出成功", QString("波形数据已导出到:\n%1").arg(fileName));

    // 发送导出请求信号
    emit exportRequested();
}

void WaveformAnalysis::onZoomInButtonClicked()
{
    m_zoomLevel *= 1.25;

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 获取当前轴范围
    double xMin = m_axisX->min();
    double xMax = m_axisX->max();
    double yMin = m_axisY->min();
    double yMax = m_axisY->max();

    // 计算新范围（缩小显示范围 = 放大）
    double xCenter = (xMin + xMax) / 2.0;
    double yCenter = (yMin + yMax) / 2.0;
    double xHalfRange = (xMax - xMin) / (2.0 * 1.25);
    double yHalfRange = (yMax - yMin) / (2.0 * 1.25);

    // 设置新范围
    m_axisX->setRange(xCenter - xHalfRange, xCenter + xHalfRange);
    m_axisY->setRange(yCenter - yHalfRange, yCenter + yHalfRange);

    // 更新图表
    m_chart->update();
#endif

    LOG_INFO(QString("图表已放大, 缩放级别: %1").arg(m_zoomLevel));
}

void WaveformAnalysis::onZoomOutButtonClicked()
{
    m_zoomLevel /= 1.25;

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 获取当前轴范围
    double xMin = m_axisX->min();
    double xMax = m_axisX->max();
    double yMin = m_axisY->min();
    double yMax = m_axisY->max();

    // 计算新范围（放大显示范围 = 缩小）
    double xCenter = (xMin + xMax) / 2.0;
    double yCenter = (yMin + yMax) / 2.0;
    double xHalfRange = (xMax - xMin) / 2.0 * 1.25;
    double yHalfRange = (yMax - yMin) / 2.0 * 1.25;

    // 设置新范围
    m_axisX->setRange(xCenter - xHalfRange, xCenter + xHalfRange);
    m_axisY->setRange(yCenter - yHalfRange, yCenter + yHalfRange);

    // 更新图表
    m_chart->update();
#endif

    LOG_INFO(QString("图表已缩小, 缩放级别: %1").arg(m_zoomLevel));
}

void WaveformAnalysis::onZoomResetButtonClicked()
{
    m_zoomLevel = 1.0;

#if defined(QT_CHARTS_LIB) || defined(QT_CHARTS_SUPPORT)
    // 重置自动缩放
    if (m_autoscaleCheck->isChecked()) {
        updateWaveformDisplay();
    }
    else {
        // 使用默认范围
        m_axisX->setRange(0, 100);
        m_axisY->setRange(-5, 5);
    }

    // 更新图表
    m_chart->update();
#endif

    LOG_INFO("图表缩放已重置");
}

void WaveformAnalysis::onMeasureButtonClicked()
{
    LOG_INFO("测量按钮点击");

    // 计算并显示统计信息
    calculateStatistics();

    // 切换到测量结果标签页
    m_rightTabWidget->setCurrentIndex(1);
}

void WaveformAnalysis::onSettingsButtonClicked()
{
    LOG_INFO("设置按钮点击");

    // 简单的消息框示例，实际应该显示配置对话框
    QMessageBox::information(this, "设置", "波形分析设置功能正在开发中");
}

void WaveformAnalysis::onAutoScaleChanged(bool checked)
{
    LOG_INFO(QString("自动缩放设置已更改: %1").arg(checked ? "启用" : "禁用"));

    // 如果启用自动缩放，则更新显示
    if (checked) {
        updateWaveformDisplay();
    }
}