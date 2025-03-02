#pragma once

#include <QObject>
#include <QMenu>
#include <QAction>
#include <QMenuBar>
#include <QMap>
#include <QString>

// 前向声明
class FX3ToolMainWin;

// 需要包含这个头文件以获取AppState枚举
#include "AppStateMachine.h"

/**
 * @brief 菜单控制器类，负责管理主窗口的菜单栏
 *
 * 此类封装了菜单栏的创建、更新和事件处理逻辑，
 * 从主窗口分离菜单管理职责，提高代码可维护性。
 */
class FX3MenuController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param mainWindow 主窗口指针
     */
    explicit FX3MenuController(FX3ToolMainWin* mainWindow);

    /**
     * @brief 初始化菜单栏
     *
     * 创建菜单结构并设置初始状态
     */
    void setupMenuBar();

    /**
     * @brief 根据应用状态更新菜单栏
     *
     * 根据当前应用状态启用或禁用菜单项
     *
     * @param state 当前应用状态
     */
    void updateMenuBarState(AppState state);

signals:
    /**
     * @brief 菜单动作触发信号
     *
     * 当用户点击菜单项时发射此信号
     *
     * @param action 被触发的动作名称
     */
    void menuActionTriggered(const QString& action);

public slots:
    /**
     * @brief 处理菜单动作
     *
     * 当菜单项被点击时调用此槽函数
     *
     * @param action 被触发的QAction指针
     */
    void onMenuAction(QAction* action);

private:
    FX3ToolMainWin* m_mainWindow;                    ///< 主窗口指针
    QMap<QString, QAction*> m_menuActions;           ///< 菜单动作映射表
};