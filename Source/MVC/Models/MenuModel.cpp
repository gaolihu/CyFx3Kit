// MVC/Models/MenuModel.cpp
#include "MenuModel.h"
#include "Logger.h"
#include <QSettings>
#include <QMutexLocker>

MenuModel* MenuModel::getInstance()
{
    static MenuModel s;
    return &s;
}

MenuModel::MenuModel()
    : QObject(nullptr)
{
    LOG_INFO("菜单Model构建");

    // 首先初始化默认菜单项
    initializeDefaultMenuItems();

    // 然后尝试从配置加载（可能会覆盖默认设置）
    loadMenuConfig();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单模型已创建"));
}

MenuModel::~MenuModel()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单模型已销毁"));
}

void MenuModel::setMenuItemEnabled(const QString& actionName, bool enabled)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试启用/禁用不存在的菜单项: %1").arg(actionName));
        return;
    }

    if (m_menuItems[actionName].enabled != enabled) {
        m_menuItems[actionName].enabled = enabled;
        emit signal_MN_M_menuItemEnabledChanged(actionName, enabled);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单项状态变更: %1 -> %2")
            .arg(actionName)
            .arg(enabled ? LocalQTCompat::fromLocal8Bit("启用") : LocalQTCompat::fromLocal8Bit("禁用")));
    }
}

bool MenuModel::isMenuItemEnabled(const QString& actionName) const
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试获取不存在的菜单项状态: %1").arg(actionName));
        return false;
    }

    return m_menuItems[actionName].enabled;
}

void MenuModel::setMenuItemVisible(const QString& actionName, bool visible)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试设置不存在的菜单项可见性: %1").arg(actionName));
        return;
    }

    if (m_menuItems[actionName].visible != visible) {
        m_menuItems[actionName].visible = visible;
        emit signal_MN_M_menuItemVisibilityChanged(actionName, visible);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单项可见性变更: %1 -> %2")
            .arg(actionName)
            .arg(visible ? LocalQTCompat::fromLocal8Bit("可见") : LocalQTCompat::fromLocal8Bit("隐藏")));
    }
}

bool MenuModel::isMenuItemVisible(const QString& actionName) const
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试获取不存在的菜单项可见性: %1").arg(actionName));
        return false;
    }

    return m_menuItems[actionName].visible;
}

void MenuModel::setMenuItemText(const QString& actionName, const QString& text)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试设置不存在的菜单项文本: %1").arg(actionName));
        return;
    }

    if (m_menuItems[actionName].text != text) {
        m_menuItems[actionName].text = text;
        emit signal_MN_M_menuItemTextChanged(actionName, text);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单项文本变更: %1 -> \"%2\"")
            .arg(actionName).arg(text));
    }
}

QString MenuModel::getMenuItemText(const QString& actionName) const
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试获取不存在的菜单项文本: %1").arg(actionName));
        return QString();
    }

    return m_menuItems[actionName].text;
}

void MenuModel::setMenuItemIcon(const QString& actionName, const QString& iconPath)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试设置不存在的菜单项图标: %1").arg(actionName));
        return;
    }

    if (m_menuItems[actionName].iconPath != iconPath) {
        m_menuItems[actionName].iconPath = iconPath;
        emit signal_MN_M_menuItemIconChanged(actionName, iconPath);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单项图标变更: %1 -> \"%2\"")
            .arg(actionName).arg(iconPath));
    }
}

QString MenuModel::getMenuItemIcon(const QString& actionName) const
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试获取不存在的菜单项图标: %1").arg(actionName));
        return QString();
    }

    return m_menuItems[actionName].iconPath;
}

void MenuModel::setMenuItemShortcut(const QString& actionName, const QString& shortcut)
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试设置不存在的菜单项快捷键: %1").arg(actionName));
        return;
    }

    if (m_menuItems[actionName].shortcut != shortcut) {
        m_menuItems[actionName].shortcut = shortcut;
        emit signal_MN_M_menuItemShortcutChanged(actionName, shortcut);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单项快捷键变更: %1 -> \"%2\"")
            .arg(actionName).arg(shortcut));
    }
}

