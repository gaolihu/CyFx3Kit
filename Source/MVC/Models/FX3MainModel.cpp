// Source/MVC/Models/FX3MainModel.cpp

#include "FX3MainModel.h"
#include "Logger.h"
#include <QSettings>

FX3MainModel* FX3MainModel::getInstance()
{
    static FX3MainModel s_instance;
    return &s_instance;
}

FX3MainModel::FX3MainModel()
    : QObject(nullptr)
    , m_deviceConnected(false)
    , m_transferring(false)
    , m_closing(false)
    , m_videoWidth(1920)
    , m_videoHeight(1080)
    , m_videoFormat(0x39)  // 默认为RAW10
    , m_bytesTransferred(0)
    , m_transferRate(0.0)
    , m_errorCount(0)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主模型构建入口"));
    // 初始化模型
    initialize();
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主模型构建完成"));
}

FX3MainModel::~FX3MainModel()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主模型销毁中"));
}

void FX3MainModel::initialize()
{
    try {
        // 尝试从设置加载状态
        QSettings settings("FX3Tool", "MainSettings");

        // 加载视频配置
        {
            std::lock_guard<std::mutex> lock(m_videoConfigMutex);
            m_videoWidth = settings.value("videoWidth", 1920).toUInt();
            m_videoHeight = settings.value("videoHeight", 1080).toUInt();
            m_videoFormat = settings.value("videoFormat", 0x39).toUInt();
        }

        // 加载命令目录
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            m_commandDir = settings.value("commandDir", "").toString();
            LOG_INFO("命令目录：" + m_commandDir);
        }

        // 连接信号
        connectSignals();

        LOG_INFO(LocalQTCompat::fromLocal8Bit("FX3主模型初始化完成"));
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("FX3主模型初始化异常: %1").arg(e.what()));
    }
}

void FX3MainModel::connectSignals()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("连接主模型信号"));
    // 连接状态机信号
    connect(&AppStateMachine::instance(), &AppStateMachine::stateChanged,
        this, [this](AppState newState, AppState oldState, const QString& reason) {
            emit signal_FX3Main_M_appStateChanged(newState, oldState, reason);

            // 更新模型状态
            switch (newState) {
            case AppState::DEVICE_ABSENT:
            case AppState::DEVICE_ERROR:
                break;
            case AppState::IDLE:
            case AppState::CONFIGURED:
                setTransferring(false);
                break;
            case AppState::TRANSFERRING:
                setTransferring(true);
                break;
            case AppState::SHUTDOWN:
                setClosing(true);
                break;
            default:
                break;
            }
        });
}

AppState FX3MainModel::getAppState() const
{
    return AppStateMachine::instance().currentState();
}

bool FX3MainModel::isDeviceConnected() const
{
    return m_deviceConnected;
}

bool FX3MainModel::isTransferring() const
{
    return m_transferring;
}

bool FX3MainModel::isClosing() const
{
    return m_closing;
}

void FX3MainModel::setTransferring(bool transferring)
{
    if (m_transferring != transferring) {
        m_transferring = transferring;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("数据传输状态变更: %1").arg(transferring ? "传输中" : "已停止"));
        emit signal_FX3Main_M_transferStateChanged(transferring);
    }
}

void FX3MainModel::setClosing(bool closing)
{
    if (m_closing != closing) {
        m_closing = closing;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("应用程序关闭状态变更: %1").arg(closing ? "正在关闭" : "正常运行"));
        emit signal_FX3Main_M_closingStateChanged(closing);
    }
}

void FX3MainModel::getVideoConfig(uint16_t& width, uint16_t& height, uint8_t& format) const
{
    std::lock_guard<std::mutex> lock(m_videoConfigMutex);

    width = m_videoWidth;
    height = m_videoHeight;
    format = m_videoFormat;
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取视频配置, 宽: %1, 高: %2, 格式: %3")
        .arg(width).arg(height).arg(format));
}

