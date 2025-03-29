// Source/MVC/Views/DataAnalysisView.h
#pragma once

#include <QWidget>
#include <memory>

class QProgressDialog;

namespace Ui {
    class DataAnalysisClass;
}

/**
 * @brief 数据索引视图类
 *
 * 负责显示USB数据索引界面，专注于显示索引信息和提供用户交互
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
    Ui::DataAnalysisClass* getUi() const { return ui; }

    /**
     * @brief 获取选中的行索引列表
     * @return 选中的行索引列表
     */
    QList<int> getSelectedRows() const;

public slots:
    /**
     * @brief 显示消息对话框
     * @param title 标题
     * @param message 消息内容
     * @param isError 是否为错误消息
     */
    void slot_DA_V_showMessageDialog(const QString& title, const QString& message, bool isError = false);

    /**
     * @brief 显示进度对话框
     * @param title 对话框标题
     * @param text 显示文本
     * @param min 最小值
     * @param max 最大值
     */
    void slot_DA_V_showProgressDialog(const QString& title, const QString& text, int min, int max);

    /**
     * @brief 更新进度对话框
     * @param value 当前进度值
     */
    void slot_DA_V_updateProgressDialog(int value);

    /**
     * @brief 隐藏进度对话框
     */
    void slot_DA_V_hideProgressDialog();

    /**
     * @brief 更新UI状态
     * @param hasData 是否有数据
     */
    void slot_DA_V_updateUIState(bool hasData);

    /**
     * @brief 更新状态栏
     * @param statusText 状态文本
     * @param dataCount 数据项数量
     */
    void slot_DA_V_updateStatusBar(const QString& statusText, int dataCount);

signals:
    /**
     * @brief 导入数据按钮点击信号
     */
    void signal_DA_V_importDataClicked();

    /**
     * @brief 导出数据按钮点击信号
     */
    void signal_DA_V_exportDataClicked();

    /**
     * @brief 清除数据按钮点击信号
     */
    void signal_DA_V_clearDataClicked();

    /**
     * @brief 表格行选择变化信号
     * @param rows 选中的行索引列表
     */
    void signal_DA_V_selectionChanged(const QList<int>& rows);

    /**
     * @brief 应用筛选按钮点击信号
     * @param filterText 筛选文本
     */
    void signal_DA_V_applyFilterClicked(const QString& filterText);

    /**
     * @brief 从文件加载数据请求信号
     * @param filePath 文件路径
     */
    void signal_DA_V_loadDataFromFileRequested(const QString& filePath);

private slots:
    /**
     * @brief 处理表格选择变化
     */
    void slot_DA_V_onTableSelectionChanged();

    /**
     * @brief 处理应用筛选按钮点击
     */
    void slot_DA_V_onApplyFilterClicked();

    /**
     * @brief 处理从文件加载按钮点击
     */
    void slot_DA_V_onLoadFromFileClicked();

private:
    /**
     * @brief 简化UI
     * 隐藏不需要的UI元素，专注于索引展示
     */
    void simplifyUI();

    /**
     * @brief 连接信号和槽
     */
    void connectSignals();

private:
    Ui::DataAnalysisClass* ui;          ///< UI对象
    QProgressDialog* m_progressDialog;  ///< 进度对话框
};