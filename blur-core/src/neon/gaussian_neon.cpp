#if defined(__ARM_NEON__) || defined(__aarch64__)

#include "gaussian.h"
#include <arm_neon.h>
#include <algorithm>
#include <vector>
#include <cstring>

namespace blur {

static void storePixel(uint8_t* dst, uint8x8_t result8) {
    vst1_lane_u8(dst,     result8, 0);
    vst1_lane_u8(dst + 1, result8, 1);
    vst1_lane_u8(dst + 2, result8, 2);
    vst1_lane_u8(dst + 3, result8, 3);
}

// A plain vld1_u8 always reads 8 bytes (2 RGBA pixels). That's fine when both
// pixels are known to be in-bounds, but edge taps only need 1 valid pixel —
// reading the other 4 bytes can walk past the end of the last row of the
// buffer. This reads exactly 4 bytes via memcpy (safe, no alignment/strict-
// aliasing assumptions) and duplicates them into both lanes of a u8x8; only
// the low lanes are ever read back out by callers of this helper.
static uint8x8_t loadSinglePixel(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return vreinterpret_u8_u32(vdup_n_u32(v));
}

void blurHorizontalNEON(const Bitmap& src, Bitmap& dst,
                        const std::vector<int16_t>& kf, int radius,
                        int startRow, int endRow) {
    int width = src.width;
    int channels = src.channels;

    for (int y = startRow; y < endRow; ++y) {
        const uint8_t* srcRow = src.getRowConst(y);
        uint8_t* dstRow = dst.getRow(y);

        int leftEdge = std::min(radius, width);
        for (int x = 0; x < leftEdge; ++x) {
            int32x4_t sum = vdupq_n_s32(0);
            for (int k = -radius; k <= radius; ++k) {
                int sx = std::min(std::max(x + k, 0), width - 1);
                const uint8_t* pixel = srcRow + static_cast<ptrdiff_t>(sx) * channels;
                int16x8_t px16 = vreinterpretq_s16_u16(vmovl_u8(loadSinglePixel(pixel)));
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, vget_low_s16(px16), w);
            }
            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
            storePixel(dstRow + static_cast<ptrdiff_t>(x) * channels, result8);
        }

        int interiorStart = radius;
        int interiorEnd = width - radius;
        if (interiorEnd > interiorStart) {
            int pairs = (interiorEnd - interiorStart) / 2;
            for (int p = 0; p < pairs; ++p) {
                int x0 = interiorStart + p * 2;
                int32x4_t sum0 = vdupq_n_s32(0);
                int32x4_t sum1 = vdupq_n_s32(0);

                for (int k = -radius; k <= radius; ++k) {
                    // x0+k and x0+k+1 are both valid columns of this row (the
                    // pair loop never runs past interiorEnd), so this 8-byte
                    // load covers exactly 2 real, adjacent pixels: one feeds
                    // output x0's tap k, the other feeds output x0+1's tap k
                    // (whose window is shifted by exactly one column).
                    const uint8_t* pixel = srcRow + static_cast<ptrdiff_t>(x0 + k) * channels;
                    int16x8_t px16 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(pixel)));
                    int16x4_t w_v = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);

                    sum0 = vmlal_s16(sum0, vget_low_s16(px16), w_v);
                    sum1 = vmlal_s16(sum1, vget_high_s16(px16), w_v);
                }

                int16x4_t r0 = vqrshrn_n_s32(sum0, 8);
                uint8x8_t r0_8 = vqmovun_s16(vcombine_s16(r0, vdup_n_s16(0)));
                storePixel(dstRow + static_cast<ptrdiff_t>(x0) * channels, r0_8);

