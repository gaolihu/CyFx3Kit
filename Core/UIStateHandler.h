#pragma once

#include <QObject>
#include <QString>
#include "ui_FX3ToolMainWin.h"
#include "AppStateMachine.h"

/**
 * @brief UI状态处理器类，负责管理UI状态
 *
 * 此类根据应用程序状态更新UI元素，处理状态转换和显示反馈。
 */
class UIStateHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param ui UI对象引用
     * @param parent 父对象
     */
    explicit UIStateHandler(Ui::FX3ToolMainWinClass& ui, QObject* parent = nullptr);

    /**
     * @brief 更新传输统计信息
     * @param transferSpeed 传输速度(MB/s)
     * @param totalBytes 总字节数
     * @param totalTime 总时间(秒)
     */
    void updateTransferStats(double transferSpeed, uint64_t totalBytes, uint32_t totalTime);

    /**
     * @brief 更新USB速度显示
     * @param speed USB速度(0=未知, 1=FS, 2=HS, 3=SS)
     */
    void updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3);

    /**
     * @brief 显示错误消息
     * @param errorMsg 错误消息
     * @param details 详细信息
     */
    void showErrorMessage(const QString& errorMsg, const QString& details);

    /**
     * @brief 准备关闭
     *
     * 在应用程序关闭前调用，防止后续UI访问
     */
    void prepareForClose();

public slots:
    /**
     * @brief 处理状态变化
     * @param newState 新状态
     * @param oldState 旧状态
     * @param reason 原因
     */
    void onStateChanged(AppState newState, AppState oldState, const QString& reason);

private:
    /**
     * @brief 更新按钮状态
     * @param state 应用状态
     */
    void updateButtonStates(AppState state);

    /**
     * @brief 更新状态标签
     * @param state 应用状态
     */
    void updateStatusLabels(AppState state);

    /**
     * @brief 检查UI是否有效
     * @return 如果UI有效返回true，否则返回false
     */
    bool validUI() const;

    /**
     * @brief 更新UI控件启用/禁用状态
     * @param enableStart 是否启用开始按钮
     * @param enableStop 是否启用停止按钮
     * @param enableReset 是否启用重置按钮
     */
    void updateButtons(bool enableStart, bool enableStop, bool enableReset);

    /**
     * @brief 更新状态文本
     * @param statusText USB状态文本
     * @param transferStatusText 传输状态文本
     */
    void updateStatusLabels(const QString& statusText, const QString& transferStatusText);

    Ui::FX3ToolMainWinClass& m_ui;  ///< UI引用
    bool m_validUI;                 ///< UI有效性标志
};