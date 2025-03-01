#pragma once

#include <QObject>
#include "AppStateMachine.h"
#include "ui_FX3ToolMainWin.h"
#include "Logger.h"

// UI状态处理类 - 负责根据应用状态更新UI
class UIStateHandler : public QObject {
    Q_OBJECT

public:
    explicit UIStateHandler(Ui::FX3ToolMainWinClass& ui, QObject* parent = nullptr);
    ~UIStateHandler() = default;

    // 准备关闭，停止所有UI更新
    void prepareForClose() {
        m_isClosing = true;
        LOG_INFO(QString::fromLocal8Bit("UI状态处理器已准备关闭"));
    }

    // 检查是否可以安全更新UI
    bool canUpdateUI() const {
        // 检查应用程序是否正在关闭
        if (QApplication::closingDown()) {
            return false;
        }

        // 检查UI处理器是否正在关闭
        if (m_isClosing.load()) {
            return false;
        }

        // 确保在主线程中调用
        if (QThread::currentThread() != QApplication::instance()->thread()) {
            // 如果不在主线程，只允许某些操作
            return false;
        }

        return true;
    }

public slots:
    // 当应用状态改变时更新UI
    void onStateChanged(AppState newState, AppState oldState, const QString& reason);

    // 更新传输速度和数据统计
    void updateTransferStats(uint64_t transferred, double speed, uint64_t elapsedTimeSeconds = 0);

    // 更新USB设备速度显示
    void updateUsbSpeedDisplay(const QString& speedDesc, bool isUSB3);

    // 显示错误消息
    void showErrorMessage(const QString& title, const QString& message);

private:
    // 根据应用状态更新按钮状态
    void updateButtonStates(AppState state);

    // 更新状态栏文本
    void updateStatusTexts(AppState state, const QString& additionalInfo = QString());

    // 格式化数据大小显示
    QString formatDataSize(uint64_t bytes);

    QString formatElapsedTime(uint64_t seconds) {
        uint64_t hours = seconds / 3600;
        uint64_t minutes = (seconds % 3600) / 60;
        uint64_t secs = seconds % 60;

        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }

    // UI引用
    Ui::FX3ToolMainWinClass& m_ui;

    // 上次更新的传输状态
    uint64_t m_lastTransferred = 0;
    double m_lastSpeed = 0.0;
    std::atomic<bool> m_isClosing{ false };
};