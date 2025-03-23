﻿// Source/MVC/Controllers/FileSaveController.h
#pragma once

#include <QObject>
#include <QTimer>
#include "FileSaveModel.h"
#include "FileSaveView.h"
#include "DataPacket.h"

class QWidget;
class QThread;

/**
 * @brief 文件保存控制器类
 *
 * 管理文件保存逻辑，协调模型和视图之间的交互。
 * 处理来自界面的用户操作，更新模型数据，控制保存流程。
 * 使用独立工作线程进行异步文件保存以提高响应性能。
 */
class FileSaveController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit FileSaveController(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~FileSaveController();

    /**
     * @brief 初始化控制器
     * @return 初始化结果，true表示成功
     */
    bool initialize();

    /**
     * @brief 是否正在保存
     * @return 当前是否正在保存
     */
    bool isSaving() const;

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 创建文件保存视图
     * @param parent 父窗口指针
     * @return 文件保存视图指针
     */
    FileSaveView* createSaveView(QWidget* parent = nullptr);

    QString getCurrentFileName() const { return m_model->getFullSavePath(); }

public slots:
    /**
     * @brief 启动保存
     * @return 启动结果，true表示成功
     */
    bool slot_FS_C_startSaving();

    /**
     * @brief 停止保存
     * @return 停止结果，true表示成功
     */
    bool slot_FS_C_stopSaving();

    /**
     * @brief 显示设置视图
     * @param parent 父窗口指针
     */
    void slot_FS_C_showSettings(QWidget* parent = nullptr);

    /**
     * @brief 处理数据包
     * @param packet 数据包
     */
    void slot_FS_C_processDataPacket(const DataPacket& packet);

    /**
     * @brief 处理数据包批次
     * @param packets 数据包批次
     */
    void slot_FS_C_processDataBatch(const DataPacketBatch& packets);

    /**
     * @brief 是否启用自动保存
     * @return 是否启用自动保存
     */
    bool slot_FS_C_isAutoSaveEnabled() const;

    /**
     * @brief 设置是否启用自动保存
     * @param enabled 启用状态
     */
    void slot_FS_C_setAutoSaveEnabled(bool enabled);

    /**
     * @brief 处理视图保存参数变更
     * @param parameters 新的保存参数
     */
    void slot_FS_C_onViewParametersChanged(const SaveParameters& parameters);

signals:
    /**
     * @brief 保存开始信号
     */
    void signal_FS_C_saveStarted();

    /**
     * @brief 保存停止信号
     */
    void signal_FS_C_saveStopped();

    /**
     * @brief 保存完成信号
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void signal_FS_C_saveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 保存错误信号
     * @param error 错误消息
     */
    void signal_FS_C_saveError(const QString& error);

private slots:
    /**
     * @brief 处理模型状态变更
     * @param status 新的保存状态
     */
    void slot_FS_C_onModelStatusChanged(SaveStatus status);

    /**
     * @brief 处理模型统计信息更新
     * @param statistics 新的统计信息
     */
    void slot_FS_C_onModelStatisticsUpdated(const SaveStatistics& statistics);

    /**
     * @brief 处理模型保存完成
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void slot_FS_C_onModelSaveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 处理模型保存错误
     * @param error 错误消息
     */
    void slot_FS_C_onModelSaveError(const QString& error);

    /**
     * @brief 处理视图开始保存请求
     */
    void slot_FS_C_onViewStartSaveRequested();

    /**
     * @brief 处理视图停止保存请求
     */
    void slot_FS_C_onViewStopSaveRequested();

    /**
     * @brief 处理工作线程保存进度更新
     * @param bytesWritten 已写入字节数
     * @param fileCount 文件数量
     */
    void slot_FS_C_onWorkerSaveProgress(uint64_t bytesWritten, int fileCount);

    /**
     * @brief 处理工作线程保存完成
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void slot_FS_C_onWorkerSaveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 处理工作线程保存错误
     * @param error 错误消息
     */
    void slot_FS_C_onWorkerSaveError(const QString& error);

private:
    /**
     * @brief 更新保存统计
     */
    void updateSaveStatistics();

    /**
     * @brief 连接模型信号
     */
    void connectModelSignals();

    /**
     * @brief 连接视图信号
     * @param view 视图对象
     */
    void connectViewSignals(FileSaveView* view);

    /**
     * @brief 连接工作线程信号
     */
    void connectWorkerSignals();

private:
    FileSaveModel* m_model;               // 模型引用
    FileSaveView* m_currentView;          // 当前视图引用
    QTimer m_statsUpdateTimer;            // 统计更新定时器
    uint16_t m_currentWidth;              // 当前图像宽度
    uint16_t m_currentHeight;             // 当前图像高度
    uint8_t m_currentFormat;              // 当前图像格式
    bool m_initialized;                   // 初始化标志
};