package com.blur

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.RenderEffect
import android.graphics.RenderNode
import android.graphics.Shader
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.util.Log
import android.view.PixelCopy
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.annotation.RequiresApi
import androidx.annotation.VisibleForTesting
import kotlin.math.max

/**
 * A glassmorphic panel that shows a **blurred copy of what's behind it** —
 * specifically the content of a target [SurfaceView] (a map, a video, a camera
 * preview, custom GL).
 *
 * Why this exists and why it's not the same as [BlurView]: `BlurView` blurs
 * its own children. A panel over live GPU content needs *backdrop* blur — the
 * SurfaceView pixels behind the panel, blurred, with the panel's own content
 * sharp on top. And that content is GPU-rendered into a `SurfaceView` (e.g. a
 * map's GLSurfaceView), which a `Canvas` cannot read at all — so the only way
 * to get those pixels is [PixelCopy] (API 24+), a GPU->CPU readback. That
 * readback, not the blur, is the real cost, so we capture at a downscale.
 *
 * The blur backend is chosen per API level:
 *  - **31+ (Android 12):** GPU [RenderEffect] on a [RenderNode] — no CPU blur,
 *    and it blurs only the backdrop layer, never the sharp children.
 *  - **24–30:** the C++/NEON blur ([NativeBlur]) on the downscaled capture.
 *  - **21–23:** no PixelCopy available -> falls back to a translucent scrim
 *    ([overlayColor]); there is no true backdrop blur on these versions.
 *
 * When the content moves, the blurred backdrop trails it by a frame or two
 * (PixelCopy is async) and is captured at low resolution — an accepted
 * tradeoff, see README. Children (the panel content) are drawn sharp on top.
 */
class BackdropBlurView(context: Context) : FrameLayout(context) {

    var blurRadius: Float = 16f
        set(value) { field = value.coerceAtLeast(0f); invalidate() }

    var overlayColor: Int = Color.TRANSPARENT
        set(value) { field = value; invalidate() }

    /** Capture at 1/downsample resolution. Higher = cheaper readback, softer. */
    var downsample: Int = defaultDownsampleForApi()
        set(value) { field = value.coerceAtLeast(1) }

    var blurEnabled: Boolean = true
        set(value) {
            field = value
            if (value) startLoop() else stopLoop()
            invalidate()
        }

    private var explicitTarget: SurfaceView? = null

    private val bitmapPaint = Paint(Paint.FILTER_BITMAP_FLAG or Paint.ANTI_ALIAS_FLAG)
    private val mainHandler = Handler(Looper.getMainLooper())

    private var captureThread: HandlerThread? = null
    private var captureHandler: Handler? = null

    @Volatile private var frame: Frame? = null
    @Volatile private var capturing = false
    private var looping = false

    private var renderNode: RenderNode? = null

    /** A captured (and, on 24–30, blurred) frame plus where the panel maps into it. */
    private class Frame(val bitmap: Bitmap, val src: Rect)

    init {
        setWillNotDraw(false)
    }

    /** Point the blur at a specific surface (otherwise the first SurfaceView in the tree is used). */
    fun setTargetSurfaceView(target: SurfaceView?) {
        explicitTarget = target
    }

    /** The most recent captured (raw on 31+, CPU-blurred on 24–30) backdrop. For tests. */
    @VisibleForTesting
    fun lastCapturedBitmapForTest(): Bitmap? = frame?.bitmap

    /** The capture thread's handler, lazily starting the thread if needed. For tests. */
    @VisibleForTesting
    fun ensureCaptureHandlerForTest(): Handler {
        captureHandler?.let { return it }
        val t = HandlerThread("backdrop-capture-test").also { it.start() }
        captureThread = t
        return Handler(t.looper).also { captureHandler = it }
    }