QString MenuModel::getMenuItemShortcut(const QString& actionName) const
{
    QMutexLocker locker(&m_dataMutex);

    if (!m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试获取不存在的菜单项快捷键: %1").arg(actionName));
        return QString();
    }

    return m_menuItems[actionName].shortcut;
}

void MenuModel::addMenuItem(const QString& actionName, MenuItemType menuType,
    const QString& text, bool enabled,
    const QString& iconPath, const QString& shortcut)
{
    QMutexLocker locker(&m_dataMutex);

    if (m_menuItems.contains(actionName)) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试添加已存在的菜单项: %1").arg(actionName));
        return;
    }

    MenuItem item;
    item.text = text;
    item.enabled = enabled;
    item.visible = true;
    item.iconPath = iconPath;
    item.shortcut = shortcut;
    item.type = menuType;

    m_menuItems[actionName] = item;

    emit signal_MN_M_menuItemAdded(actionName, menuType);
    // LOG_INFO(LocalQTCompat::fromLocal8Bit("已添加菜单项: %1 (\"%2\")").arg(actionName).arg(text));
}

QStringList MenuModel::getMenuItemsByType(MenuItemType menuType) const
{
    QMutexLocker locker(&m_dataMutex);

    QStringList result;
    for (auto it = m_menuItems.constBegin(); it != m_menuItems.constEnd(); ++it) {
        if (it.value().type == menuType) {
            result.append(it.key());
        }
    }

    return result;
}

QStringList MenuModel::getAllMenuItems() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_menuItems.keys();
}

bool MenuModel::menuItemExists(const QString& actionName) const
{
    QMutexLocker locker(&m_dataMutex);
    return m_menuItems.contains(actionName);
}

bool MenuModel::saveMenuConfig()
{
    try {
        QSettings settings("FX3Tool", "MenuSettings");
        QMutexLocker locker(&m_dataMutex);

        // 保存菜单项配置
        settings.beginGroup("MenuItems");
        for (auto it = m_menuItems.constBegin(); it != m_menuItems.constEnd(); ++it) {
            const QString& key = it.key();
            const MenuItem& item = it.value();

            settings.beginGroup(key);
            settings.setValue("text", item.text);
            settings.setValue("enabled", item.enabled);
            settings.setValue("visible", item.visible);
            settings.setValue("iconPath", item.iconPath);
            settings.setValue("shortcut", item.shortcut);
            settings.setValue("type", static_cast<int>(item.type));
            settings.endGroup();
        }
        settings.endGroup();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单配置已保存到系统设置"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("保存菜单配置异常: %1").arg(e.what()));
        return false;
    }
}

bool MenuModel::loadMenuConfig()
{
    try {
        QSettings settings("FX3Tool", "MenuSettings");

        // 如果没有任何设置，使用默认设置
        if (!settings.childGroups().contains("MenuItems")) {
            LOG_INFO(LocalQTCompat::fromLocal8Bit("未找到菜单配置，使用默认设置"));
            return false;
        }

        QMutexLocker locker(&m_dataMutex);

        // 加载菜单项配置
        settings.beginGroup("MenuItems");
        QStringList menuItems = settings.childGroups();

        for (const QString& key : menuItems) {
            settings.beginGroup(key);

            // 如果是现有菜单项，更新属性
            if (m_menuItems.contains(key)) {
                MenuItem& item = m_menuItems[key];
                item.text = settings.value("text", item.text).toString();
                item.enabled = settings.value("enabled", item.enabled).toBool();
                item.visible = settings.value("visible", item.visible).toBool();
                item.iconPath = settings.value("iconPath", item.iconPath).toString();
                item.shortcut = settings.value("shortcut", item.shortcut).toString();
                // 不从设置中加载类型，保持原始类型
            }
            // 否则，创建新菜单项
            else {
                MenuItem item;
                item.text = settings.value("text").toString();
                item.enabled = settings.value("enabled", true).toBool();
                item.visible = settings.value("visible", true).toBool();
                item.iconPath = settings.value("iconPath").toString();
                item.shortcut = settings.value("shortcut").toString();
                item.type = static_cast<MenuItemType>(settings.value("type", 0).toInt());

                if (!item.text.isEmpty()) {
                    m_menuItems[key] = item;
                }
            }

            settings.endGroup();
        }

        settings.endGroup();

        emit signal_MN_M_menuConfigChanged();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单配置已从系统设置加载"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("加载菜单配置异常: %1").arg(e.what()));
        return false;
    }
}

