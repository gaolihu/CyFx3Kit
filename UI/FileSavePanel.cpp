#include "FileSavePanel.h"
#include "Logger.h"

// FileSaveSettingsDialog 实现
FileSaveSettingsDialog::FileSaveSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
    updateCompressionControls();
}

void FileSaveSettingsDialog::setupUI() {
    setWindowTitle(fromLocal8Bit("文件保存设置"));
    setMinimumWidth(450);

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 基本设置组
    QGroupBox* basicGroup = new QGroupBox(fromLocal8Bit("基本设置"), this);
    QFormLayout* basicLayout = new QFormLayout(basicGroup);

    // 保存路径设置
    QHBoxLayout* pathLayout = new QHBoxLayout();
    m_pathEdit = new QLineEdit(this);
    QPushButton* browseButton = new QPushButton(fromLocal8Bit("浏览..."), this);
    connect(browseButton, &QPushButton::clicked, this, &FileSaveSettingsDialog::onBrowseClicked);
    pathLayout->addWidget(m_pathEdit);
    pathLayout->addWidget(browseButton);
    basicLayout->addRow(fromLocal8Bit("保存路径:"), pathLayout);

    // 文件格式选择
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("RAW (原始数据)", static_cast<int>(FileFormat::RAW));
    m_formatCombo->addItem("BMP (位图)", static_cast<int>(FileFormat::BMP));
    m_formatCombo->addItem("TIFF (图像)", static_cast<int>(FileFormat::TIFF));
    m_formatCombo->addItem("PNG (压缩图像)", static_cast<int>(FileFormat::PNG));
    m_formatCombo->addItem("CSV (元数据)", static_cast<int>(FileFormat::CSV));
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &FileSaveSettingsDialog::onFormatChanged);
    basicLayout->addRow(fromLocal8Bit("文件格式:"), m_formatCombo);

    // 文件前缀
    m_prefixEdit = new QLineEdit(this);
    basicLayout->addRow(fromLocal8Bit("文件前缀:"), m_prefixEdit);

    // 添加基本设置组到主布局
    mainLayout->addWidget(basicGroup);

    // 高级设置组
    QGroupBox* advancedGroup = new QGroupBox(fromLocal8Bit("高级设置"), this);
    QFormLayout* advancedLayout = new QFormLayout(advancedGroup);

    // 自动命名选项
    m_autoNamingCheck = new QCheckBox(fromLocal8Bit("自动命名文件"), this);
    advancedLayout->addRow("", m_autoNamingCheck);

    // 创建子文件夹选项
    m_createSubfolderCheck = new QCheckBox(fromLocal8Bit("创建日期子文件夹"), this);
    advancedLayout->addRow("", m_createSubfolderCheck);

    // 附加时间戳选项
    m_appendTimestampCheck = new QCheckBox(fromLocal8Bit("文件名附加时间戳"), this);
    advancedLayout->addRow("", m_appendTimestampCheck);

    // 保存元数据选项
    m_saveMetadataCheck = new QCheckBox(fromLocal8Bit("保存元数据文件"), this);
    advancedLayout->addRow("", m_saveMetadataCheck);

    // 使用异步写入选项
    m_useAsyncWriterCheck = new QCheckBox(fromLocal8Bit("使用异步文件写入"), this);
    m_useAsyncWriterCheck->setToolTip(fromLocal8Bit("启用后使用单独线程写入文件，可能提高性能"));
    advancedLayout->addRow("", m_useAsyncWriterCheck);

    // 压缩级别设置
    m_compressionSpin = new QSpinBox(this);
    m_compressionSpin->setRange(0, 9);
    m_compressionSpin->setSingleStep(1);
    m_compressionSpin->setPrefix(fromLocal8Bit("级别: "));
    m_compressionSpin->setToolTip(fromLocal8Bit("0: 不压缩, 9: 最大压缩"));
    m_compressionLabel = new QLabel(fromLocal8Bit("压缩级别:"), this);
    advancedLayout->addRow(m_compressionLabel, m_compressionSpin);

    // 添加高级设置组到主布局
    mainLayout->addWidget(advancedGroup);

    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okButton = new QPushButton(fromLocal8Bit("确定"), this);
    QPushButton* cancelButton = new QPushButton(fromLocal8Bit("取消"), this);
    connect(okButton, &QPushButton::clicked, this, &FileSaveSettingsDialog::onAccepted);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void FileSaveSettingsDialog::loadSettings() {
    // 从文件保存管理器获取当前设置
    SaveParameters params = FileSaveManager::instance().getSaveParameters();

    m_pathEdit->setText(params.basePath);
    m_prefixEdit->setText(params.filePrefix);

    // 设置下拉框索引
    int formatIndex = m_formatCombo->findData(static_cast<int>(params.format));
    if (formatIndex >= 0) {
        m_formatCombo->setCurrentIndex(formatIndex);
    }

    m_autoNamingCheck->setChecked(params.autoNaming);
    m_createSubfolderCheck->setChecked(params.createSubfolder);
    m_appendTimestampCheck->setChecked(params.appendTimestamp);
    m_saveMetadataCheck->setChecked(params.saveMetadata);
    m_compressionSpin->setValue(params.compressionLevel);

    // 异步写入器状态从FileSaveManager获取
    bool useAsyncWriter = dynamic_cast<AsyncFileWriter*>(FileSaveManager::instance().m_fileWriter.get()) != nullptr;
    m_useAsyncWriterCheck->setChecked(useAsyncWriter);
}

