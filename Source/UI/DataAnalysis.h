#pragma once

#include <QWidget>
#include <QVector>
#include <QByteArray>
#include <QTableWidgetItem>
#include <QFileDialog>
#include <QMessageBox>
#include "ui_DataAnalysis.h"

/**
 * @brief 数据分析窗口类，负责数据可视化和分析功能
 */
class DataAnalysis : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit DataAnalysis(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DataAnalysis();

    /**
     * @brief 设置要分析的数据
     * @param data 要分析的数据包集合
     */
    void setAnalysisData(const QVector<QByteArray>& data);

signals:
    /**
     * @brief 请求保存数据信号
     */
    void saveDataRequested();

    /**
     * @brief 请求显示视频信号
     */
    void videoDisplayRequested();

private slots:
    /**
     * @brief 视频预览按钮点击事件处理
     */
    void onVideoShowButtonClicked();

    /**
     * @brief 保存数据按钮点击事件处理
     */
    void onSaveDataButtonClicked();

    /**
     * @brief 表格项选择事件处理
     */
    void onTableItemSelected();

private:
    // UI组件
    Ui::DataAnalysisClass ui;

    // 当前分析的数据
    QVector<QByteArray> m_analysisData;

    /**
     * @brief 初始化UI界面
     */
    void initializeUI();

    /**
     * @brief 连接信号槽
     */
    void connectSignals();

    /**
     * @brief 创建统计图表
     */
    void createStatisticsChart();

    /**
     * @brief 更新表格数据
     */
    void updateTableData();

    /**
     * @brief 更新统计信息
     */
    void updateStatistics();

    /**
     * @brief 清空统计信息
     */
    void clearStatistics();
};