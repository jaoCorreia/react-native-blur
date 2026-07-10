#include "cache.h"
#include "bitmap.h"
#include <gtest/gtest.h>
#include <vector>

using namespace blur;

namespace {

Bitmap makeBitmap(std::vector<uint8_t>& buffer, int w, int h, uint8_t fill) {
    buffer.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, fill);
    Bitmap bmp;
    bmp.pixels = buffer.data();
    bmp.width = w;
    bmp.height = h;
    bmp.stride = w * 4;
    bmp.channels = 4;
    return bmp;
}

BlurCacheKey makeKey(uint64_t hash, int w, int h, int radius) {
    return BlurCacheKey{hash, w, h, radius, -1.0f, 1};
}

} // namespace

class BlurCacheTest : public ::testing::Test {
protected:
    void SetUp() override { BlurCache::instance().clear(); }
    void TearDown() override { BlurCache::instance().clear(); }
};

TEST_F(BlurCacheTest, MissOnEmptyCache) {
    std::vector<uint8_t> outBuf;
    Bitmap out = makeBitmap(outBuf, 4, 4, 0);
    EXPECT_FALSE(BlurCache::instance().get(makeKey(1, 4, 4, 3), out));
}

TEST_F(BlurCacheTest, RoundTripsExactBytes) {
    std::vector<uint8_t> srcBuf;
    Bitmap src = makeBitmap(srcBuf, 4, 4, 42);
    auto key = makeKey(1, 4, 4, 3);

    BlurCache::instance().put(key, src);

    std::vector<uint8_t> outBuf;
    Bitmap out = makeBitmap(outBuf, 4, 4, 0);
    ASSERT_TRUE(BlurCache::instance().get(key, out));
    for (auto b : outBuf) EXPECT_EQ(b, 42);
}

TEST_F(BlurCacheTest, DistinctKeysDoNotCollide) {
    std::vector<uint8_t> bufA;
    Bitmap a = makeBitmap(bufA, 4, 4, 10);
    std::vector<uint8_t> bufB;
    Bitmap b = makeBitmap(bufB, 4, 4, 20);

    BlurCache::instance().put(makeKey(1, 4, 4, 3), a);
    BlurCache::instance().put(makeKey(2, 4, 4, 3), b);

    std::vector<uint8_t> outBuf;
    Bitmap out = makeBitmap(outBuf, 4, 4, 0);
    ASSERT_TRUE(BlurCache::instance().get(makeKey(2, 4, 4, 3), out));
    for (auto v : outBuf) EXPECT_EQ(v, 20);
}

TEST_F(BlurCacheTest, EvictsUnderByteBudgetNotEntryCount) {
    // Each entry is far larger than 16 count-based eviction would assume;
    // pushing well past the byte budget must trigger eviction of the least
    // recently used entries, not grow unbounded.
    const int dim = 2048; // 2048*2048*4 = 16MB per entry
    std::vector<uint8_t> buf;
    Bitmap bmp = makeBitmap(buf, dim, dim, 7);

    for (int i = 0; i < 8; ++i) {
        BlurCache::instance().put(makeKey(static_cast<uint64_t>(i), dim, dim, 3), bmp);
    }

    EXPECT_LE(BlurCache::instance().totalBytes(), 64u * 1024 * 1024);

    std::vector<uint8_t> outBuf;
    Bitmap out = makeBitmap(outBuf, dim, dim, 0);
    EXPECT_FALSE(BlurCache::instance().get(makeKey(0, dim, dim, 3), out))
        << "oldest entry should have been evicted once the byte budget was exceeded";
    EXPECT_TRUE(BlurCache::instance().get(makeKey(7, dim, dim, 3), out))
        << "most recently inserted entry should still be cached";
}

TEST_F(BlurCacheTest, GetRefreshesRecencyOrder) {
    const int dim = 2048;
    std::vector<uint8_t> buf;
    Bitmap bmp = makeBitmap(buf, dim, dim, 7);

    BlurCache::instance().put(makeKey(100, dim, dim, 3), bmp);
    for (int i = 0; i < 3; ++i) {
        BlurCache::instance().put(makeKey(static_cast<uint64_t>(i), dim, dim, 3), bmp);
    }

    // Touch key 100 so it becomes the most recently used entry.
    std::vector<uint8_t> outBuf;
    Bitmap out = makeBitmap(outBuf, dim, dim, 0);
    ASSERT_TRUE(BlurCache::instance().get(makeKey(100, dim, dim, 3), out));

    // Pushing more entries past the budget should evict key 0 (now the
    // oldest untouched entry) before it evicts the just-refreshed key 100.
    for (int i = 3; i < 6; ++i) {
        BlurCache::instance().put(makeKey(static_cast<uint64_t>(i), dim, dim, 3), bmp);
    }

    EXPECT_TRUE(BlurCache::instance().get(makeKey(100, dim, dim, 3), out));
}
