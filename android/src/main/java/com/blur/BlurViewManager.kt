package com.blur

import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.annotations.ReactProp

@ReactModule(name = BlurViewManager.NAME)
class BlurViewManager : SimpleViewManager<BlurView>() {

    override fun getName(): String = NAME

    override fun createViewInstance(context: ThemedReactContext): BlurView {
        return BlurView(context)
    }

    @ReactProp(name = "blurRadius", defaultFloat = 10f)
    fun setBlurRadius(view: BlurView, radius: Float) {
        view.setBlurRadius(radius)
    }

    @ReactProp(name = "overlayColor")
    fun setOverlayColor(view: BlurView, color: String?) {
        view.setOverlayColor(color ?: "transparent")
    }

    companion object {
        const val NAME = "BlurView"
    }
}
