package com.blur

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RenderEffect
import android.graphics.Shader
import android.os.Build
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout

class BlurView(context: Context) : FrameLayout(context) {
    private var blurRadius: Float = 10f
    private var overlayColor: Int = Color.TRANSPARENT
    private var cachedBitmap: Bitmap? = null
    private var needsRedraw: Boolean = true
    private val backgroundPaint = Paint(Paint.ANTI_ALIAS_FLAG)

    init {
        setWillNotDraw(false)
    }

    fun setBlurRadius(radius: Float) {
        if (blurRadius != radius) {
            blurRadius = radius
            needsRedraw = true
            invalidate()
        }
    }

    fun setOverlayColor(color: String) {
        try {
            val parsed = Color.parseColor(color)
            if (overlayColor != parsed) {
                overlayColor = parsed
                invalidate()
            }
        } catch (e: IllegalArgumentException) {
            Log.e(TAG, "Invalid overlay color: $color", e)
        }
    }

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)
        if (changed) {
            needsRedraw = true
        }
    }

    override fun dispatchDraw(canvas: Canvas) {
        if (blurRadius <= 0f || width <= 0 || height <= 0) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                setRenderEffect(null)
            }
            super.dispatchDraw(canvas)
            return
        }

        if (needsRedraw) {
            captureAndBlur()
            needsRedraw = false
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            drawWithRenderEffect(canvas)
        } else {
            drawWithCachedBitmap(canvas)
        }

        if (overlayColor != Color.TRANSPARENT) {
            canvas.drawColor(overlayColor)
        }

        super.dispatchDraw(canvas)
    }

    // RenderEffect (API 31+) applies to a View's composited output, not to a
    // Paint — it's set on this View itself so the raw (unblurred) bitmap
    // drawn below gets blurred by the GPU at composition time.
    private fun drawWithRenderEffect(canvas: Canvas) {
        val effect = RenderEffect.createBlurEffect(
            blurRadius,
            blurRadius,
            Shader.TileMode.CLAMP
        )
        setRenderEffect(effect)
        drawWithCachedBitmap(canvas)
    }

    private fun drawWithCachedBitmap(canvas: Canvas) {
        cachedBitmap?.let {
            canvas.drawBitmap(it, 0f, 0f, backgroundPaint)
        }
    }

    private fun captureAndBlur() {
        val w = width
        val h = height
        if (w <= 0 || h <= 0) return

        try {
            val bitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)

            drawChildContent(canvas)

            val useGPU = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S

            if (!useGPU && NativeBlur.isAvailable() && blurRadius > 0f) {
                NativeBlur.applyBlur(bitmap, blurRadius.toInt())
            }

            cachedBitmap?.recycle()
            cachedBitmap = bitmap
        } catch (e: Exception) {
            Log.e(TAG, "Failed to capture/blur: ${e.message}", e)
        }
    }

    private fun drawChildContent(canvas: Canvas) {
        val childCount = childCount
        for (i in 0 until childCount) {
            val child = getChildAt(i) ?: continue
            canvas.save()
            canvas.translate(child.left.toFloat(), child.top.toFloat())
            child.draw(canvas)
            canvas.restore()
        }
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        cachedBitmap?.recycle()
        cachedBitmap = null
    }

    companion object {
        private const val TAG = "BlurView"
    }
}
