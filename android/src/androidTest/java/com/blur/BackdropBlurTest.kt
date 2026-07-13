package com.blur

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.ViewGroup
import android.view.ViewTreeObserver
import android.widget.FrameLayout
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * On-device validation of the backdrop-blur mechanism. Runs on an ARM
 * emulator/device (the NEON path). Covers the two things that can only be
 * proven on real hardware: that PixelCopy actually reads a SurfaceView's
 * pixels through [BackdropBlurView], and that the blur backend smooths them.
 */
@RunWith(AndroidJUnit4::class)
class BackdropBlurTest {

    private val instrumentation get() = InstrumentationRegistry.getInstrumentation()

    /** The blur backend we wire in (24–30 tier) must actually smooth a hard edge. */
    @Test
    fun blurSmoothsAHardEdge() {
        if (!NativeBlur.isAvailable()) return

        val size = 64
        val bmp = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
        for (y in 0 until size) for (x in 0 until size) {
            bmp.setPixel(x, y, if (x < size / 2) Color.RED else Color.BLUE)
        }

        NativeBlur.applyBlur(bmp, 6)

        // A column right at the seam should now be a red/blue blend, not pure.
        val mid = bmp.getPixel(size / 2, size / 2)
        assertTrue("Blur should mix red into the seam", Color.red(mid) in 1..254)
        assertTrue("Blur should mix blue into the seam", Color.blue(mid) in 1..254)
        bmp.recycle()
    }

    /** BackdropBlurView must capture the pixels of the SurfaceView behind it. */
    @Test
    fun capturesSurfaceViewContent() {
        ActivityScenario.launch(BackdropTestActivity::class.java).use { scenario ->
            lateinit var backdrop: BackdropBlurView
            val surfaceDrawn = CountDownLatch(1)
            val laidOut = CountDownLatch(1)

            scenario.onActivity { act ->
                val root = FrameLayout(act)
                val surface = SurfaceView(act)
                surface.holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(h: SurfaceHolder) {
                        val c = h.lockCanvas() ?: return
                        val w = c.width
                        val half = w / 2
                        val paint = Paint()
                        paint.color = Color.RED
                        c.drawRect(0f, 0f, half.toFloat(), c.height.toFloat(), paint)
                        paint.color = Color.BLUE
                        c.drawRect(half.toFloat(), 0f, w.toFloat(), c.height.toFloat(), paint)
                        h.unlockCanvasAndPost(c)
                        surfaceDrawn.countDown()
                    }
                    override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, ht: Int) {}
                    override fun surfaceDestroyed(h: SurfaceHolder) {}
                })

                backdrop = BackdropBlurView(act)
                backdrop.setTargetSurfaceView(surface)

                val match = FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.MATCH_PARENT
                )
                root.addView(surface, match)
                root.addView(backdrop, FrameLayout.LayoutParams(match))
                act.setContentView(root)

                backdrop.viewTreeObserver.addOnGlobalLayoutListener(
                    object : ViewTreeObserver.OnGlobalLayoutListener {
                        override fun onGlobalLayout() {
                            if (backdrop.width > 0 && backdrop.height > 0) {
                                backdrop.viewTreeObserver.removeOnGlobalLayoutListener(this)
                                laidOut.countDown()
                            }
                        }
                    }
                )
            }

            assertTrue("surface never drew", surfaceDrawn.await(5, TimeUnit.SECONDS))
            assertTrue("backdrop never laid out", laidOut.await(5, TimeUnit.SECONDS))
            // Let the posted surface buffer actually reach the compositor.
            Thread.sleep(500)

            val captured = CountDownLatch(1)
            instrumentation.runOnMainSync {
                backdrop.ensureCaptureHandlerForTest()
                backdrop.capture { captured.countDown() }
            }
            assertTrue("capture never completed", captured.await(5, TimeUnit.SECONDS))

            val bmp = backdrop.lastCapturedBitmapForTest()
            assertNotNull("PixelCopy produced no bitmap — capture failed", bmp)
            bmp!!

            // Left edge should read red-dominant, right edge blue-dominant, even
            // after downscale/blur — proof we captured the surface's content.
            val leftPx = bmp.getPixel(bmp.width / 10, bmp.height / 2)
            val rightPx = bmp.getPixel(bmp.width * 9 / 10, bmp.height / 2)
            assertTrue(
                "left side should be red-dominant, was #${Integer.toHexString(leftPx)}",
                Color.red(leftPx) > Color.blue(leftPx)
            )
            assertTrue(
                "right side should be blue-dominant, was #${Integer.toHexString(rightPx)}",
                Color.blue(rightPx) > Color.red(rightPx)
            )
        }
    }
}
