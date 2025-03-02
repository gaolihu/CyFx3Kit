// ChannelSelect.cpp
#include "ChannelSelect.h"
#include "Logger.h"

ChannelSelect::ChannelSelect(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    // 初始化UI
    initializeUI();

    // 连接信号槽
    connectSignals();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道选择窗口已创建"));
}

ChannelSelect::~ChannelSelect()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道选择窗口被销毁"));
}

void ChannelSelect::initializeUI()
{
    // 设置窗口标题
    setWindowTitle(LocalQTCompat::fromLocal8Bit("通道配置"));

    // 设置窗口模态属性（应用程序级别模态）
    setWindowModality(Qt::ApplicationModal);

    // 设置其他初始状态
    ui.CHen_0->setEnabled(true);

    updateUIState();
}

void ChannelSelect::connectSignals()
{
    // 连接按钮信号
    connect(ui.pushButton, &QPushButton::clicked, this, &ChannelSelect::onSaveButtonClicked);
    connect(ui.pushButton_2, &QPushButton::clicked, this, &ChannelSelect::onCancelButtonClicked);

    // 连接通道使能复选框信号
    connect(ui.CHen_0, &QCheckBox::toggled, this, &ChannelSelect::onChannelEnableChanged);
    connect(ui.CHen_1, &QCheckBox::toggled, this, &ChannelSelect::onChannelEnableChanged);
    connect(ui.CHen_2, &QCheckBox::toggled, this, &ChannelSelect::onChannelEnableChanged);
    connect(ui.CHen_3, &QCheckBox::toggled, this, &ChannelSelect::onChannelEnableChanged);

    // 连接PN交换复选框信号
    connect(ui.PN_0, &QCheckBox::toggled, this, &ChannelSelect::onPNStatusChanged);
    connect(ui.PN_1, &QCheckBox::toggled, this, &ChannelSelect::onPNStatusChanged);
    connect(ui.PN_2, &QCheckBox::toggled, this, &ChannelSelect::onPNStatusChanged);
    connect(ui.PN_3, &QCheckBox::toggled, this, &ChannelSelect::onPNStatusChanged);

    // 连接抓取类型下拉框信号
    connect(ui.comboBox_5, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ChannelSelect::onCaptureTypeChanged);
}

void ChannelSelect::updateUIState()
{
    // 更新UI状态，根据当前选择调整控件状态
    // 例如，如果某个通道被禁用，可能需要禁用其相关的PN交换复选框
    bool ch0Enabled = ui.CHen_0->isChecked();
    bool ch1Enabled = ui.CHen_1->isChecked();
    bool ch2Enabled = ui.CHen_2->isChecked();
    bool ch3Enabled = ui.CHen_3->isChecked();

    ui.PN_0->setEnabled(ch0Enabled);
    ui.PN_1->setEnabled(ch1Enabled);
    ui.PN_2->setEnabled(ch2Enabled);
    ui.PN_3->setEnabled(ch3Enabled);

    // 根据抓取类型更新界面
    int captureType = ui.comboBox_5->currentIndex();
    bool isVideoMode = (captureType == 0 || captureType == 1); // VIDEO或VIDEO+DSC

    // 如果是视频模式，启用视频宽度和高度输入框
    ui.videoWidth->setEnabled(isVideoMode);
    ui.videoHeigh->setEnabled(isVideoMode);
}

ChannelConfig ChannelSelect::getCurrentConfig()
{
    ChannelConfig config;

    // 获取视频宽度和高度
    bool ok = false;
    config.videoWidth = ui.videoWidth->text().toInt(&ok);
    if (!ok || config.videoWidth <= 0) {
        config.videoWidth = 1920; // 默认值
    }

    ok = false;
    config.videoHeight = ui.videoHeigh->text().toInt(&ok);
    if (!ok || config.videoHeight <= 0) {
        config.videoHeight = 1080; // 默认值
    }

    // 获取抓取类型
    config.captureType = ui.comboBox_5->currentIndex();

    // 获取通道使能状态
    config.channelEnabled[0] = ui.CHen_0->isChecked();
    config.channelEnabled[1] = ui.CHen_1->isChecked();
    config.channelEnabled[2] = ui.CHen_2->isChecked();
    config.channelEnabled[3] = ui.CHen_3->isChecked();

    // 获取PN交换状态
    config.pnSwapped[0] = ui.PN_0->isChecked();
    config.pnSwapped[1] = ui.PN_1->isChecked();
    config.pnSwapped[2] = ui.PN_2->isChecked();
    config.pnSwapped[3] = ui.PN_3->isChecked();

    // 获取其他可能的附加参数
    if (!ui.TE_Value->text().isEmpty()) {
        config.additionalParams["TE"] = ui.TE_Value->text().toDouble();
    }

    return config;
}

