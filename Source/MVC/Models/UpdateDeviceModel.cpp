// Source/MVC/Models/UpdateDeviceModel.cpp
#include "UpdateDeviceModel.h"
#include "Logger.h"

UpdateDeviceModel* UpdateDeviceModel::getInstance()
{
    static UpdateDeviceModel s;
    return &s;
}

UpdateDeviceModel::UpdateDeviceModel(QObject* parent)
    : QObject(parent)
    , m_status(UpdateStatus::IDLE)
    , m_currentDevice(DeviceType::SOC)
    , m_progress(0)
    , m_updateTimer(nullptr)
{
    LOG_INFO("设备升级模型已创建");
}

UpdateDeviceModel::~UpdateDeviceModel()
{
    // 确保定时器被清理
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }

    LOG_INFO("设备升级模型已销毁");
}

void UpdateDeviceModel::setSOCFilePath(const QString& filePath)
{
    if (m_socFilePath != filePath) {
        m_socFilePath = filePath;
        emit signal_UD_M_filePathChanged(DeviceType::SOC, filePath);
        LOG_INFO(QString("SOC文件路径已设置: %1").arg(filePath));
    }
}

QString UpdateDeviceModel::getSOCFilePath() const
{
    return m_socFilePath;
}

void UpdateDeviceModel::setPHYFilePath(const QString& filePath)
{
    if (m_phyFilePath != filePath) {
        m_phyFilePath = filePath;
        emit signal_UD_M_filePathChanged(DeviceType::PHY, filePath);
        LOG_INFO(QString("PHY文件路径已设置: %1").arg(filePath));
    }
}

QString UpdateDeviceModel::getPHYFilePath() const
{
    return m_phyFilePath;
}

UpdateStatus UpdateDeviceModel::getStatus() const
{
    return m_status;
}

void UpdateDeviceModel::setStatus(UpdateStatus status)
{
    if (m_status != status) {
        m_status = status;
        emit signal_UD_M_statusChanged(status);
        LOG_INFO(QString("升级状态已更改为: %1").arg(static_cast<int>(status)));
    }
}

DeviceType UpdateDeviceModel::getCurrentDeviceType() const
{
    return m_currentDevice;
}

void UpdateDeviceModel::setCurrentDeviceType(DeviceType type)
{
    if (m_currentDevice != type) {
        m_currentDevice = type;
        LOG_INFO(QString("当前升级设备类型已设置为: %1").arg(static_cast<int>(type)));
    }
}

int UpdateDeviceModel::getProgress() const
{
    return m_progress;
}

void UpdateDeviceModel::setProgress(int progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        emit signal_UD_M_progressChanged(progress);

        // 如果进度到达100%，表示升级完成
        if (progress >= 100 && m_status == UpdateStatus::UPDATING) {
            setStatus(UpdateStatus::COMPLETED);

            // 根据设备类型生成完成消息
            QString message;
            if (m_currentDevice == DeviceType::SOC) {
                message = LocalQTCompat::fromLocal8Bit("SOC升级成功");
            }
            else {
                message = LocalQTCompat::fromLocal8Bit("PHY升级成功");
            }

            setStatusMessage(message);
            emit signal_UD_M_updateCompleted(true, message);
        }
    }
}

QString UpdateDeviceModel::getStatusMessage() const
{
    return m_statusMessage;
}

void UpdateDeviceModel::setStatusMessage(const QString& message)
{
    if (m_statusMessage != message) {
        m_statusMessage = message;
        LOG_INFO(QString("升级状态消息已设置: %1").arg(message));
    }
}

bool UpdateDeviceModel::validateFile(const QString& filePath, const QString& fileType, QString& errorMessage)
{
    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        errorMessage = LocalQTCompat::fromLocal8Bit("文件不存在: ") + filePath;
        LOG_ERROR(errorMessage);
        return false;
    }

    // 检查文件大小
    qint64 fileSize = fileInfo.size();
    if (fileSize <= 0) {
        errorMessage = LocalQTCompat::fromLocal8Bit("文件大小为0: ") + filePath;
        LOG_ERROR(errorMessage);
        return false;
    }

    // 检查文件类型
    QString suffix = fileInfo.suffix().toLower();
    if (fileType == "SOC" && suffix != "soc") {
        errorMessage = LocalQTCompat::fromLocal8Bit("请选择.soc格式的文件");
        LOG_ERROR(errorMessage);
        return false;
    }
    else if (fileType == "ISO" && suffix != "iso") {
        errorMessage = LocalQTCompat::fromLocal8Bit("请选择.iso格式的文件");
        LOG_ERROR(errorMessage);
        return false;
    }

    LOG_INFO(QString("文件验证通过: %1").arg(filePath));
    return true;
}

