#include "pool.h"
#include <algorithm>

namespace blur {

namespace {

constexpr size_t kMinSizeClass = 4096;
constexpr size_t kMaxThreadCacheBytes = 64 * 1024 * 1024; // cap per-thread cache growth

size_t sizeClass(size_t n) {
    size_t cls = kMinSizeClass;
    while (cls < n) cls <<= 1;
    return cls;
}

} // namespace

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
    auto it = cache.buckets.find(sizeClass(sizeInBytes));

    if (it != cache.buckets.end() && it->second.capacity() >= sizeInBytes) {
        auto result = std::move(it->second);
        cache.totalBytes -= result.capacity();
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

    // Bound how much memory a single thread can pin in cached buffers;
    // beyond the cap, just let the buffer free normally instead of growing forever.
    if (cache.totalBytes + cap > kMaxThreadCacheBytes) {
        std::vector<uint8_t>().swap(buffer);
        return;
    }

    size_t cls = sizeClass(cap);
    auto it = cache.buckets.find(cls);
    if (it == cache.buckets.end() || it->second.capacity() < cap) {
        if (it != cache.buckets.end()) {
            cache.totalBytes -= it->second.capacity();
        }
        cache.totalBytes += cap;
        cache.buckets[cls] = std::move(buffer);
    }

    std::vector<uint8_t>().swap(buffer);
}

void BufferPool::clear() {
    auto& cache = threadCache();
    cache.buckets.clear();
    cache.totalBytes = 0;
}

} // namespace blur
