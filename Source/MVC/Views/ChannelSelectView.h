// Source/MVC/Views/ChannelSelectView.h
#pragma once

#include <QWidget>
#include <memory>

namespace Ui {
    class ChannelSelectClass;
}

class ChannelSelectController;
struct ChannelConfig;

/**
 * @brief 通道选择视图类
 *
 * 负责显示通道配置界面
 */
class ChannelSelectView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit ChannelSelectView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ChannelSelectView();

    /**
     * @brief 获取UI对象
     * @return UI对象指针
     */
    Ui::ChannelSelectClass* getUi() { return ui; }

    /**
     * @brief 接受配置
     * 关闭窗口并返回 Accepted 结果
     */
    void acceptConfig();

    /**
     * @brief 拒绝配置
     * 关闭窗口并返回 Rejected 结果
     */
    void rejectConfig();

signals:
    /**
     * @brief 配置变更信号
     * @param config 新的通道配置
     */
    void configChanged(const ChannelConfig& config);

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

private:
    Ui::ChannelSelectClass* ui;                       ///< UI对象
    std::unique_ptr<ChannelSelectController> m_controller; ///< 控制器对象
};