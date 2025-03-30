#pragma once

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMap>
#include <QIcon>
#include "MenuModel.h"

// 前置声明
namespace Ui {
    class MenuBar;
}

/**
 * @brief 菜单视图类，负责菜单的显示和用户交互
 *
 * 此类封装了菜单视图的创建和更新逻辑，接收模型变更通知和用户操作。
 * 它是MVC架构中的View角色，负责与用户界面交互。
 */
class MenuView : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit MenuView(QMainWindow* mainWindow, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MenuView();

    /**
     * @brief 初始化菜单栏
     */
    void initializeMenuBar();

    /**
     * @brief 获取指定菜单项的QAction指针
     * @param actionName 菜单项名称
     * @return QAction指针，若不存在则返回nullptr
     */
    QAction* getMenuAction(const QString& actionName) const;

    /**
     * @brief 获取菜单栏
     * @return 菜单栏指针
     */
    QMenuBar* getMenuBar() const;

    /**
     * @brief 添加自定义菜单项
     * @param actionName 菜单项名称
     * @param menuType 菜单类型
     * @param text 菜单文本
     * @param enabled 是否启用
     * @param iconPath 图标路径（可选）
     * @param shortcut 快捷键（可选）
     * @return 新创建的QAction指针
     */
    QAction* addMenuItem(const QString& actionName, MenuItemType menuType,
        const QString& text, bool enabled = true,
        const QString& iconPath = QString(),
        const QString& shortcut = QString());

    /**
 * @brief 获取所有菜单项名称
 * @return 所有菜单项名称列表
 */
    QStringList getAllMenuActions() const
    {
        return m_actions.keys();
    }

public slots:
    /**
     * @brief 设置菜单项启用状态
     * @param actionName 菜单项名称
     * @param enabled 是否启用
     */
    void slot_MN_V_setMenuItemEnabled(const QString& actionName, bool enabled);

    /**
     * @brief 设置菜单项可见状态
     * @param actionName 菜单项名称
     * @param visible 是否可见
     */
    void slot_MN_V_setMenuItemVisible(const QString& actionName, bool visible);

    /**
     * @brief 设置菜单项文本
     * @param actionName 菜单项名称
     * @param text 菜单文本
     */
    void slot_MN_V_setMenuItemText(const QString& actionName, const QString& text);

    /**
     * @brief 设置菜单项图标
     * @param actionName 菜单项名称
     * @param iconPath 图标路径
     */
    void slot_MN_V_setMenuItemIcon(const QString& actionName, const QString& iconPath);

    /**
     * @brief 设置菜单项快捷键
     * @param actionName 菜单项名称
     * @param shortcut 快捷键文本
     */
    void slot_MN_V_setMenuItemShortcut(const QString& actionName, const QString& shortcut);

    /**
     * @brief 处理新菜单项添加
     * @param actionName 菜单项名称
     * @param menuType 菜单类型
     */
    void slot_MN_V_menuItemAdded(const QString& actionName, MenuItemType menuType);

    /**
     * @brief 处理菜单配置变更
     */
    void slot_MN_V_menuConfigChanged();

signals:
    /**
     * @brief 菜单动作触发信号
     * @param actionName 菜单项名称
     */
    void signal_MN_V_menuActionTriggered(const QString& actionName);

private slots:
    /**
     * @brief 处理菜单动作
     */
    void slot_MN_V_onMenuAction();

private:
    /**
     * @brief 创建菜单
     */
    void createMenus();

    /**
     * @brief 连接模型信号
     */
    void connectModelSignals();

    /**
     * @brief 获取指定类型的菜单
     * @param type 菜单类型
     * @return 菜单指针，若不存在则返回nullptr
     */
    QMenu* getMenuByType(MenuItemType type) const;

    /**
     * @brief 从模型同步菜单状态
     */
    void syncMenusFromModel();

    /**
     * @brief 创建菜单项
     * @param actionName 菜单项名称
     * @param menuType 菜单类型
     * @param text 菜单文本
     * @param enabled 是否启用
     * @param iconPath 图标路径（可选）
     * @param shortcut 快捷键（可选）
     * @return 创建的 QAction 指针
     */
    QAction* createMenuItem(const QString& actionName, MenuItemType menuType,
        const QString& text, bool enabled = true,
        const QString& iconPath = QString(),
        const QString& shortcut = QString());

private:
    QMainWindow* m_mainWindow;                 ///< 主窗口指针
    QMap<QString, QAction*> m_actions;         ///< 菜单动作映射表
    QMap<MenuItemType, QMenu*> m_menus;        ///< 菜单类型映射表
};