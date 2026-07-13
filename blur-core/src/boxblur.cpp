#include "boxblur.h"
#include "pool.h"
#include "thread_pool.h"
#include <algorithm>
#include <vector>
#include <future>
#include <cmath>
#include <cstring>

#if defined(__SSE4_1__) || defined(__AVX2__)
#include <smmintrin.h>
#elif defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace blur {

static constexpr int PREFETCH_DIST = 4;

int boxRadiusFromGaussian(int gaussianRadius) {
    if (gaussianRadius <= 3) return 0;
    float sigma = static_cast<float>(gaussianRadius) / 3.0f;
    return std::max(1, static_cast<int>(std::round(sigma * 1.05f)));
}

static void boxBlurHorizontal(const Bitmap& src, Bitmap& dst, int radius,
                               int startRow, int endRow) {
    int width = src.width;
    int channels = src.channels;

    for (int y = startRow; y < endRow; ++y) {
        const uint8_t* srcRow = src.getRowConst(y);
        uint8_t* dstRow = dst.getRow(y);

        int sum[4] = {0, 0, 0, 0};
        int count = 0;

        int initEnd = std::min(radius - 1, width - 1);
        for (int i = 0; i <= initEnd; ++i) {
            const uint8_t* p = srcRow + static_cast<ptrdiff_t>(i) * channels;
            for (int c = 0; c < channels; ++c) sum[c] += p[c];
            count++;
        }

        for (int x = 0; x < width; ++x) {
            int newRight = x + radius;
            if (newRight < width) {
                const uint8_t* p = srcRow + static_cast<ptrdiff_t>(newRight) * channels;
#if defined(__SSE4_1__) || defined(__AVX2__)
                __m128i px = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*(const int32_t*)p));
                __m128i s = _mm_loadu_si128((const __m128i*)sum);
                _mm_storeu_si128((__m128i*)sum, _mm_add_epi32(s, px));
#elif defined(__ARM_NEON__) || defined(__aarch64__)
                uint8x8_t px8 = vld1_u8(p);
                int32x4_t px32 = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(vmovl_u8(px8))));
                int32x4_t s = vld1q_s32(sum);
                vst1q_s32(sum, vaddq_s32(s, px32));
#else
                for (int c = 0; c < channels; ++c) sum[c] += p[c];
#endif
                count++;
            }

            int oldLeft = x - radius - 1;
            if (oldLeft >= 0) {
                const uint8_t* p = srcRow + static_cast<ptrdiff_t>(oldLeft) * channels;
#if defined(__SSE4_1__) || defined(__AVX2__)
                __m128i px = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*(const int32_t*)p));
                __m128i s = _mm_loadu_si128((const __m128i*)sum);
                _mm_storeu_si128((__m128i*)sum, _mm_sub_epi32(s, px));
#elif defined(__ARM_NEON__) || defined(__aarch64__)
                uint8x8_t px8 = vld1_u8(p);
                int32x4_t px32 = vreinterpretq_s32_u32(vmovl_u16(vget_low_u16(vmovl_u8(px8))));
                int32x4_t s = vld1q_s32(sum);
                vst1q_s32(sum, vsubq_s32(s, px32));
#else
                for (int c = 0; c < channels; ++c) sum[c] -= p[c];
#endif
                count--;
            }

            uint8_t* dstPixel = dstRow + static_cast<ptrdiff_t>(x) * channels;
            for (int c = 0; c < channels; ++c)
                dstPixel[c] = static_cast<uint8_t>(std::clamp(sum[c] / count, 0, 255));
        }
    }
}

static void boxBlurVertical(const Bitmap& src, Bitmap& dst, int radius,
                             int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;
    int colCount = endCol - startCol;
    int rowWidth = colCount * channels;

    std::vector<int> colSum(static_cast<size_t>(rowWidth), 0);

    int initEnd = std::min(radius - 1, height - 1);
    for (int i = 0; i <= initEnd; ++i) {
        const uint8_t* row = src.getRowConst(i) + static_cast<ptrdiff_t>(startCol) * channels;
#if defined(__SSE4_1__) || defined(__AVX2__)
        for (int x = 0; x < rowWidth; x += channels) {
            __m128i px = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*(const int32_t*)(row + x)));
            __m128i s = _mm_loadu_si128((const __m128i*)(&colSum[static_cast<size_t>(x)]));
            _mm_storeu_si128((__m128i*)(&colSum[static_cast<size_t>(x)]), _mm_add_epi32(s, px));
        }
