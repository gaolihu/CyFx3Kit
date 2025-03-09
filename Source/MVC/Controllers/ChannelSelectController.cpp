// Source/MVC/Controllers/ChannelSelectController.cpp

#include "ChannelSelectController.h"
#include "ChannelSelectView.h"
#include "ui_ChannelSelect.h"
#include "ChannelSelectModel.h"
#include "Logger.h"
#include <QMessageBox>

ChannelSelectController::ChannelSelectController(ChannelSelectView* view)
    : QObject(view)
    , m_view(view)
    , m_ui(nullptr)
    , m_isInitialized(false)
    , m_isBatchUpdate(false)
{
    // 获取UI对象
    if (m_view) {
        m_ui = m_view->getUi();
    }

    // 获取或创建模型实例
    m_model = ChannelSelectModel::getInstance();
}

ChannelSelectController::~ChannelSelectController()
{
    // 不需要释放m_model，因为它是共享指针
    // 不需要释放m_ui和m_view，因为它们由外部管理
    LOG_INFO("通道选择控制器已销毁");
}

void ChannelSelectController::initialize()
{
    if (m_isInitialized || !m_ui) {
        return;
    }

    // 连接信号与槽
    connectSignals();

    // 加载配置
    loadConfig();

    // 更新UI状态
    updateUIState();

    m_isInitialized = true;
    LOG_INFO("通道选择控制器已初始化");
}

void ChannelSelectController::loadConfig()
{
    if (!m_model || !m_ui) {
        LOG_ERROR("加载配置失败：模型或UI对象为空");
        return;
    }

    // 设置批量更新标志，避免中间状态触发不必要的信号
    m_isBatchUpdate = true;

    // 应用模型数据到UI
    applyModelToUI();

    m_isBatchUpdate = false;
    LOG_INFO("通道配置已加载");
}

void ChannelSelectController::saveConfig()
{
    if (!m_model || !m_ui) {
        LOG_ERROR("保存配置失败：模型或UI对象为空");
        return;
    }

    // 验证配置
    if (!validateConfig()) {
        return;
    }

    // 应用UI数据到模型
    applyUIToModel();

    // 保存配置到存储
    m_model->saveConfig();

    LOG_INFO("通道配置已保存");
}

void ChannelSelectController::resetToDefault()
{
    if (!m_model || !m_ui) {
        return;
    }

    // 重置模型到默认状态
    m_model->resetToDefault();

    // 应用默认配置到UI
    m_isBatchUpdate = true;
    applyModelToUI();
    m_isBatchUpdate = false;

    LOG_INFO("通道配置已重置为默认值");
}

void ChannelSelectController::connectSignals()
{
    if (!m_ui) {
        return;
    }

    // 抓取设置
    connect(m_ui->comboBox_5, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ChannelSelectController::onCaptureTypeChanged);
    connect(m_ui->CLK_PN, &QCheckBox::toggled,
        this, &ChannelSelectController::onClockPNSwapChanged);

    // 通道使能
    connect(m_ui->CHen_0, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelEnableChanged(0, checked); });
    connect(m_ui->CHen_1, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelEnableChanged(1, checked); });
    connect(m_ui->CHen_2, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelEnableChanged(2, checked); });
    connect(m_ui->CHen_3, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelEnableChanged(3, checked); });

    // 通道PN交换
    connect(m_ui->PN_0, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelPNSwapChanged(0, checked); });
    connect(m_ui->PN_1, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelPNSwapChanged(1, checked); });
    connect(m_ui->PN_2, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelPNSwapChanged(2, checked); });
    connect(m_ui->PN_3, &QCheckBox::toggled,
        this, [this](bool checked) { onChannelPNSwapChanged(3, checked); });

    // 通道交换
    connect(m_ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { onChannelSwapChanged(0, index); });
    connect(m_ui->comboBox_2, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { onChannelSwapChanged(1, index); });
    connect(m_ui->comboBox_3, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { onChannelSwapChanged(2, index); });
    connect(m_ui->comboBox_4, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int index) { onChannelSwapChanged(3, index); });

    // 测试模式
    connect(m_ui->checkBox, &QCheckBox::toggled,
        this, &ChannelSelectController::onTestModeEnabledChanged);
    connect(m_ui->comboBox_6, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ChannelSelectController::onTestModeTypeChanged);

    // 视频参数
    connect(m_ui->videoHeigh, &QLineEdit::textChanged,
        this, &ChannelSelectController::onVideoHeightChanged);
    connect(m_ui->videoWidth, &QLineEdit::textChanged,
        this, &ChannelSelectController::onVideoWidthChanged);
    connect(m_ui->TE_Value, &QLineEdit::textChanged,
        this, &ChannelSelectController::onTEValueChanged);

    // 按钮点击
    connect(m_ui->pushButton, &QPushButton::clicked,
        this, &ChannelSelectController::onSaveButtonClicked);
    connect(m_ui->pushButton_2, &QPushButton::clicked,
        this, &ChannelSelectController::onCancelButtonClicked);
}