void ChannelSelect::setConfigToUI(const ChannelConfig& config)
{
    // 设置视频宽度和高度
    ui.videoWidth->setText(QString::number(config.videoWidth));
    ui.videoHeigh->setText(QString::number(config.videoHeight));

    // 设置抓取类型
    ui.comboBox_5->setCurrentIndex(config.captureType);

    // 设置通道使能状态
    ui.CHen_0->setChecked(config.channelEnabled[0]);
    ui.CHen_1->setChecked(config.channelEnabled[1]);
    ui.CHen_2->setChecked(config.channelEnabled[2]);
    ui.CHen_3->setChecked(config.channelEnabled[3]);

    // 设置PN交换状态
    ui.PN_0->setChecked(config.pnSwapped[0]);
    ui.PN_1->setChecked(config.pnSwapped[1]);
    ui.PN_2->setChecked(config.pnSwapped[2]);
    ui.PN_3->setChecked(config.pnSwapped[3]);

    // 设置其他参数
    if (config.additionalParams.contains("TE")) {
        ui.TE_Value->setText(QString::number(config.additionalParams["TE"].toDouble()));
    }

    // 更新UI状态
    updateUIState();
}

bool ChannelSelect::validateParameters()
{
    // 验证输入参数的有效性
    bool isValid = true;
    QString errorMsg;

    // 验证视频宽度
    bool ok = false;
    int width = ui.videoWidth->text().toInt(&ok);
    if (!ok || width <= 0 || width > 4096) {
        isValid = false;
        errorMsg += LocalQTCompat::fromLocal8Bit("- 视频宽度必须是1-4096之间的有效数字\n");
    }

    // 验证视频高度
    ok = false;
    int height = ui.videoHeigh->text().toInt(&ok);
    if (!ok || height <= 0 || height > 4096) {
        isValid = false;
        errorMsg += LocalQTCompat::fromLocal8Bit("- 视频高度必须是1-4096之间的有效数字\n");
    }

    // 如果有TE值，验证其有效性
    if (!ui.TE_Value->text().isEmpty()) {
        ok = false;
        double te = ui.TE_Value->text().toDouble(&ok);
        if (!ok || te <= 0) {
            isValid = false;
            errorMsg += LocalQTCompat::fromLocal8Bit("- TE值必须是正数\n");
        }
    }

    // 如果验证失败，显示错误信息
    if (!isValid) {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("参数错误"),
            LocalQTCompat::fromLocal8Bit("请修正以下错误：\n") + errorMsg);
    }

    return isValid;
}

void ChannelSelect::onSaveButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置保存按钮点击"));

    // 验证参数有效性
    if (!validateParameters()) {
        return;
    }

    // 获取当前配置
    ChannelConfig config = getCurrentConfig();

    // 发送配置变更信号
    emit channelConfigChanged(config);

    // 关闭窗口
    close();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置已保存并应用"));
}

void ChannelSelect::onCancelButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道配置取消按钮点击"));

    // 直接关闭窗口，不保存更改
    close();
}

void ChannelSelect::onChannelEnableChanged(bool checked)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("通道使能状态已更改"));
    updateUIState();
}

void ChannelSelect::onPNStatusChanged(bool checked)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("PN交换状态已更改"));
}

void ChannelSelect::onCaptureTypeChanged(int index)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("抓取类型已更改为: %1").arg(index));
    updateUIState();
}