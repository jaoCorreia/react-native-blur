package com.blur

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Log
import java.io.File
import java.io.FileOutputStream

object BlurProcessor {
    private const val TAG = "BlurProcessor"

    fun blurBitmap(bitmap: Bitmap, radius: Int, sigma: Float = -1f): Bitmap {
        if (!NativeBlur.isAvailable()) {
            Log.e(TAG, "Native library not available")
            return bitmap
        }

        val config = bitmap.config ?: Bitmap.Config.ARGB_8888
        val working = bitmap.copy(config, bitmap.isMutable)

        NativeBlur.applyBlur(working, radius, sigma)

        return working
    }

    fun blurFile(inputPath: String, outputPath: String, radius: Int, sigma: Float = -1f): Boolean {
        return try {
            val original = BitmapFactory.decodeFile(inputPath)
                ?: return false

            val result = blurBitmap(original, radius, sigma)

            val outputFile = File(outputPath)
            outputFile.parentFile?.mkdirs()

            FileOutputStream(outputFile).use { stream ->
                result.compress(Bitmap.CompressFormat.PNG, 100, stream)
            }

            if (result !== original) original.recycle()
            result.recycle()

            true
        } catch (e: Exception) {
            Log.e(TAG, "blurFile failed: ${e.message}", e)
            false
        }
    }
}
