// Source/MVC/Models/ChannelSelectModel.cpp

#include "ChannelSelectModel.h"
#include "Logger.h"
#include <QSettings>

ChannelSelectModel* ChannelSelectModel::getInstance()
{
    static ChannelSelectModel s;
    return &s;
}

ChannelSelectModel::ChannelSelectModel()
    : QObject(nullptr)
    , m_config(createDefaultConfig())
{
    LOG_INFO("通道配置模型已创建");
}

ChannelSelectModel::~ChannelSelectModel()
{
    LOG_INFO("通道配置模型已销毁");
}

ChannelConfig ChannelSelectModel::getConfig() const
{
    return m_config;
}

void ChannelSelectModel::setConfig(const ChannelConfig& config)
{
    m_config = config;
    emit configChanged(m_config);
    LOG_INFO("通道配置已更新");
}

bool ChannelSelectModel::saveConfig()
{
    try {
        QSettings settings("FX3Tool", "ChannelConfig");

        // 保存基本配置
        settings.setValue("captureType", m_config.captureType);
        settings.setValue("clockPNSwap", m_config.clockPNSwap);
        settings.setValue("testModeEnabled", m_config.testModeEnabled);
        settings.setValue("testModeType", m_config.testModeType);
        settings.setValue("videoWidth", m_config.videoWidth);
        settings.setValue("videoHeight", m_config.videoHeight);
        settings.setValue("teValue", m_config.teValue);

        // 保存通道配置
        settings.beginGroup("Channels");
        for (int i = 0; i < 4; i++) {
            settings.setValue(QString("channel%1Enabled").arg(i), m_config.channelEnabled[i]);
            settings.setValue(QString("channel%1PNSwap").arg(i), m_config.channelPNSwap[i]);
            settings.setValue(QString("channel%1Swap").arg(i), m_config.channelSwap[i]);
        }
        settings.endGroup();

        LOG_INFO("通道配置已保存到存储");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存通道配置失败: %1").arg(e.what()));
        return false;
    }
}

bool ChannelSelectModel::loadConfig()
{
    try {
        QSettings settings("FX3Tool", "ChannelConfig");

        // 读取基本配置，如果不存在则使用默认值
        m_config.captureType = settings.value("captureType", 0).toInt();
        m_config.clockPNSwap = settings.value("clockPNSwap", false).toBool();
        m_config.testModeEnabled = settings.value("testModeEnabled", false).toBool();
        m_config.testModeType = settings.value("testModeType", 0).toInt();
        m_config.videoWidth = settings.value("videoWidth", 1920).toInt();
        m_config.videoHeight = settings.value("videoHeight", 1080).toInt();
        m_config.teValue = settings.value("teValue", 60.0).toDouble();

        // 读取通道配置
        settings.beginGroup("Channels");
        for (int i = 0; i < 4; i++) {
            // 第一个通道始终启用
            if (i == 0) {
                m_config.channelEnabled[i] = true;
            }
            else {
                m_config.channelEnabled[i] = settings.value(
                    QString("channel%1Enabled").arg(i), true).toBool();
            }

            m_config.channelPNSwap[i] = settings.value(
                QString("channel%1PNSwap").arg(i), false).toBool();

            m_config.channelSwap[i] = settings.value(
                QString("channel%1Swap").arg(i), i).toInt();
        }
        settings.endGroup();

        LOG_INFO("通道配置已从存储加载");
        emit configChanged(m_config);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载通道配置失败: %1").arg(e.what()));
        // 加载失败时使用默认配置
        resetToDefault();
        return false;
    }
}

void ChannelSelectModel::resetToDefault()
{
    m_config = createDefaultConfig();
    emit configChanged(m_config);
    LOG_INFO("通道配置已重置为默认值");
}

ChannelConfig ChannelSelectModel::createDefaultConfig()
{
    ChannelConfig config;

    // 设置默认值
    config.captureType = 0;  // VIDEO
    config.clockPNSwap = false;

    // 默认所有通道都启用
    config.channelEnabled = { true, true, true, true };

    // 默认不交换PN
    config.channelPNSwap = { false, false, false, false };

    // 默认通道映射按顺序
    config.channelSwap = { 0, 1, 2, 3 };

    // 默认测试模式关闭
    config.testModeEnabled = false;
    config.testModeType = 0;

    // 默认视频参数
    config.videoWidth = 1920;
    config.videoHeight = 1080;
    config.teValue = 60.0;

    return config;
}