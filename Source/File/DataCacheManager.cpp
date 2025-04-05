// Source/File/DataCacheManager.cpp

#include "FileManager.h"
#include "Logger.h"

DataCacheManager::DataCacheManager(size_t maxCacheSize)
    : m_maxCacheSize(maxCacheSize)
{
    // 预分配缓存空间以减少重新分配
    m_cache.reserve(static_cast<int>(maxCacheSize / 2));
}

DataCacheManager::~DataCacheManager() {
    clearCache();
}

void DataCacheManager::addToCache(const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);

    // 预分配内存以避免频繁的内存分配
    if (m_cache.capacity() < m_cache.size() + data.size()) {
        // 使用二次增长策略
        size_t newCapacity = std::max(static_cast<size_t>(m_cache.size() + data.size()),
            static_cast<size_t>(m_cache.capacity() * 1.5));
        // 确保不超过最大缓存大小
        newCapacity = std::min(newCapacity, m_maxCacheSize);
        m_cache.reserve(static_cast<int>(newCapacity));
    }

    // 检查是否超出最大缓存大小
    if (m_cache.size() + data.size() > static_cast<int>(m_maxCacheSize)) {
        // 缓存已满，处理策略：
        if (data.size() >= static_cast<int>(m_maxCacheSize)) {
            // 新数据比最大缓存还大，清空缓存并只保留新数据的尾部
            m_cache.clear();
            m_cache.append(data.right(static_cast<int>(m_maxCacheSize * 0.9)));
            LOG_WARN(LocalQTCompat::fromLocal8Bit("数据大小(%1)超过最大缓存(%2), 只保留尾部")
                .arg(data.size())
                .arg(m_maxCacheSize));
        }
        else {
            // 优化移除旧数据的方式
            int excessSize = m_cache.size() + data.size() - static_cast<int>(m_maxCacheSize);

            // 通过创建新的QByteArray并只复制需要保留的部分，减少内存移动操作
            QByteArray newCache;
            newCache.reserve(static_cast<int>(m_maxCacheSize));
            newCache.append(m_cache.constData() + excessSize, m_cache.size() - excessSize);
            newCache.append(data);
            m_cache = std::move(newCache);
        }
    }
    else {
        // 缓存未满，直接添加
        m_cache.append(data);
    }
}

QByteArray DataCacheManager::getCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_cache;
}

void DataCacheManager::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cache.clear();
    // 显式减少分配的内存
    m_cache.squeeze();
}

size_t DataCacheManager::getCurrentCacheSize() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return static_cast<size_t>(m_cache.size());
}

void DataCacheManager::setMaxCacheSize(size_t maxSize) {
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