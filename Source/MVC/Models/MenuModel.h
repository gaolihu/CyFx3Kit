// MVC/Models/MenuModel.h
#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QMutex>
#include <memory>

// 包含AppState枚举
#include "AppStateMachine.h"

/**
 * @brief 菜单项类型枚举
 */
enum class MenuItemType {
    FILE,       ///< 文件菜单项
    DEVICE,     ///< 设备菜单项
    VIEW,       ///< 视图菜单项
    TOOL,       ///< 工具菜单项
    SETTING,    ///< 设置菜单项
    HELP        ///< 帮助菜单项
};

/**
 * @brief 菜单模型类，负责管理菜单数据和状态
 *
 * 此类采用单例模式，使用MVC架构中的Model角色，
 * 存储菜单项的数据和状态，并提供数据访问接口。
 */
class MenuModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的引用
     */
    static MenuModel* getInstance();

    /**
     * @brief 设置菜单项启用状态
     * @param actionName 菜单项名称
     * @param enabled 是否启用
     */
    void setMenuItemEnabled(const QString& actionName, bool enabled);

    /**
     * @brief 获取菜单项启用状态
     * @param actionName 菜单项名称
     * @return 菜单项是否启用
     */
    bool isMenuItemEnabled(const QString& actionName) const;

    /**
     * @brief 设置菜单项可见状态
     * @param actionName 菜单项名称
     * @param visible 是否可见
     */
    void setMenuItemVisible(const QString& actionName, bool visible);

    /**
     * @brief 获取菜单项可见状态
     * @param actionName 菜单项名称
     * @return 菜单项是否可见
     */
    bool isMenuItemVisible(const QString& actionName) const;

    /**
     * @brief 设置菜单项文本
     * @param actionName 菜单项名称
     * @param text 菜单文本
     */
    void setMenuItemText(const QString& actionName, const QString& text);

    /**
     * @brief 获取菜单项文本
     * @param actionName 菜单项名称
     * @return 菜单项文本
     */
    QString getMenuItemText(const QString& actionName) const;

    /**
     * @brief 设置菜单项图标
     * @param actionName 菜单项名称
     * @param iconPath 图标路径
     */
    void setMenuItemIcon(const QString& actionName, const QString& iconPath);

    /**
     * @brief 获取菜单项图标路径
     * @param actionName 菜单项名称
     * @return 菜单项图标路径
     */
    QString getMenuItemIcon(const QString& actionName) const;

    /**
     * @brief 设置菜单项快捷键
     * @param actionName 菜单项名称
     * @param shortcut 快捷键文本
     */
    void setMenuItemShortcut(const QString& actionName, const QString& shortcut);

    /**
     * @brief 获取菜单项快捷键
     * @param actionName 菜单项名称
     * @return 菜单项快捷键文本
     */
    QString getMenuItemShortcut(const QString& actionName) const;

    /**
     * @brief 添加自定义菜单项
     * @param actionName 菜单项名称
     * @param menuType 菜单类型
     * @param text 菜单文本
     * @param enabled 是否启用
     * @param iconPath 图标路径（可选）
     * @param shortcut 快捷键（可选）
     */
    void addMenuItem(const QString& actionName, MenuItemType menuType,
        const QString& text, bool enabled = true,
        const QString& iconPath = QString(),
        const QString& shortcut = QString());

    /**
     * @brief 获取指定类型的所有菜单项名称
     * @param menuType 菜单类型
     * @return 菜单项名称列表
     */
    QStringList getMenuItemsByType(MenuItemType menuType) const;

    /**
     * @brief 获取所有菜单项名称
     * @return 所有菜单项名称列表
     */
    QStringList getAllMenuItems() const;

    /**
     * @brief 菜单项是否存在
     * @param actionName 菜单项名称
     * @return 是否存在
     */
    bool menuItemExists(const QString& actionName) const;

    /**
     * @brief 保存菜单配置到设置
     * @return 是否保存成功
     */
    bool saveMenuConfig();

    /**
     * @brief 从设置加载菜单配置
     * @return 是否加载成功
     */
    bool loadMenuConfig();

    /**
     * @brief 根据应用状态更新菜单状态
     * @param state 应用状态
     */
    void updateMenuStateForAppState(AppState state);

signals:
    /**
     * @brief 菜单项启用状态变更信号
     * @param actionName 菜单项名称
     * @param enabled 是否启用
     */
    void menuItemEnabledChanged(const QString& actionName, bool enabled);

    /**
     * @brief 菜单项可见状态变更信号
     * @param actionName 菜单项名称
     * @param visible 是否可见
     */
    void menuItemVisibilityChanged(const QString& actionName, bool visible);

    /**
     * @brief 菜单项文本变更信号
     * @param actionName 菜单项名称
     * @param text 新的文本
     */
    void menuItemTextChanged(const QString& actionName, const QString& text);

    /**
     * @brief 菜单项图标变更信号
     * @param actionName 菜单项名称
     * @param iconPath 新的图标路径
     */
    void menuItemIconChanged(const QString& actionName, const QString& iconPath);

    /**
     * @brief 菜单项快捷键变更信号
     * @param actionName 菜单项名称
     * @param shortcut 新的快捷键
     */
    void menuItemShortcutChanged(const QString& actionName, const QString& shortcut);

    /**
     * @brief 菜单项添加信号
     * @param actionName 菜单项名称
     * @param menuType 菜单类型
     */
    void menuItemAdded(const QString& actionName, MenuItemType menuType);

    /**
     * @brief 菜单配置变更信号
     */
    void menuConfigChanged();

private:
    /**
     * @brief 构造函数 (私有)
     */
    MenuModel();

    /**
     * @brief 析构函数 (私有)
     */
    ~MenuModel();

    /**
     * @brief 禁用拷贝构造函数
     */
    MenuModel(const MenuModel&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    MenuModel& operator=(const MenuModel&) = delete;

    /**
     * @brief 初始化默认菜单项
     */
    void initializeDefaultMenuItems();

private:
    /**
     * @brief 菜单项数据结构
     */
    struct MenuItem {
        QString text;           ///< 菜单文本
        bool enabled;           ///< 是否启用
        bool visible;           ///< 是否可见
        QString iconPath;       ///< 图标路径
        QString shortcut;       ///< 快捷键
        MenuItemType type;      ///< 菜单类型
    };

    QMap<QString, MenuItem> m_menuItems;  ///< 菜单项映射表
    mutable QMutex m_dataMutex;           ///< 数据互斥锁
};