#include "FileSaveManager.h"
#include "Logger.h"

bool StandardFileWriter::open(const QString& filename) {
    close(); // 确保之前的文件已关闭

    m_file.setFileName(filename);
    m_isOpen = m_file.open(QIODevice::WriteOnly);

    if (!m_isOpen) {
        m_lastError = m_file.errorString();
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("打开文件失败: %1 - %2").arg(filename).arg(m_lastError));
        return false;
    }

    LOG_INFO(LocalQTCompat::fromLocal8Bit("文件已打开: %1").arg(filename));
    return true;
}

bool StandardFileWriter::write(const QByteArray& data) {
    if (!m_isOpen) {
        m_lastError = LocalQTCompat::fromLocal8Bit("文件未打开");
        LOG_ERROR(m_lastError);
        return false;
    }

    qint64 written = m_file.write(data);
    if (written != data.size()) {
        m_lastError = m_file.errorString();
        LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件写入错误: %1").arg(m_lastError));
        return false;
    }

    // 刷新文件，确保数据立即写入磁盘
    m_file.flush();

    return true;
}

bool StandardFileWriter::close() {
    if (m_isOpen) {
        m_file.flush();
        m_file.close();
        m_isOpen = false;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件已关闭"));
    }
    return true;
}

QString StandardFileWriter::getLastError() const {
    return m_lastError;
}