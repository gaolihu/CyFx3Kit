#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <vector>
#include <memory>

class CommandManager : public QObject
{
    Q_OBJECT

public:
    // 命令类型枚举放在类内部
    enum class CommandType {
        CMD_START,
        CMD_FRAME_SIZE,
        CMD_END
    };
    Q_ENUM(CommandType)

        static CommandManager& instance();

    // 设置命令目录
    bool setCommandDirectory(const QString& path);

    // 获取指定类型的命令数据
    std::vector<uint8_t> getCommand(CommandType type) const;

    // 获取当前命令目录
    QString getCommandDirectory() const { return m_commandDir; }

    // 检查命令文件是否完整
    bool validateCommands() const;

signals:
    void commandDirectoryChanged(const QString& newPath);
    void commandLoadError(const QString& error);

private:
    CommandManager();
    ~CommandManager() = default;
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;

    bool loadCommandFile(const QString& filename, std::vector<uint8_t>& data);
    void clearCommands();

    QString m_commandDir;
    QMap<CommandType, std::vector<uint8_t>> m_commands;

    static const QMap<CommandType, QString> COMMAND_FILES;
};