// Source/MVC/Views/FileOperationView.cpp
#include "FileOperationView.h"
#include "ui_SaveFileBox.h"
#include "Logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QSettings>

FileOperationView::FileOperationView(QWidget* parent)
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

FileOperationView::~FileOperationView()
{
    delete ui;
    LOG_INFO("文件保存视图已销毁");
}

void FileOperationView::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    m_width = width;
    m_height = height;
    m_format = format;

    LOG_INFO(QString("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));
}

bool FileOperationView::isSaving() const
{
    return m_saving;
}

void FileOperationView::prepareForShow()
{
    // 显示窗口前更新界面信息
    // 设置总行数
    QString totalLines = QString::number(m_height);
    ui->totalLinesEdit->setText(totalLines);

    // 设置默认保存路径
    if (ui->pathEdit->text().isEmpty()) {
        SaveParameters params = FileOperationModel::getInstance()->getSaveParameters();
        ui->pathEdit->setText(params.basePath);
    }

    // 更新行范围的最大值
    ui->toLineSpinBox->setMaximum(totalLines.toInt());

    // 强制选择RAW格式并禁用其他选项
    ui->rawRadioButton->setChecked(true);
    ui->csvRadioButton->setEnabled(false);
    ui->txtRadioButton->setEnabled(false);
    ui->bmpRadioButton->setEnabled(false);

    updateUIState();
}

void FileOperationView::slot_FO_V_updateStatusDisplay(SaveStatus status)
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

void FileOperationView::slot_FO_V_updateStatisticsDisplay(const SaveStatistics& stats)
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

void FileOperationView::slot_FO_V_onSaveStarted()
{
    slot_FO_V_updateStatusDisplay(SaveStatus::FS_SAVING);
}

void FileOperationView::slot_FO_V_onSaveStopped()
{
    slot_FO_V_updateStatusDisplay(SaveStatus::FS_IDLE);
}

void FileOperationView::slot_FO_V_onSaveCompleted(const QString& path, uint64_t totalBytes)
{
    slot_FO_V_updateStatusDisplay(SaveStatus::FS_COMPLETED);

    QString message = LocalQTCompat::fromLocal8Bit("文件保存完成\n路径: %1\n总大小: %2 MB")
        .arg(path)
        .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2);

    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("保存完成"),
        message);
}

void FileOperationView::slot_FO_V_onSaveError(const QString& error)
{
    slot_FO_V_updateStatusDisplay(SaveStatus::FS_ERROR);

    QMessageBox::critical(this,
        LocalQTCompat::fromLocal8Bit("保存错误"),
        error);
}

void FileOperationView::slot_FO_V_onSaveButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存按钮点击"));

    if (m_saving) {
        // 如果正在保存，则停止保存
        emit signal_FO_V_stopSaveRequested();
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
    emit signal_FO_V_saveParametersChanged(params);

    // 请求开始保存
    emit signal_FO_V_startSaveRequested();
}

void FileOperationView::slot_FO_V_onCancelButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("取消按钮点击"));

    // 如果正在保存，询问是否停止
    if (m_saving) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            LocalQTCompat::fromLocal8Bit("确认"),
            LocalQTCompat::fromLocal8Bit("当前正在保存文件，是否停止？"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            emit signal_FO_V_stopSaveRequested();
        }
    }

    // 隐藏窗口
    hide();
}


void FileOperationView::slot_FO_V_onBrowseFolderButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择文件路径按钮点击"));

    QSettings settings("FX3Tool", "FileOperationPath");

    QString lastPath = settings.value("LastSelectedPath", QCoreApplication::applicationDirPath()).toString();
    QString defaultPath = ui->pathEdit->text().isEmpty() ? lastPath : ui->pathEdit->text();

    QString dir = QFileDialog::getExistingDirectory(
        this,
        LocalQTCompat::fromLocal8Bit("选择保存目录"),
        defaultPath,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        ui->pathEdit->setText(dir);
        settings.setValue("LastSelectedPath", dir);  // 保存当前选择的目录
    }
}

void FileOperationView::slot_FO_V_onSaveRangeRadioButtonToggled(bool checked)
{
    ui->rangeFrame->setEnabled(checked);
    updateUIState();
}

void FileOperationView::setupUI()
{
    // UI 已由 ui->setupUi(this) 在构造函数中完成初始化
}

void FileOperationView::connectSignals()
{
    // 连接按钮信号
    connect(ui->saveButton, &QPushButton::clicked, this, &FileOperationView::slot_FO_V_onSaveButtonClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &FileOperationView::slot_FO_V_onCancelButtonClicked);
    connect(ui->browseFolderButton, &QPushButton::clicked, this, &FileOperationView::slot_FO_V_onBrowseFolderButtonClicked);

    // 连接单选按钮信号
    connect(ui->saveRangeRadioButton, &QRadioButton::toggled, this, &FileOperationView::slot_FO_V_onSaveRangeRadioButtonToggled);

    // 连接复选框和其他控件信号以更新UI状态
    connect(ui->lineRangeCheckBox, &QCheckBox::toggled, this, &FileOperationView::updateUIState);
    connect(ui->columnRangeCheckBox, &QCheckBox::toggled, this, &FileOperationView::updateUIState);
    connect(ui->maxBytesPerLineCheckBox, &QCheckBox::toggled, this, &FileOperationView::updateUIState);
    connect(ui->csvRadioButton, &QRadioButton::toggled, this, &FileOperationView::updateUIState);
    connect(ui->txtRadioButton, &QRadioButton::toggled, this, &FileOperationView::updateUIState);
    connect(ui->rawRadioButton, &QRadioButton::toggled, this, &FileOperationView::updateUIState);
    connect(ui->bmpRadioButton, &QRadioButton::toggled, this, &FileOperationView::updateUIState);
    connect(ui->splitByLinesRadioButton, &QRadioButton::toggled, this, &FileOperationView::updateUIState);
}

SaveParameters FileOperationView::collectSaveParameters()
{
    // 获取当前保存参数
    SaveParameters params = FileOperationModel::getInstance()->getSaveParameters();

    // 更新文件路径
    if (!ui->pathEdit->text().isEmpty()) {
        params.basePath = ui->pathEdit->text();
    }

    // 强制使用RAW格式
    params.format = FileFormat::RAW;

    // 更新文件前缀
    params.filePrefix = ui->prefixEdit->text();

    // 更新图像参数
    params.options["width"] = m_width;
    params.options["height"] = m_height;
    params.options["format"] = m_format;

    // 更新其他选项
    params.autoNaming = true;  // 始终使用自动命名
    params.appendTimestamp = ui->appendTimestampCheckBox->isChecked();
    params.createSubfolder = ui->createSubfolderCheckBox->isChecked();

    // 强制启用自动保存
    params.options["auto_save"] = true;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新文件保存参数：路径=%1，格式=RAW，自动保存设置：已启用")
        .arg(params.basePath));

    return params;
}

void FileOperationView::updateUIState()
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