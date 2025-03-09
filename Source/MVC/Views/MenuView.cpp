#include "MenuView.h"
#include "Logger.h"
#include "AppStateMachine.h"
#include <QIcon>
#include <QKeySequence>

MenuView::MenuView(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    LOG_INFO("菜单view构建");

    if (m_mainWindow) {
        // 直接创建菜单结构
        createMenus();

        // 连接模型信号
        connectModelSignals();

        // 从模型同步菜单状态
        syncMenusFromModel();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单视图已创建"));
    }
    else {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建菜单视图失败：主窗口指针为空"));
    }
}

MenuView::~MenuView()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单视图已销毁"));
}

void MenuView::initializeMenuBar()
{
    try {
        // 对每个菜单项设置连接
        for (const QString& actionName : MenuModel::getInstance()->getAllMenuItems()) {
            QAction* action = findChild<QAction*>(actionName);
            if (action) {
                bool connected = connect(action, &QAction::triggered, this,
                    [this, actionName]() {
                        emit menuActionTriggered(actionName);
                    });
                if (!connected) {
                    LOG_WARN(LocalQTCompat::fromLocal8Bit("菜单项信号连接失败: %1").arg(actionName));
                }
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化菜单栏异常: %1").arg(e.what()));
    }
}

QAction* MenuView::getMenuAction(const QString& actionName) const
{
    return m_actions.value(actionName, nullptr);
}

QMenuBar* MenuView::getMenuBar() const
{
    return m_mainWindow ? m_mainWindow->menuBar() : nullptr;
}

QAction* MenuView::addMenuItem(const QString& actionName, MenuItemType menuType,
    const QString& text, bool enabled,
    const QString& iconPath, const QString& shortcut)
{
    // 获取目标菜单
    QMenu* targetMenu = getMenuByType(menuType);
    if (!targetMenu) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("添加菜单项失败：未找到目标菜单类型 %1").arg(static_cast<int>(menuType)));
        return nullptr;
    }

    // 创建新的QAction
    QAction* action = new QAction(text, targetMenu);
    action->setEnabled(enabled);

    // 设置图标（如果有）
    if (!iconPath.isEmpty()) {
        action->setIcon(QIcon(iconPath));
    }

    // 设置快捷键（如果有）
    if (!shortcut.isEmpty()) {
        action->setShortcut(QKeySequence(shortcut));
    }

    // 设置对象名
    action->setObjectName(actionName);

    // 添加到目标菜单
    targetMenu->addAction(action);

    // 添加到映射表
    m_actions[actionName] = action;

    // 连接信号
    connect(action, &QAction::triggered, this, &MenuView::onMenuAction);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已添加菜单项: %1").arg(actionName));

    return action;
}

void MenuView::slot_setMenuItemEnabled(const QString& actionName, bool enabled)
{
    QAction* action = getMenuAction(actionName);
    if (action) {
        action->setEnabled(enabled);
    }
}

void MenuView::slot_setMenuItemVisible(const QString& actionName, bool visible)
{
    QAction* action = getMenuAction(actionName);
    if (action) {
        action->setVisible(visible);
    }
}

void MenuView::slot_setMenuItemText(const QString& actionName, const QString& text)
{
    QAction* action = getMenuAction(actionName);
    if (action) {
        action->setText(text);
    }
}

void MenuView::slot_setMenuItemIcon(const QString& actionName, const QString& iconPath)
{
    QAction* action = getMenuAction(actionName);
    if (action && !iconPath.isEmpty()) {
        action->setIcon(QIcon(iconPath));
    }
}

void MenuView::slot_setMenuItemShortcut(const QString& actionName, const QString& shortcut)
{
    QAction* action = getMenuAction(actionName);
    if (action && !shortcut.isEmpty()) {
        action->setShortcut(QKeySequence(shortcut));
    }
}

void MenuView::slot_menuItemAdded(const QString& actionName, MenuItemType menuType)
{
    // 如果菜单项不存在，从模型添加
    if (!m_actions.contains(actionName)) {
        MenuModel* model = MenuModel::getInstance();

        QString text = model->getMenuItemText(actionName);
        bool enabled = model->isMenuItemEnabled(actionName);
        QString iconPath = model->getMenuItemIcon(actionName);
        QString shortcut = model->getMenuItemShortcut(actionName);

        addMenuItem(actionName, menuType, text, enabled, iconPath, shortcut);
    }
    else {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单项已存在: %1").arg(actionName));
        return;
    }
}