    // --- lifecycle ----------------------------------------------------------

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (blurEnabled) startLoop()
    }

    override fun onDetachedFromWindow() {
        stopLoop()
        super.onDetachedFromWindow()
    }

    private fun startLoop() {
        if (looping || !isAttachedToWindow) return
        if (Build.VERSION.SDK_INT < 24) return // scrim-only tier: nothing to capture
        looping = true
        val t = HandlerThread("backdrop-capture").also { it.start() }
        captureThread = t
        captureHandler = Handler(t.looper)
        scheduleNext(0)
    }

    private fun stopLoop() {
        looping = false
        mainHandler.removeCallbacksAndMessages(null)
        captureThread?.quitSafely()
        captureThread = null
        captureHandler = null
    }

    private fun scheduleNext(delayMs: Long = MIN_INTERVAL_MS) {
        if (!looping) return
        mainHandler.postDelayed({ requestCapture() }, delayMs)
    }

    // --- capture ------------------------------------------------------------

    private fun resolveTarget(): SurfaceView? =
        explicitTarget ?: (rootView as? ViewGroup)?.let { findSurfaceView(it) }

    private fun findSurfaceView(group: ViewGroup): SurfaceView? {
        for (i in 0 until group.childCount) {
            val child = group.getChildAt(i)
            if (child === this) continue // never capture ourselves
            if (child is SurfaceView) return child
            if (child is ViewGroup) findSurfaceView(child)?.let { return it }
        }
        return null
    }

    private fun requestCapture() {
        if (!looping || capturing) return
        capture { scheduleNext() }
    }

    /**
     * One capture+blur cycle. [onDone] runs (on the capture thread) when the
     * cycle finishes, success or not. Exposed for tests to drive a single
     * deterministic pass without the live loop.
     */
    @VisibleForTesting
    @RequiresApi(24)
    fun capture(onDone: () -> Unit) {
        val target = resolveTarget()
        val w = width
        val h = height
        val handler = captureHandler ?: captureThread?.looper?.let { Handler(it) }
        if (handler == null || target == null || w == 0 || h == 0 ||
            target.width == 0 || target.height == 0) {
            onDone(); return
        }

        val sw = target.width
        val sh = target.height
        val f = downsample
        val bw = max(1, sw / f)
        val bh = max(1, sh / f)

        // Where does this panel sit over the target surface? Map that rect into
        // the downscaled capture's coordinate space.
        val my = IntArray(2).also { getLocationInWindow(it) }
        val ty = IntArray(2).also { target.getLocationInWindow(it) }
        val relLeft = my[0] - ty[0]
        val relTop = my[1] - ty[1]
        val src = Rect(
            (relLeft.toFloat() / sw * bw).toInt().coerceIn(0, bw),
            (relTop.toFloat() / sh * bh).toInt().coerceIn(0, bh),
            ((relLeft + w).toFloat() / sw * bw).toInt().coerceIn(0, bw),
            ((relTop + h).toFloat() / sh * bh).toInt().coerceIn(0, bh)
        )
        if (src.width() == 0 || src.height() == 0) { onDone(); return }

        val bmp = Bitmap.createBitmap(bw, bh, Bitmap.Config.ARGB_8888)
        capturing = true
        try {
            PixelCopy.request(target, bmp, { result ->
                if (result == PixelCopy.SUCCESS) {
                    // 24–30: blur the whole downscaled capture on the CPU now
                    // (cheap at this size); 31+ keeps it raw and blurs on the
                    // GPU at draw time via RenderEffect.
                    if (Build.VERSION.SDK_INT < 31) {
                        val r = max(1, (blurRadius / f).toInt())
                        NativeBlur.applyBlur(bmp, r)
                    }
                    frame = Frame(bmp, src)
                    postInvalidateOnAnimation()
                } else {
                    Log.w(TAG, "PixelCopy failed: $result")
                    bmp.recycle()
                }
                capturing = false
                onDone()
            }, handler)
        } catch (e: Exception) {
            Log.w(TAG, "PixelCopy request threw: ${e.message}")
            bmp.recycle()
            capturing = false
            onDone()
        }
    }

    // --- draw ---------------------------------------------------------------

    override fun dispatchDraw(canvas: Canvas) {
        drawBackdrop(canvas)
        if (overlayColor != Color.TRANSPARENT) canvas.drawColor(overlayColor)
        super.dispatchDraw(canvas) // panel content, sharp, on top
    }

    private fun drawBackdrop(canvas: Canvas) {
        val f = frame ?: return
        if (f.bitmap.isRecycled) return
        val dst = RectF(0f, 0f, width.toFloat(), height.toFloat())

        if (Build.VERSION.SDK_INT >= 31 && canvas.isHardwareAccelerated) {
            drawBackdropGpu(canvas, f, dst)
        } else {
            // 24–30 path (already CPU-blurred), or a non-HW canvas fallback.
            canvas.drawBitmap(f.bitmap, f.src, dst, bitmapPaint)
        }
    }

    @RequiresApi(31)
    private fun drawBackdropGpu(canvas: Canvas, f: Frame, dst: RectF) {
        val node = (renderNode ?: RenderNode("backdrop").also { renderNode = it })
        node.setPosition(0, 0, width, height)
        val rc = node.beginRecording()
        rc.drawBitmap(f.bitmap, f.src, dst, bitmapPaint)
        node.endRecording()
        node.setRenderEffect(
            RenderEffect.createBlurEffect(
                blurRadius.coerceAtLeast(0.01f),
                blurRadius.coerceAtLeast(0.01f),
                Shader.TileMode.CLAMP
            )
        )
        canvas.drawRenderNode(node)
    }

    private fun defaultDownsampleForApi(): Int =
        if (Build.VERSION.SDK_INT >= 31) 4 else 8

    companion object {
        private const val TAG = "BackdropBlurView"
        // ~30 fps ceiling for the capture loop; the readback can't sustain 60.
        private const val MIN_INTERVAL_MS = 33L
    }
}
