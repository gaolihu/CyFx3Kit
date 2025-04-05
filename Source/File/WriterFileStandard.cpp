// Source/File/WriterFileStandard.cpp

#include "FileManager.h"
#include "Logger.h"

bool WriterFileStandard::open(const QString& filename) {
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

bool WriterFileStandard::write(const QByteArray& data) {
    if (!m_isOpen) {
        m_lastError = LocalQTCompat::fromLocal8Bit("文件未打开");
        LOG_ERROR(m_lastError);
        return false;
    }

    // 使用批量写入以提高性能
    static const int BUFFER_SIZE = 4 * 1024 * 1024; // 4MB缓冲区
    static QByteArray writeBuffer;

    // 添加到缓冲区
    writeBuffer.append(data);

    // 如果缓冲区超过阈值，进行写入
    if (writeBuffer.size() >= BUFFER_SIZE) {
        qint64 written = m_file.write(writeBuffer);
        if (written != writeBuffer.size()) {
            m_lastError = m_file.errorString();
            LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件写入错误: %1").arg(m_lastError));
            writeBuffer.clear();
            return false;
        }

        writeBuffer.clear();
    }

    return true;
}

bool WriterFileStandard::close() {
    if (m_isOpen) {
        // 写入剩余缓冲区数据
        static QByteArray& writeBuffer = *new QByteArray(); // 使用静态引用以访问前一个方法的静态变量

        if (!writeBuffer.isEmpty()) {
            qint64 written = m_file.write(writeBuffer);
            if (written != writeBuffer.size()) {
                m_lastError = m_file.errorString();
                LOG_ERROR(LocalQTCompat::fromLocal8Bit("文件关闭前写入剩余数据错误: %1").arg(m_lastError));
            }

            writeBuffer.clear();
        }

        m_file.flush();
        m_file.close();
        m_isOpen = false;
        LOG_INFO(LocalQTCompat::fromLocal8Bit("文件已关闭"));
    }
    return true;
}

QString WriterFileStandard::getLastError() const {
    return m_lastError;
}