void FileSaveSettingsDialog::saveSettings() {
    // 创建保存参数
    SaveParameters params;
    params.basePath = m_pathEdit->text();
    params.filePrefix = m_prefixEdit->text();
    params.format = static_cast<FileFormat>(m_formatCombo->currentData().toInt());
    params.autoNaming = m_autoNamingCheck->isChecked();
    params.createSubfolder = m_createSubfolderCheck->isChecked();
    params.appendTimestamp = m_appendTimestampCheck->isChecked();
    params.saveMetadata = m_saveMetadataCheck->isChecked();
    params.compressionLevel = m_compressionSpin->value();

    // 更新文件保存管理器设置
    FileSaveManager::instance().setSaveParameters(params);

    // 设置异步写入器
    FileSaveManager::instance().setUseAsyncWriter(m_useAsyncWriterCheck->isChecked());
}

void FileSaveSettingsDialog::onAccepted() {
    saveSettings();
    accept();
}

void FileSaveSettingsDialog::onBrowseClicked() {
    QString dir = QFileDialog::getExistingDirectory(
        this,
        fromLocal8Bit("选择保存目录"),
        m_pathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        m_pathEdit->setText(dir);
    }
}

void FileSaveSettingsDialog::onFormatChanged(int index) {
    updateCompressionControls();
}

void FileSaveSettingsDialog::updateCompressionControls() {
    // 根据当前选择的格式启用或禁用压缩控件
    FileFormat format = static_cast<FileFormat>(m_formatCombo->currentData().toInt());
    bool enableCompression = (format == FileFormat::PNG || format == FileFormat::TIFF);

    m_compressionSpin->setEnabled(enableCompression);
    m_compressionLabel->setEnabled(enableCompression);
}

// FileSavePanel 实现
FileSavePanel::FileSavePanel(QWidget* parent)
    : QWidget(parent)
    , m_saving(false)
{
    setupUI();
    connectSignals();
}

