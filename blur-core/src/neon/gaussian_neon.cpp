#if defined(__ARM_NEON__) || defined(__aarch64__)

#include "gaussian.h"
#include <arm_neon.h>
#include <algorithm>
#include <vector>

namespace blur {

static std::vector<int16_t> precomputeKernelFixed(const std::vector<float>& kernel) {
    std::vector<int16_t> kf(kernel.size());
    for (size_t i = 0; i < kernel.size(); ++i) {
        kf[i] = static_cast<int16_t>(kernel[i] * 256.0f + 0.5f);
    }
    return kf;
}

void blurHorizontalNEON(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startRow, int endRow) {
    int width = src.width;
    int channels = src.channels;
    auto kf = precomputeKernelFixed(kernel);

    for (int y = startRow; y < endRow; ++y) {
        const uint8_t* srcRow = src.getRowConst(y);
        uint8_t* dstRow = dst.getRow(y);

        for (int x = 0; x < width; ++x) {
            int32x4_t sum = vdupq_n_s32(0);

            for (int k = -radius; k <= radius; ++k) {
                int sampleX = std::min(std::max(x + k, 0), width - 1);
                const uint8_t* pixel = srcRow
                    + static_cast<ptrdiff_t>(sampleX) * channels;

                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                int16x4_t px16s = vreinterpret_s16_u16(px16low);

                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, px16s, w);
            }

            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));

            uint8_t* dstPixel = dstRow + static_cast<ptrdiff_t>(x) * channels;
            vst1_lane_u8(dstPixel,     result8, 0);
            vst1_lane_u8(dstPixel + 1, result8, 1);
            vst1_lane_u8(dstPixel + 2, result8, 2);
            vst1_lane_u8(dstPixel + 3, result8, 3);
        }
    }
}

void blurVerticalNEON(const Bitmap& src, Bitmap& dst,
                      const std::vector<float>& kernel, int radius,
                      int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;
    auto kf = precomputeKernelFixed(kernel);

    for (int x = startCol; x < endCol; ++x) {
        for (int y = 0; y < height; ++y) {
            int32x4_t sum = vdupq_n_s32(0);

            for (int k = -radius; k <= radius; ++k) {
                int sampleY = std::min(std::max(y + k, 0), height - 1);
                const uint8_t* pixel = src.getRowConst(sampleY)
                    + static_cast<ptrdiff_t>(x) * channels;

                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                int16x4_t px16s = vreinterpret_s16_u16(px16low);

                int16x4_t w = vdup_n_s16(kf[static_cast<size_t>(k + radius)]);
                sum = vmlal_s16(sum, px16s, w);
            }

            int16x4_t result16 = vqrshrn_n_s32(sum, 8);
            uint8x8_t result8 = vqmovun_s16(vcombine_s16(result16, vdup_n_s16(0)));

            uint8_t* dstPixel = dst.getRow(y)
                + static_cast<ptrdiff_t>(x) * channels;
            vst1_lane_u8(dstPixel,     result8, 0);
            vst1_lane_u8(dstPixel + 1, result8, 1);
            vst1_lane_u8(dstPixel + 2, result8, 2);
            vst1_lane_u8(dstPixel + 3, result8, 3);
        }
    }
}

} // namespace blur

#endif // __ARM_NEON__ || __aarch64__
