// Source/MVC/Views/DataAnalysisView.h
#pragma once

#include <QWidget>
#include <memory>

namespace Ui {
    class DataAnalysisClass;
}

class DataAnalysisController;
struct StatisticsInfo;

/**
 * @brief 数据分析视图类
 *
 * 负责显示数据分析界面
 */
class DataAnalysisView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit DataAnalysisView(QWidget* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DataAnalysisView();

    /**
     * @brief 获取UI对象
     * @return UI对象指针
     */
    Ui::DataAnalysisClass* getUi() { return ui; }

public slots:
    /**
     * @brief 更新统计信息显示
     * @param info 统计信息
     */
    void updateStatisticsDisplay(const StatisticsInfo& info);

    /**
     * @brief 显示消息对话框
     * @param title 标题
     * @param message 消息内容
     * @param isError 是否为错误消息
     */
    void showMessageDialog(const QString& title, const QString& message, bool isError = false);

    /**
     * @brief 清除数据表格
     */
    void clearDataTable();

    /**
     * @brief 更新UI状态
     * @param hasData 是否有数据
     */
    void updateUIState(bool hasData);

signals:
    /**
     * @brief 视频预览按钮点击信号
     */
    void videoPreviewClicked();

    /**
     * @brief 保存数据按钮点击信号
     */
    void saveDataClicked();

    /**
     * @brief 导入数据按钮点击信号
     */
    void importDataClicked();

    /**
     * @brief 导出数据按钮点击信号
     */
    void exportDataClicked();

    /**
     * @brief 表格行选择变化信号
     * @param rows 选中的行索引列表
     */
    void selectionChanged(const QList<int>& rows);

private:
    /**
     * @brief 初始化UI
     */
    void initializeUI();

    /**
     * @brief 初始化表格
     */
    void initializeTable();

    /**
     * @brief 创建统计信息面板
     */
    void createStatisticsPanel();

    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

private:
    Ui::DataAnalysisClass* ui;                           ///< UI对象
    std::unique_ptr<DataAnalysisController> m_controller; ///< 控制器对象
    QWidget* m_statisticsPanel;                          ///< 统计信息面板
};