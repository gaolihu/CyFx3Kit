#pragma once

#include <QWidget>
#include <QDialog>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QFrame>
#include <QFileDialog>
#include <QProgressBar>
#include <QStatusBar>
#include <QMessageBox>
#include "FileSaveManager.h"

// 文件保存设置对话框
class FileSaveSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit FileSaveSettingsDialog(QWidget* parent = nullptr);

private slots:
    void onAccepted();
    void onBrowseClicked();
    void onFormatChanged(int index);

private:
    void setupUI();
    void loadSettings();
    void saveSettings();
    void updateCompressionControls();

private:
    QLineEdit* m_pathEdit;
    QComboBox* m_formatCombo;
    QLineEdit* m_prefixEdit;
    QCheckBox* m_autoNamingCheck;
    QCheckBox* m_createSubfolderCheck;
    QCheckBox* m_appendTimestampCheck;
    QCheckBox* m_saveMetadataCheck;
    QSpinBox* m_compressionSpin;
    QCheckBox* m_useAsyncWriterCheck;
    QLabel* m_compressionLabel;
};

// 文件保存控制面板
class FileSavePanel : public QWidget {
    Q_OBJECT

public:
    explicit FileSavePanel(QWidget* parent = nullptr);

public slots:
    // 外部调用接口
    void startSaving();
    void stopSaving();
    bool isSaving() const { return m_saving; }

private slots:
    void onStartSaveClicked();
    void onSettingsClicked();
    void onSaveStatusChanged(SaveStatus status);
    void onSaveProgressUpdated(const SaveStatistics& stats);
    void onSaveCompleted(const QString& path, uint64_t totalBytes);
    void onSaveError(const QString& error);

private:
    void setupUI();
    void connectSignals();
    void updateUIForSaving(bool saving);

private:
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QLabel* m_speedLabel;
    QLabel* m_fileCountLabel;
    QLabel* m_totalSizeLabel;
    QPushButton* m_startSaveButton;

    bool m_saving;
};