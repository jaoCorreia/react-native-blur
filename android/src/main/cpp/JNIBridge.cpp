#include <jni.h>
#include <android/bitmap.h>
#include <android/log.h>
#include "gaussian.h"
#include "bitmap.h"
#include <cstring>

#define LOG_TAG "NativeBlur"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT void JNICALL
Java_com_blur_NativeBlur_nativeApplyBlur(
    JNIEnv* env,
    jobject /* this */,
    jobject bitmap,
    jint radius,
    jfloat sigma
) {
    if (bitmap == nullptr) {
        LOGE("nativeApplyBlur: bitmap is null");
        return;
    }

    AndroidBitmapInfo info;
    int ret = AndroidBitmap_getInfo(env, bitmap, &info);
    if (ret != ANDROID_BITMAP_RESULT_SUCCESS) {
        LOGE("nativeApplyBlur: AndroidBitmap_getInfo failed, ret=%d", ret);
        return;
    }

    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        LOGE("nativeApplyBlur: unsupported format=%d, only RGBA_8888 supported", info.format);
        return;
    }

    void* pixelData = nullptr;
    ret = AndroidBitmap_lockPixels(env, bitmap, &pixelData);
    if (ret != ANDROID_BITMAP_RESULT_SUCCESS) {
        LOGE("nativeApplyBlur: AndroidBitmap_lockPixels failed, ret=%d", ret);
        return;
    }

    blur::Bitmap bmp;
    bmp.pixels = static_cast<uint8_t*>(pixelData);
    bmp.width = static_cast<int>(info.width);
    bmp.height = static_cast<int>(info.height);
    bmp.stride = static_cast<int>(info.stride);
    bmp.channels = 4;

    LOGD("Applying blur: %dx%d, radius=%d, sigma=%.2f",
         bmp.width, bmp.height, radius, sigma);

    blur::gaussianBlur(bmp, static_cast<int>(radius), static_cast<float>(sigma));

    AndroidBitmap_unlockPixels(env, bitmap);

    LOGD("Blur applied successfully");
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_blur_NativeBlur_nativeGetVersion(
    JNIEnv* env,
    jobject /* this */
) {
    return env->NewStringUTF("1.0.0");
}
