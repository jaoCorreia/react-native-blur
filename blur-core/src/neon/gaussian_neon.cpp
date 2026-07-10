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
                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                int16x4_t px16s = vreinterpret_s16_u16(px16low);
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, px16s, w);
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
                    const uint8_t* src = srcRow + static_cast<ptrdiff_t>(x0 + k) * channels;
                    uint8x8_t px2 = vld1_u8(src);
                    uint16x8_t px16 = vmovl_u8(px2);
                    uint16x4_t px0 = vget_low_u16(px16);
                    int16x4_t px0_s = vreinterpret_s16_u16(px0);

                    int16_t w = kf[static_cast<size_t>(k + radius)];
                    int16x4_t w_v = vdup_n_s16(w);
                    sum0 = vmlal_s16(sum0, px0_s, w_v);

                    uint8x8_t px1_shifted = vext_u8(px2, vdup_n_u8(0), 4);
                    uint16x8_t px1_16 = vmovl_u8(px1_shifted);
                    uint16x4_t px1 = vget_low_u16(px1_16);
                    int16x4_t px1_s = vreinterpret_s16_u16(px1);
                    sum1 = vmlal_s16(sum1, px1_s, w_v);
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
                    uint8x8_t px8 = vld1_u8(pixel);
                    uint16x8_t px16 = vmovl_u8(px8);
                    uint16x4_t px16low = vget_low_u16(px16);
                    int16x4_t px16s = vreinterpret_s16_u16(px16low);
                    int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                    sum = vmlal_s16(sum, px16s, w);
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
                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                int16x4_t px16s = vreinterpret_s16_u16(px16low);
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, px16s, w);
            }
            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
            storePixel(dstRow + static_cast<ptrdiff_t>(x) * channels, result8);
        }
    }
}

void blurVerticalNEON(const Bitmap& src, Bitmap& dst,
                      const std::vector<int16_t>& kf, int radius,
                      int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;

    for (int x = startCol; x < endCol; ++x) {
        int topEdge = std::min(radius, height);
        for (int y = 0; y < topEdge; ++y) {
            int32x4_t sum = vdupq_n_s32(0);
            for (int k = -radius; k <= radius; ++k) {
                int sy = std::min(std::max(y + k, 0), height - 1);
                const uint8_t* pixel = src.getRowConst(sy) + static_cast<ptrdiff_t>(x) * channels;
                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                int16x4_t px16s = vreinterpret_s16_u16(px16low);
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, px16s, w);
            }
            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
            storePixel(dst.getRow(y) + static_cast<ptrdiff_t>(x) * channels, result8);
        }

        int interiorStart = radius;
        int interiorEnd = height - radius;
        if (interiorEnd > interiorStart) {
            int pairs = (interiorEnd - interiorStart) / 2;
            for (int p = 0; p < pairs; ++p) {
                int y0 = interiorStart + p * 2;
                int32x4_t sum0 = vdupq_n_s32(0);
                int32x4_t sum1 = vdupq_n_s32(0);

                for (int k = -radius; k <= radius; ++k) {
                    const uint8_t* row0 = src.getRowConst(y0 + k);
                    const uint8_t* row1 = src.getRowConst(y0 + 1 + k);
                    uint8x8_t px0 = vld1_u8(row0 + static_cast<ptrdiff_t>(x) * channels);
                    uint8x8_t px1 = vld1_u8(row1 + static_cast<ptrdiff_t>(x) * channels);

                    uint16x8_t px0_16 = vmovl_u8(px0);
                    uint16x4_t px0_rgba = vget_low_u16(px0_16);
                    int16x4_t px0_s = vreinterpret_s16_u16(px0_rgba);

                    uint16x8_t px1_16 = vmovl_u8(px1);
                    uint16x4_t px1_rgba = vget_low_u16(px1_16);
                    int16x4_t px1_s = vreinterpret_s16_u16(px1_rgba);

                    int16_t w = kf[static_cast<size_t>(k + radius)];
                    int16x4_t w_v = vdup_n_s16(w);
                    sum0 = vmlal_s16(sum0, px0_s, w_v);
                    sum1 = vmlal_s16(sum1, px1_s, w_v);
                }

                int16x4_t r0 = vqrshrn_n_s32(sum0, 8);
                uint8x8_t r0_8 = vqmovun_s16(vcombine_s16(r0, vdup_n_s16(0)));
                storePixel(dst.getRow(y0) + static_cast<ptrdiff_t>(x) * channels, r0_8);

                int16x4_t r1 = vqrshrn_n_s32(sum1, 8);
                uint8x8_t r1_8 = vqmovun_s16(vcombine_s16(r1, vdup_n_s16(0)));
                storePixel(dst.getRow(y0 + 1) + static_cast<ptrdiff_t>(x) * channels, r1_8);
            }

            if ((interiorEnd - interiorStart) % 2 == 1) {
                int y = interiorStart + pairs * 2;
                int32x4_t sum = vdupq_n_s32(0);
                for (int k = -radius; k <= radius; ++k) {
                    const uint8_t* pixel = src.getRowConst(y + k) + static_cast<ptrdiff_t>(x) * channels;
                    uint8x8_t px8 = vld1_u8(pixel);
                    uint16x8_t px16 = vmovl_u8(px8);
                    uint16x4_t px16low = vget_low_u16(px16);
                    int16x4_t px16s = vreinterpret_s16_u16(px16low);
                    int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                    sum = vmlal_s16(sum, px16s, w);
                }
                int16x4_t result16 = vqrshrn_n_s32(sum, 8);
                uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
                storePixel(dst.getRow(y) + static_cast<ptrdiff_t>(x) * channels, result8);
            }
        }

        int bottomStart = std::max(radius, height - radius);
        for (int y = bottomStart; y < height; ++y) {
            if (y < radius) continue;
            int32x4_t sum = vdupq_n_s32(0);
            for (int k = -radius; k <= radius; ++k) {
                int sy = std::min(std::max(y + k, 0), height - 1);
                const uint8_t* pixel = src.getRowConst(sy) + static_cast<ptrdiff_t>(x) * channels;
                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                int16x4_t px16s = vreinterpret_s16_u16(px16low);
                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, px16s, w);
            }
            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));
            storePixel(dst.getRow(y) + static_cast<ptrdiff_t>(x) * channels, result8);
        }
    }
}

} // namespace blur

#endif // __ARM_NEON__ || __aarch64__
