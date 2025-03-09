﻿// Source/MVC/Views/FileSaveView.cpp
#include "FileSaveView.h"
#include "ui_SaveFileBox.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>

FileSaveView::FileSaveView(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SaveFileBox)
    , m_width(1920)
    , m_height(1080)
    , m_format(0x39)  // 默认 RAW10
    , m_saving(false)
{
    ui->setupUi(this);

    // 初始化UI状态
    ui->rangeFrame->setEnabled(false);
    updateUIState();

    // 连接信号
    connectSignals();

    LOG_INFO("文件保存视图已创建");
}

FileSaveView::~FileSaveView()
{
    delete ui;
    LOG_INFO("文件保存视图已销毁");
}

void FileSaveView::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    m_width = width;
    m_height = height;
    m_format = format;

    LOG_INFO(QString("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));
}

bool FileSaveView::isSaving() const
{
    return m_saving;
}

void FileSaveView::prepareForShow()
{
    // 显示窗口前更新界面信息
    // 设置总行数
    QString totalLines = QString::number(m_height);
    ui->totalLinesEdit->setText(totalLines);

    // 设置默认保存路径
    if (ui->pathEdit->text().isEmpty()) {
        SaveParameters params = FileSaveModel::getInstance()->getSaveParameters();
        ui->pathEdit->setText(params.basePath);
    }

    // 更新行范围的最大值
    ui->toLineSpinBox->setMaximum(totalLines.toInt());

    // 根据图像格式自动选择相应的文件格式
    switch (m_format) {
    case 0x38: // RAW8
    case 0x39: // RAW10
    case 0x3A: // RAW12
        ui->rawRadioButton->setChecked(true);
        break;
    default:
        ui->csvRadioButton->setChecked(true);
        break;
    }

    updateUIState();
}

void FileSaveView::updateStatusDisplay(SaveStatus status)
{
    switch (status) {
    case SaveStatus::FS_IDLE:
        ui->statusLabel->setText(LocalQTCompat::fromLocal8Bit("空闲"));
        ui->progressBar->setValue(0);
        ui->progressBar->setRange(0, 100);
        ui->saveButton->setText(LocalQTCompat::fromLocal8Bit("开始保存"));
        m_saving = false;
        break;
    case SaveStatus::FS_SAVING:
        ui->statusLabel->setText(LocalQTCompat::fromLocal8Bit("保存中"));
        ui->progressBar->setRange(0, 0); // 设置为动画模式
        ui->saveButton->setText(LocalQTCompat::fromLocal8Bit("停止保存"));
        m_saving = true;
        break;
    case SaveStatus::FS_PAUSED:
        ui->statusLabel->setText(LocalQTCompat::fromLocal8Bit("已暂停"));
        break;
    case SaveStatus::FS_COMPLETED:
        ui->statusLabel->setText(LocalQTCompat::fromLocal8Bit("已完成"));
        ui->progressBar->setValue(100);
        ui->progressBar->setRange(0, 100);
        ui->saveButton->setText(LocalQTCompat::fromLocal8Bit("开始保存"));
        m_saving = false;
        break;
    case SaveStatus::FS_ERROR:
        ui->statusLabel->setText(LocalQTCompat::fromLocal8Bit("错误"));
        ui->progressBar->setRange(0, 100);
        ui->saveButton->setText(LocalQTCompat::fromLocal8Bit("开始保存"));
        m_saving = false;
        break;
    }

    // 根据状态启用/禁用设置选项
    ui->formatGroupBox->setEnabled(!m_saving);
    ui->saveOptionsGroupBox->setEnabled(!m_saving);
    ui->saveRangeGroupBox->setEnabled(!m_saving);
    ui->displayOptionsGroupBox->setEnabled(!m_saving);
}

void FileSaveView::updateStatisticsDisplay(const SaveStatistics& stats)
{
    // 更新进度条
    if (stats.progress > 0 && m_saving) {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(static_cast<int>(stats.progress));
    }

    // 更新保存速率标签
    ui->speedLabel->setText(LocalQTCompat::fromLocal8Bit("速度: %1 MB/s").arg(stats.saveRate, 0, 'f', 2));

    // 更新文件计数标签
    ui->fileCountLabel->setText(LocalQTCompat::fromLocal8Bit("文件数: %1").arg(stats.fileCount));

    // 更新总字节数标签
    QString sizeText;
    if (stats.totalBytes < 1024 * 1024) {
        sizeText = LocalQTCompat::fromLocal8Bit("已保存: %1 KB").arg(stats.totalBytes / 1024.0, 0, 'f', 2);
    }
    else if (stats.totalBytes < 1024 * 1024 * 1024) {
        sizeText = LocalQTCompat::fromLocal8Bit("已保存: %1 MB").arg(stats.totalBytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
    else {
        sizeText = LocalQTCompat::fromLocal8Bit("已保存: %1 GB").arg(stats.totalBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
    ui->totalSizeLabel->setText(sizeText);
}

void FileSaveView::onSaveStarted()
{
    updateStatusDisplay(SaveStatus::FS_SAVING);
}

void FileSaveView::onSaveStopped()
{
    updateStatusDisplay(SaveStatus::FS_IDLE);
}

void FileSaveView::onSaveCompleted(const QString& path, uint64_t totalBytes)
{
    updateStatusDisplay(SaveStatus::FS_COMPLETED);

    QString message = LocalQTCompat::fromLocal8Bit("文件保存完成\n路径: %1\n总大小: %2 MB")
        .arg(path)
        .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2);

    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("保存完成"),
        message);
}

void FileSaveView::onSaveError(const QString& error)
{
    updateStatusDisplay(SaveStatus::FS_ERROR);

    QMessageBox::critical(this,
        LocalQTCompat::fromLocal8Bit("保存错误"),
        error);
}

void FileSaveView::onSaveButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存按钮点击"));

    if (m_saving) {
        // 如果正在保存，则停止保存
        emit stopSaveRequested();
        return;
    }

    // 如果路径为空，提示选择路径
    if (ui->pathEdit->text().isEmpty()) {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("警告"),
            LocalQTCompat::fromLocal8Bit("请选择保存路径"));
        return;
    }

    // 更新保存参数
    SaveParameters params = collectSaveParameters();
    emit saveParametersChanged(params);

    // 请求开始保存
    emit startSaveRequested();
}

void FileSaveView::onCancelButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("取消按钮点击"));

    // 如果正在保存，询问是否停止
    if (m_saving) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            LocalQTCompat::fromLocal8Bit("确认"),
            LocalQTCompat::fromLocal8Bit("当前正在保存文件，是否停止？"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            emit stopSaveRequested();
        }
    }

    // 隐藏窗口
    hide();
}

