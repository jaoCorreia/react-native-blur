package com.blur;

import android.graphics.Bitmap;
import android.util.Log;

public class NativeBlur {
    private static final String TAG = "NativeBlur";
    private static boolean libraryLoaded = false;

    static {
        try {
            System.loadLibrary("blur-jni");
            libraryLoaded = true;
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "Failed to load native library: " + e.getMessage());
            libraryLoaded = false;
        }
    }

    public static boolean isAvailable() {
        return libraryLoaded;
    }

    public static void applyBlur(Bitmap bitmap, int radius) {
        applyBlur(bitmap, radius, -1.0f);
    }

    public static void applyBlur(Bitmap bitmap, int radius, float sigma) {
        if (!libraryLoaded) {
            Log.e(TAG, "Native library not loaded, cannot apply blur");
            return;
        }
        if (bitmap == null) {
            Log.e(TAG, "Bitmap is null");
            return;
        }
        nativeApplyBlur(bitmap, radius, sigma);
    }

    public static String getVersion() {
        if (!libraryLoaded) return "unavailable";
        return nativeGetVersion();
    }

    private static native void nativeApplyBlur(Bitmap bitmap, int radius, float sigma);

    private static native String nativeGetVersion();
}
