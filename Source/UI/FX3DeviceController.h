#pragma once

#include <QObject>
#include <QMessageBox>
#include <QLineEdit>
#include <QComboBox>
#include <cstdint>

// Forward declarations
class FX3ToolMainWin;
class FX3DeviceManager;

/**
 * @brief 设备控制器类，负责处理设备相关操作
 *
 * 此类封装了与设备交互的所有操作，包括启动/停止传输、重置设备等。
 * 通过委托模式从主窗口分离设备控制逻辑，提高代码可维护性。
 */
class FX3DeviceController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param mainWindow 主窗口指针
     * @param deviceManager 设备管理器指针
     */
    FX3DeviceController(FX3ToolMainWin* mainWindow, FX3DeviceManager* deviceManager);

    /**
     * @brief 开始数据传输
     *
     * 验证参数后调用设备管理器启动传输
     */
    void startTransfer();

    /**
     * @brief 停止数据传输
     */
    void stopTransfer();

    /**
     * @brief 重置设备
     */
    void resetDevice();

    /**
     * @brief 验证图像参数
     *
     * 从UI获取图像宽度、高度和格式，并进行验证
     *
     * @param width 输出参数，存储验证通过的宽度
     * @param height 输出参数，存储验证通过的高度
     * @param capType 输出参数，存储验证通过的格式
     * @return 验证是否通过
     */
    bool validateImageParameters(uint16_t& width, uint16_t& height, uint8_t& capType);

private:
    FX3ToolMainWin* m_mainWindow;      ///< 主窗口指针，用于访问UI元素
    FX3DeviceManager* m_deviceManager; ///< 设备管理器指针，用于执行设备操作
};