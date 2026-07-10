#include "gaussian.h"
#include "boxblur.h"
#include "bitmap.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

using namespace blur;

namespace {

Bitmap makeBitmap(std::vector<uint8_t>& buffer, int w, int h) {
    buffer.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 0);
    Bitmap bmp;
    bmp.pixels = buffer.data();
    bmp.width = w;
    bmp.height = h;
    bmp.stride = w * 4;
    bmp.channels = 4;
    return bmp;
}

void setPixel(Bitmap& bmp, int x, int y, uint8_t v) {
    uint8_t* p = bmp.getRow(y) + static_cast<ptrdiff_t>(x) * 4;
    p[0] = p[1] = p[2] = v;
    p[3] = 255;
}

} // namespace

// gaussianBlur() routes radius > 5 through boxBlur3Pass (BlurMethod::Auto).
// A single bright pixel on a dark field must blur symmetrically in both
// axes; a horizontal/vertical-only bug (e.g. the vertical pass reading the
// wrong buffer) breaks this while still "looking blurred" in casual checks.
class BoxBlurSymmetryTest : public ::testing::TestWithParam<int> {};

TEST_P(BoxBlurSymmetryTest, SymmetricAroundCenter) {
    int radius = GetParam();
    int size = 41;
    int center = size / 2;

    std::vector<uint8_t> buf;
    Bitmap bmp = makeBitmap(buf, size, size);
    setPixel(bmp, center, center, 255);

    gaussianBlur(bmp, radius);

    for (int y = 0; y < size; ++y) {
        const uint8_t* row = bmp.getRowConst(y);
        int mirrorY = size - 1 - y;
        const uint8_t* mirrorRow = bmp.getRowConst(mirrorY);
        for (int x = 0; x < size; ++x) {
            int mirrorX = size - 1 - x;
            size_t off = static_cast<size_t>(x) * 4;
            size_t offMirrorX = static_cast<size_t>(mirrorX) * 4;

            EXPECT_EQ(row[off], row[offMirrorX])
                << "radius=" << radius << " asymmetric on X at (" << x << "," << y << ")";
            EXPECT_EQ(row[off], mirrorRow[off])
                << "radius=" << radius << " asymmetric on Y at (" << x << "," << y << ")";
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Radii, BoxBlurSymmetryTest,
                         ::testing::Values(6, 10, 15, 20, 30));

TEST(BoxBlur3PassTest, ActuallyBlursBothAxes) {
    int size = 41;
    int center = size / 2;

    std::vector<uint8_t> buf;
    Bitmap bmp = makeBitmap(buf, size, size);
    setPixel(bmp, center, center, 255);

    boxBlur3Pass(bmp, /*gaussianRadius=*/20);

    const uint8_t* centerRow = bmp.getRowConst(center);
    uint8_t rightOfCenter = centerRow[static_cast<size_t>(center + 3) * 4];
    uint8_t belowCenter = bmp.getRowConst(center + 3)[static_cast<size_t>(center) * 4];

    // A bug that skips the horizontal pass leaves columns away from the
    // center untouched horizontally (value stays 0); this must not happen.
    EXPECT_GT(rightOfCenter, 0) << "horizontal pass did not spread the blur";
    EXPECT_GT(belowCenter, 0) << "vertical pass did not spread the blur";
}