                int16x4_t r1 = vqrshrn_n_s32(sum1, 8);
                uint8x8_t r1_8 = vqmovun_s16(vcombine_s16(r1, vdup_n_s16(0)));
                storePixel(dstRow + static_cast<ptrdiff_t>(x0 + 1) * channels, r1_8);
            }

            if ((interiorEnd - interiorStart) % 2 == 1) {
                int x = interiorStart + pairs * 2;
                int32x4_t sum = vdupq_n_s32(0);
                for (int k = -radius; k <= radius; ++k) {
                    const uint8_t* pixel = srcRow + static_cast<ptrdiff_t>(x + k) * channels;
                    int16x8_t px16 = vreinterpretq_s16_u16(vmovl_u8(loadSinglePixel(pixel)));
                    int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                    sum = vmlal_s16(sum, vget_low_s16(px16), w);
                }
                int16x4_t result16 = vqrshrn_n_s32(sum, 8);
                uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
                storePixel(dstRow + static_cast<ptrdiff_t>(x) * channels, result8);
            }
        }

        int rightStart = std::max(radius, width - radius);
        for (int x = rightStart; x < width; ++x) {
            if (x < radius) continue;
            int32x4_t sum = vdupq_n_s32(0);
            for (int k = -radius; k <= radius; ++k) {
                int sx = std::min(std::max(x + k, 0), width - 1);
                const uint8_t* pixel = srcRow + static_cast<ptrdiff_t>(sx) * channels;
                int16x8_t px16 = vreinterpretq_s16_u16(vmovl_u8(loadSinglePixel(pixel)));
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, vget_low_s16(px16), w);
            }
            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
            storePixel(dstRow + static_cast<ptrdiff_t>(x) * channels, result8);
        }
    }
}

// y outer, columns inner: for a fixed y, the (2*radius+1) source rows
// touched by the k-loop stay resident in L1 across the whole column strip,
// since each row is re-approached only ~8 bytes later than last time. The
// previous column-major version (x outer, y inner) evicted every row from
// cache before it could be reused, since a full height-deep sweep happened
// between touches of the same row.
void blurVerticalNEON(const Bitmap& src, Bitmap& dst,
                      const std::vector<int16_t>& kf, int radius,
                      int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;
    int colCount = endCol - startCol;
    int pairs = colCount / 2;

    for (int y = 0; y < height; ++y) {
        uint8_t* dstRow = dst.getRow(y);

        for (int p = 0; p < pairs; ++p) {
            int x0 = startCol + p * 2;
            int32x4_t sum0 = vdupq_n_s32(0);
            int32x4_t sum1 = vdupq_n_s32(0);

            for (int k = -radius; k <= radius; ++k) {
                int sampleY = std::clamp(y + k, 0, height - 1);
                // x0 and x0+1 are both valid columns of row sampleY (the pair
                // loop never runs past endCol), so this is a genuine 2-pixel
                // load, not an over-read into the next row.
                const uint8_t* row = src.getRowConst(sampleY) + static_cast<ptrdiff_t>(x0) * channels;
                int16x8_t px16 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(row)));
                int16x4_t w_v = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);

                sum0 = vmlal_s16(sum0, vget_low_s16(px16), w_v);
                sum1 = vmlal_s16(sum1, vget_high_s16(px16), w_v);
            }

            int16x4_t r0 = vqrshrn_n_s32(sum0, 8);
            uint8x8_t r0_8 = vqmovun_s16(vcombine_s16(r0, vdup_n_s16(0)));
            storePixel(dstRow + static_cast<ptrdiff_t>(x0) * channels, r0_8);

            int16x4_t r1 = vqrshrn_n_s32(sum1, 8);
            uint8x8_t r1_8 = vqmovun_s16(vcombine_s16(r1, vdup_n_s16(0)));
            storePixel(dstRow + static_cast<ptrdiff_t>(x0 + 1) * channels, r1_8);
        }

        if (colCount % 2 == 1) {
            int x = startCol + pairs * 2;
            int32x4_t sum = vdupq_n_s32(0);
            for (int k = -radius; k <= radius; ++k) {
                int sampleY = std::clamp(y + k, 0, height - 1);
                const uint8_t* pixel = src.getRowConst(sampleY) + static_cast<ptrdiff_t>(x) * channels;
                int16x8_t px16 = vreinterpretq_s16_u16(vmovl_u8(loadSinglePixel(pixel)));
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, vget_low_s16(px16), w);
            }
            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
            storePixel(dstRow + static_cast<ptrdiff_t>(x) * channels, result8);
        }
    }
}

} // namespace blur

#endif // __ARM_NEON__ || __aarch64__
