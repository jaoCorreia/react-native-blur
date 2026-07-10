package com.blur

import android.graphics.Color
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.RNBlurViewManagerInterface
import com.facebook.react.viewmanagers.RNBlurViewManagerDelegate

@ReactModule(name = BlurViewManager.NAME)
class BlurViewManager(
    private val reactContext: ReactApplicationContext
) : SimpleViewManager<BlurView>(),
    RNBlurViewManagerInterface<BlurView> {

    private val delegate = RNBlurViewManagerDelegate(this)

    override fun getDelegate(): ViewManagerDelegate<BlurView> = delegate

    override fun getName(): String = NAME

    override fun createViewInstance(context: ThemedReactContext): BlurView {
        return BlurView(context)
    }

    @ReactProp(name = "blurRadius", defaultFloat = 10f)
    override fun setBlurRadius(view: BlurView, radius: Float) {
        view.setBlurRadius(radius)
    }

    @ReactProp(name = "overlayColor")
    override fun setOverlayColor(view: BlurView, color: String?) {
        view.setOverlayColor(color ?: "transparent")
    }

    override fun getExportedCustomBubblingEventTypeConstants(): MutableMap<String, Any> {
        return mutableMapOf()
    }

    companion object {
        const val NAME = "BlurView"
    }
}
