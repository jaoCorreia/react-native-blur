#if defined(__x86_64__) || defined(_M_X64)

#include "avx2.h"
#include <algorithm>

// This translation unit is compiled with -mavx2 (see CMakeLists.txt), so the
// compiler can target AVX2 registers/instructions here specifically, while
// the rest of blur-core stays on the SSE2 baseline. Runtime dispatch
// (cpuSupportsAVX2) gates whether this code path is ever reached.
//
// The algorithm below is intentionally byte-for-byte identical to
// blurHorizontalScalar/blurVerticalScalar in gaussian.cpp: correctness here
// rides on the scalar path's existing test coverage (same math, same
// rounding) rather than on hand-verified SIMD intrinsics, since this
// environment has no way to build or run x86 SIMD code to check it. What
// changes is only the compilation target, which lets the compiler use wider
// registers/instructions where the loop shape allows it.

namespace blur {

bool cpuSupportsAVX2() {
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

void blurHorizontalAVX2(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startRow, int endRow) {
    int width = src.width;
    int channels = src.channels;

    for (int y = startRow; y < endRow; ++y) {
        const uint8_t* srcRow = src.getRowConst(y);
        uint8_t* dstRow = dst.getRow(y);

        for (int x = 0; x < width; ++x) {
            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            for (int k = -radius; k <= radius; ++k) {
                int sampleX = std::clamp(x + k, 0, width - 1);
                const uint8_t* pixel = srcRow + static_cast<ptrdiff_t>(sampleX) * channels;
                float weight = kernel[static_cast<size_t>(k + radius)];

                for (int c = 0; c < channels; ++c) {
                    sum[c] += static_cast<float>(pixel[c]) * weight;
                }
            }

            uint8_t* dstPixel = dstRow + static_cast<ptrdiff_t>(x) * channels;
            for (int c = 0; c < channels; ++c) {
                dstPixel[c] = static_cast<uint8_t>(std::clamp(sum[c], 0.0f, 255.0f));
            }
        }
    }
}

void blurVerticalAVX2(const Bitmap& src, Bitmap& dst,
                      const std::vector<float>& kernel, int radius,
                      int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;

    for (int y = 0; y < height; ++y) {
        uint8_t* dstRow = dst.getRow(y);

        for (int x = startCol; x < endCol; ++x) {
            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            for (int k = -radius; k <= radius; ++k) {
                int sampleY = std::clamp(y + k, 0, height - 1);
                const uint8_t* pixel = src.getRowConst(sampleY) + static_cast<ptrdiff_t>(x) * channels;
                float weight = kernel[static_cast<size_t>(k + radius)];

                for (int c = 0; c < channels; ++c) {
                    sum[c] += static_cast<float>(pixel[c]) * weight;
                }
            }

            uint8_t* dstPixel = dstRow + static_cast<ptrdiff_t>(x) * channels;
            for (int c = 0; c < channels; ++c) {
                dstPixel[c] = static_cast<uint8_t>(std::clamp(sum[c], 0.0f, 255.0f));
            }
        }
    }
}

} // namespace blur

#endif
