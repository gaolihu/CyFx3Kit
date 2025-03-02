// UpdataDevice.cpp
#include "UpdataDevice.h"
#include "Logger.h"
#include <QFileInfo>

UpdataDevice::UpdataDevice(QWidget* parent)
    : QWidget(parent)
    , m_isUpdating(false)
{
    ui.setupUi(this);

    initializeUI();
    connectSignals();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级窗口已创建"));
}

UpdataDevice::~UpdataDevice()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级窗口被销毁"));
}

void UpdataDevice::initializeUI()
{
    // 设置窗口标题
    setWindowTitle(LocalQTCompat::fromLocal8Bit("设备升级"));

    // 设置窗口固定大小
    setFixedSize(572, 351);

    // 设置窗口模态属性
    setWindowModality(Qt::ApplicationModal);

    // 初始状态下禁用升级按钮
    ui.updata_ok->setEnabled(false);
    ui.pushButton_2->setEnabled(false);

    // 进度条初始化
    ui.progressBar->setValue(0);
    ui.progressBar_2->setValue(0);

    // 清空提示信息
    ui.tishi->setText("");

    updateUIState();
}

void UpdataDevice::connectSignals()
{
    // 连接文件选择按钮信号
    connect(ui.updataopen, &QPushButton::clicked, this, &UpdataDevice::onSOCFileOpenButtonClicked);
    connect(ui.pushButton, &QPushButton::clicked, this, &UpdataDevice::onISOFileOpenButtonClicked);

    // 连接升级按钮信号
    connect(ui.updata_ok, &QPushButton::clicked, this, &UpdataDevice::onSOCUpdateButtonClicked);
    connect(ui.pushButton_2, &QPushButton::clicked, this, &UpdataDevice::onPHYUpdateButtonClicked);
}

void UpdataDevice::updateUIState()
{
    // 根据是否正在升级更新UI状态
    bool isUpdating = m_isUpdating;

    // 文件选择按钮禁用状态
    ui.updataopen->setEnabled(!isUpdating);
    ui.pushButton->setEnabled(!isUpdating);

    // SOC升级按钮状态
    bool hasSOCFile = !m_socFilePath.isEmpty();
    ui.updata_ok->setEnabled(hasSOCFile && !isUpdating);

    // PHY升级按钮状态
    bool hasISOFile = !m_isoFilePath.isEmpty();
    ui.pushButton_2->setEnabled(hasISOFile && !isUpdating);
}

bool UpdataDevice::validateFile(const QString& filePath, const QString& fileType)
{
    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("文件错误"),
            LocalQTCompat::fromLocal8Bit("文件不存在: %1").arg(filePath));
        return false;
    }

    // 检查文件大小
    qint64 fileSize = fileInfo.size();
    if (fileSize <= 0) {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("文件错误"),
            LocalQTCompat::fromLocal8Bit("文件大小为0: %1").arg(filePath));
        return false;
    }

    // 检查文件类型
    QString suffix = fileInfo.suffix().toLower();
    if (fileType == "SOC" && suffix != "soc") {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("文件类型错误"),
            LocalQTCompat::fromLocal8Bit("请选择.soc格式的文件"));
        return false;
    }
    else if (fileType == "ISO" && suffix != "iso") {
        QMessageBox::warning(this,
            LocalQTCompat::fromLocal8Bit("文件类型错误"),
            LocalQTCompat::fromLocal8Bit("请选择.iso格式的文件"));
        return false;
    }

    return true;
}

void UpdataDevice::onSOCFileOpenButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择SOC文件"));

    // 弹出文件选择对话框
    QString filePath = QFileDialog::getOpenFileName(this,
        LocalQTCompat::fromLocal8Bit("选择SOC文件"),
        QDir::homePath(),
        LocalQTCompat::fromLocal8Bit("SOC文件 (*.soc)"));

    // 如果没有选择文件则返回
    if (filePath.isEmpty()) {
        return;
    }

    // 验证文件
    if (!validateFile(filePath, "SOC")) {
        return;
    }

    // 保存文件路径并更新UI
    m_socFilePath = filePath;
    ui.lineEdit->setText(filePath);

    // 更新UI状态
    updateUIState();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已选择SOC文件: %1").arg(filePath));
}

void UpdataDevice::onISOFileOpenButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("选择ISO文件"));

    // 弹出文件选择对话框
    QString filePath = QFileDialog::getOpenFileName(this,
        LocalQTCompat::fromLocal8Bit("选择ISO文件"),
        QDir::homePath(),
        LocalQTCompat::fromLocal8Bit("ISO文件 (*.iso)"));

    // 如果没有选择文件则返回
    if (filePath.isEmpty()) {
        return;
    }

    // 验证文件
    if (!validateFile(filePath, "ISO")) {
        return;
    }

    // 保存文件路径并更新UI
    m_isoFilePath = filePath;
    ui.lineEdit_2->setText(filePath);

    // 更新UI状态
    updateUIState();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已选择ISO文件: %1").arg(filePath));
}