void FileSaveView::onBrowseFolderButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择文件路径按钮点击"));

    QString dir = QFileDialog::getExistingDirectory(
        this,
        LocalQTCompat::fromLocal8Bit("选择保存目录"),
        ui->pathEdit->text().isEmpty() ? QDir::homePath() : ui->pathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        ui->pathEdit->setText(dir);
    }
}

void FileSaveView::onSaveRangeRadioButtonToggled(bool checked)
{
    ui->rangeFrame->setEnabled(checked);
    updateUIState();
}

void FileSaveView::setupUI()
{
    // UI 已由 ui->setupUi(this) 在构造函数中完成初始化
}

void FileSaveView::connectSignals()
{
    // 连接按钮信号
    connect(ui->saveButton, &QPushButton::clicked, this, &FileSaveView::onSaveButtonClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &FileSaveView::onCancelButtonClicked);
    connect(ui->browseFolderButton, &QPushButton::clicked, this, &FileSaveView::onBrowseFolderButtonClicked);

    // 连接单选按钮信号
    connect(ui->saveRangeRadioButton, &QRadioButton::toggled, this, &FileSaveView::onSaveRangeRadioButtonToggled);

    // 连接复选框和其他控件信号以更新UI状态
    connect(ui->lineRangeCheckBox, &QCheckBox::toggled, this, &FileSaveView::updateUIState);
    connect(ui->columnRangeCheckBox, &QCheckBox::toggled, this, &FileSaveView::updateUIState);
    connect(ui->maxBytesPerLineCheckBox, &QCheckBox::toggled, this, &FileSaveView::updateUIState);
    connect(ui->csvRadioButton, &QRadioButton::toggled, this, &FileSaveView::updateUIState);
    connect(ui->txtRadioButton, &QRadioButton::toggled, this, &FileSaveView::updateUIState);
    connect(ui->rawRadioButton, &QRadioButton::toggled, this, &FileSaveView::updateUIState);
    connect(ui->bmpRadioButton, &QRadioButton::toggled, this, &FileSaveView::updateUIState);
    connect(ui->splitByLinesRadioButton, &QRadioButton::toggled, this, &FileSaveView::updateUIState);
}

