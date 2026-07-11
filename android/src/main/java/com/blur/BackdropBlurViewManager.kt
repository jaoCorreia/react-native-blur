package com.blur

import android.graphics.Color
import android.util.Log
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.annotations.ReactProp

@ReactModule(name = BackdropBlurViewManager.NAME)
class BackdropBlurViewManager : SimpleViewManager<BackdropBlurView>() {

    override fun getName(): String = NAME

    override fun createViewInstance(context: ThemedReactContext): BackdropBlurView {
        return BackdropBlurView(context)
    }

    @ReactProp(name = "blurRadius", defaultFloat = 16f)
    fun setBlurRadius(view: BackdropBlurView, radius: Float) {
        view.blurRadius = radius
    }

    @ReactProp(name = "overlayColor")
    fun setOverlayColor(view: BackdropBlurView, color: String?) {
        view.overlayColor = try {
            if (color.isNullOrEmpty()) Color.TRANSPARENT else Color.parseColor(color)
        } catch (e: IllegalArgumentException) {
            Log.e(NAME, "Invalid overlay color: $color", e)
            Color.TRANSPARENT
        }
    }

    // 0 (default) means "let the view pick per API level" — 4 on API 31+, 8 below.
    @ReactProp(name = "downsample", defaultInt = 0)
    fun setDownsample(view: BackdropBlurView, factor: Int) {
        if (factor > 0) view.downsample = factor
    }

    @ReactProp(name = "blurEnabled", defaultBoolean = true)
    fun setBlurEnabled(view: BackdropBlurView, enabled: Boolean) {
        view.blurEnabled = enabled
    }

    companion object {
        const val NAME = "BackdropBlurView"
    }
}
