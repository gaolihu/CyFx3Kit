// Source/MVC/Views/UpdateDeviceView.cpp
#include "UpdateDeviceView.h"
#include "Logger.h"

UpdateDeviceView::UpdateDeviceView(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    initializeUI();
    connectSignals();

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级视图已创建"));
}

UpdateDeviceView::~UpdateDeviceView()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级视图已销毁"));
}

void UpdateDeviceView::initializeUI()
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

    // 设置按钮初始文本
    ui.updata_ok->setText(LocalQTCompat::fromLocal8Bit("发送"));
    ui.pushButton_2->setText(LocalQTCompat::fromLocal8Bit("开始"));

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级视图UI已初始化"));
}

void UpdateDeviceView::connectSignals()
{
    // 连接文件选择按钮信号
    connect(ui.updataopen, &QPushButton::clicked, this, &UpdateDeviceView::slot_UD_V_onSOCFileOpenButtonClicked);
    connect(ui.pushButton, &QPushButton::clicked, this, &UpdateDeviceView::slot_UD_V_onPHYFileOpenButtonClicked);

    // 连接升级按钮信号
    connect(ui.updata_ok, &QPushButton::clicked, this, &UpdateDeviceView::slot_UD_V_onSOCUpdateButtonClicked);
    connect(ui.pushButton_2, &QPushButton::clicked, this, &UpdateDeviceView::slot_UD_V_onPHYUpdateButtonClicked);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级视图信号已连接"));
}

QString UpdateDeviceView::showFileSelectDialog(DeviceType deviceType)
{
    QString title, filter, initialPath;

    if (deviceType == DeviceType::SOC) {
        title = LocalQTCompat::fromLocal8Bit("选择SOC文件");
        filter = LocalQTCompat::fromLocal8Bit("SOC文件 (*.soc)");
        initialPath = ui.lineEdit->text().isEmpty() ? QDir::homePath() : ui.lineEdit->text();
    }
    else {
        title = LocalQTCompat::fromLocal8Bit("选择ISO文件");
        filter = LocalQTCompat::fromLocal8Bit("ISO文件 (*.iso)");
        initialPath = ui.lineEdit_2->text().isEmpty() ? QDir::homePath() : ui.lineEdit_2->text();
    }

    return QFileDialog::getOpenFileName(this, title, initialPath, filter);
}

bool UpdateDeviceView::showConfirmDialog(const QString& message)
{
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        LocalQTCompat::fromLocal8Bit("确认升级"),
        message,
        QMessageBox::Yes | QMessageBox::No);

    return reply == QMessageBox::Yes;
}

void UpdateDeviceView::showMessageDialog(const QString& title, const QString& message, bool isError)
{
    if (isError) {
        QMessageBox::critical(this, title, message);
    }
    else {
        QMessageBox::information(this, title, message);
    }
}

void UpdateDeviceView::slot_UD_V_updateSOCFilePath(const QString& filePath)
{
    ui.lineEdit->setText(filePath);

    // 有效路径则启用SOC升级按钮
    ui.updata_ok->setEnabled(!filePath.isEmpty());

    LOG_INFO(QString("SOC文件路径已更新到UI: %1").arg(filePath));
}

void UpdateDeviceView::slot_UD_V_updatePHYFilePath(const QString& filePath)
{
    ui.lineEdit_2->setText(filePath);

    // 有效路径则启用PHY升级按钮
    ui.pushButton_2->setEnabled(!filePath.isEmpty());

    LOG_INFO(QString("PHY文件路径已更新到UI: %1").arg(filePath));
}

void UpdateDeviceView::slot_UD_V_updateSOCProgress(int progress)
{
    ui.progressBar->setValue(progress);
}

void UpdateDeviceView::slot_UD_V_updatePHYProgress(int progress)
{
    ui.progressBar_2->setValue(progress);
}

void UpdateDeviceView::slot_UD_V_updateStatusMessage(const QString& message)
{
    ui.tishi->setText(message);
}

void UpdateDeviceView::slot_UD_V_updateSOCButtonState(bool isUpdating)
{
    if (isUpdating) {
        ui.updata_ok->setText(LocalQTCompat::fromLocal8Bit("升级中"));
    }
    else {
        ui.updata_ok->setText(LocalQTCompat::fromLocal8Bit("发送"));
    }

    ui.updata_ok->setEnabled(!isUpdating && !ui.lineEdit->text().isEmpty());
}

void UpdateDeviceView::slot_UD_V_updatePHYButtonState(bool isUpdating)
{
    if (isUpdating) {
        ui.pushButton_2->setText(LocalQTCompat::fromLocal8Bit("升级中"));
    }
    else {
        ui.pushButton_2->setText(LocalQTCompat::fromLocal8Bit("开始"));
    }

    ui.pushButton_2->setEnabled(!isUpdating && !ui.lineEdit_2->text().isEmpty());
}

void UpdateDeviceView::updateUIState(bool isUpdating, DeviceType currentDevice)
{
    // 设置文件选择按钮状态
    ui.updataopen->setEnabled(!isUpdating);
    ui.pushButton->setEnabled(!isUpdating);

    // 根据当前升级设备类型更新按钮状态
    if (isUpdating) {
        if (currentDevice == DeviceType::SOC) {
            slot_UD_V_updateSOCButtonState(true);
            slot_UD_V_updatePHYButtonState(false);
            ui.pushButton_2->setEnabled(false);
        }
        else {
            slot_UD_V_updatePHYButtonState(true);
            slot_UD_V_updateSOCButtonState(false);
            ui.updata_ok->setEnabled(false);
        }
    }
    else {
        // 不在升级状态，根据是否有文件路径启用按钮
        slot_UD_V_updateSOCButtonState(false);
        slot_UD_V_updatePHYButtonState(false);
    }

    LOG_INFO(QString("UI状态已更新: 升级中=%1, 设备类型=%2")
        .arg(isUpdating ? "是" : "否")
        .arg(currentDevice == DeviceType::SOC ? "SOC" : "PHY"));
}

void UpdateDeviceView::slot_UD_V_onSOCFileOpenButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("SOC文件选择按钮被点击"));
    emit signal_UD_V_socFileSelectClicked();
}

void UpdateDeviceView::slot_UD_V_onPHYFileOpenButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("PHY文件选择按钮被点击"));
    emit signal_UD_V_phyFileSelectClicked();
}

void UpdateDeviceView::slot_UD_V_onSOCUpdateButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("SOC升级按钮被点击"));
    emit signal_UD_V_socUpdateClicked();
}

void UpdateDeviceView::slot_UD_V_onPHYUpdateButtonClicked()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("PHY升级按钮被点击"));
    emit signal_UD_V_phyUpdateClicked();
}