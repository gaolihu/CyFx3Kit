// Source/MVC/Models/UpdateDeviceModel.h
#pragma once

#include <QObject>
#include <QString>
#include <QFileInfo>
#include <QTimer>

/**
 * @brief 升级设备类型枚举
 */
enum class DeviceType {
    SOC,    // 主控SOC
    PHY     // 物理层PHY
};

/**
 * @brief 升级状态枚举
 */
enum class UpdateStatus {
    IDLE,       // 空闲
    UPDATING,   // 升级中
    COMPLETED,  // 已完成
    FAILED      // 失败
};

/**
 * @brief 设备升级模型类
 *
 * 负责管理设备升级的数据和状态
 */
class UpdateDeviceModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的引用
     */
    static UpdateDeviceModel* getInstance();

    /**
     * @brief 设置SOC文件路径
     * @param filePath 文件路径
     */
    void setSOCFilePath(const QString& filePath);

    /**
     * @brief 获取SOC文件路径
     * @return SOC文件路径
     */
    QString getSOCFilePath() const;

    /**
     * @brief 设置PHY文件路径
     * @param filePath 文件路径
     */
    void setPHYFilePath(const QString& filePath);

    /**
     * @brief 获取PHY文件路径
     * @return PHY文件路径
     */
    QString getPHYFilePath() const;

    /**
     * @brief 获取当前升级状态
     * @return 升级状态
     */
    UpdateStatus getStatus() const;

    /**
     * @brief 设置当前升级状态
     * @param status 新的升级状态
     */
    void setStatus(UpdateStatus status);

    /**
     * @brief 获取当前升级类型
     * @return 设备类型
     */
    DeviceType getCurrentDeviceType() const;

    /**
     * @brief 设置当前升级类型
     * @param type 设备类型
     */
    void setCurrentDeviceType(DeviceType type);

    /**
     * @brief 获取当前升级进度
     * @return 升级进度(0-100)
     */
    int getProgress() const;

    /**
     * @brief 设置当前升级进度
     * @param progress 升级进度(0-100)
     */
    void setProgress(int progress);

    /**
     * @brief 获取当前状态消息
     * @return 状态消息
     */
    QString getStatusMessage() const;

    /**
     * @brief 设置当前状态消息
     * @param message 状态消息
     */
    void setStatusMessage(const QString& message);

    /**
     * @brief 验证文件是否有效
     * @param filePath 文件路径
     * @param fileType 文件类型("SOC"或"ISO")
     * @param errorMessage 错误消息输出参数
     * @return 验证结果，true表示有效
     */
    bool validateFile(const QString& filePath, const QString& fileType, QString& errorMessage);

    /**
     * @brief 开始升级
     * @param deviceType 设备类型
     * @return 启动结果，true表示成功
     */
    bool startUpdate(DeviceType deviceType);

    /**
     * @brief 停止升级
     * @return 停止结果，true表示成功
     */
    bool stopUpdate();

    /**
     * @brief 重置模型状态
     */
    void reset();

signals:
    /**
     * @brief 升级状态变更信号
     * @param status 新的升级状态
     */
    void statusChanged(UpdateStatus status);

    /**
     * @brief 升级进度变更信号
     * @param progress 新的进度值(0-100)
     */
    void progressChanged(int progress);

    /**
     * @brief 升级完成信号
     * @param success 是否成功
     * @param message 完成消息
     */
    void updateCompleted(bool success, const QString& message);

    /**
     * @brief 文件路径变更信号
     * @param deviceType 设备类型
     * @param filePath 新的文件路径
     */
    void filePathChanged(DeviceType deviceType, const QString& filePath);

private:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit UpdateDeviceModel(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~UpdateDeviceModel();

    /**
     * @brief 模拟升级过程(演示用)
     * @param deviceType 设备类型
     */
    void simulateUpdate(DeviceType deviceType);

private:
    QString m_socFilePath;         // SOC文件路径
    QString m_phyFilePath;         // PHY文件路径
    UpdateStatus m_status;         // 当前升级状态
    DeviceType m_currentDevice;    // 当前升级设备类型
    int m_progress;                // 当前升级进度
    QString m_statusMessage;       // 当前状态消息
    QTimer* m_updateTimer;         // 升级定时器(模拟用)
};