SaveParameters FileSaveView::collectSaveParameters()
{
    // 获取当前保存参数
    SaveParameters params = FileSaveModel::getInstance()->getSaveParameters();

    // 更新文件路径
    if (!ui->pathEdit->text().isEmpty()) {
        params.basePath = ui->pathEdit->text();
    }

    // 更新文件格式
    if (ui->csvRadioButton->isChecked()) {
        params.format = FileFormat::CSV;
    }
    else if (ui->txtRadioButton->isChecked()) {
        params.format = FileFormat::TEXT;
    }
    else if (ui->rawRadioButton->isChecked()) {
        params.format = FileFormat::RAW;
    }
    else if (ui->bmpRadioButton->isChecked()) {
        params.format = FileFormat::BMP;
    }

    // 更新文件前缀
    params.filePrefix = ui->prefixEdit->text();

    // 更新图像参数
    params.options["width"] = m_width;
    params.options["height"] = m_height;
    params.options["format"] = m_format;

    // 更新其他选项
    params.autoNaming = true;
    params.appendTimestamp = ui->appendTimestampCheckBox->isChecked();
    params.createSubfolder = ui->createSubfolderCheckBox->isChecked();

    // 设置保存范围参数
    if (ui->saveRangeRadioButton->isChecked()) {
        if (ui->lineRangeCheckBox->isChecked()) {
            params.options["from_line"] = ui->fromLineSpinBox->value();
            params.options["to_line"] = ui->toLineSpinBox->value();
        }

        if (ui->columnRangeCheckBox->isChecked()) {
            params.options["from_column"] = ui->fromColumnSpinBox->value();
            params.options["to_column"] = ui->toColumnSpinBox->value();
        }
    }
    else if (ui->splitByLinesRadioButton->isChecked()) {
        params.options["lines_per_file"] = ui->linesPerFileSpinBox->value();
    }

    // 每行最大显示字节数
    if (ui->maxBytesPerLineCheckBox->isChecked()) {
        params.options["bytes_per_line"] = ui->bytesPerLineComboBox->currentText().toInt();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新文件保存参数：路径=%1，格式=%2")
        .arg(params.basePath)
        .arg(static_cast<int>(params.format)));

    return params;
}

void FileSaveView::updateUIState()
{
    // 启用/禁用行列范围输入框
    bool enableLineRange = ui->saveRangeRadioButton->isChecked() && ui->lineRangeCheckBox->isChecked();
    ui->fromLineSpinBox->setEnabled(enableLineRange);
    ui->toLineSpinBox->setEnabled(enableLineRange);

    bool enableColumnRange = ui->saveRangeRadioButton->isChecked() && ui->columnRangeCheckBox->isChecked();
    ui->fromColumnSpinBox->setEnabled(enableColumnRange);
    ui->toColumnSpinBox->setEnabled(enableColumnRange);

    // 启用/禁用每个文件行数设置
    ui->linesPerFileSpinBox->setEnabled(ui->splitByLinesRadioButton->isChecked());

    // 启用/禁用每行最大字节数设置
    ui->bytesPerLineComboBox->setEnabled(ui->maxBytesPerLineCheckBox->isChecked());

    // 如果是图像文件格式，禁用一些选项
    bool isImageFormat = ui->bmpRadioButton->isChecked();
    ui->saveRangeGroupBox->setEnabled(!isImageFormat);
    ui->displayOptionsGroupBox->setEnabled(!isImageFormat);
}