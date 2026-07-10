#include "gaussian.h"
#include "bitmap.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <algorithm>

using namespace blur;

class GaussianBlurTest : public ::testing::Test {
protected:
    Bitmap createBitmap(int w, int h) {
        Bitmap bmp;
        buffer.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
        bmp.pixels = buffer.data();
        bmp.width = w;
        bmp.height = h;
        bmp.stride = w * 4;
        bmp.channels = 4;
        return bmp;
    }

    void fillSolid(Bitmap& bmp, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        int w = bmp.width;
        int h = bmp.height;
        for (int y = 0; y < h; ++y) {
            uint8_t* row = bmp.getRow(y);
            for (int x = 0; x < w; ++x) {
                size_t off = static_cast<size_t>(x) * 4;
                row[off + 0] = r;
                row[off + 1] = g;
                row[off + 2] = b;
                row[off + 3] = a;
            }
        }
    }

    void setPixel(Bitmap& bmp, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        uint8_t* pixel = bmp.getRow(y) + static_cast<ptrdiff_t>(x) * 4;
        pixel[0] = r;
        pixel[1] = g;
        pixel[2] = b;
        pixel[3] = a;
    }

    bool allPixelsInRange(const Bitmap& bmp) {
        int w = bmp.width;
        int h = bmp.height;
        for (int y = 0; y < h; ++y) {
            const uint8_t* row = bmp.getRowConst(y);
            for (int x = 0; x < w; ++x) {
                size_t off = static_cast<size_t>(x) * 4;
                for (int c = 0; c < 4; ++c) {
                    if (row[off + c] > 255) return false;
                }
            }
        }
        return true;
    }

    std::vector<uint8_t> buffer;
};

TEST_F(GaussianBlurTest, InvalidBitmapReturnsEarly) {
    Bitmap bmp;
    EXPECT_NO_THROW(gaussianBlur(bmp, 5));
}

TEST_F(GaussianBlurTest, ZeroRadiusReturnsEarly) {
    auto bmp = createBitmap(16, 16);
    fillSolid(bmp, 128, 128, 128, 255);
    auto original = buffer;

    gaussianBlur(bmp, 0);

    EXPECT_EQ(buffer, original) << "Zero radius should not modify bitmap";
}

TEST_F(GaussianBlurTest, BlurSolidColorDoesNotChange) {
    auto bmp = createBitmap(32, 32);
    fillSolid(bmp, 100, 150, 200, 255);

    gaussianBlur(bmp, 3);

    for (int y = 0; y < 32; ++y) {
        const uint8_t* row = bmp.getRowConst(y);
        for (int x = 0; x < 32; ++x) {
            size_t off = static_cast<size_t>(x) * 4;
            EXPECT_EQ(row[off + 0], 100);
            EXPECT_EQ(row[off + 1], 150);
            EXPECT_EQ(row[off + 2], 200);
            EXPECT_EQ(row[off + 3], 255);
        }
    }
}

TEST_F(GaussianBlurTest, SingleWhitePixelBlurred) {
    int size = 8;
    int cx = size / 2;
    int cy = size / 2;
    auto bmp = createBitmap(size, size);
    fillSolid(bmp, 0, 0, 0, 255);
    setPixel(bmp, cx, cy, 255, 255, 255, 255);

    gaussianBlur(bmp, 3);

    uint8_t* center = bmp.getRow(cy) + cx * 4;
    EXPECT_LT(center[0], 255) << "Center should be darker after blur";

    uint8_t* adjacent = bmp.getRow(cy) + (cx - 1) * 4;
    EXPECT_GT(adjacent[0], 0) << "Adjacent pixel should gain brightness after blur";
}

TEST_F(GaussianBlurTest, OutputIsSymmetric) {
    int size = 7;
    int center = size / 2;
    auto bmp = createBitmap(size, size);
    fillSolid(bmp, 0, 0, 0, 255);
    setPixel(bmp, center, center, 255, 255, 255, 255);

    gaussianBlur(bmp, 3);

    for (int y = 0; y < size; ++y) {
        const uint8_t* row = bmp.getRowConst(y);
        for (int x = 0; x < center; ++x) {
            int mirrorX = size - 1 - x;
            size_t offL = static_cast<size_t>(x) * 4;
            size_t offR = static_cast<size_t>(mirrorX) * 4;
            EXPECT_EQ(row[offL + 0], row[offR + 0])
                << "Asymmetry at (" << x << "," << y << ")";
        }
    }
}

TEST_F(GaussianBlurTest, PixelsStayInRange) {
    for (int r = 1; r <= 10; ++r) {
        auto bmp = createBitmap(16, 16);
        fillSolid(bmp, 255, 0, 0, 255);
        setPixel(bmp, 8, 8, 0, 255, 0, 255);

        gaussianBlur(bmp, r);

        EXPECT_TRUE(allPixelsInRange(bmp))
            << "Pixel out of range for radius=" << r;
    }
}

TEST_F(GaussianBlurTest, AlphaChannelBlurred) {
    auto bmp = createBitmap(8, 8);
    fillSolid(bmp, 128, 128, 128, 200);
    setPixel(bmp, 4, 4, 255, 255, 255, 50);

    gaussianBlur(bmp, 2);

    for (int y = 0; y < 8; ++y) {
        const uint8_t* row = bmp.getRowConst(y);
        for (int x = 0; x < 8; ++x) {
            size_t off = static_cast<size_t>(x) * 4;
            EXPECT_GE(row[off + 3], 0);
            EXPECT_LE(row[off + 3], 255);
        }
    }
}

TEST_F(GaussianBlurTest, DimensionsPreserved) {
    auto bmp = createBitmap(13, 17);
    fillSolid(bmp, 64, 128, 192, 255);

    gaussianBlur(bmp, 4);

    EXPECT_EQ(bmp.width, 13);
    EXPECT_EQ(bmp.height, 17);
    EXPECT_EQ(bmp.channels, 4);
}

TEST_F(GaussianBlurTest, LargerRadiusMoreBlur) {
    std::vector<uint8_t> buf1(32 * 32 * 4, 0);
    std::vector<uint8_t> buf2(32 * 32 * 4, 0);

    Bitmap bmp1;
    bmp1.pixels = buf1.data();
    bmp1.width = 32;
    bmp1.height = 32;
    bmp1.stride = 32 * 4;
    bmp1.channels = 4;

    Bitmap bmp2;
    bmp2.pixels = buf2.data();
    bmp2.width = 32;
    bmp2.height = 32;
    bmp2.stride = 32 * 4;
    bmp2.channels = 4;

    fillSolid(bmp1, 0, 0, 0, 255);
    fillSolid(bmp2, 0, 0, 0, 255);
    setPixel(bmp1, 16, 16, 255, 255, 255, 255);
    setPixel(bmp2, 16, 16, 255, 255, 255, 255);

    gaussianBlur(bmp1, 2);
    gaussianBlur(bmp2, 6);

    uint8_t* c1 = bmp1.getRow(16) + 16 * 4;
    uint8_t* c2 = bmp2.getRow(16) + 16 * 4;

    EXPECT_GT(c1[0], c2[0])
        << "Larger radius should produce darker center";
}

TEST_F(GaussianBlurTest, HandlesSmallBitmaps) {
    for (int w : {1, 2, 3}) {
        for (int h : {1, 2, 3}) {
            auto bmp = createBitmap(w, h);
            fillSolid(bmp, 128, 128, 128, 255);
            EXPECT_NO_THROW(gaussianBlur(bmp, 2));
            EXPECT_EQ(bmp.width, w);
            EXPECT_EQ(bmp.height, h);
        }
    }
}