void UpdataDevice::onSOCUpdateButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始SOC升级"));

    // 再次验证文件
    if (!validateFile(m_socFilePath, "SOC")) {
        return;
    }

    // 确认升级
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        LocalQTCompat::fromLocal8Bit("确认升级"),
        LocalQTCompat::fromLocal8Bit("确定要开始SOC固件升级吗？\n升级过程中请勿断开设备电源！"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("用户取消SOC升级"));
        return;
    }

    // 设置升级状态
    m_isUpdating = true;
    updateUIState();

    // 设置按钮文本
    ui.updata_ok->setText(LocalQTCompat::fromLocal8Bit("升级中"));

    // 设置提示信息
    ui.tishi->setText(LocalQTCompat::fromLocal8Bit("SOC升级中，请勿断开电源..."));

    // 模拟升级进度（实际实现需连接到设备管理器）
    ui.progressBar->setValue(0);

    // 这里应该调用设备管理器的实际升级方法
    // 为了演示，使用一个计时器模拟升级进度
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        static int progress = 0;
        progress += 5;

        // 更新进度条
        ui.progressBar->setValue(progress);

        // 模拟升级完成
        if (progress >= 100) {
            timer->stop();
            timer->deleteLater();

            // 恢复状态
            m_isUpdating = false;
            ui.updata_ok->setText(LocalQTCompat::fromLocal8Bit("发送"));
            ui.tishi->setText(LocalQTCompat::fromLocal8Bit("SOC升级完成"));

            // 更新UI状态
            updateUIState();

            // 通知升级完成
            emit updateCompleted(true, LocalQTCompat::fromLocal8Bit("SOC升级成功"));

            // 提示用户
            QMessageBox::information(this,
                LocalQTCompat::fromLocal8Bit("升级完成"),
                LocalQTCompat::fromLocal8Bit("SOC固件升级成功！"));

            LOG_INFO(LocalQTCompat::fromLocal8Bit("SOC升级完成"));
        }
        });

    // 启动计时器，每100毫秒更新一次
    timer->start(100);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("SOC升级任务已启动"));
}

void UpdataDevice::onPHYUpdateButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始PHY升级"));

    // 再次验证文件
    if (!validateFile(m_isoFilePath, "ISO")) {
        return;
    }

    // 确认升级
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        LocalQTCompat::fromLocal8Bit("确认升级"),
        LocalQTCompat::fromLocal8Bit("确定要开始PHY固件升级吗？\n升级过程中请勿断开设备电源！"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        LOG_INFO(LocalQTCompat::fromLocal8Bit("用户取消PHY升级"));
        return;
    }

    // 设置升级状态
    m_isUpdating = true;
    updateUIState();

    // 设置按钮文本
    ui.pushButton_2->setText(LocalQTCompat::fromLocal8Bit("升级中"));

    // 设置提示信息
    ui.tishi->setText(LocalQTCompat::fromLocal8Bit("PHY升级中，请勿断开电源..."));

    // 模拟升级进度（实际实现需连接到设备管理器）
    ui.progressBar_2->setValue(0);

    // 这里应该调用设备管理器的实际升级方法
    // 为了演示，使用一个计时器模拟升级进度
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this, timer]() {
        static int progress = 0;
        progress += 2;

        // 更新进度条
        ui.progressBar_2->setValue(progress);

        // 模拟升级完成
        if (progress >= 100) {
            timer->stop();
            timer->deleteLater();

            // 恢复状态
            m_isUpdating = false;
            ui.pushButton_2->setText(LocalQTCompat::fromLocal8Bit("开始"));
            ui.tishi->setText(LocalQTCompat::fromLocal8Bit("PHY升级完成"));

            // 更新UI状态
            updateUIState();

            // 通知升级完成
            emit updateCompleted(true, LocalQTCompat::fromLocal8Bit("PHY升级成功"));

            // 提示用户
            QMessageBox::information(this,
                LocalQTCompat::fromLocal8Bit("升级完成"),
                LocalQTCompat::fromLocal8Bit("PHY固件升级成功！"));

            LOG_INFO(LocalQTCompat::fromLocal8Bit("PHY升级完成"));
        }
        });

    // 启动计时器，每200毫秒更新一次（PHY升级通常慢一些）
    timer->start(200);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("PHY升级任务已启动"));
}

void UpdataDevice::onUpdateProgressChanged(int progress)
{
    // 更新当前的进度条
    if (ui.updata_ok->text() == LocalQTCompat::fromLocal8Bit("升级中")) {
        ui.progressBar->setValue(progress);
    }
    else if (ui.pushButton_2->text() == LocalQTCompat::fromLocal8Bit("升级中")) {
        ui.progressBar_2->setValue(progress);
    }
}

void UpdataDevice::onUpdateStatusChanged(bool success, const QString& message)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("升级状态变更: %1, %2").arg(success ? "成功" : "失败").arg(message));

    // 设置提示信息
    ui.tishi->setText(message);

    // 如果升级完成，恢复状态
    if (ui.updata_ok->text() == LocalQTCompat::fromLocal8Bit("升级中")) {
        ui.updata_ok->setText(LocalQTCompat::fromLocal8Bit("发送"));
    }
    else if (ui.pushButton_2->text() == LocalQTCompat::fromLocal8Bit("升级中")) {
        ui.pushButton_2->setText(LocalQTCompat::fromLocal8Bit("开始"));
    }

    // 重置升级状态
    m_isUpdating = false;
    updateUIState();

    // 显示升级结果
    if (success) {
        QMessageBox::information(this,
            LocalQTCompat::fromLocal8Bit("升级成功"),
            message);
    }
    else {
        QMessageBox::critical(this,
            LocalQTCompat::fromLocal8Bit("升级失败"),
            message);
    }

    // 发送升级完成信号
    emit updateCompleted(success, message);
}