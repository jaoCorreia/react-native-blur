#include "boxblur.h"
#include "pool.h"
#include "thread_pool.h"
#include <algorithm>
#include <vector>
#include <future>
#include <cmath>
#include <cstring>
#include <cstdio>

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

        // Preload the window for the position just before x=0, i.e.
        // [-1-radius, -1+radius] clamped to [0, width-1] = [0, radius-1].
        // Using `radius` here (instead of `radius - 1`) double-counts
        // position `radius` itself, since the x=0 iteration below also adds
        // it via `newRight` — inflating the denominator by one for every
        // position in the row, not just near this edge.
        int initEnd = std::min(radius - 1, width - 1);
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

// y-major sweep: keeps a running sum per column so each row is read once,
// sequentially, instead of striding through the image once per column.
static void boxBlurVertical(const Bitmap& src, Bitmap& dst, int radius,
                             int startCol, int endCol) {
    int height = src.height;
    int channels = src.channels;
    int colCount = endCol - startCol;
    int rowWidth = colCount * channels;

    std::vector<int> colSum(static_cast<size_t>(rowWidth), 0);

    // See the matching comment in boxBlurHorizontal: this preloads the
    // window for y=-1, which is [0, radius-1], not [0, radius].
    int initEnd = std::min(radius - 1, height - 1);
    for (int i = 0; i <= initEnd; ++i) {
        const uint8_t* row = src.getRowConst(i) + static_cast<ptrdiff_t>(startCol) * channels;
        for (int x = 0; x < rowWidth; ++x) colSum[static_cast<size_t>(x)] += row[x];
    }
    int count = initEnd + 1;

    for (int y = 0; y < height; ++y) {
        int newBottom = y + radius;
        if (newBottom < height) {
            const uint8_t* row = src.getRowConst(newBottom) + static_cast<ptrdiff_t>(startCol) * channels;
            for (int x = 0; x < rowWidth; ++x) colSum[static_cast<size_t>(x)] += row[x];
            count++;
        }

        int oldTop = y - radius - 1;
        if (oldTop >= 0) {
            const uint8_t* row = src.getRowConst(oldTop) + static_cast<ptrdiff_t>(startCol) * channels;
            for (int x = 0; x < rowWidth; ++x) colSum[static_cast<size_t>(x)] -= row[x];
            count--;
        }

        uint8_t* dstRow = dst.getRow(y) + static_cast<ptrdiff_t>(startCol) * channels;
        for (int x = 0; x < rowWidth; ++x) {
            dstRow[x] = static_cast<uint8_t>(std::clamp(colSum[static_cast<size_t>(x)] / count, 0, 255));
        }
    }
}

void boxBlur1D(const Bitmap& src, Bitmap& dst, int radius) {
    ThreadPool& pool = ThreadPool::instance();
    int numThreads = pool.threadCount();

    // The vertical pass must read the horizontal pass's output, not src again,
    // so it needs its own scratch buffer rather than writing directly into dst.
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

    ScopedBuffer tempBuffer(static_cast<size_t>(w) * h * ch);
    Bitmap temp;
    temp.pixels = tempBuffer.get();
    temp.width = w;
    temp.height = h;
    temp.stride = w * ch;
    temp.channels = ch;

    boxBlur1D(bitmap, temp, boxRadius);
    fprintf(stderr, "[diag] after pass1 (in temp), center=%d, center+3=%d\n",
            temp.getRowConst(h/2)[static_cast<size_t>(w/2)*ch],
            temp.getRowConst(h/2)[static_cast<size_t>(w/2+3)*ch]);

    boxBlur1D(temp, bitmap, boxRadius);
    fprintf(stderr, "[diag] after pass2 (in bitmap), center=%d, center+3=%d\n",
            bitmap.getRowConst(h/2)[static_cast<size_t>(w/2)*ch],
            bitmap.getRowConst(h/2)[static_cast<size_t>(w/2+3)*ch]);

    boxBlur1D(bitmap, temp, boxRadius);
    fprintf(stderr, "[diag] after pass3 (in temp), center=%d, center+3=%d\n",
            temp.getRowConst(h/2)[static_cast<size_t>(w/2)*ch],
            temp.getRowConst(h/2)[static_cast<size_t>(w/2+3)*ch]);

    std::memcpy(bitmap.pixels, temp.pixels, bitmap.totalBytes());
}

} // namespace blur
