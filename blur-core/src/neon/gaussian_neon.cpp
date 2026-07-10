#if defined(__ARM_NEON__) || defined(__aarch64__)

#include "gaussian.h"
#include <arm_neon.h>
#include <algorithm>

namespace blur {

void blurHorizontalNEON(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startRow, int endRow) {
    int width = src.width;
    int channels = src.channels;

    for (int y = startRow; y < endRow; ++y) {
        const uint8_t* srcRow = src.getRowConst(y);
        uint8_t* dstRow = dst.getRow(y);

        for (int x = 0; x < width; ++x) {
            float32x4_t sum = vdupq_n_f32(0.0f);

            for (int k = -radius; k <= radius; ++k) {
                int sampleX = std::min(std::max(x + k, 0), width - 1);
                const uint8_t* pixel = srcRow
                    + static_cast<ptrdiff_t>(sampleX) * channels;

                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                uint32x4_t px32 = vmovl_u16(px16low);
                float32x4_t pxf = vcvtq_f32_u32(px32);

                float32x4_t w = vdupq_n_f32(kernel[static_cast<size_t>(k + radius)]);
                sum = vmlaq_f32(sum, pxf, w);
            }

            uint32x4_t result32 = vcvtq_u32_f32(sum);
            uint16x4_t result16 = vmovn_u32(result32);
            uint16x8_t result16x8 = vcombine_u16(result16, vdup_n_u16(0));
            uint8x8_t result8 = vmovn_u16(result16x8);

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

    for (int x = startCol; x < endCol; ++x) {
        for (int y = 0; y < height; ++y) {
            float32x4_t sum = vdupq_n_f32(0.0f);

            for (int k = -radius; k <= radius; ++k) {
                int sampleY = std::min(std::max(y + k, 0), height - 1);
                const uint8_t* pixel = src.getRowConst(sampleY)
                    + static_cast<ptrdiff_t>(x) * channels;

                uint8x8_t px8 = vld1_u8(pixel);
                uint16x8_t px16 = vmovl_u8(px8);
                uint16x4_t px16low = vget_low_u16(px16);
                uint32x4_t px32 = vmovl_u16(px16low);
                float32x4_t pxf = vcvtq_f32_u32(px32);

                float32x4_t w = vdupq_n_f32(kernel[static_cast<size_t>(k + radius)]);
                sum = vmlaq_f32(sum, pxf, w);
            }

            uint32x4_t result32 = vcvtq_u32_f32(sum);
            uint16x4_t result16 = vmovn_u32(result32);
            uint16x8_t result16x8 = vcombine_u16(result16, vdup_n_u16(0));
            uint8x8_t result8 = vmovn_u16(result16x8);

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
