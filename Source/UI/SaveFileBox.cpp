// SaveFileBox.cpp
#include "SaveFileBox.h"
#include "Logger.h"
#include <QVBoxLayout>

SaveFileBox::SaveFileBox(QWidget* parent)
    : QWidget(parent)
    , m_fileSavePanel(nullptr)
    , m_width(1920)
    , m_height(1080)
    , m_format(0x39)  // 默认 RAW10
{
    ui.setupUi(this);

    // 初始化文件保存组件
    if (!initializeFileSaveComponents()) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化文件保存组件失败"));
    }

    // 设置窗口属性
    setWindowTitle(LocalQTCompat::fromLocal8Bit("文件保存设置"));
    setWindowFlags(Qt::Dialog);
    setWindowModality(Qt::ApplicationModal);

    // 连接按钮信号
    connect(ui.saveButton, &QPushButton::clicked, this, &SaveFileBox::onSaveButtonClicked);
    connect(ui.cancelButton, &QPushButton::clicked, this, &SaveFileBox::onCancelButtonClicked);
    connect(ui.browseFolderButton, &QPushButton::clicked, this, &SaveFileBox::onBrowseFolderButtonClicked);

    // 连接单选按钮信号
    connect(ui.saveRangeRadioButton, &QRadioButton::toggled, this, &SaveFileBox::onSaveRangeRadioButtonToggled);

    // 连接复选框信号
    connect(ui.lineRangeCheckBox, &QCheckBox::toggled, this, &SaveFileBox::onLineRangeCheckBoxToggled);
    connect(ui.columnRangeCheckBox, &QCheckBox::toggled, this, &SaveFileBox::onColumnRangeCheckBoxToggled);

    // 连接文件格式选择信号
    connect(ui.csvRadioButton, &QRadioButton::toggled, this, &SaveFileBox::onFileFormatChanged);
    connect(ui.txtRadioButton, &QRadioButton::toggled, this, &SaveFileBox::onFileFormatChanged);
    connect(ui.rawRadioButton, &QRadioButton::toggled, this, &SaveFileBox::onFileFormatChanged);
    connect(ui.bmpRadioButton, &QRadioButton::toggled, this, &SaveFileBox::onFileFormatChanged);

    // 连接 FileSaveManager 信号
    connect(&FileSaveManager::instance(), &FileSaveManager::saveCompleted,
        this, &SaveFileBox::onSaveManagerCompleted);
    connect(&FileSaveManager::instance(), &FileSaveManager::saveError,
        this, &SaveFileBox::onSaveManagerError);

    // 初始化 UI 状态
    ui.rangeFrame->setEnabled(false);
    updateUIState();
}

SaveFileBox::~SaveFileBox()
{
    cleanupResources();
    // FileSavePanel 会自动由 Qt 删除，因为它是作为子控件添加的
}

void SaveFileBox::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    m_width = width;
    m_height = height;
    m_format = format;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));
}

void SaveFileBox::prepareForShow()
{
    // 显示窗口前更新界面信息
    // 这里可以设置总行数等信息
    QString totalLines = QString::number(m_height); // 假设行数等于图像高度
    ui.totalLinesEdit->setText(totalLines);

    // 设置默认保存路径
    if (ui.pathEdit->text().isEmpty()) {
        ui.pathEdit->setText(QDir::homePath() + "/FX3Data");
    }

    // 更新行范围的最大值
    ui.toLineSpinBox->setMaximum(totalLines.toInt());

    // 根据图像格式自动选择相应的文件格式
    switch (m_format) {
    case 0x38: // RAW8
    case 0x39: // RAW10
    case 0x3A: // RAW12
        ui.rawRadioButton->setChecked(true);
        break;
    default:
        ui.csvRadioButton->setChecked(true);
        break;
    }

    updateUIState();
}

bool SaveFileBox::isSaving() const
{
    return m_fileSavePanel && m_fileSavePanel->isSaving();
}

