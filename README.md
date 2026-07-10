# react-native-blur

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

- Separable Gaussian blur (horizontal + vertical 1D passes)
- ARM NEON SIMD intrinsics for ARM64/ARMv7
- Multithreaded processing via thread pool
- Zero-copy where possible (operates directly on Bitmap pixels)
- Android Bitmap capture in Kotlin, blur in C++
- GoogleTest unit tests
- Google Benchmark performance tracking
- AddressSanitizer for memory safety
- CI/CD with GitHub Actions

## Installation

```bash
npm install react-native-blur
```

## Usage

### BlurView Component

```tsx
import { BlurView } from 'react-native-blur';

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
import { NativeBlur } from 'react-native-blur';

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

Approximate benchmarks on ARM64 (Pixel 7):

| Resolution | Radius | Time (scalar) | Time (NEON) |
|------------|--------|---------------|-------------|
| 256x256    | 5      | 2.1 ms        | 1.2 ms      |
| 512x512    | 5      | 8.5 ms        | 4.8 ms      |
| 1024x1024  | 5      | 34 ms         | 18 ms       |
| 1024x1024  | 15     | 95 ms         | 52 ms       |

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
4. **Android Build** + **Instrumentation Tests**
5. **TypeScript Type Check**

## Roadmap

- [x] v1: Gaussian Blur in C++
- [x] v1.1: NEON SIMD optimization
- [x] v1.2: Multithreaded processing
- [x] v1.3: Unit tests + benchmarks + CI
- [ ] v2: Intelligent cache (skip recalculation on unchanged content)
- [ ] v3: GPU backend (Vulkan / OpenGL ES)
- [ ] v4: iOS support (Objective-C++ bridge)
- [ ] v5: Progressive blur (multi-pass)
- [ ] v6: Motion blur / directional blur

## License

MIT
