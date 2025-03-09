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
     * @param widthEdit 宽度输入框指针
     * @param heightEdit 高度输入框指针
     * @param typeCombo 捕获类型下拉框指针
     * @param speedLabel USB速度标签指针
     * @param bytesLabel 传输字节数标签指针
     * @param stateLabel 设备状态标签指针
     * @param startButton 开始传输按钮指针
     * @param stopButton 停止传输按钮指针
     * @param resetButton 重置设备按钮指针
     */
    void initUIComponents(
        QLineEdit* widthEdit,
        QLineEdit* heightEdit,
        QComboBox* typeCombo,
        QLabel* speedLabel,
        QLabel* bytesLabel,
        QLabel* stateLabel
    );

    /**
     * @brief 连接按钮信号
     */
    void connectSignals();

    // IDeviceView接口实现
    uint16_t getImageWidth(bool* ok = nullptr) const override;
    uint16_t getImageHeight(bool* ok = nullptr) const override;
    uint8_t getCaptureType() const override;
    void updateImageWidth(uint16_t width) override;
    void updateImageHeight(uint16_t height) override;
    void updateCaptureType(uint8_t captureType) override;
    void updateUsbSpeed(double speed) override;
    void updateTransferredBytes(uint64_t bytes) override;
    void updateDeviceState(DeviceState state) override;
    void showErrorMessage(const QString& message) override;
    bool showConfirmDialog(const QString& title, const QString& message) override;

signals:
    // IDeviceView接口信号实现
    void startTransferRequested() override;
    void stopTransferRequested() override;
    void resetDeviceRequested() override;
    void imageParametersChanged() override;

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
    QLabel* m_speedLabel;          ///< USB速度标签
    QLabel* m_bytesLabel;          ///< 传输字节数标签
    QLabel* m_stateLabel;          ///< 设备状态标签
    QPushButton* m_startButton;    ///< 开始传输按钮
    QPushButton* m_stopButton;     ///< 停止传输按钮
    QPushButton* m_resetButton;    ///< 重置设备按钮

    QWidget* m_parentWidget;       ///< 父窗口指针，用于显示对话框
};