// Source/MVC/Models/FileSaveModel.h
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QDateTime>
#include <QMutex>
#include <atomic>
#include <memory>
#include <FileSaveManager.h>

/**
 * @brief 文件保存模型类
 *
 * 文件保存模型管理所有与文件保存相关的数据和状态，
 * 包括保存参数、状态和统计信息。
 */
class FileSaveModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 模型实例的引用
     */
    static FileSaveModel* getInstance();

    /**
     * @brief 获取保存参数
     * @return 当前保存参数
     */
    SaveParameters getSaveParameters() const;

    /**
     * @brief 设置保存参数
     * @param parameters 新的保存参数
     */
    void setSaveParameters(const SaveParameters& parameters);

    /**
     * @brief 获取当前状态
     * @return 当前保存状态
     */
    SaveStatus getStatus() const;

    /**
     * @brief 设置状态
     * @param status 新的保存状态
     */
    void setStatus(SaveStatus status);

    /**
     * @brief 获取保存统计信息
     * @return 当前保存统计信息
     */
    SaveStatistics getStatistics() const;

    /**
     * @brief 更新保存统计信息
     * @param statistics 新的统计信息
     */
    void updateStatistics(const SaveStatistics& statistics);

    /**
     * @brief 重置保存统计信息
     */
    void resetStatistics();

    /**
     * @brief 获取保存目标完整路径
     * @return 完整的保存路径字符串
     */
    QString getFullSavePath() const;

    /**
     * @brief 获取保存选项值
     * @param key 选项键
     * @param defaultValue 默认值
     * @return 选项值，如不存在则返回默认值
     */
    QVariant getOption(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 设置保存选项
     * @param key 选项键
     * @param value 选项值
     */
    void setOption(const QString& key, const QVariant& value);

    /**
     * @brief 设置图像参数
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     */
    void setImageParameters(uint16_t width, uint16_t height, uint8_t format);

    /**
     * @brief 使用异步文件写入器
     * @param use 是否使用异步写入
     */
    void setUseAsyncWriter(bool use);

    /**
     * @brief 是否使用异步文件写入器
     * @return 是否使用异步写入
     */
    bool isUsingAsyncWriter() const;

    /**
     * @brief 保存当前配置到系统设置
     * @return 保存结果，true表示成功
     */
    bool saveConfigToSettings();

    /**
     * @brief 从系统设置加载配置
     * @return 加载结果，true表示成功
     */
    bool loadConfigFromSettings();

    /**
     * @brief 重置为默认配置
     */
    void resetToDefault();

signals:
    /**
     * @brief 参数变更信号
     * @param parameters 新的保存参数
     */
    void parametersChanged(const SaveParameters& parameters);

    /**
     * @brief 状态变更信号
     * @param status 新的保存状态
     */
    void statusChanged(SaveStatus status);

    /**
     * @brief 统计信息更新信号
     * @param statistics 新的统计信息
     */
    void statisticsUpdated(const SaveStatistics& statistics);

    /**
     * @brief 保存完成信号
     * @param path 保存路径
     * @param totalBytes 总字节数
     */
    void saveCompleted(const QString& path, uint64_t totalBytes);

    /**
     * @brief 保存错误信号
     * @param error 错误消息
     */
    void saveError(const QString& error);

private:
    /**
     * @brief 构造函数 (私有)
     */
    FileSaveModel();

    /**
     * @brief 析构函数 (私有)
     */
    ~FileSaveModel();

    /**
     * @brief 禁用拷贝构造函数
     */
    FileSaveModel(const FileSaveModel&) = delete;

    /**
     * @brief 禁用赋值操作符
     */
    FileSaveModel& operator=(const FileSaveModel&) = delete;

private:
    SaveParameters m_parameters;                       // 保存参数
    std::atomic<SaveStatus> m_status;                  // 当前状态
    SaveStatistics m_statistics;                       // 统计信息
    mutable QMutex m_dataMutex;                        // 数据互斥锁
    bool m_useAsyncWriter;                             // 使用异步写入器
};