bool SaveFileBox::initializeFileSaveComponents()
{
    try {
        // 获取容器
        QFrame* container = ui.fileSaveContainer;
        if (!container) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("未找到文件保存容器控件"));
            return false;
        }

        // 清除容器中的所有布局和控件
        QLayout* oldLayout = container->layout();
        if (oldLayout) {
            delete oldLayout;
        }

        // 创建新的垂直布局
        QVBoxLayout* layout = new QVBoxLayout(container);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(2);

        // 创建文件保存面板
        m_fileSavePanel = new FileSavePanel(container);
        if (!m_fileSavePanel) {
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("创建文件保存面板失败"));
            return false;
        }

        // 将面板添加到布局
        layout->addWidget(m_fileSavePanel);
        container->setLayout(layout);

        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存面板初始化成功"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("初始化文件保存组件异常: %1").arg(e.what()));
        return false;
    }
}

void SaveFileBox::updateSaveParameters()
{
    // 获取当前保存参数
    SaveParameters params = FileSaveManager::instance().getSaveParameters();

    // 更新文件路径
    if (!ui.pathEdit->text().isEmpty()) {
        params.basePath = ui.pathEdit->text();
    }

    // 更新文件格式（根据单选按钮选择）
    if (ui.csvRadioButton->isChecked()) {
        params.format = FileFormat::CSV;
    }
    else if (ui.txtRadioButton->isChecked()) {
        params.format = FileFormat::RAW;  // 简单文本文件
    }
    else if (ui.rawRadioButton->isChecked()) {
        params.format = FileFormat::RAW;  // 原始数据
    }
    else if (ui.bmpRadioButton->isChecked()) {
        params.format = FileFormat::BMP;  // BMP图像
    }

    // 更新文件前缀
    params.filePrefix = ui.prefixEdit->text();

    // 更新图像参数
    params.options["width"] = m_width;
    params.options["height"] = m_height;
    params.options["format"] = m_format;

    // 更新其他选项
    params.autoNaming = true;
    params.appendTimestamp = ui.appendTimestampCheckBox->isChecked();
    params.createSubfolder = ui.createSubfolderCheckBox->isChecked();

    // 设置保存范围参数
    if (ui.saveRangeRadioButton->isChecked()) {
        if (ui.lineRangeCheckBox->isChecked()) {
            params.options["from_line"] = ui.fromLineSpinBox->value();
            params.options["to_line"] = ui.toLineSpinBox->value();
        }

        if (ui.columnRangeCheckBox->isChecked()) {
            params.options["from_column"] = ui.fromColumnSpinBox->value();
            params.options["to_column"] = ui.toColumnSpinBox->value();
        }
    }
    else if (ui.splitByLinesRadioButton->isChecked()) {
        params.options["lines_per_file"] = ui.linesPerFileSpinBox->value();
    }

    // 每行最大显示字节数
    if (ui.maxBytesPerLineCheckBox->isChecked()) {
        params.options["bytes_per_line"] = ui.bytesPerLineComboBox->currentText().toInt();
    }

    // 更新保存参数
    FileSaveManager::instance().setSaveParameters(params);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("更新文件保存参数：路径=%1，格式=%2")
        .arg(params.basePath)
        .arg(static_cast<int>(params.format)));
}

void SaveFileBox::updateUIState()
{
    // 启用/禁用行列范围输入框
    bool enableLineRange = ui.saveRangeRadioButton->isChecked() && ui.lineRangeCheckBox->isChecked();
    ui.fromLineSpinBox->setEnabled(enableLineRange);
    ui.toLineSpinBox->setEnabled(enableLineRange);

    bool enableColumnRange = ui.saveRangeRadioButton->isChecked() && ui.columnRangeCheckBox->isChecked();
    ui.fromColumnSpinBox->setEnabled(enableColumnRange);
    ui.toColumnSpinBox->setEnabled(enableColumnRange);

    // 启用/禁用每个文件行数设置
    ui.linesPerFileSpinBox->setEnabled(ui.splitByLinesRadioButton->isChecked());

    // 启用/禁用每行最大字节数设置
    ui.bytesPerLineComboBox->setEnabled(ui.maxBytesPerLineCheckBox->isChecked());

    // 如果是图像文件格式，禁用一些选项
    bool isImageFormat = ui.bmpRadioButton->isChecked();
    ui.saveRangeGroupBox->setEnabled(!isImageFormat);
    ui.displayOptionsGroupBox->setEnabled(!isImageFormat);
}

