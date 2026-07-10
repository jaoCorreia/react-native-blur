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

# Benchmarks
cmake -B build -S blur-core -DBLUR_BUILD_BENCHMARKS=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/blur-bench

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

The library automatically selects the fastest algorithm:

| Radius | Method | Complexity | When |
|--------|--------|-----------|------|
| ≤ 5 | Gaussian separable + NEON | O(N × r) | Precise, small radius |
| > 5 | Box Blur 3-pass | O(N), **radius-independent** | Auto, loss < 3% |
| ≥ 8 | Downscale 2x/4x + Blur | O(N/4) | Reduces pixel count |

### Benchmarks (x86\_64, 4-core, AVX2)

| Resolution | Radius | Time | Method |
|------------|--------|------|--------|
| 256x256 | 5 | **0.005ms** | Gaussian + cache |
| 512x512 | 5 | **0.04ms** | Gaussian + cache |
| 512x512 | 10 | **0.05ms** | Box 3-pass + AVX2 |
| 1024x1024 | 5 | **0.17ms** | Gaussian + AVX2 |
| 1024x1024 | 15 | **0.17ms** | Box 3-pass + AVX2 |
| 2048x2048 | 8 | **0.95ms** | Box 3-pass + AVX2 |
| 4096x4096 | 8 | **4.07ms** | Box 3-pass + AVX2 |

*Measured on GitHub Actions ubuntu-latest (x86\_64, 4-core shared runner — noisy, treat as approximate). ARM64 (Pixel 7) is ~1.8x faster with NEON.*

### Speedup from v1.0 (first release)

| Resolution | Radius | v1.0 | v1.4.3 | Speedup |
|------------|--------|------|--------|---------|
| 512x512 | 10 | 4.68ms | 0.05ms | **94x** |
| 1024x1024 | 15 | 24.2ms | 0.17ms | **142x** |
| 2048x2048 | 8 | 59.5ms | 0.95ms | **63x** |
| 4096x4096 | 8 | 316ms | 4.07ms | **78x** |

### Comparison with other libraries (512x512, radius 10)

| Library | Time (est.) | Method | Android Support | Notes |
|---------|------------|--------|----------------|-------|
| **react-native-cpp-blur (this)** | **0.05 ms** | C++ + AVX2/NEON + Box 3-pass | All versions (API 21+) | Consistent across devices |
| `@react-native-community/blur` | ~15-25 ms | RenderScript | API 17-30 (deprecated) | Discontinued on API 31+ |
| `expo-blur` (best case) | ~2-5 ms | RenderEffect (GPU) | API 31+ only | Native Android blur shader |
| `expo-blur` (fallback) | ~50-100 ms | Bitmap downscale | API 21-30 | Heavy quality loss |
| `react-native-blur` (old) | ~20-35 ms | View snapshot hack | All versions | UI thread blocking |

**Key advantages of this library:**

- **Box Blur 3-pass** - O(1) per pixel. 512x512 R10: **0.05ms** (was 4.9ms, 94x faster)
- **AVX2 + NEON dual SIMD** - AVX2 on x86_64, fixed-point NEON on ARM64. 2-6x universal speedup
- **Cache-friendly vertical sweeps** - y-major iteration with running column sums, L1 cache resident
- **Auto algorithm selection** - Gaussian for precision (R≤5), Box for speed (R>5), Downscale (R≥8)
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
2. **C++ Benchmarks**
3. **Address Sanitizer** (memory safety)
4. **CMake install() / find_package()** — installs the package to a throwaway prefix and builds/runs a standalone consumer against it
5. **aarch64 QEMU** — cross-compiles + runs full test suite for NEON path
6. **Android Gradle Build** — compiles Kotlin + cross-compiles blur-core for all 4 ABIs via NDK
7. **TypeScript Type Check**

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
