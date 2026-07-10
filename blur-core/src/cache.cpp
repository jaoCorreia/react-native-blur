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

bool BlurCache::get(const BlurCacheKey& key, std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = index.find(key);
    if (it != index.end()) {
        auto listIt = it->second;
        out = listIt->data;
        lru.splice(lru.end(), lru, listIt);
        return true;
    }
    return false;
}

void BlurCache::put(BlurCacheKey key, const Bitmap& result) {
    std::lock_guard<std::mutex> lock(mutex);

    auto it = index.find(key);
    if (it != index.end()) {
        lru.splice(lru.end(), lru, it->second);
        std::memcpy(it->second->data.data(), result.pixels, result.totalBytes());
        return;
    }

    Entry entry;
    entry.key = key;
    entry.data.resize(result.totalBytes());
    std::memcpy(entry.data.data(), result.pixels, result.totalBytes());

    lru.push_back(std::move(entry));
    auto listIt = std::prev(lru.end());
    index[key] = listIt;

    while (lru.size() > MAX_ENTRIES) {
        auto oldest = lru.front();
        index.erase(oldest.key);
        lru.pop_front();
    }
}

void BlurCache::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    index.clear();
    lru.clear();
}

size_t BlurCache::size() const {
    std::lock_guard<std::mutex> lock(mutex);
    return lru.size();
}

} // namespace blur