void MenuModel::updateMenuStateForAppState(AppState state)
{
    // 根据应用状态设置菜单项启用状态
    bool transferring = (state == AppState::TRANSFERRING);
    bool deviceConnected = (state != AppState::DEVICE_ABSENT && state != AppState::DEVICE_ERROR);
    bool idle = (state == AppState::IDLE || state == AppState::CONFIGURED);

    // 传输控制
    setMenuItemEnabled("startAction", idle && deviceConnected);
    setMenuItemEnabled("stopAction", transferring);
    setMenuItemEnabled("resetAction", deviceConnected && !transferring);

    // 功能模块
    setMenuItemEnabled("channelAction", deviceConnected && !transferring);
    setMenuItemEnabled("dataAction", deviceConnected);
    setMenuItemEnabled("videoAction", deviceConnected);
    setMenuItemEnabled("waveformAction", deviceConnected);

    // 文件操作
    setMenuItemEnabled("saveAction", idle);
    setMenuItemEnabled("exportAction", idle);
    setMenuItemEnabled("openAction", idle);
    setMenuItemEnabled("fileOptions", true);

    // 升级操作
    setMenuItemEnabled("updateAction", deviceConnected && !transferring);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已根据应用状态更新菜单状态"));
}

void MenuModel::initializeDefaultMenuItems()
{
    // 文件菜单项
    addMenuItem("openAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("打开命令文件(&O)..."), true, "", "Ctrl+O");
    addMenuItem("saveAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("保存数据(&S)..."), true, "", "Ctrl+S");
    addMenuItem("exportAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("导出数据(&E)..."), true, "", "Ctrl+E");
    addMenuItem("fileOptions", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("文件选项(&I)..."), true, "", "Ctrl+I");
    addMenuItem("exitAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("退出(&X)"), true, "", "Alt+F4");

    // 设备菜单项
    addMenuItem("startAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("开始传输(&S)"), true, "", "F5");
    addMenuItem("stopAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("停止传输(&T)"), false, "", "F6");
    addMenuItem("resetAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("重置设备(&R)"), true, "", "F7");
    addMenuItem("updateAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("设备升级(&U)..."), true);

    // 视图菜单项
    addMenuItem("channelAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("通道配置(&C)"), true, "", "Alt+1");
    addMenuItem("dataAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("数据分析(&D)"), true, "", "Alt+2");
    addMenuItem("videoAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("视频显示(&V)"), true, "", "Alt+3");
    addMenuItem("waveformAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("波形分析(&W)"), true, "", "Alt+4");

    // 工具菜单项
    addMenuItem("settingsAction", MenuItemType::TOOL,
        LocalQTCompat::fromLocal8Bit("设置(&S)..."), true);
    addMenuItem("clearLogAction", MenuItemType::TOOL,
        LocalQTCompat::fromLocal8Bit("清除日志(&C)"), true, "", "Ctrl+L");

    // 帮助菜单项
    addMenuItem("helpContentAction", MenuItemType::HELP,
        LocalQTCompat::fromLocal8Bit("帮助内容(&H)..."), true, "", "F1");
    addMenuItem("aboutAction", MenuItemType::HELP,
        LocalQTCompat::fromLocal8Bit("关于(&A)..."), true);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已初始化默认菜单项"));
}