// Source/MVC/Models/VideoDisplayModel.cpp
#include "VideoDisplayModel.h"
#include "Logger.h"
#include <QSettings>

VideoDisplayModel* VideoDisplayModel::getInstance()
{
    static VideoDisplayModel s;
    return &s;
}

VideoDisplayModel::VideoDisplayModel()
    : QObject(nullptr)
    , m_config(createDefaultConfig())
{
    // 创建默认的空白图像
    m_renderImage = QImage(m_config.width, m_config.height, QImage::Format_RGB888);
    m_renderImage.fill(Qt::black);

    LOG_INFO("视频配置模型已创建");
}

VideoDisplayModel::~VideoDisplayModel()
{
    LOG_INFO("视频配置模型已销毁");
}

VideoConfig VideoDisplayModel::getConfig() const
{
    return m_config;
}

void VideoDisplayModel::setConfig(const VideoConfig& config)
{
    // 检查是否需要更新渲染图像尺寸
    if (m_config.width != config.width || m_config.height != config.height) {
        m_renderImage = QImage(config.width, config.height, QImage::Format_RGB888);
        m_renderImage.fill(Qt::black);
        emit renderImageChanged(m_renderImage);
    }

    // 更新配置
    m_config = config;
    emit configChanged(m_config);

    LOG_INFO("视频配置已更新");
}

QByteArray VideoDisplayModel::getFrameData() const
{
    return m_frameData;
}

void VideoDisplayModel::setFrameData(const QByteArray& data)
{
    m_frameData = data;
    emit frameDataChanged(m_frameData);
}

QImage VideoDisplayModel::getRenderImage() const
{
    return m_renderImage;
}

void VideoDisplayModel::setRenderImage(const QImage& image)
{
    m_renderImage = image;
    emit renderImageChanged(m_renderImage);
}

bool VideoDisplayModel::saveConfig()
{
    try {
        QSettings settings("FX3Tool", "VideoConfig");

        // 保存视频参数
        settings.setValue("width", m_config.width);
        settings.setValue("height", m_config.height);
        settings.setValue("format", m_config.format);
        settings.setValue("colorMode", m_config.colorMode);
        settings.setValue("dataMode", m_config.dataMode);
        settings.setValue("colorArrangement", m_config.colorArrangement);
        settings.setValue("virtualChannel", m_config.virtualChannel);

        LOG_INFO("视频配置已保存到存储");
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("保存视频配置失败: %1").arg(e.what()));
        return false;
    }
}

bool VideoDisplayModel::loadConfig()
{
    try {
        QSettings settings("FX3Tool", "VideoConfig");

        // 读取视频参数
        m_config.width = settings.value("width", 1920).toUInt();
        m_config.height = settings.value("height", 1080).toUInt();
        m_config.format = settings.value("format", 0x39).toUInt();
        m_config.colorMode = settings.value("colorMode", 1).toInt();
        m_config.dataMode = settings.value("dataMode", 0).toInt();
        m_config.colorArrangement = settings.value("colorArrangement", 0).toInt();
        m_config.virtualChannel = settings.value("virtualChannel", 0).toInt();

        // 重置运行状态
        m_config.isRunning = false;

        // 更新渲染图像尺寸
        m_renderImage = QImage(m_config.width, m_config.height, QImage::Format_RGB888);
        m_renderImage.fill(Qt::black);

        LOG_INFO("视频配置已从存储加载");
        emit configChanged(m_config);
        emit renderImageChanged(m_renderImage);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(QString("加载视频配置失败: %1").arg(e.what()));
        // 加载失败时使用默认配置
        resetToDefault();
        return false;
    }
}

void VideoDisplayModel::resetToDefault()
{
    m_config = createDefaultConfig();

    // 重置渲染图像
    m_renderImage = QImage(m_config.width, m_config.height, QImage::Format_RGB888);
    m_renderImage.fill(Qt::black);

    emit configChanged(m_config);
    emit renderImageChanged(m_renderImage);

    LOG_INFO("视频配置已重置为默认值");
}

VideoConfig VideoDisplayModel::createDefaultConfig()
{
    VideoConfig config;

    // 设置默认值
    config.width = 1920;
    config.height = 1080;
    config.format = 0x39;  // RAW10
    config.colorMode = 1;  // 30Bit RGB
    config.dataMode = 0;
    config.colorArrangement = 0;  // R-G-B
    config.virtualChannel = 0;
    config.fps = 0.0;
    config.isRunning = false;

    return config;
}