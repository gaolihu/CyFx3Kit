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
    // ��������ö�ٷ������ڲ�
    enum class CommandType {
        CMD_START,
        CMD_FRAME_SIZE,
        CMD_END
    };
    Q_ENUM(CommandType)

        static CommandManager& instance();

    // ��������Ŀ¼
    bool setCommandDirectory(const QString& path);

    // ��ȡָ�����͵���������
    std::vector<uint8_t> getCommand(CommandType type) const;

    // ��ȡ��ǰ����Ŀ¼
    QString getCommandDirectory() const { return m_commandDir; }

    // ��������ļ��Ƿ�����
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