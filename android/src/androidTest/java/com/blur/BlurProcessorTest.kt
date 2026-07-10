package com.blur

import android.graphics.Bitmap
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class BlurProcessorTest {

    private fun createTestBitmap(width: Int = 64, height: Int = 64): Bitmap {
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888)
        val pixels = IntArray(width * height)

        for (y in 0 until height) {
            for (x in 0 until width) {
                val r = (x * 255 / width)
                val g = (y * 255 / height)
                val b = ((x + y) * 127 / (width + height))
                val a = 255
                pixels[y * width + x] = (a shl 24) or (r shl 16) or (g shl 8) or b
            }
        }
        bitmap.setPixels(pixels, 0, width, 0, 0, width, height)
        return bitmap
    }

    @Test
    fun testBlurDoesNotCrash() {
        val bitmap = createTestBitmap(128, 128)
        val result = BlurProcessor.blurBitmap(bitmap, 5)
        assertNotNull("Blur result should not be null", result)
        assertEquals("Width should be preserved", 128, result.width)
        assertEquals("Height should be preserved", 128, result.height)
        result.recycle()
        bitmap.recycle()
    }

    @Test
    fun testBlurPreservesDimensions() {
        val sizes = listOf(32 to 32, 64 to 128, 256 to 256)
        for ((w, h) in sizes) {
            val bitmap = createTestBitmap(w, h)
            val result = BlurProcessor.blurBitmap(bitmap, 3)
            assertEquals("Width preserved: ${w}x${h}", w, result.width)
            assertEquals("Height preserved: ${w}x${h}", h, result.height)
            result.recycle()
            bitmap.recycle()
        }
    }

    @Test
    fun testBlurWithDifferentRadii() {
        val bitmap = createTestBitmap(64, 64)
        for (radius in listOf(1, 3, 5, 10)) {
            val result = BlurProcessor.blurBitmap(bitmap, radius)
            assertNotNull("Result should not be null for radius=$radius", result)
            result.recycle()
        }
        bitmap.recycle()
    }

    @Test
    fun testBlurWithCustomSigma() {
        val bitmap = createTestBitmap(64, 64)
        for (sigma in listOf(0.5f, 1.0f, 5.0f, -1f)) {
            val result = BlurProcessor.blurBitmap(bitmap.copy(Bitmap.Config.ARGB_8888, false), 5, sigma)
            assertNotNull("Result should not be null for sigma=$sigma", result)
            result.recycle()
        }
        bitmap.recycle()
    }

    @Test
    fun testAlphaChannelPreservedRange() {
        val bitmap = createTestBitmap(32, 32)
        val result = BlurProcessor.blurBitmap(bitmap, 4)

        val pixels = IntArray(32 * 32)
        result.getPixels(pixels, 0, 32, 0, 0, 32, 32)

        for (pixel in pixels) {
            val alpha = (pixel shr 24) and 0xFF
            assertTrue("Alpha should be in 0-255, got $alpha", alpha in 0..255)
        }

        result.recycle()
        bitmap.recycle()
    }

    @Test
    fun testZeroRadiusReturnsSameImage() {
        val bitmap = createTestBitmap(32, 32)
        val before = IntArray(32 * 32)
        bitmap.getPixels(before, 0, 32, 0, 0, 32, 32)

        val result = BlurProcessor.blurBitmap(bitmap, 0)
        val after = IntArray(32 * 32)
        result.getPixels(after, 0, 32, 0, 0, 32, 32)

        assertArrayEquals("Zero radius should not change pixels", before, after)

        result.recycle()
        bitmap.recycle()
    }

    @Test
    fun testBlurChangesImage() {
        val bitmap = createTestBitmap(32, 32)
        val before = IntArray(32 * 32)
        bitmap.getPixels(before, 0, 32, 0, 0, 32, 32)

        val result = BlurProcessor.blurBitmap(bitmap, 5)
        val after = IntArray(32 * 32)
        result.getPixels(after, 0, 32, 0, 0, 32, 32)

        var changed = false
        for (i in before.indices) {
            if (before[i] != after[i]) {
                changed = true
                break
            }
        }
        assertTrue("Blur should change pixel values", changed)

        result.recycle()
        bitmap.recycle()
    }

    @Test
    fun testBlurDoesNotLeakMemory() {
        for (i in 0 until 10) {
            val bitmap = createTestBitmap(128, 128)
            val result = BlurProcessor.blurBitmap(bitmap, 8)
            assertNotNull("Iteration $i failed", result)
            result.recycle()
            bitmap.recycle()
        }
    }
}