void FileSavePanel::setupUI() {
    // 创建布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // 标题和设置按钮
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(fromLocal8Bit("文件保存控制"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    QPushButton* settingsButton = new QPushButton(fromLocal8Bit("设置"), this);
    settingsButton->setToolTip(fromLocal8Bit("文件保存设置"));
    connect(settingsButton, &QPushButton::clicked, this, &FileSavePanel::onSettingsClicked);

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(settingsButton);
    mainLayout->addLayout(titleLayout);

    // 状态和进度
    QHBoxLayout* statusLayout = new QHBoxLayout();
    QLabel* statusTextLabel = new QLabel(fromLocal8Bit("状态:"), this);
    m_statusLabel = new QLabel(fromLocal8Bit("空闲"), this);
    statusLayout->addWidget(statusTextLabel);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    mainLayout->addLayout(statusLayout);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // 信息标签组
    QHBoxLayout* infoLayout = new QHBoxLayout();
    m_speedLabel = new QLabel(fromLocal8Bit("速度: 0 MB/s"), this);
    m_fileCountLabel = new QLabel(fromLocal8Bit("文件数: 0"), this);
    m_totalSizeLabel = new QLabel(fromLocal8Bit("已保存: 0 KB"), this);

    infoLayout->addWidget(m_speedLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_fileCountLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_totalSizeLabel);
    mainLayout->addLayout(infoLayout);

    // 开始/停止按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_startSaveButton = new QPushButton(fromLocal8Bit("开始保存"), this);
    m_startSaveButton->setMinimumWidth(100);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_startSaveButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // 设置固定高度
    setMinimumHeight(150);
}

void FileSavePanel::connectSignals() {
    // 连接按钮信号
    connect(m_startSaveButton, &QPushButton::clicked, this, &FileSavePanel::onStartSaveClicked);

    // 连接FileSaveManager信号
    connect(&FileSaveManager::instance(), &FileSaveManager::saveStatusChanged,
        this, &FileSavePanel::onSaveStatusChanged);
    connect(&FileSaveManager::instance(), &FileSaveManager::saveProgressUpdated,
        this, &FileSavePanel::onSaveProgressUpdated);
    connect(&FileSaveManager::instance(), &FileSaveManager::saveCompleted,
        this, &FileSavePanel::onSaveCompleted);
    connect(&FileSaveManager::instance(), &FileSaveManager::saveError,
        this, &FileSavePanel::onSaveError);
}

void FileSavePanel::updateUIForSaving(bool saving) {
    if (saving) {
        m_startSaveButton->setText(fromLocal8Bit("停止保存"));
        m_progressBar->setRange(0, 0); // 设置为动画模式
    }
    else {
        m_startSaveButton->setText(fromLocal8Bit("开始保存"));
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
    }
}

void FileSavePanel::startSaving() {
    if (!m_saving) {
        onStartSaveClicked();
    }
}

void FileSavePanel::stopSaving() {
    if (m_saving) {
        onStartSaveClicked();
    }
}

void FileSavePanel::onStartSaveClicked() {
    if (!m_saving) {
        // 开始保存
        if (FileSaveManager::instance().startSaving()) {
            updateUIForSaving(true);
            m_saving = true;
        }
        else {
            QMessageBox::warning(this,
                fromLocal8Bit("保存错误"),
                fromLocal8Bit("无法开始文件保存，请检查设置。"));
        }
    }
    else {
        // 停止保存
        if (FileSaveManager::instance().stopSaving()) {
            updateUIForSaving(false);
            m_saving = false;
        }
        else {
            QMessageBox::warning(this,
                fromLocal8Bit("保存错误"),
                fromLocal8Bit("无法停止文件保存，请重试。"));
        }
    }
}

void FileSavePanel::onSettingsClicked() {
    // 保存过程中不允许更改设置
    if (m_saving) {
        QMessageBox::information(this,
            fromLocal8Bit("设置"),
            fromLocal8Bit("请先停止保存后再更改设置。"));
        return;
    }

    FileSaveSettingsDialog dialog(this);
    dialog.exec();
}

void FileSavePanel::onSaveStatusChanged(SaveStatus status) {
    switch (status) {
    case SaveStatus::FS_IDLE:
        m_statusLabel->setText(fromLocal8Bit("空闲"));
        m_progressBar->setValue(0);
        updateUIForSaving(false);
        m_saving = false;
        break;
    case SaveStatus::FS_SAVING:
        m_statusLabel->setText(fromLocal8Bit("保存中"));
        updateUIForSaving(true);
        m_saving = true;
        break;
    case SaveStatus::FS_PAUSED:
        m_statusLabel->setText(fromLocal8Bit("已暂停"));
        break;
    case SaveStatus::FS_COMPLETED:
        m_statusLabel->setText(fromLocal8Bit("已完成"));
        updateUIForSaving(false);
        m_saving = false;
        break;
    case SaveStatus::FS_ERROR:
        m_statusLabel->setText(fromLocal8Bit("错误"));
        updateUIForSaving(false);
        m_saving = false;
        break;
    }
}

void FileSavePanel::onSaveProgressUpdated(const SaveStatistics& stats) {
    // 更新进度条（可选的进度指示）
    m_progressBar->setRange(0, 0);

    // 更新保存速率标签
    m_speedLabel->setText(fromLocal8Bit("速度: %1 MB/s").arg(stats.saveRate, 0, 'f', 2));

    // 更新文件计数标签
    m_fileCountLabel->setText(fromLocal8Bit("文件数: %1").arg(stats.fileCount));

    // 更新总字节数标签
    QString sizeText;
    if (stats.totalBytes < 1024 * 1024) {
        sizeText = fromLocal8Bit("已保存: %1 KB").arg(stats.totalBytes / 1024.0, 0, 'f', 2);
    }
    else if (stats.totalBytes < 1024 * 1024 * 1024) {
        sizeText = fromLocal8Bit("已保存: %1 MB").arg(stats.totalBytes / (1024.0 * 1024.0), 0, 'f', 2);
    }
    else {
        sizeText = fromLocal8Bit("已保存: %1 GB").arg(stats.totalBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
    m_totalSizeLabel->setText(sizeText);
}

void FileSavePanel::onSaveCompleted(const QString& path, uint64_t totalBytes) {
    QString message = fromLocal8Bit("文件保存完成\n路径: %1\n总大小: %2 MB")
        .arg(path)
        .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2);

    m_statusLabel->setText(fromLocal8Bit("完成"));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(100);

    // 显示完成通知
    QMessageBox::information(this,
        fromLocal8Bit("保存完成"),
        message);
}

void FileSavePanel::onSaveError(const QString& error) {
    m_statusLabel->setText(fromLocal8Bit("错误"));
    updateUIForSaving(false);
    m_saving = false;

    QMessageBox::critical(this,
        fromLocal8Bit("保存错误"),
        error);
}