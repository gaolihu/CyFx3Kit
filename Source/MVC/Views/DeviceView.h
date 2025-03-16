// Source/MVC/Views/DeviceView.h
#pragma once

#include <QObject>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include "IDeviceView.h"

/**
 * @brief 设备视图类
 *
 * 实现设备视图接口，提供与UI组件交互的具体实现。
 */
class DeviceView : public QObject, public IDeviceView
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit DeviceView(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DeviceView();

    /**
     * @brief 初始化UI组件
     */
    void initUIComponents(
        QLineEdit* widthEdit,
        QLineEdit* heightEdit,
        QComboBox* typeCombo,
        QLabel* usbSpeedLabel,
        QLabel* usbStatusLabel,
        QLabel* transferStatusLabel,
        QLabel* transferRateLabel,
        QLabel* totalBytesLabel,
        QLabel* totalTimeLabel,
        QPushButton* startButton,
        QPushButton* stopButton,
        QPushButton* resetButton
    );

    /**
     * @brief 连接按钮信号
     */
    void connectSignals();

    // IDeviceView接口实现
    uint16_t getImageWidth(bool* ok = nullptr) const override;
    uint16_t getImageHeight(bool* ok = nullptr) const override;
    uint8_t getCaptureType() const override;
    void showErrorMessage(const QString& message) override;
    bool showConfirmDialog(const QString& title, const QString& message) override;

signals:
    // IDeviceView接口信号实现
    void signal_Dev_V_startTransferRequested() override;
    void signal_Dev_V_stopTransferRequested() override;
    void signal_Dev_V_resetDeviceRequested() override;
    void signal_Dev_V_imageParametersChanged() override;

private slots:
    /**
     * @brief 开始按钮点击处理
     */
    void onStartButtonClicked();

    /**
     * @brief 停止按钮点击处理
     */
    void onStopButtonClicked();

    /**
     * @brief 重置按钮点击处理
     */
    void onResetButtonClicked();

    /**
     * @brief 宽度输入框内容变更处理
     */
    void onWidthTextChanged();

    /**
     * @brief 高度输入框内容变更处理
     */
    void onHeightTextChanged();

    /**
     * @brief 捕获类型下拉框选择变更处理
     */
    void onCaptureTypeChanged();

private:
    /**
     * @brief 更新按钮状态
     * @param deviceState 设备状态
     */
    void updateButtonStates(DeviceState deviceState);

    /**
     * @brief 获取状态文本
     * @param state 设备状态
     * @return 对应的文本描述
     */
    QString getStateText(DeviceState state) const;

private:
    // UI组件指针
    QLineEdit* m_widthEdit;        ///< 图像宽度输入框
    QLineEdit* m_heightEdit;       ///< 图像高度输入框
    QComboBox* m_typeCombo;        ///< 捕获类型下拉框
    QLabel* m_usbSpeedLabel;       ///< USB速度标签
    QLabel* m_usbStatusLabel;      ///< USB状态标签
    QLabel* m_transferStatusLabel; ///< USB传输状态标签
    QLabel* m_transferRateLabel;   ///< USB传输速率标签
    QLabel* m_totalBytesLabel;     ///< USB总共传输字节标签
    QLabel* m_totalTimeLabel;      ///< USB传输时间标签
    QPushButton* m_startButton;    ///< 开始传输按钮
    QPushButton* m_stopButton;     ///< 停止传输按钮
    QPushButton* m_resetButton;    ///< 重置设备按钮

    QWidget* m_parentWidget;       ///< 父窗口指针，用于显示对话框
};