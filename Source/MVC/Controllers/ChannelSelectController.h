// Source/MVC/Controllers/FX3MainController.h
#pragma once

#include <QObject>
#include <QWidget>
#include <memory>

class ChannelSelectView;
class ChannelSelectModel;
namespace Ui { class ChannelSelectClass; }

/**
 * @brief 通道选择控制器
 *
 * 处理通道配置界面的业务逻辑，包括：
 * - 抓取设置
 * - 通道配置和使能
 * - 通道交换和PN交换
 * - 测试模式设置
 * - 视频参数控制
 */
class ChannelSelectController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param view 关联的视图对象
     */
    explicit ChannelSelectController(ChannelSelectView* view);

    /**
     * @brief 析构函数
     */
    ~ChannelSelectController();

    /**
     * @brief 初始化控制器
     * 连接信号和槽，设置初始状态
     */
    void initialize();

    /**
     * @brief 加载通道配置
     * 从模型加载配置并更新UI
     */
    void loadConfig();

    /**
     * @brief 保存通道配置
     * 将UI配置保存到模型
     */
    void saveConfig();

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

public slots:
    /**
     * @brief 处理抓取类型变更
     * @param index 组合框索引
     */
    void onCaptureTypeChanged(int index);

    /**
     * @brief 处理时钟PN交换状态变更
     * @param checked 是否选中
     */
    void onClockPNSwapChanged(bool checked);

    /**
     * @brief 处理通道使能状态变更
     * @param channelIndex 通道索引
     * @param enabled 是否使能
     */
    void onChannelEnableChanged(int channelIndex, bool enabled);

    /**
     * @brief 处理通道PN交换状态变更
     * @param channelIndex 通道索引
     * @param swapped 是否交换
     */
    void onChannelPNSwapChanged(int channelIndex, bool swapped);

    /**
     * @brief 处理通道交换设置变更
     * @param channelIndex 通道索引
     * @param targetChannel 目标通道索引
     */
    void onChannelSwapChanged(int channelIndex, int targetChannel);

    /**
     * @brief 处理测试模式开关状态变更
     * @param enabled 是否启用
     */
    void onTestModeEnabledChanged(bool enabled);

    /**
     * @brief 处理测试模式类型变更
     * @param index 组合框索引
     */
    void onTestModeTypeChanged(int index);

    /**
     * @brief 处理视频高度变更
     * @param height 高度值
     */
    void onVideoHeightChanged(const QString& height);

    /**
     * @brief 处理视频宽度变更
     * @param width 宽度值
     */
    void onVideoWidthChanged(const QString& width);

    /**
     * @brief 处理TE值变更
     * @param teValue TE值
     */
    void onTEValueChanged(const QString& teValue);

    /**
     * @brief 确认保存按钮点击处理
     */
    void onSaveButtonClicked();

    /**
     * @brief 取消设置按钮点击处理
     */
    void onCancelButtonClicked();

private:
    /**
     * @brief 连接UI组件的信号与槽
     */
    void connectSignals();

    /**
     * @brief 更新UI状态
     * 根据当前配置更新组件状态
     */
    void updateUIState();

    /**
     * @brief 验证配置参数
     * @return 验证结果，true表示验证通过
     */
    bool validateConfig();

    /**
     * @brief 应用模型数据到UI
     */
    void applyModelToUI();

    /**
     * @brief 应用UI数据到模型
     */
    void applyUIToModel();

private:
    ChannelSelectView* m_view;                 ///< 视图对象
    Ui::ChannelSelectClass* m_ui;              ///< UI对象
    ChannelSelectModel* m_model;               ///< 模型对象

    bool m_isInitialized;                      ///< 初始化标志
    bool m_isBatchUpdate;                      ///< 批量更新标志
};