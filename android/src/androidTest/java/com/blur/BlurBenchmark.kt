package com.blur

import android.graphics.Bitmap
import android.util.Log
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * On-device blur benchmark. Runs on real hardware (Firebase Test Lab) or a
 * local device/emulator — on ARM it exercises the NEON path phones use.
 *
 * Deliberately calls [NativeBlur.applyBlur] directly rather than going through
 * [BlurView]: on API 31+ BlurView uses the GPU RenderEffect and skips the C++
 * blur entirely, so driving it through the view would measure nothing on the
 * native path.
 *
 * Methodology mirrors the C++ harness (blur-core/tests/benchmark_blur.cpp):
 *   - cold blur: restore pristine input AND clear the cache before every timed
 *     run, so each is one blur of fresh content (the per-frame cost).
 *   - cache hit: restore the exact cached input and re-blur, isolating the
 *     hash + lookup + copy-out cost for static content.
 * Restores and cache clears happen outside the timed region.
 *
 * Not run in normal CI (no emulator there). Select explicitly, e.g. on Test Lab:
 *   --test-targets "class com.blur.BlurBenchmark"
 * Results are emitted to logcat under the tag "BlurBench" as one JSON object
 * per case, plus a summary line — parse those from the captured log.
 *
 * DVFS note: a single-run Test Lab pass previously showed box radius 6 (the
 * first case in the box sweep) costing 10-20% more than radius 10/15 tested
 * right after it — consistent with the CPU governor still ramping up from
 * idle rather than any property of the algorithm. Two mitigations: (1) a
 * fixed CPU warmup that runs before any timed case to get the governor to a
 * steady clock, and (2) randomizing case order per invocation (seed logged)
 * so "tested first" doesn't correlate with any one radius across repeated
 * runs.
 */
@RunWith(AndroidJUnit4::class)
class BlurBenchmark {

    companion object {
        private const val TAG = "BlurBench"
        private const val WARMUP = 3
        private const val ITERATIONS = 11 // odd -> unambiguous median
        private const val CPU_WARMUP_MS = 1500L
    }

    @Before
    fun requireNativeLib() {
        assertTrue(
            "Native blur library must load for the benchmark to mean anything",
            NativeBlur.isAvailable()
        )
    }

    private fun pristinePixels(size: Int): IntArray {
        val pixels = IntArray(size * size)
        for (y in 0 until size) {
            for (x in 0 until size) {
                val r = x * 255 / size
                val g = y * 255 / size
                val b = (x + y) * 127 / size
                pixels[y * size + x] = (255 shl 24) or (r shl 16) or (g shl 8) or b
            }
        }
        return pixels
    }

    private fun medianMs(samplesNs: LongArray): Double {
        samplesNs.sort()
        return samplesNs[samplesNs.size / 2] / 1_000_000.0
    }

    private fun benchCase(size: Int, radius: Int) {
        val pristine = pristinePixels(size)
        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)

        fun restore() = bitmap.setPixels(pristine, 0, size, 0, 0, size, size)

        // Cold: fresh input + empty cache each run.
        repeat(WARMUP) { restore(); NativeBlur.clearCache(); NativeBlur.applyBlur(bitmap, radius) }
        val cold = LongArray(ITERATIONS)
        for (i in 0 until ITERATIONS) {
            restore()
            NativeBlur.clearCache()
            val t0 = System.nanoTime()
            NativeBlur.applyBlur(bitmap, radius)
            cold[i] = System.nanoTime() - t0
        }

        // Cache hit: prime once, then restore the exact cached input and re-blur.
        restore(); NativeBlur.clearCache(); NativeBlur.applyBlur(bitmap, radius)
        repeat(WARMUP) { restore(); NativeBlur.applyBlur(bitmap, radius) }
        val hit = LongArray(ITERATIONS)
        for (i in 0 until ITERATIONS) {
            restore()
            val t0 = System.nanoTime()
            NativeBlur.applyBlur(bitmap, radius)
            hit[i] = System.nanoTime() - t0
        }

        bitmap.recycle()

        val coldMs = medianMs(cold)
        val hitMs = medianMs(hit)
        Log.i(
            TAG,
            """{"size":$size,"radius":$radius,""" +
                """"cold_ms_median":${"%.4f".format(coldMs)},""" +
                """"cachehit_ms_median":${"%.4f".format(hitMs)},""" +
                """"iterations":$ITERATIONS}"""
        )
    }

    // Spins the CPU on a throwaway blur for a fixed wall-clock duration before any
    // timed case runs, so the governor is at a steady clock rather than ramping
    // up during the (position-dependent) first few timed cases.
    private fun cpuWarmup() {
        val size = 1024
        val pristine = pristinePixels(size)
        val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        val deadline = System.nanoTime() + CPU_WARMUP_MS * 1_000_000
        while (System.nanoTime() < deadline) {
            bitmap.setPixels(pristine, 0, size, 0, 0, size, size)
            NativeBlur.clearCache()
            NativeBlur.applyBlur(bitmap, 10)
        }
        bitmap.recycle()
    }

    @Test
    fun benchmarkBlur() {
        Log.i(TAG, "BEGIN device=${android.os.Build.MODEL} api=${android.os.Build.VERSION.SDK_INT} abi=${android.os.Build.SUPPORTED_ABIS.firstOrNull()}")

        cpuWarmup()

        // All cases in one pool, order randomized per invocation so "tested first"
        // (and any residual DVFS ramp) doesn't correlate with a particular radius
        // across repeated runs. Seed is logged for reproducibility.
        val cases = buildList {
            // Gaussian path (radius <= 5): resolution scaling.
            for (size in intArrayOf(256, 512, 1024)) add(size to 5)
            // Box path (radius > 5): radius sweep at fixed resolution (independence)...
            for (radius in intArrayOf(6, 10, 15)) add(1024 to radius)
            // ...and resolution sweep at fixed radius (O(N)). 4096 left out: memory/time on low-end phones.
            for (size in intArrayOf(256, 512, 2048)) add(size to 10)
        }
        val seed = System.nanoTime()
        val shuffled = cases.shuffled(java.util.Random(seed))
        Log.i(TAG, "SEED $seed")

        for ((size, radius) in shuffled) benchCase(size, radius)

        Log.i(TAG, "END")
    }
}
