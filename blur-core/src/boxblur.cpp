#include "boxblur.h"
#include "thread_pool.h"
#include <algorithm>
#include <vector>
#include <future>
#include <cmath>
#include <cstring>

namespace blur {

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

        int initEnd = std::min(radius, width - 1);
        for (int i = 0; i <= initEnd; ++i) {
            for (int c = 0; c < channels; ++c) {
                sum[c] += srcRow[i * channels + c];
            }
            count++;
        }

        for (int x = 0; x < width; ++x) {
            int newRight = x + radius;
            if (newRight < width) {
                const uint8_t* p = srcRow + newRight * channels;
                for (int c = 0; c < channels; ++c) sum[c] += p[c];
                count++;
            }

            int oldLeft = x - radius - 1;
            if (oldLeft >= 0) {
                const uint8_t* p = srcRow + oldLeft * channels;
                for (int c = 0; c < channels; ++c) sum[c] -= p[c];
                count--;
            }

            uint8_t* dstPixel = dstRow + x * channels;
            for (int c = 0; c < channels; ++c) {
                dstPixel[c] = static_cast<uint8_t>(std::clamp(sum[c] / count, 0, 255));
            }
        }
    }
}

static void boxBlurVertical(const Bitmap& src, Bitmap& dst, int radius,
                             int startCol, int endCol) {
    int width = src.width;
    int height = src.height;
    int channels = src.channels;

    for (int x = startCol; x < endCol; ++x) {
        int sum[4] = {0, 0, 0, 0};
        int count = 0;

        int initEnd = std::min(radius, height - 1);
        for (int i = 0; i <= initEnd; ++i) {
            const uint8_t* pixel = src.getRowConst(i) + x * channels;
            for (int c = 0; c < channels; ++c) sum[c] += pixel[c];
            count++;
        }

        for (int y = 0; y < height; ++y) {
            int newBottom = y + radius;
            if (newBottom < height) {
                const uint8_t* p = src.getRowConst(newBottom) + x * channels;
                for (int c = 0; c < channels; ++c) sum[c] += p[c];
                count++;
            }

            int oldTop = y - radius - 1;
            if (oldTop >= 0) {
                const uint8_t* p = src.getRowConst(oldTop) + x * channels;
                for (int c = 0; c < channels; ++c) sum[c] -= p[c];
                count--;
            }

            uint8_t* dstPixel = dst.getRow(y) + x * channels;
            for (int c = 0; c < channels; ++c) {
                dstPixel[c] = static_cast<uint8_t>(std::clamp(sum[c] / count, 0, 255));
            }
        }
    }
}

void boxBlur1D(const Bitmap& src, Bitmap& dst, int radius) {
    ThreadPool& pool = ThreadPool::instance();
    int numThreads = pool.threadCount();

    std::vector<std::future<void>> futures;

    int rowsPerThread = (src.height + numThreads - 1) / numThreads;
    for (int t = 0; t < numThreads; ++t) {
        int startRow = t * rowsPerThread;
        int endRow = std::min(startRow + rowsPerThread, src.height);
        if (startRow >= endRow) break;
        futures.emplace_back(pool.enqueue([&, startRow, endRow]() {
            boxBlurHorizontal(src, dst, radius, startRow, endRow);
        }));
    }
    for (auto& f : futures) f.get();
    futures.clear();

    int colsPerThread = (src.width + numThreads - 1) / numThreads;
    for (int t = 0; t < numThreads; ++t) {
        int startCol = t * colsPerThread;
        int endCol = std::min(startCol + colsPerThread, src.width);
        if (startCol >= endCol) break;
        futures.emplace_back(pool.enqueue([&, startCol, endCol]() {
            boxBlurVertical(src, dst, radius, startCol, endCol);
        }));
    }
    for (auto& f : futures) f.get();
}

void boxBlur3Pass(Bitmap& bitmap, int gaussianRadius) {
    if (!bitmap.isValid()) return;

    int boxRadius = boxRadiusFromGaussian(gaussianRadius);
    if (boxRadius <= 0) return;

    int w = bitmap.width;
    int h = bitmap.height;
    int ch = bitmap.channels;

    std::vector<uint8_t> tempBuffer(static_cast<size_t>(w) * h * ch);
    Bitmap temp;
    temp.pixels = tempBuffer.data();
    temp.width = w;
    temp.height = h;
    temp.stride = w * ch;
    temp.channels = ch;

    boxBlur1D(bitmap, temp, boxRadius);
    boxBlur1D(temp, bitmap, boxRadius);
    boxBlur1D(bitmap, temp, boxRadius);

    std::memcpy(bitmap.pixels, temp.pixels, bitmap.totalBytes());
}

} // namespace blur
