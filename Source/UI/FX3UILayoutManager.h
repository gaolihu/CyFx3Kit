#pragma once

#include <QObject>
#include <QWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QComboBox>
#include <QFrame>
#include <QStatusBar>
#include <QToolBar>
#include <QIcon>
#include <QTextEdit>

// 前向声明
class FX3ToolMainWin;

/**
 * @brief UI布局管理器类，负责主窗口界面布局
 *
 * 此类封装了主窗口的界面布局创建和管理逻辑，
 * 从主窗口分离UI布局职责，提高代码可维护性。
 */
class FX3UILayoutManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param mainWindow 主窗口指针
     */
    explicit FX3UILayoutManager(FX3ToolMainWin* mainWindow);

    /**
     * @brief 初始化主界面布局
     *
     * 创建并设置主窗口的整体布局结构
     */
    void initializeMainLayout();

    /**
     * @brief 创建主页内容
     *
     * 创建并返回主页标签页的内容
     *
     * @return 主页内容小部件
     */
    QWidget* createHomeTabContent();

    /**
     * @brief 创建工具栏
     *
     * 创建并设置主窗口的工具栏
     */
    void createToolBar();

    /**
     * @brief 调整状态栏
     *
     * 调整状态栏的大小和外观
     */
    void adjustStatusBar();

private:
    /**
     * @brief 创建控制面板
     *
     * 创建左侧控制面板
     *
     * @return 控制面板小部件
     */
    QWidget* createControlPanel();

    /**
     * @brief 创建状态面板
     *
     * 创建左下方状态信息面板
     *
     * @return 状态面板小部件
     */
    QWidget* createStatusPanel();

    /**
     * @brief 创建日志面板
     *
     * 创建右侧日志显示面板
     *
     * @return 日志面板小部件
     */
    QWidget* createLogPanel();

    FX3ToolMainWin* m_mainWindow;      ///< 主窗口指针
    QTabWidget* m_tabWidget;           ///< 标签页控件指针
    QSplitter* m_mainSplitter;         ///< 主分割器指针
};