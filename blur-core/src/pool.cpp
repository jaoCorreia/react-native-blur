#include "pool.h"
#include <mutex>
#include <algorithm>

namespace blur {

static std::mutex gPoolMutex;

BufferPool& BufferPool::instance() {
    static BufferPool pool;
    return pool;
}

BufferPool::ThreadCache& BufferPool::threadCache() {
    thread_local ThreadCache cache;
    return cache;
}

std::vector<uint8_t> BufferPool::acquire(size_t sizeInBytes) {
    if (sizeInBytes == 0) return {};

    auto& cache = threadCache();
    auto it = cache.buckets.find(sizeInBytes);

    if (it != cache.buckets.end() && it->second.capacity() >= sizeInBytes) {
        auto result = std::move(it->second);
        cache.buckets.erase(it);
        result.resize(sizeInBytes);
        return result;
    }

    return std::vector<uint8_t>(sizeInBytes);
}

void BufferPool::release(std::vector<uint8_t>& buffer) {
    if (buffer.empty()) return;

    auto& cache = threadCache();
    size_t cap = buffer.capacity();

    auto it = cache.buckets.find(cap);
    if (it == cache.buckets.end() || it->second.capacity() < cap) {
        std::vector<uint8_t>().swap(cache.buckets[cap]);
        cache.buckets[cap] = std::move(buffer);
    }

    std::vector<uint8_t>().swap(buffer);
}

void BufferPool::clear() {
    std::lock_guard<std::mutex> lock(gPoolMutex);
    threadCache().buckets.clear();
}

} // namespace blur