void SaveFileBox::cleanupResources()
{
    // 停止文件保存（如果正在进行）
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));
        m_fileSavePanel->stopSaving();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存资源已清理"));
}

void SaveFileBox::onSaveButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("保存按钮点击"));

    // 如果已经在保存中，则停止保存
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        m_fileSavePanel->stopSaving();
        ui.saveButton->setText(LocalQTCompat::fromLocal8Bit("开始保存"));
        return;
    }

    // 更新保存参数
    updateSaveParameters();

    // 如果路径为空，提示选择路径
    if (ui.pathEdit->text().isEmpty()) {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("警告"),
            LocalQTCompat::fromLocal8Bit("请选择保存路径"));
        return;
    }

    // 启动保存
    if (m_fileSavePanel) {
        m_fileSavePanel->startSaving();
        ui.saveButton->setText(LocalQTCompat::fromLocal8Bit("停止保存"));
    }
    else {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("错误"),
            LocalQTCompat::fromLocal8Bit("文件保存面板未初始化"));
        close();
    }
}

void SaveFileBox::onCancelButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("取消按钮点击"));

    // 如果正在保存，询问是否停止
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            LocalQTCompat::fromLocal8Bit("确认"),
            LocalQTCompat::fromLocal8Bit("当前正在保存文件，是否停止并退出？"),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            m_fileSavePanel->stopSaving();
            close();
        }
    }
    else {
        // 不在保存，直接关闭
        close();
    }
}

void SaveFileBox::onBrowseFolderButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择文件路径按钮点击"));

    QString dir = QFileDialog::getExistingDirectory(
        this,
        LocalQTCompat::fromLocal8Bit("选择保存目录"),
        ui.pathEdit->text().isEmpty() ? QDir::homePath() : ui.pathEdit->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (!dir.isEmpty()) {
        ui.pathEdit->setText(dir);
    }
}

void SaveFileBox::onSaveRangeRadioButtonToggled(bool checked)
{
    ui.rangeFrame->setEnabled(checked);
    updateUIState();
}

void SaveFileBox::onLineRangeCheckBoxToggled(bool checked)
{
    updateUIState();
}

void SaveFileBox::onColumnRangeCheckBoxToggled(bool checked)
{
    updateUIState();
}

void SaveFileBox::onFileFormatChanged()
{
    updateUIState();
}

void SaveFileBox::onSaveManagerCompleted(const QString& path, uint64_t totalBytes)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存完成：路径=%1，总大小=%2字节")
        .arg(path).arg(totalBytes));

    // 更新按钮文本
    ui.saveButton->setText(LocalQTCompat::fromLocal8Bit("开始保存"));

    // 发送保存完成信号
    emit saveCompleted(path, totalBytes);

    // 显示保存完成消息
    QMessageBox::information(this,
        LocalQTCompat::fromLocal8Bit("保存完成"),
        LocalQTCompat::fromLocal8Bit("文件保存完成\n路径: %1\n总大小: %2 MB")
        .arg(path)
        .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2));
}

void SaveFileBox::onSaveManagerError(const QString& error)
{
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误：%1").arg(error));

    // 更新按钮文本
    ui.saveButton->setText(LocalQTCompat::fromLocal8Bit("开始保存"));

    // 发送保存错误信号
    emit saveError(error);

    // 显示错误消息
    QMessageBox::critical(this,
        LocalQTCompat::fromLocal8Bit("保存错误"),
        LocalQTCompat::fromLocal8Bit("文件保存错误：%1").arg(error));
}