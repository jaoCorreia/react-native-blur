# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

`react-native-cpp-blur`: high-performance Gaussian blur for React Native. Android-only at runtime today (iOS is roadmap; `BlurView` renders a plain `View` on other platforms). Three layers, each in its own directory:

- **`blur-core/`** — pure C++17 static library (CMake), zero Android/RN dependencies. Installable standalone via `find_package(blur-core)` → `blur::blur-core`.
- **`android/`** — JNI bridge (`src/main/cpp/JNIBridge.cpp`) + Kotlin view/manager. `android/CMakeLists.txt` pulls in blur-core via `add_subdirectory` and builds `libblur-jni.so`.
- **`src/`** — TypeScript API (`BlurView` component, `NativeBlur` TurboModule spec).

## Commands

```bash
npm run typecheck                  # tsc --noEmit ("lint" is the same command)

# C++ unit tests (GoogleTest, fetched via FetchContent)
cmake -B build -S blur-core -DBLUR_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure

# Single test (suites: KernelTest, GaussianBlurTest, BoxBlur3PassTest, BlurCacheTest)
./build/blur-tests --gtest_filter='BoxBlur3PassTest.*'

# Benchmarks (Google Benchmark; use Release or numbers are meaningless)
cmake -B build -S blur-core -DBLUR_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/blur-bench --benchmark_min_time=0.2s --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true   # median/stddev, not one noisy sample

# AddressSanitizer run
cmake -B build -S blur-core -DBLUR_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build && ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build

# Android library (NO committed Gradle wrapper — needs standalone Gradle
# compatible with AGP 8.2.0; CI pins 8.4. JDK 17, NDK 26.1.10909125.)
gradle -p android assembleDebug
gradle -p android connectedAndroidTest   # instrumentation tests, needs device/emulator
```

`npm run test:cpp` / `npm run benchmark` do the same builds but inside `blur-core/build/` instead of `./build/`. If a newer CMake (4.x) rejects a FetchContent dependency's policy version, add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` (CI passes it everywhere).

## Architecture

### C++ core: everything dispatches through `gaussianBlur()`

`blur::gaussianBlur(bitmap, options)` in [blur-core/src/gaussian.cpp](blur-core/src/gaussian.cpp) is the single entry point and algorithm selector:

1. **Cache check** — `BlurCache` (`cache.cpp`): FNV-1a pixel hash + LRU keyed by (hash, dims, radius, sigma, scale), 64MB byte budget, `std::mutex`-guarded singleton.
2. **Method selection** — radius > 5 (with `BlurMethod::Auto`) → `boxBlur3Pass()` (`boxblur.cpp`, O(N), radius-independent, **scalar** — no SIMD); otherwise separable Gaussian (horizontal + vertical 1D passes). Validated on 3 physical Android devices (Firebase Test Lab, 3 repeats each, DVFS-warmup + randomized case order — see `scripts/testlab-benchmark.sh`, `android/src/androidTest/java/com/blur/BlurBenchmark.kt`): Box radius 6 is never faster than Gaussian radius 5 at 1024² — ~1.7× slower on a budget device (Galaxy A06), ~13% slower mid-range (Galaxy A53 5G), ~12% slower flagship (Pixel 9 Pro). Gap shrinks budget→flagship but never crosses over. An earlier *single-run* Test Lab pass had suggested Box was faster on the A53/Pixel; that reading was a Gaussian-side measurement outlier (DVFS ramp + one-shot noise), not a real effect — don't resurrect it. Net: the ">5" threshold is not leaving perf on the table anywhere tested; the more promising lever is SIMD-accelerating `boxBlur3Pass` (still pure scalar) to shrink the gap, not moving the cutoff.
3. **Auto-downscale — dead code under `Auto`.** `autoScaleFactor()` (`scale.cpp`) returns 2 for radius ≥ 8, but the box branch (radius > 5) returns before the downscale code is reached, and every radius that would downscale is also > 5. The downscale path only runs under an explicit `BlurMethod::Gaussian`, which nothing in the JNI/RN bridge sets. It's a latent optimization, not a live path — don't describe it as active.
4. **Per-pass SIMD dispatch** inside `blurHorizontal`/`blurVertical` (Gaussian path only): NEON (fixed-point int16 kernel) on ARM, AVX2 on x86_64 behind a **runtime** `cpuSupportsAVX2()` cpuid check, scalar fallback. Rows/columns are chunked across `ThreadPool::instance().parallelFor()`.
5. Temp images come from `ScopedBuffer`/`BufferPool` (`pool.cpp`): thread-local, size-classed, 64MB cap per thread.

