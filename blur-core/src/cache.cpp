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
    auto it = cache.find(key);
    if (it != cache.end()) {
        out = it->second;
        return true;
    }
    return false;
}

void BlurCache::put(BlurCacheKey key, const Bitmap& result) {
    std::vector<uint8_t> copy(result.totalBytes());
    std::memcpy(copy.data(), result.pixels, result.totalBytes());
    cache[std::move(key)] = std::move(copy);

    if (cache.size() > 16) {
        auto oldest = cache.begin();
        cache.erase(oldest);
    }
}

void BlurCache::clear() {
    cache.clear();
}

size_t BlurCache::size() const {
    return cache.size();
}

} // namespace blur
