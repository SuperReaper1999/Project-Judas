#include "AndroidPlatform.h"
#include <jni.h>
#include <atomic>
namespace {std::atomic<bool> audioFocused{true};}
bool AndroidAudioFocused(){return audioFocused.load(std::memory_order_acquire);}
extern "C" JNIEXPORT void JNICALL
Java_org_projectjudas_runtime_JudasActivity_nativeAudioFocus(JNIEnv*,jclass,jboolean focused){
    audioFocused.store(focused==JNI_TRUE,std::memory_order_release);
}
