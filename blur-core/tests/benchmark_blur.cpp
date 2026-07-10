#include "gaussian.h"
#include "bitmap.h"
#include <benchmark/benchmark.h>
#include <vector>
#include <algorithm>

using namespace blur;

static std::vector<uint8_t> gBuffer;

static Bitmap createTestBitmap(int size) {
    gBuffer.resize(static_cast<size_t>(size) * static_cast<size_t>(size) * 4);

    Bitmap bmp;
    bmp.pixels = gBuffer.data();
    bmp.width = size;
    bmp.height = size;
    bmp.stride = size * 4;
    bmp.channels = 4;

    for (int y = 0; y < size; ++y) {
        uint8_t* row = bmp.getRow(y);
        for (int x = 0; x < size; ++x) {
            size_t off = static_cast<size_t>(x) * 4;
            row[off + 0] = static_cast<uint8_t>((x * 255) / size);
            row[off + 1] = static_cast<uint8_t>((y * 255) / size);
            row[off + 2] = static_cast<uint8_t>(((x + y) * 127) / size);
            row[off + 3] = 255;
        }
    }
    return bmp;
}

static void BM_GaussianBlur_256_Radius5(benchmark::State& state) {
    auto bmp = createTestBitmap(256);
    for (auto _ : state) {
        gaussianBlur(bmp, 5);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_256_Radius5)->Unit(benchmark::kMillisecond);

static void BM_GaussianBlur_512_Radius5(benchmark::State& state) {
    auto bmp = createTestBitmap(512);
    for (auto _ : state) {
        gaussianBlur(bmp, 5);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_512_Radius5)->Unit(benchmark::kMillisecond);

static void BM_GaussianBlur_512_Radius10(benchmark::State& state) {
    auto bmp = createTestBitmap(512);
    for (auto _ : state) {
        gaussianBlur(bmp, 10);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_512_Radius10)->Unit(benchmark::kMillisecond);

static void BM_GaussianBlur_1024_Radius5(benchmark::State& state) {
    auto bmp = createTestBitmap(1024);
    for (auto _ : state) {
        gaussianBlur(bmp, 5);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_1024_Radius5)->Unit(benchmark::kMillisecond);

static void BM_GaussianBlur_1024_Radius15(benchmark::State& state) {
    auto bmp = createTestBitmap(1024);
    for (auto _ : state) {
        gaussianBlur(bmp, 15);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_1024_Radius15)->Unit(benchmark::kMillisecond);

static void BM_GaussianBlur_2048_Radius8(benchmark::State& state) {
    auto bmp = createTestBitmap(2048);
    for (auto _ : state) {
        gaussianBlur(bmp, 8);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_2048_Radius8)->Unit(benchmark::kMillisecond);

static void BM_GaussianBlur_4096_Radius8(benchmark::State& state) {
    auto bmp = createTestBitmap(4096);
    for (auto _ : state) {
        gaussianBlur(bmp, 8);
        benchmark::DoNotOptimize(bmp.pixels);
    }
}
BENCHMARK(BM_GaussianBlur_4096_Radius8)->Unit(benchmark::kMillisecond);

static void BM_KernelGeneration_Radius10(benchmark::State& state) {
    for (auto _ : state) {
        auto kernel = generateGaussianKernel(10);
        benchmark::DoNotOptimize(kernel.data());
    }
}
BENCHMARK(BM_KernelGeneration_Radius10);
