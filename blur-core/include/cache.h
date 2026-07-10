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

    bool get(const BlurCacheKey& key, std::vector<uint8_t>& out);
    void put(BlurCacheKey key, const Bitmap& result);
    void clear();
    size_t size() const;

private:
    BlurCache() = default;

    static constexpr size_t MAX_ENTRIES = 16;

    struct Entry {
        std::vector<uint8_t> data;
        BlurCacheKey key;
    };

    mutable std::mutex mutex;
    std::unordered_map<BlurCacheKey, std::list<Entry>::iterator, BlurCacheKeyHash> index;
    std::list<Entry> lru;
};

} // namespace blur
