#include "MenuController.h"
#include "Logger.h"
#include <QMainWindow>

MenuController::MenuController(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_view(nullptr)
    , m_mainWindow(mainWindow)
{
    if (!m_mainWindow) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建菜单控制器失败：主窗口指针为空"));
        return;
    }

    try {
        m_view = new MenuView(m_mainWindow, this);
        if (!m_view) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("菜单视图创建失败"));
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建菜单视图异常: %1").arg(e.what()));
    }
}

MenuController::~MenuController()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单控制器已销毁"));
}

bool MenuController::initialize()
{
    if (!m_view) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化菜单控制器失败：视图为空"));
        return false;
    }

    try {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("开始初始化菜单控制器"));

        // 1. 首先初始化视图
        m_view->initializeMenuBar();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单视图初始化完成"));

        // 2. 确保模型和视图同步
        syncModelWithView();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("模型与视图同步完成"));

        // 3. 连接信号和槽
        connectSignals();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("信号槽连接完成"));

        // 4. 使用延迟更新菜单状态，确保视图完全初始化
        QTimer::singleShot(200, this, [this]() {
            try {
                LOG_INFO(LocalQTCompat::fromLocal8Bit("开始延迟更新菜单状态"));
                AppState currentState = AppStateMachine::instance().currentState();
                LOG_INFO(LocalQTCompat::fromLocal8Bit("当前应用状态: %1").arg(static_cast<int>(currentState)));
                updateMenuStateForAppState(currentState);
                LOG_INFO(LocalQTCompat::fromLocal8Bit("延迟更新菜单状态完成"));
            }
            catch (const std::exception& e) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("延迟更新菜单状态异常: %1").arg(e.what()));
            }
            catch (...) {
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("延迟更新菜单状态未知异常"));
            }
            });

        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单控制器已初始化"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化菜单控制器异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化菜单控制器未知异常"));
    }
    return false;
}

void MenuController::updateMenuStateForAppState(AppState state)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始根据应用状态更新菜单: %1").arg(static_cast<int>(state)));

    try {
        // 检查视图和模型是否就绪
        if (!m_view || !m_view->getMenuBar()) {
            LOG_WARN(LocalQTCompat::fromLocal8Bit("菜单视图未完全初始化，跳过状态更新"));
            return;
        }

        // 更新模型中的菜单状态
        MenuModel::getInstance()->updateMenuStateForAppState(state);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单状态更新完成"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("更新菜单状态异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("更新菜单状态未知异常"));
    }
}

MenuView* MenuController::getMenuView() const
{
    return m_view;
}

void MenuController::syncModelWithView()
{
    if (!m_view) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("同步模型与视图失败：视图为空"));
        return;
    }

    try {
        MenuModel* model = MenuModel::getInstance();

        // 获取视图中的所有菜单项
        QStringList viewActions = m_view->getAllMenuActions();
        LOG_INFO(LocalQTCompat::fromLocal8Bit("视图中的菜单项数量: %1").arg(viewActions.size()));

        // 确保模型中包含视图中的所有菜单项
        for (const QString& actionName : viewActions) {
            if (!model->menuItemExists(actionName)) {
                LOG_WARN(LocalQTCompat::fromLocal8Bit("模型中缺少菜单项: %1，尝试添加").arg(actionName));

                // 获取视图中的菜单项信息
                QAction* action = m_view->getMenuAction(actionName);
                if (action) {
                    // 确定菜单类型
                    MenuItemType type = determineMenuType(actionName);

                    // 添加到模型
                    model->addMenuItem(
                        actionName,
                        type,
                        action->text(),
                        action->isEnabled(),
                        "", // 图标暂时留空
                        action->shortcut().toString()
                    );

                    LOG_INFO(LocalQTCompat::fromLocal8Bit("已将菜单项添加到模型: %1").arg(actionName));
                }
            }
        }

        // 确保视图包含模型中的所有菜单项
        QStringList modelActions = model->getAllMenuItems();
        for (const QString& actionName : modelActions) {
            if (!viewActions.contains(actionName)) {
                LOG_WARN(LocalQTCompat::fromLocal8Bit("视图中缺少菜单项: %1").arg(actionName));
                // 视图缺少该项的处理...根据实际需求决定是否添加到视图
            }
        }

        LOG_INFO(LocalQTCompat::fromLocal8Bit("模型与视图同步完成"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("同步模型与视图异常: %1").arg(e.what()));
    }
    catch (...) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("同步模型与视图未知异常"));
    }
}

// 辅助方法：根据菜单项名称确定其类型
MenuItemType MenuController::determineMenuType(const QString& actionName)
{
    // 基于命名规则判断菜单类型
    if (actionName.endsWith("Action")) {
        if (actionName == "openAction" || actionName == "saveAction" ||
            actionName == "exportAction" || actionName == "exitAction" ||
            actionName == "fileOptions") {
            return MenuItemType::FILE;
        }
        else if (actionName == "startAction" || actionName == "stopAction" ||
            actionName == "resetAction" || actionName == "updateAction") {
            return MenuItemType::DEVICE;
        }
        else if (actionName == "channelAction" || actionName == "dataAction" ||
            actionName == "videoAction" || actionName == "waveformAction") {
            return MenuItemType::VIEW;
        }
        else if (actionName == "settingsAction" || actionName == "clearLogAction") {
            return MenuItemType::TOOL;
        }
        else if (actionName == "helpContentAction" || actionName == "aboutAction") {
            return MenuItemType::HELP;
        }
    }

    // 默认类型
    LOG_WARN(LocalQTCompat::fromLocal8Bit("无法确定菜单项类型: %1，使用默认类型").arg(actionName));
    return MenuItemType::TOOL;
}

void MenuController::handleMenuAction(const QString& actionName)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("菜单控制器处理动作: %1").arg(actionName));

    // 发送信号到主窗口
    emit menuActionTriggered(actionName);

    // 特殊处理一些菜单项
    if (actionName == "exitAction") {
        if (m_mainWindow) {
            // 触发主窗口关闭
            m_mainWindow->close();
        }
    }
}

void MenuController::handleAppStateChanged(AppState newState, AppState oldState, const QString& reason)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("应用状态变更触发菜单更新: %1 -> %2, 原因: %3")
        .arg(static_cast<int>(oldState))
        .arg(static_cast<int>(newState))
        .arg(reason));

    // 更新菜单状态
    updateMenuStateForAppState(newState);
}

void MenuController::connectSignals()
{
    // 连接视图信号到控制器槽
    if (m_view) {
        bool connected = connect(m_view, &MenuView::menuActionTriggered,
            this, &MenuController::handleMenuAction,
            Qt::ConnectionType::QueuedConnection);  // 使用队列连接增强线程安全性

        if (!connected) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("菜单动作信号连接失败"));
        }
    }

    // 连接应用状态机信号
    bool stateConnected = connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged,
        this, &MenuController::handleAppStateChanged,
        Qt::ConnectionType::QueuedConnection);

    if (!stateConnected) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("应用状态机信号连接失败"));
    }
}