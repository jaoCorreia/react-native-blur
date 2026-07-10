#include "cache.h"
#include <cstring>
#include <mutex>

namespace blur {

uint64_t hashBitmap(const Bitmap& bitmap) {
    if (!bitmap.isValid()) return 0;

    const uint8_t* data = bitmap.pixels;
    size_t totalBytes = bitmap.totalBytes();

    uint64_t hash = 14695981039346656037ULL;

    size_t step = std::max(size_t(1), totalBytes / 1024);
    for (size_t i = 0; i < totalBytes; i += step) {
        hash ^= static_cast<uint64_t>(data[i]);
        hash *= 1099511628211ULL;
    }

    return hash;
}

BlurCache& BlurCache::instance() {
    static BlurCache instance;
    return instance;
}

bool BlurCache::get(const BlurCacheKey& key, Bitmap& out) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = index.find(key);
    if (it == index.end()) return false;

    auto listIt = it->second;
    if (listIt->data.size() != out.totalBytes()) return false;

    std::memcpy(out.pixels, listIt->data.data(), listIt->data.size());
    lru.splice(lru.end(), lru, listIt);
    return true;
}

void BlurCache::put(BlurCacheKey key, const Bitmap& result) {
    size_t bytes = result.totalBytes();
    if (bytes == 0 || bytes > MAX_BYTES) return;

    std::lock_guard<std::mutex> lock(mutex);

    auto it = index.find(key);
    if (it != index.end()) {
        lru.splice(lru.end(), lru, it->second);
        std::memcpy(it->second->data.data(), result.pixels, bytes);
        return;
    }

    Entry entry;
    entry.key = key;
    entry.data.resize(bytes);
    std::memcpy(entry.data.data(), result.pixels, bytes);

    lru.push_back(std::move(entry));
    auto listIt = std::prev(lru.end());
    index[key] = listIt;
    cachedBytes += bytes;

    while (cachedBytes > MAX_BYTES && !lru.empty()) {
        cachedBytes -= lru.front().data.size();
        index.erase(lru.front().key);
        lru.pop_front();
    }
}

void BlurCache::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    index.clear();
    lru.clear();
    cachedBytes = 0;
}

size_t BlurCache::size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return lru.size();
}

size_t BlurCache::totalBytes() const {
    std::lock_guard<std::mutex> lock(mutex);
    return cachedBytes;
}

} // namespace blur