bool UpdateDeviceModel::startUpdate(DeviceType deviceType)
{
    // 检查当前是否正在升级
    if (m_status == UpdateStatus::UPDATING) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("无法启动新的升级任务，当前正在进行升级"));
        return false;
    }

    // 检查文件路径
    QString filePath = (deviceType == DeviceType::SOC) ? m_socFilePath : m_phyFilePath;
    QString fileType = (deviceType == DeviceType::SOC) ? "SOC" : "ISO";
    QString errorMessage;

    if (!validateFile(filePath, fileType, errorMessage)) {
        LOG_ERROR(QString("文件验证失败: %1").arg(errorMessage));
        return false;
    }

    // 设置当前升级设备类型
    setCurrentDeviceType(deviceType);

    // 重置进度
    setProgress(0);

    // 设置状态消息
    QString statusMessage;
    if (deviceType == DeviceType::SOC) {
        statusMessage = LocalQTCompat::fromLocal8Bit("SOC升级中，请勿断开电源...");
    }
    else {
        statusMessage = LocalQTCompat::fromLocal8Bit("PHY升级中，请勿断开电源...");
    }
    setStatusMessage(statusMessage);

    // 设置状态为升级中
    setStatus(UpdateStatus::UPDATING);

    // 开始升级过程
    LOG_INFO(QString("%1升级过程开始").arg(deviceType == DeviceType::SOC ? "SOC" : "PHY"));

    // 实际项目中，应该调用真实的设备升级API
    // 这里使用模拟升级过程做演示
    simulateUpdate(deviceType);

    return true;
}

bool UpdateDeviceModel::stopUpdate()
{
    // 检查当前是否正在升级
    if (m_status != UpdateStatus::UPDATING) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("没有正在进行的升级任务"));
        return false;
    }

    // 停止定时器
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }

    // 设置状态为失败
    setStatus(UpdateStatus::FAILED);

    // 设置状态消息
    QString message = LocalQTCompat::fromLocal8Bit("升级过程被用户中断");
    setStatusMessage(message);

    // 发送升级完成信号（失败）
    emit signal_UD_M_updateCompleted(false, message);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("升级过程已停止"));
    return true;
}

void UpdateDeviceModel::reset()
{
    // 停止任何正在进行的升级
    if (m_status == UpdateStatus::UPDATING) {
        stopUpdate();
    }

    // 重置状态
    m_progress = 0;
    m_status = UpdateStatus::IDLE;
    m_statusMessage.clear();

    // 不清除文件路径，以便用户可以重新启动升级

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设备升级模型已重置"));
}

void UpdateDeviceModel::simulateUpdate(DeviceType deviceType)
{
    // 创建定时器模拟升级过程
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
    }

    m_updateTimer = new QTimer(this);

    // 根据设备类型设置不同的升级速度
    int interval = (deviceType == DeviceType::SOC) ? 100 : 200; // SOC升级快些，PHY升级慢些
    int increment = (deviceType == DeviceType::SOC) ? 5 : 2;    // SOC每次增量大些，PHY每次增量小些

    connect(m_updateTimer, &QTimer::timeout, this, [this, increment]() {
        // 增加进度
        int newProgress = m_progress + increment;

        // 确保不超过100%
        if (newProgress > 100) {
            newProgress = 100;
        }

        // 更新进度
        setProgress(newProgress);

        // 如果达到100%，停止定时器
        if (newProgress >= 100) {
            m_updateTimer->stop();
            delete m_updateTimer;
            m_updateTimer = nullptr;
        }
        });

    // 启动定时器
    m_updateTimer->start(interval);
    LOG_INFO(QString("模拟%1升级过程已启动").arg(deviceType == DeviceType::SOC ? "SOC" : "PHY"));
}