#include "gaussian.h"
#include "kernel.h"
#include "thread_pool.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <memory>

namespace blur {

void blurHorizontalScalar(const Bitmap& src, Bitmap& dst,
                          const std::vector<float>& kernel, int radius,
                          int startRow, int endRow) {
    int width = src.width;
    int channels = src.channels;
    int kernelSize = static_cast<int>(kernel.size());

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
                dstPixel[c] = static_cast<uint8_t>(
                    std::clamp(sum[c], 0.0f, 255.0f)
                );
            }
        }
    }
}

void blurVerticalScalar(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;

    for (int x = startCol; x < endCol; ++x) {
        for (int y = 0; y < height; ++y) {
            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};

            for (int k = -radius; k <= radius; ++k) {
                int sampleY = std::clamp(y + k, 0, height - 1);
                const uint8_t* pixel = src.getRowConst(sampleY)
                                     + static_cast<ptrdiff_t>(x) * channels;
                float weight = kernel[static_cast<size_t>(k + radius)];

                for (int c = 0; c < channels; ++c) {
                    sum[c] += static_cast<float>(pixel[c]) * weight;
                }
            }

            uint8_t* dstPixel = dst.getRow(y) + static_cast<ptrdiff_t>(x) * channels;
            for (int c = 0; c < channels; ++c) {
                dstPixel[c] = static_cast<uint8_t>(
                    std::clamp(sum[c], 0.0f, 255.0f)
                );
            }
        }
    }
}

void blurHorizontal(const Bitmap& src, Bitmap& dst,
                    const std::vector<float>& kernel, int radius) {
    ThreadPool& pool = ThreadPool::instance();
    int height = src.height;
    int numThreads = pool.threadCount();
    int rowsPerThread = (height + numThreads - 1) / numThreads;

    std::vector<std::future<void>> futures;
    futures.reserve(static_cast<size_t>(numThreads));

    for (int t = 0; t < numThreads; ++t) {
        int startRow = t * rowsPerThread;
        int endRow = std::min(startRow + rowsPerThread, height);
        if (startRow >= endRow) break;

        futures.emplace_back(pool.enqueue([&, startRow, endRow]() {
#if defined(__ARM_NEON__) || defined(__aarch64__)
            blurHorizontalNEON(src, dst, kernel, radius, startRow, endRow);
#else
            blurHorizontalScalar(src, dst, kernel, radius, startRow, endRow);
#endif
        }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

void blurVertical(const Bitmap& src, Bitmap& dst,
                  const std::vector<float>& kernel, int radius) {
    ThreadPool& pool = ThreadPool::instance();
    int width = src.width;
    int numThreads = pool.threadCount();
    int colsPerThread = (width + numThreads - 1) / numThreads;

    std::vector<std::future<void>> futures;
    futures.reserve(static_cast<size_t>(numThreads));

    for (int t = 0; t < numThreads; ++t) {
        int startCol = t * colsPerThread;
        int endCol = std::min(startCol + colsPerThread, width);
        if (startCol >= endCol) break;

        futures.emplace_back(pool.enqueue([&, startCol, endCol]() {
#if defined(__ARM_NEON__) || defined(__aarch64__)
            blurVerticalNEON(src, dst, kernel, radius, startCol, endCol);
#else
            blurVerticalScalar(src, dst, kernel, radius, startCol, endCol);
#endif
        }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

void gaussianBlur(Bitmap& bitmap, int radius, float sigma) {
    if (!bitmap.isValid() || radius <= 0) return;

    auto kernel = generateGaussianKernel(radius, sigma);

    int width = bitmap.width;
    int height = bitmap.height;
    int channels = bitmap.channels;

    std::vector<uint8_t> tempBuffer(
        static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels)
    );

    Bitmap temp;
    temp.pixels = tempBuffer.data();
    temp.width = width;
    temp.height = height;
    temp.stride = width * channels;
    temp.channels = channels;

    blurHorizontal(bitmap, temp, kernel, radius);
    blurVertical(temp, bitmap, kernel, radius);
}

} // namespace blur
