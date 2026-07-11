# react-native-cpp-blur

[![npm version](https://img.shields.io/npm/v/react-native-cpp-blur)](https://www.npmjs.com/package/react-native-cpp-blur)
[![npm downloads](https://img.shields.io/npm/dm/react-native-cpp-blur)](https://www.npmjs.com/package/react-native-cpp-blur)
[![CI](https://github.com/jaoCorreia/react-native-blur/actions/workflows/ci.yml/badge.svg)](https://github.com/jaoCorreia/react-native-blur/actions/workflows/ci.yml)
[![license](https://img.shields.io/npm/l/react-native-cpp-blur)](LICENSE)

High-performance cross-platform Gaussian blur for React Native with C++/NEON SIMD optimizations.

Built for the New Architecture (TurboModules + Fabric). The blur algorithm runs in native C++ with ARM NEON SIMD acceleration on Android, multithreading via a custom thread pool, and a clean separable Gaussian implementation.

## Architecture

```
React Native (JS/TS)
        |
        v
TurboModule / Fabric (Kotlin)
        |
        v
JNI
        |
        v
C++ (blur-core)
        |
        +-- Gaussian Blur (separable, 2-pass)
        +-- SIMD / NEON (ARM64)
        +-- Thread Pool (std::thread)
        +-- Bitmap operations
```

- **blur-core/**: Pure C++ library with no Android dependencies. Reusable standalone.
- **android/**: JNI bridge + Kotlin TurboModule + Fabric View.
- **src/**: TypeScript API for React Native.

## Features

- **Separable Gaussian blur** (horizontal + vertical 1D passes) for O(n) complexity
- **ARM NEON SIMD intrinsics** - process 4 pixels per instruction on ARM64/ARMv7 (~1.8x speedup)
- **Multithreaded** - custom thread pool distributes rows/columns across all CPU cores
- **Zero-copy** - operates directly on Android Bitmap pixel buffers (no intermediate copies)
- **Consistent across all Android versions** (API 21+) - no RenderScript, no API level restrictions
- **GoogleTest** unit tests (29 tests) + **Google Benchmark** performance tracking
- **AddressSanitizer** validated (zero memory errors)
- **CI/CD** with GitHub Actions on every push

## Installation

```bash
npm install react-native-cpp-blur
```

## Usage

### BlurView Component

```tsx
import { BlurView } from 'react-native-cpp-blur';

function App() {
  return (
    <BlurView blurRadius={15} overlayColor="rgba(255,255,255,0.1)" style={{ flex: 1 }}>
      <YourContent />
    </BlurView>
  );
}
```

### Programmatic Blur

```tsx
import { NativeBlur } from 'react-native-cpp-blur';

await NativeBlur.blurImage('/path/to/input.png', '/path/to/output.png', 10, 2.5);
```

## Development

### Build C++ core

```bash
# Unit tests
cmake -B build -S blur-core -DBLUR_BUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure

# Benchmarks (Release is mandatory — Debug numbers are meaningless)
cmake -B build -S blur-core -DBLUR_BUILD_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Repetitions + aggregates so you get median/stddev, not one noisy sample.
./build/blur-bench --benchmark_min_time=0.2s \
  --benchmark_repetitions=10 --benchmark_report_aggregates_only=true

# With AddressSanitizer
cmake -B build -S blur-core -DBLUR_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build
```

### Use blur-core standalone (CMake)

`blur-core` has no React Native or Android dependency, so it can be installed and consumed by any CMake project via `find_package`, independent of this repo's RN/Gradle build:

```bash
cmake -B build -S blur-core -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /path/to/install
```

```cmake
# In a consumer's CMakeLists.txt
find_package(blur-core REQUIRED)
target_link_libraries(your-target PRIVATE blur::blur-core)
```

### Build Android

There's no committed Gradle wrapper in this repo, so use a Gradle install compatible with AGP 8.2 (CI pins 8.4):

```bash
gradle -p android assembleRelease
```

### Run Android instrumentation tests

```bash
cd android && ./gradlew connectedAndroidTest
```

## Performance

> **Read the ratios, not the milliseconds.** Absolute times depend entirely on the CPU — a phone, a CI runner and a laptop all differ by an order of magnitude, so no single ms number is "the" performance. What's stable across hardware is the *shape*: complexity and ratios. Those are documented here; for numbers on your own machine, run `npm run benchmark`.

### Algorithmic cost (hardware-independent)

Under the default `Auto` method, the algorithm is chosen by radius:

| Radius | Algorithm | Cost | Notes |
|--------|-----------|------|-------|
| ≤ 5 | Gaussian separable (NEON / AVX2) | O(N · r) | precise; SIMD-accelerated |
| > 5 | Box blur 3-pass | O(N), **radius-independent** | approximates Gaussian, error < 3% |

N = pixel count, r = radius. These are properties of the algorithms, so they hold on any CPU.

> A downscale path (2×/4×, `scale.cpp`) also exists but is **currently unreachable under the default `Auto` method**: for radius > 5 the box branch runs at full resolution and `return`s before the downscale code. It only executes under an explicit `BlurMethod::Gaussian` with radius ≥ 8 — which nothing in the JNI / RN bridge sets. Treat it as a latent optimization, not a live path.

### What gets measured

Two very different costs, which older tables conflated:

- **Cold blur** — one blur of fresh content, cache cleared. The per-frame cost `BlurView` pays whenever content changes.
- **Cache hit** — identical content re-blurred; only the FNV-1a hash + LRU lookup + copy-out runs. Applies only to a *static* background that doesn't change frame to frame.

The harness ([`blur-core/tests/benchmark_blur.cpp`](blur-core/tests/benchmark_blur.cpp)) restores pristine input and clears the cache each repetition (timing paused), reports wall-clock time over 10+ repetitions (median), and uses real-time because the blur fans out over a thread pool (so `cpu_time` would measure the blocked calling thread, not the work).

### Ratios (these transfer across machines)

Measured in a single run; each ratio cancels the host CPU, so it reproduces on other hardware even though the raw ms won't:

| Relationship | What it isolates | Result |
|--------------|------------------|--------|
| Cache hit vs cold blur | cost of unchanged content | **~60–90× cheaper** |
| Box radius 6 → 30 at fixed resolution | radius-independence of the box path | **~flat (within ±4%)** |
| 4× the pixels at fixed radius (mid sizes) | pixel-count scaling | **~4× the time (≈ linear)** |

Scaling degrades at the extremes: a fixed thread-pool overhead floor makes tiny images sub-linear, and memory bandwidth caps very large ones once a frame outgrows cache (4096² RGBA = 64 MB → super-linear).

### Absolute times

Not hand-written here, because a single machine's ms would just be another misleading table. CI regenerates them each run and uploads JSON artifacts:

- `benchmark-arm64` — `ubuntu-24.04-arm` runner, the **NEON path** phones actually use. A server-class ARM core is several times faster than a typical phone, so read it as a *relative* reference, not a phone budget.
- `benchmark-x86_64` — `ubuntu-latest`, the AVX2 path (emulator / desktop).

### Real phones (Firebase Test Lab)

> Sourced from the on-device benchmark added in [#2](https://github.com/jaoCorreia/react-native-blur/pull/2) (`scripts/testlab-benchmark.sh` + `android/src/androidTest/java/com/blur/BlurBenchmark.kt`) — merge that alongside this PR for the referenced files to exist on `main`.

Ran once via `scripts/testlab-benchmark.sh` + the on-device harness (`BlurBenchmark.kt`), which calls `NativeBlur.applyBlur` directly (bypassing `BlurView`'s API 31+ GPU path, so it always exercises the C++/NEON blur) using the same cold/cache-hit methodology as the C++ harness. This is a snapshot, not a live CI artifact — re-run the script for current numbers; don't treat these as pinned.

Box path, radius 10, cold blur, median of 11 runs:

| Device (tier) | 256² | 512² | 1024² | 2048² | Cache hit (512²) |
|---|---|---|---|---|---|
| Galaxy A06 (`SM-A065M`, budget) | 40.0 ms | 149.4 ms | 619.1 ms | 2304.4 ms | 0.36 ms (**~410×**) |
| Galaxy A53 5G (`SC-53C`, mid) | 32.0 ms | 104.3 ms | 377.1 ms | 1335.4 ms | 0.36 ms (**~290×**) |
| Pixel 9 Pro (`caiman`, flagship) | 39.8 ms | 67.1 ms | 249.3 ms | 930.2 ms | 0.06 ms (**~1080×**) |

Two things this reveals that the CI/laptop numbers couldn't:

- **The cache-hit ratio is bigger on phones than on desktop/CI (~290–1080× vs ~60–90×).** The cache-hit path is bandwidth-bound (hash + memcpy); the cold blur is compute-bound. Phone CPU cores are relatively weaker per-core than a desktop/server core, so the compute side gets relatively more expensive while the memcpy side doesn't — widening the gap. A static background benefits from the cache even more on a phone than these ratios' desktop origin would suggest.
- **The Gaussian(r5)-vs-Box(r6) ordering is device-dependent, not universal.** On the A53 and Pixel 9 Pro, Box radius 6 (446 ms, 276 ms) was *faster* than Gaussian radius 5 (689 ms, 317 ms) — box winning earlier than desktop measurements suggested. On the A06, it was the reverse (Box 6 = 751 ms > Gaussian 5 = 612 ms), matching the desktop finding. The `radius > 5 → box` threshold isn't uniformly pessimal or optimal across real hardware — retune it per measured crossover, not from one machine's numbers.

A smaller caveat: across all three devices, Box radius 6 (run first in the sequence) measured ~10–20% above radius 10/15 (run after), likely CPU frequency governor ramp-up under sustained load rather than an algorithmic effect — the "radius-independent" claim holds once warmed up, less cleanly for the very first call in a burst.

For your own hardware, `npm run benchmark` (desktop/laptop) or `scripts/testlab-benchmark.sh` (phones) are the source of truth.

### Comparison with other approaches (512×512, radius 10)

Order-of-magnitude only; other libraries' figures are estimates from their docs/issues, not measured here, and this library's own absolute time is hardware-specific (run the benchmark rather than trusting a README number).

| Library | Approx. time | Method | Android support | Notes |
|---------|--------------|--------|-----------------|-------|
| **react-native-cpp-blur (this)** | cold: O(N), radius-independent · cached: ~60–90× less | C++ + NEON/AVX2 + Box 3-pass | All versions (API 21+) | CPU-side, consistent across devices |
| `@react-native-community/blur` | ~15-25 ms (est.) | RenderScript | API 17-30 (deprecated) | Discontinued on API 31+ |
| `expo-blur` (best case) | ~2-5 ms (est.) | RenderEffect (GPU) | API 31+ only | Native Android blur shader |
| `expo-blur` (fallback) | ~50-100 ms (est.) | Bitmap downscale | API 21-30 | Heavy quality loss |
| `react-native-blur` (old) | ~20-35 ms (est.) | View snapshot hack | All versions | UI thread blocking |

**Key advantages of this library:**

- **Box Blur 3-pass** - O(N), radius-independent cost (measured flat from radius 6 to 30), so large radii don't get slower
- **NEON + AVX2 SIMD on the Gaussian path** - fixed-point NEON on ARM64, AVX2 (runtime-gated) on x86_64; the box path is scalar
- **Cache-friendly vertical sweeps** - y-major iteration with running column sums, L1 cache resident
- **Auto algorithm selection** - Gaussian (SIMD) for small radius, Box 3-pass for large radius where its radius-independent cost pays off
- **Work-stealing thread pool** - atomic counter, threads claim chunks dynamically
- **Buffer pool** - size-classed power-of-2 buckets, 64MB cap per thread, zero malloc after warmup
- **Blur cache** - FNV-1a hash + LRU with byte budget, thread-safe via `std::mutex`
- **Kernel cache** - Gaussian kernel cached by (radius, sigma), 3x faster generation
- **Android 12+ aware** - skips C++ blur when GPU RenderEffect is available, no double blur
- **29 unit tests** - GoogleTest covering kernel, Gaussian, box blur, cache
- **CI tests ARM NEON** - QEMU aarch64 cross-compile runs the full test suite for NEON path
- **Memory safe** - AddressSanitizer validated, zero heap-buffer-overflow, thread-safe cache

## Testing Strategy

### Unit Tests (C++ / GoogleTest)
- Gaussian kernel correctness (sum = 1.0, symmetry, center max)
- Blur edge cases (zero radius, single pixel, solid color)
- Pixel value range (0-255 bounds)
- Dimension preservation
- Small bitmap handling

### Performance Benchmarks (Google Benchmark)
- Multiple resolutions: 256, 512, 1024, 2048, 4096
- Multiple radii: 5, 8, 10, 15
- Tracks regressions on every PR

### Memory Safety (AddressSanitizer)
- Leak detection
- Stack use-after-return
- Buffer overflow

### Android Instrumentation Tests (JUnit)
- JNI crash safety
- Memory pressure (10x consecutive blur)
- Dimension preservation
- Alpha channel integrity
- Different radii and sigma values

## CI/CD (GitHub Actions)

On every push and PR:

1. **C++ Unit Tests** (Debug + Release) — 29 tests
2. **C++ Benchmarks** — native x86_64 (AVX2) **and** arm64 (NEON) runners; results uploaded as JSON artifacts with median/stddev over 10 repetitions
3. **Benchmark Regression** (PR only) — builds and runs the benchmark on both the base commit and the PR, then runs Google Benchmark's `compare.py` (Mann-Whitney U-test) and posts the diff to the PR job summary. Informational, not a hard gate — perf on shared runners is too noisy to fail a build on.
4. **Address Sanitizer** (memory safety)
5. **CMake install() / find_package()** — installs the package to a throwaway prefix and builds/runs a standalone consumer against it
6. **aarch64 QEMU** — cross-compiles + runs full test suite for NEON path
7. **Android Gradle Build** — compiles Kotlin + cross-compiles blur-core for all 4 ABIs via NDK
8. **TypeScript Type Check**

All jobs run in parallel. The NEON path is compiled and tested on every commit.

## Roadmap

- [x] v1: Gaussian Blur in C++
- [x] v1.1: NEON SIMD optimization (float)
- [x] v1.2: Multithreaded processing
- [x] v1.3: Unit tests + benchmarks + CI
- [x] v2: Auto-downscaling (2x/4x), fixed-point NEON (int32 MAC), blur cache
- [x] v2.1: NEON 4-wide (2 pixels/iteration, branchless interior)
- [x] v2.2: Box Blur 3-pass (O(1) per pixel, radius-independent)
- [x] v2.3: AVX2 path + cache-friendly vertical sweeps + buffer pool
- [ ] v3: GPU backend (Vulkan / OpenGL ES)
- [ ] v4: iOS support (Objective-C++ bridge)
- [ ] v5: Progressive blur (multi-pass)
- [ ] v6: Motion blur / directional blur

## License

MIT
