﻿// Source/MVC/Views/FileSaveView.h
#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QFileDialog>
#include <QMessageBox>
#include "FileSaveModel.h"

namespace Ui {
    class SaveFileBox;
}

/**
 * @brief 文件保存视图类
 *
 * 提供文件保存的完整用户界面，包括设置选项和状态显示
 * 整合了原FileSaveDialogView和FileSavePanelView的功能
 */
class FileSaveView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit FileSaveView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FileSaveView();

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 是否正在保存
     * @return 是否正在保存
     */
    bool isSaving() const;

    /**
     * @brief 准备显示视图
     */
    void prepareForShow();

signals:
    /**
     * @brief 保存参数变更信号
     * @param parameters 新的保存参数
     */
    void saveParametersChanged(const SaveParameters& parameters);

    /**
     * @brief 开始保存请求信号
     */
    void startSaveRequested();

    /**
     * @brief 停止保存请求信号
     */
    void stopSaveRequested();

public slots:
    /**
     * @brief 更新状态显示
     * @param status 保存状态
     */
    void updateStatusDisplay(SaveStatus status);

    /**
     * @brief 更新统计信息显示
     * @param stats 保存统计信息
     */
    void updateStatisticsDisplay(const SaveStatistics& stats);

    /**
     * @brief 保存开始响应
     */
    void onSaveStarted();

    /**
     * @brief 保存停止响应
     */
    void onSaveStopped();

    /**
     * @brief 保存完成响应
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void onSaveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 保存错误响应
     * @param error 错误消息
     */
    void onSaveError(const QString& error);

private slots:
    /**
     * @brief 保存按钮点击处理
     */
    void onSaveButtonClicked();

    /**
     * @brief 取消按钮点击处理
     */
    void onCancelButtonClicked();

    /**
     * @brief 浏览文件夹按钮点击处理
     */
    void onBrowseFolderButtonClicked();

    /**
     * @brief 保存范围单选按钮切换处理
     * @param checked 是否选中
     */
    void onSaveRangeRadioButtonToggled(bool checked);

private:
    /**
     * @brief 初始化UI
     */
    void setupUI();

    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

    /**
     * @brief 更新保存参数
     * @return 保存参数结构
     */
    SaveParameters collectSaveParameters();

    /**
     * @brief 更新UI控件状态
     */
    void updateUIState();

private:
    Ui::SaveFileBox* ui;                  ///< UI对象

    // 图像参数
    uint16_t m_width;                     ///< 图像宽度
    uint16_t m_height;                    ///< 图像高度
    uint8_t m_format;                     ///< 图像格式

    bool m_saving;                        ///< 是否正在保存
};