void ChannelSelectController::updateUIState()
{
    if (!m_ui) {
        return;
    }

    // 更新测试模式控件状态
    bool testModeEnabled = m_ui->checkBox->isChecked();
    m_ui->comboBox_6->setEnabled(testModeEnabled);

    // 获取抓取类型
    int captureType = m_ui->comboBox_5->currentIndex();

    // 更新通道使能状态
    // 注意：第一个通道(BYTE0)始终启用，不可禁用
    m_ui->CHen_0->setEnabled(false);
    m_ui->CHen_0->setChecked(true);

    // 更新UI元素的禁用状态
    // 根据抓取类型和其他条件可能需要禁用某些控件

    LOG_INFO("UI状态已更新");
}

bool ChannelSelectController::validateConfig()
{
    if (!m_ui) {
        return false;
    }

    bool isValid = true;
    QString errorMessage;

    // 验证视频高度
    bool ok = false;
    int height = m_ui->videoHeigh->text().toInt(&ok);
    if (!ok || height <= 0 || height > 4096) {
        isValid = false;
        errorMessage += "视频高度无效，请输入1-4096之间的值\n";
    }

    // 验证视频宽度
    ok = false;
    int width = m_ui->videoWidth->text().toInt(&ok);
    if (!ok || width <= 0 || width > 4096) {
        isValid = false;
        errorMessage += "视频宽度无效，请输入1-4096之间的值\n";
    }

    // 验证TE值
    ok = false;
    double teValue = m_ui->TE_Value->text().toDouble(&ok);
    if (!ok || teValue < 0) {
        isValid = false;
        errorMessage += "TE值无效，请输入大于0的数值\n";
    }

    // 如果验证失败，显示错误消息
    if (!isValid && m_view) {
        QMessageBox::warning(m_view, "配置验证错误", errorMessage);
    }

    return isValid;
}

void ChannelSelectController::applyModelToUI()
{
    if (!m_model || !m_ui) {
        return;
    }

    // 获取模型数据
    ChannelConfig config = m_model->getConfig();

    // 设置抓取类型
    m_ui->comboBox_5->setCurrentIndex(config.captureType);

    // 设置时钟PN交换
    m_ui->CLK_PN->setChecked(config.clockPNSwap);

    // 设置通道使能
    m_ui->CHen_0->setChecked(config.channelEnabled[0]);
    m_ui->CHen_1->setChecked(config.channelEnabled[1]);
    m_ui->CHen_2->setChecked(config.channelEnabled[2]);
    m_ui->CHen_3->setChecked(config.channelEnabled[3]);

    // 设置通道PN交换
    m_ui->PN_0->setChecked(config.channelPNSwap[0]);
    m_ui->PN_1->setChecked(config.channelPNSwap[1]);
    m_ui->PN_2->setChecked(config.channelPNSwap[2]);
    m_ui->PN_3->setChecked(config.channelPNSwap[3]);

    // 设置通道交换
    m_ui->comboBox->setCurrentIndex(config.channelSwap[0]);
    m_ui->comboBox_2->setCurrentIndex(config.channelSwap[1]);
    m_ui->comboBox_3->setCurrentIndex(config.channelSwap[2]);
    m_ui->comboBox_4->setCurrentIndex(config.channelSwap[3]);

    // 设置测试模式
    m_ui->checkBox->setChecked(config.testModeEnabled);
    m_ui->comboBox_6->setCurrentIndex(config.testModeType);

    // 设置视频参数
    m_ui->videoHeigh->setText(QString::number(config.videoHeight));
    m_ui->videoWidth->setText(QString::number(config.videoWidth));
    m_ui->TE_Value->setText(QString::number(config.teValue));
}

