#pragma once

#include <QWidget>
#include <QFileDialog>
#include <QMessageBox>
#include "ui_SaveFileBox.h"
#include "FileSaveManager.h"
#include "FileSavePanel.h"

class SaveFileBox : public QWidget
{
    Q_OBJECT

public:
    explicit SaveFileBox(QWidget* parent = nullptr);
    ~SaveFileBox();

    // 设置图像参数
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    // 显示窗口前调用此方法进行初始化
    void prepareForShow();

    // 判断是否正在保存
    bool isSaving() const;

signals:
    // 保存完成信号
    void saveCompleted(const QString& path, uint64_t totalBytes);

    // 保存错误信号
    void saveError(const QString& error);

private slots:
    // UI槽函数
    void onSaveButtonClicked();
    void onCancelButtonClicked();
    void onBrowseFolderButtonClicked();
    void onSaveRangeRadioButtonToggled(bool checked);
    void onLineRangeCheckBoxToggled(bool checked);
    void onColumnRangeCheckBoxToggled(bool checked);
    void onFileFormatChanged();

    // 文件保存管理器槽函数
    void onSaveManagerCompleted(const QString& path, uint64_t totalBytes);
    void onSaveManagerError(const QString& error);

private:
    Ui::SaveFileBoxClass ui;
    FileSavePanel* m_fileSavePanel;

    // 图像参数
    uint16_t m_width;
    uint16_t m_height;
    uint8_t m_format;

    // 初始化文件保存组件
    bool initializeFileSaveComponents();

    // 连接信号槽
    void connectSignals();

    // 更新保存参数
    void updateSaveParameters();

    // 更新UI控件状态
    void updateUIState();

    // 关闭前清理资源
    void cleanupResources();
};