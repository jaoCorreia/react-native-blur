#include "gaussian.h"
#include "kernel.h"
#include "cache.h"
#include "bitmap.h"
#include <benchmark/benchmark.h>
#include <vector>
#include <cstring>
#include <cstdint>

using namespace blur;

// Deterministic gradient so runs are reproducible and every pixel is distinct
// (defeats any accidental content-dependent shortcut in the pipeline).
static std::vector<uint8_t> makeGradient(int size) {
    std::vector<uint8_t> buf(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);
    for (int y = 0; y < size; ++y) {
        uint8_t* row = buf.data() + static_cast<size_t>(y) * static_cast<size_t>(size) * 4;
        for (int x = 0; x < size; ++x) {
            size_t off = static_cast<size_t>(x) * 4;
            row[off + 0] = static_cast<uint8_t>((x * 255) / size);
            row[off + 1] = static_cast<uint8_t>((y * 255) / size);
            row[off + 2] = static_cast<uint8_t>(((x + y) * 127) / size);
            row[off + 3] = 255;
        }
    }
    return buf;
}

static Bitmap wrap(std::vector<uint8_t>& buf, int size) {
    Bitmap bmp;
    bmp.pixels = buf.data();
    bmp.width = size;
    bmp.height = size;
    bmp.stride = size * 4;
    bmp.channels = 4;
    return bmp;
}

// Honest per-frame blur cost. gaussianBlur() mutates in place and consults a
// process-wide LRU cache, so a naive "blur the same buffer in a loop" measures
// a cache hit on every iteration after the first — not the blur. Here each
// iteration restores pristine (unblurred) input AND clears the cache, both
// with timing paused, so what's measured is one cold blur of fresh content —
// exactly what BlurView does per frame. PauseTiming overhead (~microseconds)
// is negligible against blur times of 0.1 ms and up.
static void BM_Blur(benchmark::State& state) {
    const int size = static_cast<int>(state.range(0));
    const int radius = static_cast<int>(state.range(1));

    const auto pristine = makeGradient(size);
    std::vector<uint8_t> work = pristine;
    Bitmap bmp = wrap(work, size);

    for (auto _ : state) {
        state.PauseTiming();
        std::memcpy(work.data(), pristine.data(), pristine.size());
        BlurCache::instance().clear();
        state.ResumeTiming();

        gaussianBlur(bmp, radius);
        benchmark::DoNotOptimize(bmp.pixels);
        benchmark::ClobberMemory();
    }

    // items = pixels (=> items_per_second is pixels/s, read as MP/s),
    // bytes = one full RGBA frame (=> bytes_per_second as a GB/s figure).
    const int64_t pixels = static_cast<int64_t>(size) * static_cast<int64_t>(size);
    state.SetItemsProcessed(state.iterations() * pixels);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(pristine.size()));
}
// Args are grouped to make each performance claim independently measurable
// (size, radius). Read the results as ratios within a group — those cancel
// the host CPU and stay true across machines; the absolute ms do not.
BENCHMARK(BM_Blur)
    // Gaussian path (radius <= 5), fixed radius, swept resolution:
    // expect ~O(N) growth in pixel count.
    ->Args({256, 5})
    ->Args({512, 5})
    ->Args({1024, 5})
    // Box path (radius > 5), fixed resolution, swept radius: box 3-pass is a
    // sliding window, so cost should stay ~flat as radius grows (the headline
    // "radius-independent" claim). radius 20/30 would trigger downscale under
    // method=Gaussian, but the default Auto path takes box full-res first.
    ->Args({1024, 6})
    ->Args({1024, 10})
    ->Args({1024, 15})
    ->Args({1024, 20})
    ->Args({1024, 30})
    // Box path, fixed radius, swept resolution: expect ~O(N) in pixel count.
    ->Args({256, 10})
    ->Args({512, 10})
    ->Args({2048, 10})
    ->Args({4096, 10})
    ->Unit(benchmark::kMillisecond)
    // The blur fans out over a thread pool; the calling thread mostly blocks,
    // so cpu_time measures ~nothing and wildly overstates throughput. Wall
    // time is the real per-frame latency — make it the reported metric.
    ->UseRealTime();

// Explicit cache-hit path: a static background (unchanged content) blurred
// repeatedly. Input is restored to the exact bytes that were cached, so every
// iteration is a genuine hit regardless of the hash — this isolates the cost
// of hashBitmap + lookup + copy-out, which is the real win when content is
// stable frame to frame.
static void BM_Blur_CacheHit(benchmark::State& state) {
    const int size = static_cast<int>(state.range(0));
    const int radius = static_cast<int>(state.range(1));

    const auto pristine = makeGradient(size);
    std::vector<uint8_t> work = pristine;
    Bitmap bmp = wrap(work, size);

    BlurCache::instance().clear();
    gaussianBlur(bmp, radius); // populate: key = hash(pristine)

    for (auto _ : state) {
        state.PauseTiming();
        std::memcpy(work.data(), pristine.data(), pristine.size());
        state.ResumeTiming();

        gaussianBlur(bmp, radius); // hash(pristine) -> hit
        benchmark::DoNotOptimize(bmp.pixels);
        benchmark::ClobberMemory();
    }

    const int64_t pixels = static_cast<int64_t>(size) * static_cast<int64_t>(size);
    state.SetItemsProcessed(state.iterations() * pixels);
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(pristine.size()));
}
BENCHMARK(BM_Blur_CacheHit)
    ->Args({512, 10})
    ->Args({1024, 15})
    ->Unit(benchmark::kMicrosecond)
    ->UseRealTime();

static void BM_KernelGeneration_Radius10(benchmark::State& state) {
    for (auto _ : state) {
        auto kernel = generateGaussianKernel(10);
        benchmark::DoNotOptimize(kernel.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_KernelGeneration_Radius10);