#else
        for (int x = 0; x < rowWidth; ++x)
            colSum[static_cast<size_t>(x)] += row[x];
#endif
    }
    int count = initEnd + 1;

    for (int y = 0; y < height; ++y) {
        int newBottom = y + radius;
        if (newBottom < height) {
            const uint8_t* row = src.getRowConst(newBottom) + static_cast<ptrdiff_t>(startCol) * channels;

#if defined(__SSE4_1__) || defined(__AVX2__)
            for (int x = 0; x < rowWidth; x += channels) {
                __m128i px = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*(const int32_t*)(row + x)));
                __m128i s = _mm_loadu_si128((const __m128i*)(&colSum[static_cast<size_t>(x)]));
                _mm_storeu_si128((__m128i*)(&colSum[static_cast<size_t>(x)]), _mm_add_epi32(s, px));
            }
#else
            for (int x = 0; x < rowWidth; ++x)
                colSum[static_cast<size_t>(x)] += row[x];
#endif
            count++;
        }

        int oldTop = y - radius - 1;
        if (oldTop >= 0) {
            const uint8_t* row = src.getRowConst(oldTop) + static_cast<ptrdiff_t>(startCol) * channels;
#if defined(__SSE4_1__) || defined(__AVX2__)
            for (int x = 0; x < rowWidth; x += channels) {
                __m128i px = _mm_cvtepu8_epi32(_mm_cvtsi32_si128(*(const int32_t*)(row + x)));
                __m128i s = _mm_loadu_si128((const __m128i*)(&colSum[static_cast<size_t>(x)]));
                _mm_storeu_si128((__m128i*)(&colSum[static_cast<size_t>(x)]), _mm_sub_epi32(s, px));
            }
#else
            for (int x = 0; x < rowWidth; ++x)
                colSum[static_cast<size_t>(x)] -= row[x];
#endif
            count--;
        }

        int nextFetchY = y + PREFETCH_DIST;
        if (nextFetchY < height) {
            const uint8_t* nextRow = src.getRowConst(nextFetchY) + static_cast<ptrdiff_t>(startCol) * channels;
            __builtin_prefetch(nextRow, 0, 0);
        }

        uint8_t* dstRow = dst.getRow(y) + static_cast<ptrdiff_t>(startCol) * channels;
        for (int x = 0; x < colCount; ++x) {
            size_t off = static_cast<size_t>(x) * static_cast<size_t>(channels);
            for (int c = 0; c < channels; ++c)
                dstRow[off + static_cast<size_t>(c)] = static_cast<uint8_t>(
                    std::clamp(colSum[off + static_cast<size_t>(c)] / count, 0, 255));
        }
    }
}

void boxBlur1D(const Bitmap& src, Bitmap& dst, int radius) {
    ThreadPool& pool = ThreadPool::instance();
    int numThreads = pool.threadCount();

    ScopedBuffer scratch(dst.totalBytes());
    Bitmap horizontalResult;
    horizontalResult.pixels = scratch.get();
    horizontalResult.width = dst.width;
    horizontalResult.height = dst.height;
    horizontalResult.stride = dst.stride;
    horizontalResult.channels = dst.channels;

    pool.parallelFor(src.height, std::max(1, src.height / (numThreads * 4)),
                     [&](int startRow, int endRow) {
        boxBlurHorizontal(src, horizontalResult, radius, startRow, endRow);
    });

    pool.parallelFor(src.width, std::max(1, src.width / (numThreads * 4)),
                     [&](int startCol, int endCol) {
        boxBlurVertical(horizontalResult, dst, radius, startCol, endCol);
    });
}

void boxBlur3Pass(Bitmap& bitmap, int gaussianRadius) {
    if (!bitmap.isValid()) return;

    int boxRadius = boxRadiusFromGaussian(gaussianRadius);
    if (boxRadius <= 0) return;

    int w = bitmap.width;
    int h = bitmap.height;
    int ch = bitmap.channels;
    size_t frameBytes = static_cast<size_t>(w) * static_cast<size_t>(h) * static_cast<size_t>(ch);

    ScopedBuffer tempBuffer(frameBytes);
    Bitmap temp;
    temp.pixels = tempBuffer.get();
    temp.width = w;
    temp.height = h;
    temp.stride = w * ch;
    temp.channels = ch;

    boxBlur1D(bitmap, temp, boxRadius);
    boxBlur1D(temp, bitmap, boxRadius);
    boxBlur1D(bitmap, temp, boxRadius);

    std::memcpy(bitmap.pixels, temp.pixels, frameBytes);
}

} // namespace blur
