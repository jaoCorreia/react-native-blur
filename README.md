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
- **GoogleTest** unit tests (18 tests) + **Google Benchmark** performance tracking
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

### Build Android

```bash
cd android && ./gradlew assembleRelease
```

### Run Android instrumentation tests

```bash
cd android && ./gradlew connectedAndroidTest
```

## Performance

### ARM64 NEON vs Scalar

NEON is **always enabled** on ARM64/ARMv7. The inner loop uses fixed-point integer math (`vmlal_s16`, kernel `int16`) — no float conversions. Interior pixels are processed in **pairs** (2 pixels per kernel iteration, single `vld1_u8` load, zero clamping branches).

| Resolution | Radius | Scalar C++ | NEON (ARM64) | Speedup |
|------------|--------|-----------|-------------|---------|
| 256x256    | 5      | 2.1 ms    | 1.2 ms      | 1.75x   |
| 512x512    | 5      | 8.5 ms    | 4.8 ms      | 1.77x   |
| 512x512    | 10     | 13.2 ms   | 7.1 ms      | 1.86x   |
| 1024x1024  | 5      | 34 ms     | 18 ms       | 1.89x   |
| 1024x1024  | 15     | 95 ms     | 52 ms       | 1.83x   |

*Measured on Pixel 7 (ARM64), Release build, multithreaded (8 cores).*

### x86\_64 Benchmarks (CI Runner)

| Resolution | Radius | Time  |
|------------|--------|-------|
| 256x256    | 5      | 0.87 ms |
| 512x512    | 5      | 2.96 ms |
| 1024x1024  | 5      | 11.5 ms |
| 1024x1024  | 15     | 24.2 ms |
| 2048x2048  | 8      | 59.5 ms |
| 4096x4096  | 8      | 316 ms  |

*Measured on GitHub Actions ubuntu-latest (x86\_64, 16 threads).*

### Comparison with other libraries (512x512, radius 10)

| Library | Time (est.) | Method | Android Support | Notes |
|---------|------------|--------|----------------|-------|
| **react-native-blur (this)** | **4.8 ms** | C++ + NEON + Threads | All versions (API 21+) | Consistent across devices |
| `@react-native-community/blur` | ~15-25 ms | RenderScript | API 17-30 (deprecated) | Discontinued on API 31+ |
| `expo-blur` (best case) | ~2-5 ms | RenderEffect (GPU) | API 31+ only | Native Android blur shader |
| `expo-blur` (fallback) | ~50-100 ms | Bitmap downscale | API 21-30 | Heavy quality loss |
| `react-native-blur` (old) | ~20-35 ms | View snapshot hack | All versions | UI thread blocking |

**Key advantages of this library:**

- **Box Blur 3-pass** - O(1) per pixel, independent of radius. Auto-triggers for radius > 5. 512x512 R10 went from 4.9ms to **0.086ms** (57x faster).
- **Auto-downscaling** - 2x (radius >= 8), 4x (radius >= 20). 16x fewer pixels at worst case.
- **No API level restrictions** - C++ blur works identically from Android 5.0 (API 21) to latest
- **No RenderScript dependency** - RenderScript was [deprecated in API 31](https://developer.android.com/guide/topics/renderscript/migrate) and removed from newer devices
- **No UI thread blocking** - Multithreaded processing via custom thread pool
- **NEON SIMD fixed-point** - `vmlal_s16` integer MAC, 2-pixel interior batches, zero branches in hot path
- **Memory safe** - AddressSanitizer validated, zero heap-buffer-overflow
- **Blur cache** - FNV-1a hash, skips recalculation on identical input

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

1. **C++ Unit Tests** (Debug + Release)
2. **C++ Benchmarks**
3. **Address Sanitizer** (memory safety)
4. **TypeScript Type Check**

All jobs run in parallel. The C++ core is fully validated on every commit.

## Roadmap

- [x] v1: Gaussian Blur in C++
- [x] v1.1: NEON SIMD optimization (float)
- [x] v1.2: Multithreaded processing
- [x] v1.3: Unit tests + benchmarks + CI
- [x] v2: Auto-downscaling (2x/4x), fixed-point NEON (int32 MAC), blur cache
- [x] v2.1: NEON 4-wide (2 pixels/iteration, branchless interior)
- [x] v2.2: Box Blur 3-pass (O(1) per pixel, radius-independent, auto for R>5)
- [ ] v3: GPU backend (Vulkan / OpenGL ES)
- [ ] v4: iOS support (Objective-C++ bridge)
- [ ] v5: Progressive blur (multi-pass)
- [ ] v6: Motion blur / directional blur

## License

MIT
