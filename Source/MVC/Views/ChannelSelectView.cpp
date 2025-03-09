// Source/MVC/Views/ChannelSelectView.cpp
#include "ChannelSelectView.h"
#include "ui_ChannelSelect.h"
#include "ChannelSelectController.h"
#include "ChannelSelectModel.h"
#include "Logger.h"

ChannelSelectView::ChannelSelectView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::ChannelSelectClass)
{
    ui->setupUi(this);

    initializeUI();

    // 创建控制器
    m_controller = std::make_unique<ChannelSelectController>(this);

    // 初始化控制器
    m_controller->initialize();

    LOG_INFO("通道选择视图已创建");
}

ChannelSelectView::~ChannelSelectView()
{
    delete ui;
    LOG_INFO("通道选择视图已销毁");
}

void ChannelSelectView::initializeUI()
{
    // 设置窗口标题
    setWindowTitle("通道配置");

    // 设置窗口标志
    setWindowFlags(Qt::Dialog);

    // 设置窗口模态
    setWindowModality(Qt::ApplicationModal);
}

void ChannelSelectView::acceptConfig()
{
    if (m_controller) {
        // 获取当前配置
        auto model = ChannelSelectModel::getInstance();
        ChannelConfig config = model->getConfig();

        // 发送配置变更信号
        emit configChanged(config);
    }

    LOG_INFO("通道配置已接受");

    // 关闭窗口
    this->close();
}

void ChannelSelectView::rejectConfig()
{
    LOG_INFO("通道配置已拒绝");

    // 关闭窗口
    this->close();
}