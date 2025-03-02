#include "FileSaveController.h"
#include "Logger.h"

FileSaveController::FileSaveController(QObject* parent)
    : QObject(parent)
    , m_width(1920)
    , m_height(1080)
    , m_format(0x39)  // Default RAW10
    , m_fileSavePanel(nullptr)
    , m_saveFileBox(nullptr)
    , m_initialized(false)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器已创建"));
    connectSignals();
}

FileSaveController::~FileSaveController()
{
    // Stop any ongoing saving operations
    if (isSaving()) {
        stopSaving();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器已销毁"));
}

bool FileSaveController::initialize()
{
    try {
        // Connect to FileSaveManager signals
        connect(&FileSaveManager::instance(), &FileSaveManager::saveCompleted,
            this, &FileSaveController::onSaveManagerCompleted);
        connect(&FileSaveManager::instance(), &FileSaveManager::saveError,
            this, &FileSaveController::onSaveManagerError);
        connect(&FileSaveManager::instance(), &FileSaveManager::saveStatusChanged,
            this, &FileSaveController::saveStatusChanged);

        m_initialized = true;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存控制器初始化成功"));
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存控制器初始化失败: %1").arg(e.what()));
        return false;
    }
}

FileSavePanel* FileSaveController::createFileSavePanel(QWidget* parent)
{
    // Create FileSavePanel instance
    m_fileSavePanel = new FileSavePanel(parent);

    // Connect signals between the panel and this controller
    connect(m_fileSavePanel, &FileSavePanel::saveStartRequested, this, &FileSaveController::startSaving);
    connect(m_fileSavePanel, &FileSavePanel::saveStopRequested, this, &FileSaveController::stopSaving);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建文件保存面板"));
    return m_fileSavePanel;
}

SaveFileBox* FileSaveController::createSaveFileBox(QWidget* parent)
{
    // Create SaveFileBox instance
    m_saveFileBox = new SaveFileBox(parent);

    // Set image parameters
    m_saveFileBox->setImageParameters(m_width, m_height, m_format);

    // Connect signals
    connect(m_saveFileBox, &SaveFileBox::saveCompleted, this, &FileSaveController::saveCompleted);
    connect(m_saveFileBox, &SaveFileBox::saveError, this, &FileSaveController::saveError);

    LOG_INFO(LocalQTCompat::fromLocal8Bit("已创建文件保存对话框"));
    return m_saveFileBox;
}

void FileSaveController::setImageParameters(uint16_t width, uint16_t height, uint8_t format)
{
    m_width = width;
    m_height = height;
    m_format = format;

    LOG_INFO(LocalQTCompat::fromLocal8Bit("设置图像参数：宽度=%1，高度=%2，格式=0x%3")
        .arg(width).arg(height).arg(format, 2, 16, QChar('0')));

    // Update SaveFileBox if it exists
    if (m_saveFileBox) {
        m_saveFileBox->setImageParameters(width, height, format);
    }

    // Update save parameters in manager
    SaveParameters params = FileSaveManager::instance().getSaveParameters();
    params.options["width"] = width;
    params.options["height"] = height;
    params.options["format"] = format;
    FileSaveManager::instance().setSaveParameters(params);
}

bool FileSaveController::isSaving() const
{
    // Check if FileSavePanel is saving
    if (m_fileSavePanel && m_fileSavePanel->isSaving()) {
        return true;
    }

    // Check if SaveFileBox is saving
    if (m_saveFileBox && m_saveFileBox->isSaving()) {
        return true;
    }

    return false;
}

void FileSaveController::startSaving()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("开始文件保存"));

    // Validate parameters
    uint16_t width = m_width;
    uint16_t height = m_height;
    uint8_t capType = m_format;

    if (!validateImageParameters(width, height, capType)) {
        emit saveError(LocalQTCompat::fromLocal8Bit("图像参数无效"));
        return;
    }

    // Update parameters in FileSaveManager
    SaveParameters params = FileSaveManager::instance().getSaveParameters();
    params.options["width"] = width;
    params.options["height"] = height;
    params.options["format"] = capType;
    FileSaveManager::instance().setSaveParameters(params);

    // Start saving using appropriate UI component
    if (m_fileSavePanel) {
        m_fileSavePanel->startSaving();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存已开始"));
}

void FileSaveController::stopSaving()
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("停止文件保存"));

    // Stop saving using appropriate UI component
    if (m_fileSavePanel) {
        m_fileSavePanel->stopSaving();
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存已停止"));
}

void FileSaveController::processDataPacket(const DataPacket& packet)
{
    // Only process if we're actively saving
    if (isSaving()) {
        FileSaveManager::instance().processDataPacket(packet);
    }
}

void FileSaveController::connectSignals()
{
    // Most signal connections are set up when creating UI components
    // or in the initialize method
}

bool FileSaveController::validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType)
{
    // Check width
    if (width == 0 || width > 4096) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像宽度"));
        return false;
    }

    // Check height
    if (height == 0 || height > 4096) {
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("无效的图像高度"));
        return false;
    }

    // Check capture type (format)
    if (capType != 0x38 && capType != 0x39 && capType != 0x3A) {
        // Default to RAW10 if invalid
        LOG_WARN(LocalQTCompat::fromLocal8Bit("无效的图像格式，使用默认值 RAW10"));
        capType = 0x39;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("图像参数验证通过 - 宽度: %1, 高度: %2, 类型: 0x%3")
        .arg(width).arg(height).arg(capType, 2, 16, QChar('0')));

    return true;
}

void FileSaveController::onSaveManagerCompleted(const QString& path, uint64_t totalBytes)
{
    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件保存完成：路径=%1，总大小=%2字节")
        .arg(path).arg(totalBytes));

    // Forward the completed signal
    emit saveCompleted(path, totalBytes);
}

void FileSaveController::onSaveManagerError(const QString& error)
{
    LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件保存错误：%1").arg(error));

    // Forward the error signal
    emit saveError(error);
}