#include "CommandManager.h"
#include "Logger.h"
#include <QFile>
#include <QDir>

const QMap<CommandManager::CommandType, QString> CommandManager::COMMAND_FILES = {
    {CommandType::CMD_START, "CMD_START"},
    {CommandType::CMD_FRAME_SIZE, "CMD_FRAME_SIZE"},
    {CommandType::CMD_END, "CMD_END"}
};

CommandManager& CommandManager::instance()
{
    static CommandManager instance;
    return instance;
}

CommandManager::CommandManager()
    : QObject(nullptr)
{
}

bool CommandManager::setCommandDirectory(const QString& path)
{
    LOG_INFO(QString("Setting command directory to: %1").arg(path));

    QDir dir(path);
    if (!dir.exists()) {
        LOG_ERROR("Command directory does not exist");
        emit commandLoadError("指定的目录不存在");
        return false;
    }

    clearCommands();
    m_commandDir = path;

    bool success = true;
    for (auto it = COMMAND_FILES.begin(); it != COMMAND_FILES.end(); ++it) {
        std::vector<uint8_t> cmdData;
        QString filePath = dir.filePath(it.value());

        if (!loadCommandFile(filePath, cmdData)) {
            LOG_ERROR(QString("Failed to load command file: %1").arg(filePath));
            success = false;
            continue;
        }

        m_commands[it.key()] = cmdData;
    }

    if (success) {
        LOG_INFO("Successfully loaded all command files");
        emit commandDirectoryChanged(path);
    }
    else {
        LOG_ERROR("Failed to load some command files");
        emit commandLoadError("部分命令文件加载失败");
    }

    return success;
}

std::vector<uint8_t> CommandManager::getCommand(CommandType type) const
{
    auto it = m_commands.find(type);
    if (it != m_commands.end()) {
        return it.value();
    }
    return std::vector<uint8_t>();
}

bool CommandManager::validateCommands() const
{
    if (m_commandDir.isEmpty()) {
        return false;
    }

    QDir dir(m_commandDir);
    for (const QString& filename : COMMAND_FILES.values()) {
        if (!dir.exists(filename)) {
            return false;
        }
    }
    return true;
}

bool CommandManager::loadCommandFile(const QString& filename, std::vector<uint8_t>& data)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_ERROR(QString("Cannot open command file: %1").arg(filename));
        return false;
    }

    QByteArray contents = file.readAll();
    data.assign(contents.begin(), contents.end());

    LOG_INFO(QString("Loaded command file: %1, size: %2 bytes")
        .arg(filename)
        .arg(data.size()));

    return true;
}

void CommandManager::clearCommands()
{
    m_commands.clear();
}
