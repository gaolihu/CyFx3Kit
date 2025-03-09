#pragma once

#include <QObject>
#include "MenuModel.h"
#include "MenuView.h"

// 前向声明
class QMainWindow;

/**
 * @brief 菜单控制器类，负责协调菜单模型和视图的交互
 *
 * 此类实现MVC架构中的Controller角色，连接模型和视图，
 * 处理用户操作，更新模型和视图。
 */
class MenuController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param mainWindow 主窗口指针
     * @param parent 父对象
     */
    explicit MenuController(QMainWindow* mainWindow, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MenuController();

    /**
     * @brief 初始化菜单控制器
     */
    bool initialize();

    /**
     * @brief 根据应用状态更新菜单状态
     * @param state 应用状态
     */
    void updateMenuStateForAppState(AppState state);

    /**
     * @brief 获取菜单视图
     * @return 菜单视图指针
     */
    MenuView* getMenuView() const;

signals:
    /**
     * @brief 菜单动作触发信号
     * @param actionName 菜单项名称
     */
    void menuActionTriggered(const QString& actionName);

private slots:
    /**
     * @brief 处理视图菜单动作
     * @param actionName 菜单项名称
     */
    void handleMenuAction(const QString& actionName);

    /**
     * @brief 处理应用状态变更
     * @param newState 新状态
     * @param oldState 旧状态
     * @param reason 变更原因
     */
    void handleAppStateChanged(AppState newState, AppState oldState, const QString& reason);

private:
    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 同步模型与视图
     * 确保模型中包含视图中的所有菜单项，反之亦然
     */
    void syncModelWithView();

    /**
     * @brief 根据菜单项名称确定其类型
     * @param actionName 菜单项名称
     * @return 菜单类型
     */
    MenuItemType determineMenuType(const QString& actionName);

private:
    MenuView* m_view;                   ///< 菜单视图
    QMainWindow* m_mainWindow;          ///< 主窗口指针
};