void FX3MainModel::setVideoConfig(uint16_t width, uint16_t height, uint8_t format)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_videoConfigMutex);
        if (m_videoWidth != width || m_videoHeight != height || m_videoFormat != format) {
            m_videoWidth = width;
            m_videoHeight = height;
            m_videoFormat = format;
            changed = true;
        }
    }

    if (changed) {
        // 保存到设置
        QSettings settings("FX3Tool", "MainSettings");
        settings.setValue("videoWidth", width);
        settings.setValue("videoHeight", height);
        settings.setValue("videoFormat", format);

        // 发送信号
        emit signal_FX3Main_M_videoConfigChanged(width, height, format);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("视频配置已更新 - 宽度: %1, 高度: %2, 格式: 0x%3")
            .arg(width).arg(height).arg(format, 2, 16, QChar('0')));
    }
}

void FX3MainModel::getDeviceInfo(QString& deviceName, QString& firmwareVersion, QString& serialNumber) const
{
    std::lock_guard<std::mutex> lock(m_deviceInfoMutex);
    deviceName = m_deviceName;
    firmwareVersion = m_firmwareVersion;
    serialNumber = m_serialNumber;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取设备信息, 名称: %1, 固件版本: %2, SN: %3")
        .arg(deviceName).arg(firmwareVersion).arg(serialNumber));
}

void FX3MainModel::setDeviceInfo(const QString& deviceName, const QString& firmwareVersion, const QString& serialNumber)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_deviceInfoMutex);
        if (m_deviceName != deviceName || m_firmwareVersion != firmwareVersion || m_serialNumber != serialNumber) {
            m_deviceName = deviceName;
            m_firmwareVersion = firmwareVersion;
            m_serialNumber = serialNumber;
            changed = true;
        }
    }

    if (changed) {
        emit signal_FX3Main_M_deviceInfoChanged(deviceName, firmwareVersion, serialNumber);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("设备信息已更新 - 名称: %1, 固件版本: %2, 序列号: %3")
            .arg(deviceName).arg(firmwareVersion).arg(serialNumber));
    }
}

void FX3MainModel::updateTransferStats(uint64_t bytesTransferred, double transferRate, uint32_t elapseMs)
{
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_bytesTransferred = bytesTransferred;
        m_transferRate = transferRate;
        m_elapseMs = elapseMs;
    }

    emit signal_FX3Main_M_transferStatsUpdated(bytesTransferred, transferRate, elapseMs);
}

void FX3MainModel::getTransferStats(uint64_t& bytesTransferred, double& transferRate, uint32_t& errorCount) const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    bytesTransferred = m_bytesTransferred;
    transferRate = m_transferRate;
    errorCount = m_errorCount;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取传输状态, 已传输: %1, 速率: %2, 错误: %3")
        .arg(bytesTransferred).arg(transferRate).arg(errorCount));
}

void FX3MainModel::resetTransferStats()
{
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_bytesTransferred = 0;
        m_transferRate = 0.0;
        m_errorCount = 0;
        m_elapseMs = 0;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("重置传输统计信息"));
    emit signal_FX3Main_M_transferStatsUpdated(0, 0.0, 0);
}

QString FX3MainModel::getCommandDirectory() const
{
    std::lock_guard<std::mutex> lock(m_configMutex);
    LOG_INFO(LocalQTCompat::fromLocal8Bit("获取命令目录: %1").arg(m_commandDir));
    return m_commandDir;
}

void FX3MainModel::setCommandDirectory(const QString& dir)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        if (m_commandDir != dir) {
            m_commandDir = dir;
            changed = true;
        }
    }

    if (changed) {
        // 保存到设置
        QSettings settings("FX3Tool", "MainSettings");
        settings.setValue("commandDir", dir);

        // 发送信号
        emit signal_FX3Main_M_commandDirectoryChanged(dir);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("命令文件目录已更新: %1").arg(dir));
    }
}