`blur::Bitmap` (`include/bitmap.h`) is a **non-owning view** (pixels/width/height/stride/channels); blur mutates pixels in place. `stride` may exceed `width*channels` (Android bitmaps). `sigma = -1` means "derive from radius" throughout the stack.

### Platform-gated sources — most code never compiles locally

`blur-core/CMakeLists.txt` compiles `src/neon/gaussian_neon.cpp` only when targeting ARM, and `src/avx2/gaussian_avx2.cpp` only on x86_64 (that one file alone gets `-mavx2`; execution is gated by the runtime cpuid check, never assume AVX2 from the compile flag). Consequence: on any given machine one SIMD path never sees a compiler, and the Kotlin/Android side only compiles under Gradle+NDK. CI exists specifically to cover these blind spots (a past release shipped Android code that didn't compile — commit 45d66bc). After touching NEON, AVX2, or Kotlin/JNI code, don't claim it builds until the corresponding CI job passes:

- **aarch64 QEMU job** — cross-compiles + runs full test suite for the NEON path (the only coverage of `gaussian_neon.cpp` on x86; on this arm64 Mac it's the AVX2 file that never compiles locally).
- **android-build job** — Gradle+NDK compiles Kotlin and blur-core for all 4 ABIs.
- **cmake-install job** — verifies `find_package(blur-core)` against `tests/consumer_smoke/`.

### Android layer: two blur backends forked on API 31

`BlurView.kt` (a `FrameLayout`) snapshots its children into an ARGB_8888 bitmap, then:

- **API 31+ (Android 12)**: uses GPU `RenderEffect` and **skips the C++ blur entirely** (no double blur). An API 31+ emulator therefore never exercises blur-core through `BlurView` — the C++ path via UI needs an API ≤ 30 device.
- **API < 31**: calls `NativeBlur.applyBlur()` → JNI → `blur::gaussianBlur` **zero-copy** on the locked bitmap buffer (`AndroidBitmap_lockPixels`). RGBA_8888 only; JNI symbols `Java_com_blur_NativeBlur_*` must match `com.blur.NativeBlur`'s `native` declarations.

Blur re-runs only when `needsRedraw` is set (radius change or layout change), not on every frame.

`BlurPackage` registers **only the view manager** (classic `SimpleViewManager`, props `blurRadius`/`overlayColor`). No native module implements the `BlurModule` TurboModule spec — `src/NativeBlur.ts` null-guards `TurboModuleRegistry.getEnforcing` in a try/catch, so the JS `NativeBlur` export is `null` at runtime (`isTurboModuleAvailable()` reports it). The Kotlin-side file-blur logic that would back it lives in `BlurProcessor.kt`. Codegen config (`RNBlurSpec`, package `com.blur`) is in package.json.

### Tests

New C++ test files must be added to the `blur-tests` executable in `blur-core/CMakeLists.txt`. Instrumentation tests live in `android/src/androidTest/` (JNI crash safety, memory pressure, alpha integrity).

## Releases

Pushing a `v*` tag triggers `.github/workflows/publish.yml`: it fails unless the tag matches `package.json` version, then publishes to npm (OIDC trusted publishing) and GitHub Packages. `blur-core/CMakeLists.txt` carries its own `project(... VERSION)` that is bumped separately.

**Benchmarking gotcha:** `gaussianBlur` consults a process-wide LRU `BlurCache`, so blurring the same buffer in a loop measures a cache hit, not the blur — this is exactly how releases through v1.4.4 got fabricated sub-0.1 ms figures. The harness in `blur-core/tests/benchmark_blur.cpp` restores pristine input and clears the cache each repetition (timing paused) and uses `->UseRealTime()` (the blur fans out over a thread pool, so cpu_time is meaningless). Keep those invariants for any new benchmark. Honest cold-blur cost is ~1 ms (512²) to ~100 ms (4096²); the cache-hit path is ~10–100 µs. README benchmark tables should be refreshed from real `blur-bench` output / CI artifacts, never hand-edited.
