#include "scale.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>

namespace blur {

int autoScaleFactor(int radius) {
    if (radius >= 20) return 4;
    if (radius >= 8) return 2;
    return 1;
}

void downscale2x(const Bitmap& src, Bitmap& dst) {
    int sw = src.width;
    int sh = src.height;
    int dw = sw / 2;
    int dh = sh / 2;
    int ch = src.channels;

    for (int y = 0; y < dh; ++y) {
        uint8_t* dstRow = dst.getRow(y);
        const uint8_t* srcRow0 = src.getRowConst(y * 2);
        const uint8_t* srcRow1 = (y * 2 + 1 < sh) ? src.getRowConst(y * 2 + 1) : srcRow0;

        for (int x = 0; x < dw; ++x) {
            int sx = x * 2;
            int sx1 = std::min(sx + 1, sw - 1);
            const uint8_t* p00 = srcRow0 + sx * ch;
            const uint8_t* p01 = srcRow0 + sx1 * ch;
            const uint8_t* p10 = srcRow1 + sx * ch;
            const uint8_t* p11 = srcRow1 + sx1 * ch;

            for (int c = 0; c < ch; ++c) {
                int sum = static_cast<int>(p00[c]) + static_cast<int>(p01[c])
                        + static_cast<int>(p10[c]) + static_cast<int>(p11[c]);
                dstRow[x * ch + c] = static_cast<uint8_t>(sum / 4);
            }
        }
    }
}

void upscale2x(const Bitmap& src, Bitmap& dst) {
    int sw = src.width;
    int sh = src.height;
    int dw = dst.width;
    int dh = dst.height;
    int ch = src.channels;

    for (int y = 0; y < dh; ++y) {
        uint8_t* dstRow = dst.getRow(y);
        float sy = static_cast<float>(y) * static_cast<float>(sh - 1) / static_cast<float>(std::max(1, dh - 1));
        int sy0 = static_cast<int>(sy);
        int sy1 = std::min(sy0 + 1, sh - 1);
        float fy = sy - static_cast<float>(sy0);

        for (int x = 0; x < dw; ++x) {
            float sx = static_cast<float>(x) * static_cast<float>(sw - 1) / static_cast<float>(std::max(1, dw - 1));
            int sx0 = static_cast<int>(sx);
            int sx1 = std::min(sx0 + 1, sw - 1);
            float fx = sx - static_cast<float>(sx0);

            const uint8_t* p00 = src.getRowConst(sy0) + sx0 * ch;
            const uint8_t* p10 = src.getRowConst(sy0) + sx1 * ch;
            const uint8_t* p01 = src.getRowConst(sy1) + sx0 * ch;
            const uint8_t* p11 = src.getRowConst(sy1) + sx1 * ch;

            for (int c = 0; c < ch; ++c) {
                float v = static_cast<float>(p00[c]) * (1.0f - fx) * (1.0f - fy)
                        + static_cast<float>(p10[c]) * fx * (1.0f - fy)
                        + static_cast<float>(p01[c]) * (1.0f - fx) * fy
                        + static_cast<float>(p11[c]) * fx * fy;
                dstRow[x * ch + c] = static_cast<uint8_t>(std::clamp(v, 0.0f, 255.0f));
            }
        }
    }
}

} // namespace blur
