#pragma once

#include "bitmap.h"
#include "scale.h"
#include <cstdint>
#include <unordered_map>
#include <list>
#include <mutex>
#include <vector>
#include <functional>

namespace blur {

struct BlurCacheKey {
    uint64_t pixelHash;
    int width;
    int height;
    int radius;
    float sigma;
    int scale;

    bool operator==(const BlurCacheKey& other) const {
        return pixelHash == other.pixelHash
            && width == other.width
            && height == other.height
            && radius == other.radius
            && sigma == other.sigma
            && scale == other.scale;
    }
};

struct BlurCacheKeyHash {
    size_t operator()(const BlurCacheKey& key) const {
        size_t h = std::hash<uint64_t>{}(key.pixelHash);
        h ^= std::hash<int>{}(key.width) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(key.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(key.radius) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<float>{}(key.sigma) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(key.scale) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

uint64_t hashBitmap(const Bitmap& bitmap);

class BlurCache {
public:
    static BlurCache& instance();

    // Writes the cached result directly into out.pixels (out must already be
    // sized to match the bitmap the key describes) instead of handing back
    // an intermediate vector for the caller to copy a second time.
    bool get(const BlurCacheKey& key, Bitmap& out);
    void put(BlurCacheKey key, const Bitmap& result);
    void clear();
    size_t size() const;
    size_t totalBytes() const;

private:
    BlurCache() = default;

    // Budget is in bytes, not entry count: a handful of full-screen bitmaps
    // can dwarf a small icon-sized cache many times over, so a fixed entry
    // count can't bound memory the way a byte budget does.
    static constexpr size_t MAX_BYTES = 64 * 1024 * 1024;

    struct Entry {
        std::vector<uint8_t> data;
        BlurCacheKey key;
    };

    mutable std::mutex mutex;
    std::unordered_map<BlurCacheKey, std::list<Entry>::iterator, BlurCacheKeyHash> index;
    std::list<Entry> lru;
    size_t cachedBytes = 0;
};

} // namespace blur