void MenuView::slot_menuConfigChanged()
{
    // 同步菜单状态
    syncMenusFromModel();
}

void MenuView::onMenuAction()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action) {
        QString actionName = action->objectName();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单动作触发: %1").arg(actionName));
        emit menuActionTriggered(actionName);
    }
}

void MenuView::createMenus()
{
    QMenuBar* menuBar = getMenuBar();
    if (!menuBar) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建菜单失败：菜单栏为空"));
        return;
    }

    // 清空菜单栏
    menuBar->clear();
    m_menus.clear();
    m_actions.clear();

    // 创建各类菜单
    m_menus[MenuItemType::FILE] = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("文件(&F)"));
    m_menus[MenuItemType::DEVICE] = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("设备(&D)"));
    m_menus[MenuItemType::VIEW] = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("视图(&V)"));
    m_menus[MenuItemType::TOOL] = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("工具(&T)"));
    //m_menus[MenuItemType::SETTING] = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("设置(&S)"));
    m_menus[MenuItemType::HELP] = menuBar->addMenu(LocalQTCompat::fromLocal8Bit("帮助(&H)"));

    // 创建并添加标准菜单项
    createMenuItem("openAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("打开命令文件(&O)..."), true, "", "Ctrl+O");
    createMenuItem("saveAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("保存数据(&S)..."), true, "", "Ctrl+S");
    createMenuItem("exportAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("导出数据(&E)..."), true, "", "Ctrl+E");
    m_menus[MenuItemType::FILE]->addSeparator();
    createMenuItem("fileOptions", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("文件选项(&I)..."), true, "", "Ctrl+I");
    m_menus[MenuItemType::FILE]->addSeparator();
    createMenuItem("exitAction", MenuItemType::FILE,
        LocalQTCompat::fromLocal8Bit("退出(&X)"), true, "", "Alt+F4");

    createMenuItem("startAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("开始传输(&S)"), true, "", "F5");
    createMenuItem("stopAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("停止传输(&T)"), false, "", "F6");
    createMenuItem("resetAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("重置设备(&R)"), true, "", "F7");
    m_menus[MenuItemType::DEVICE]->addSeparator();
    createMenuItem("updateAction", MenuItemType::DEVICE,
        LocalQTCompat::fromLocal8Bit("设备升级(&U)..."), true);

    createMenuItem("channelAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("通道配置(&C)"), true, "", "Alt+1");
    createMenuItem("dataAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("数据分析(&D)"), true, "", "Alt+2");
    createMenuItem("videoAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("视频显示(&V)"), true, "", "Alt+3");
    createMenuItem("waveformAction", MenuItemType::VIEW,
        LocalQTCompat::fromLocal8Bit("波形分析(&W)"), true, "", "Alt+4");

    createMenuItem("settingsAction", MenuItemType::TOOL,
        LocalQTCompat::fromLocal8Bit("设置(&S)..."), true);
    createMenuItem("clearLogAction", MenuItemType::TOOL,
        LocalQTCompat::fromLocal8Bit("清除日志(&C)"), true, "", "Ctrl+L");

    createMenuItem("helpContentAction", MenuItemType::HELP,
        LocalQTCompat::fromLocal8Bit("帮助内容(&H)..."), true, "", "F1");
    m_menus[MenuItemType::HELP]->addSeparator();
    createMenuItem("aboutAction", MenuItemType::HELP,
        LocalQTCompat::fromLocal8Bit("关于(&A)..."), true);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("创建菜单OK, 已创建项: %1").arg(m_actions.size()));
}

void MenuView::connectModelSignals()
{
    MenuModel* model = MenuModel::getInstance();

    LOG_INFO("连接菜单Model");
    // 连接模型信号到视图槽
    connect(model, &MenuModel::menuItemEnabledChanged,
        this, &MenuView::slot_setMenuItemEnabled,
        Qt::ConnectionType::QueuedConnection);
    connect(model, &MenuModel::menuItemVisibilityChanged,
        this, &MenuView::slot_setMenuItemVisible,
        Qt::ConnectionType::QueuedConnection);
    connect(model, &MenuModel::menuItemTextChanged,
        this, &MenuView::slot_setMenuItemText,
        Qt::ConnectionType::QueuedConnection);
    connect(model, &MenuModel::menuItemIconChanged,
        this, &MenuView::slot_setMenuItemIcon,
        Qt::ConnectionType::QueuedConnection);
    connect(model, &MenuModel::menuItemShortcutChanged,
        this, &MenuView::slot_setMenuItemShortcut,
        Qt::ConnectionType::QueuedConnection);
    connect(model, &MenuModel::menuItemAdded,
        this, &MenuView::slot_menuItemAdded,
        Qt::ConnectionType::QueuedConnection);
    connect(model, &MenuModel::menuConfigChanged,
        this, &MenuView::slot_menuConfigChanged,
        Qt::ConnectionType::QueuedConnection);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已连接模型信号"));
}

QMenu* MenuView::getMenuByType(MenuItemType type) const
{
    return m_menus.value(type, nullptr);
}

void MenuView::syncMenusFromModel()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始从模型同步菜单状态"));

    try {
        MenuModel* model = MenuModel::getInstance();

        // 输出调试信息
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("当前菜单项映射表大小: %1").arg(m_actions.size()));
        LOG_DEBUG(LocalQTCompat::fromLocal8Bit("模型菜单项数量: %1").arg(model->getAllMenuItems().size()));

        // 同步所有菜单项状态
        QStringList allItems = model->getAllMenuItems();
        for (const QString& item : allItems) {
            QAction* action = getMenuAction(item);
            if (!action) {
                LOG_WARN(LocalQTCompat::fromLocal8Bit("同步时未找到菜单项: %1").arg(item));
                continue;
            }

            // 同步启用状态
            bool enabled = model->isMenuItemEnabled(item);
            action->setEnabled(enabled);
            LOG_DEBUG(LocalQTCompat::fromLocal8Bit("同步菜单启用状态: %1 -> %2")
                .arg(item).arg(enabled ? "启用" : "禁用"));

            // 同步可见状态
            bool visible = model->isMenuItemVisible(item);
            action->setVisible(visible);

            // 同步文本
            QString text = model->getMenuItemText(item);
            if (!text.isEmpty()) {
                action->setText(text);
            }

            // 同步图标
            QString iconPath = model->getMenuItemIcon(item);
            if (!iconPath.isEmpty()) {
                action->setIcon(QIcon(iconPath));
            }

            // 同步快捷键
            QString shortcut = model->getMenuItemShortcut(item);
            if (!shortcut.isEmpty()) {
                action->setShortcut(QKeySequence(shortcut));
            }
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("从模型同步菜单状态完成"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("同步菜单状态异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("同步菜单状态未知异常"));
    }
}

QAction* MenuView::createMenuItem(const QString& actionName, MenuItemType menuType,
    const QString& text, bool enabled,
    const QString& iconPath, const QString& shortcut)
{
    // 获取目标菜单
    QMenu* targetMenu = getMenuByType(menuType);
    if (!targetMenu) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建菜单项失败：未找到目标菜单类型 %1").arg(static_cast<int>(menuType)));
        return nullptr;
    }

    // 创建新的QAction，显式设置父对象为目标菜单，确保正确的对象所有权
    QAction* action = new QAction(text, targetMenu);
    action->setObjectName(actionName); // 设置对象名，便于查找
    action->setEnabled(enabled);

    // 设置图标（如果有）
    if (!iconPath.isEmpty()) {
        action->setIcon(QIcon(iconPath));
    }

    // 设置快捷键（如果有）
    if (!shortcut.isEmpty()) {
        action->setShortcut(QKeySequence(shortcut));
    }

    // 添加到目标菜单
    targetMenu->addAction(action);

    // 添加到映射表
    m_actions[actionName] = action;

    // 连接信号
    connect(action, &QAction::triggered, this, &MenuView::onMenuAction);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建菜单项: %1").arg(actionName));
    return action;
}