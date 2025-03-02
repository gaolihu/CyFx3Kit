#pragma once

#include <QObject>
#include <QWidget>
#include <QTabWidget>
#include <QIcon>
#include <QString>
#include <QVariant>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>

// 包含模块头文件
#include "ChannelSelect.h"
#include "DataAnalysis.h"
#include "VideoDisplay.h"
#include "SaveFileBox.h"

// 前向声明
class FX3ToolMainWin;

/**
 * @brief 模块管理器类，负责管理功能模块
 *
 * 此类封装了功能模块的创建、显示和管理逻辑，
 * 从主窗口分离模块管理职责，提高代码可维护性。
 */
class FX3ModuleManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param mainWindow 主窗口指针
     */
    explicit FX3ModuleManager(FX3ToolMainWin* mainWindow);

    /**
     * @brief 显示通道配置模块
     *
     * 创建并显示通道配置窗口
     */
    void showChannelConfigModule();

    /**
     * @brief 显示数据分析模块
     *
     * 创建并显示数据分析窗口
     */
    void showDataAnalysisModule();

    /**
     * @brief 显示视频显示模块
     *
     * 创建并显示视频显示窗口
     */
    void showVideoDisplayModule();

    /**
     * @brief 显示文件保存模块
     *
     * 创建并显示文件保存对话框
     */
    void showSaveFileModule();

    /**
     * @brief 显示波形分析模块
     *
     * 创建并显示波形分析窗口（预留功能）
     */
    void showWaveformModule();

signals:
    /**
     * @brief 模块信号
     *
     * 当模块发出信号时转发给主窗口
     *
     * @param signal 信号名称
     * @param data 信号数据
     */
    void moduleSignal(const QString& signal, const QVariant& data);

private:
    /**
     * @brief 添加模块标签页
     *
     * 向标签控件添加新的模块标签页
     *
     * @param widget 模块窗口指针
     * @param tabName 标签名称
     * @param tabIndex 输出参数，存储标签索引
     */
    void addModuleTab(QWidget* widget, const QString& tabName, int& tabIndex);

    /**
     * @brief 显示模块标签页
     *
     * 显示已有或创建新的模块标签页
     *
     * @param tabIndex 标签索引引用
     * @param widget 模块窗口指针
     * @param tabName 标签名称
     * @param icon 标签图标
     */
    void showModuleTab(int& tabIndex, QWidget* widget, const QString& tabName, const QIcon& icon = QIcon());

    /**
     * @brief 移除模块标签页
     *
     * 从标签控件移除指定索引的标签页
     *
     * @param tabIndex 标签索引引用
     */
    void removeModuleTab(int& tabIndex);

    FX3ToolMainWin* m_mainWindow;           ///< 主窗口指针
    QTabWidget* m_tabWidget;                ///< 标签页控件指针

    // 子模块实例
    ChannelSelect* m_channelModule;         ///< 通道配置模块
    DataAnalysis* m_dataAnalysisModule;     ///< 数据分析模块
    VideoDisplay* m_videoDisplayModule;     ///< 视频显示模块
    SaveFileBox* m_saveFileModule;          ///< 文件保存模块

    // 标签页索引
    int m_channelTabIndex;                  ///< 通道配置标签索引
    int m_dataAnalysisTabIndex;             ///< 数据分析标签索引
    int m_videoDisplayTabIndex;             ///< 视频显示标签索引
    int m_waveformTabIndex;                 ///< 波形分析标签索引
};