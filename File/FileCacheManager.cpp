#include "FileSaveManager.h"
#include "Logger.h"

FileCacheManager::FileCacheManager(size_t maxCacheSize)
    : m_maxCacheSize(maxCacheSize)
{
    // 预分配缓存空间以减少重新分配
    m_cache.reserve(static_cast<int>(maxCacheSize / 2));
}

FileCacheManager::~FileCacheManager() {
    clearCache();
}

void FileCacheManager::addToCache(const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // 检查是否超出最大缓存大小
    if (m_cache.size() + data.size() > static_cast<int>(m_maxCacheSize)) {
        // 缓存已满，处理策略：
        // 如果新数据比最大缓存还大，则清空缓存并只保留新数据的尾部
        if (data.size() >= static_cast<int>(m_maxCacheSize)) {
            m_cache.clear();
            m_cache.append(data.right(static_cast<int>(m_maxCacheSize * 0.9)));
            LOG_WARN(LocalQTCompat::fromLocal8Bit("数据大小(%1)超过最大缓存(%2), 只保留尾部")
                .arg(data.size())
                .arg(m_maxCacheSize));
        }
        else {
            // 正常情况：丢弃部分旧数据，保留新数据
            int excessSize = m_cache.size() + data.size() - static_cast<int>(m_maxCacheSize);
            m_cache.remove(0, excessSize);
            m_cache.append(data);
        }
    }
    else {
        // 缓存未满，直接添加
        m_cache.append(data);
    }
}

QByteArray FileCacheManager::getCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cache;
}

void FileCacheManager::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
    // 显式减少分配的内存
    m_cache.squeeze();
}

size_t FileCacheManager::getCurrentCacheSize() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return static_cast<size_t>(m_cache.size());
}

void FileCacheManager::setMaxCacheSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    if (maxSize == 0) {
        LOG_WARN(LocalQTCompat::fromLocal8Bit("尝试设置缓存大小为0，使用默认值"));
        maxSize = DEFAULT_CACHE_SIZE;
    }

    m_maxCacheSize = maxSize;

    // 如果当前缓存大小超过新的最大大小，裁剪缓存
    if (m_cache.size() > static_cast<int>(maxSize)) {
        int excessSize = m_cache.size() - static_cast<int>(maxSize);
        m_cache.remove(0, excessSize);
        LOG_INFO(LocalQTCompat::fromLocal8Bit("缓存已裁剪，移除 %1 字节以适应新的最大大小").arg(excessSize));
    }
}