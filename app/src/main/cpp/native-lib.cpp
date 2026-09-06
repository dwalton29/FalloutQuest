#include <jni.h>
#include <android/log.h>

static constexpr const char* TAG = "FalloutQuest";

extern "C" JNIEXPORT jstring JNICALL
Java_com_falloutquest_app_MainActivity_nativeBootMessage(JNIEnv* env, jclass) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "FalloutQuest ARM64 native library loaded successfully");
    return env->NewStringUTF("ARM64 native library loaded successfully");
}