void ChannelSelectController::applyUIToModel()
{
    if (!m_model || !m_ui) {
        return;
    }

    // 创建新的配置对象
    ChannelConfig config;

    // 获取抓取类型
    config.captureType = m_ui->comboBox_5->currentIndex();

    // 获取时钟PN交换状态
    config.clockPNSwap = m_ui->CLK_PN->isChecked();

    // 获取通道使能状态
    config.channelEnabled[0] = m_ui->CHen_0->isChecked();
    config.channelEnabled[1] = m_ui->CHen_1->isChecked();
    config.channelEnabled[2] = m_ui->CHen_2->isChecked();
    config.channelEnabled[3] = m_ui->CHen_3->isChecked();

    // 获取通道PN交换状态
    config.channelPNSwap[0] = m_ui->PN_0->isChecked();
    config.channelPNSwap[1] = m_ui->PN_1->isChecked();
    config.channelPNSwap[2] = m_ui->PN_2->isChecked();
    config.channelPNSwap[3] = m_ui->PN_3->isChecked();

    // 获取通道交换设置
    config.channelSwap[0] = m_ui->comboBox->currentIndex();
    config.channelSwap[1] = m_ui->comboBox_2->currentIndex();
    config.channelSwap[2] = m_ui->comboBox_3->currentIndex();
    config.channelSwap[3] = m_ui->comboBox_4->currentIndex();

    // 获取测试模式设置
    config.testModeEnabled = m_ui->checkBox->isChecked();
    config.testModeType = m_ui->comboBox_6->currentIndex();

    // 获取视频参数
    config.videoHeight = m_ui->videoHeigh->text().toInt();
    config.videoWidth = m_ui->videoWidth->text().toInt();
    config.teValue = m_ui->TE_Value->text().toDouble();

    // 更新模型
    m_model->setConfig(config);
}

// 槽函数实现
void ChannelSelectController::onCaptureTypeChanged(int index)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("抓取类型已更改为: %1").arg(index));

    // 根据不同的抓取类型更新UI状态
    updateUIState();
}

void ChannelSelectController::onClockPNSwapChanged(bool checked)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("时钟PN交换状态已更改为: %1").arg(checked ? "启用" : "禁用"));
}

void ChannelSelectController::onChannelEnableChanged(int channelIndex, bool enabled)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("通道%1使能状态已更改为: %2")
        .arg(channelIndex)
        .arg(enabled ? "启用" : "禁用"));

    // 特殊处理：第一个通道不可禁用
    if (channelIndex == 0 && !enabled) {
        m_ui->CHen_0->setChecked(true);
        QMessageBox::information(m_view, "提示", "BYTE0通道不可禁用");
        return;
    }
}

void ChannelSelectController::onChannelPNSwapChanged(int channelIndex, bool swapped)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("通道%1 PN交换状态已更改为: %2")
        .arg(channelIndex)
        .arg(swapped ? "启用" : "禁用"));
}

void ChannelSelectController::onChannelSwapChanged(int channelIndex, int targetChannel)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("通道%1交换设置已更改为BYTE%2")
        .arg(channelIndex)
        .arg(targetChannel));
}

void ChannelSelectController::onTestModeEnabledChanged(bool enabled)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("测试模式已%1").arg(enabled ? "启用" : "禁用"));

    // 更新测试模式相关UI状态
    updateUIState();
}

void ChannelSelectController::onTestModeTypeChanged(int index)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("测试模式类型已更改为: %1").arg(index));
}

void ChannelSelectController::onVideoHeightChanged(const QString& height)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("视频高度已更改为: %1").arg(height));
}

void ChannelSelectController::onVideoWidthChanged(const QString& width)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("视频宽度已更改为: %1").arg(width));
}

void ChannelSelectController::onTEValueChanged(const QString& teValue)
{
    if (m_isBatchUpdate) {
        return;
    }

    LOG_INFO(QString("TE值已更改为: %1").arg(teValue));
}

void ChannelSelectController::onSaveButtonClicked()
{
    LOG_INFO("确认保存按钮点击");

    // 保存配置
    saveConfig();

    // 通知视图关闭或其他操作
    if (m_view) {
        m_view->acceptConfig();
    }
}

void ChannelSelectController::onCancelButtonClicked()
{
    LOG_INFO("取消设置按钮点击");

    // 重新加载配置（恢复之前的设置）
    loadConfig();

    // 通知视图关闭或其他操作
    if (m_view) {
        m_view->rejectConfig();
    }
}