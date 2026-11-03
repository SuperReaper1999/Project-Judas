#include "AndroidPlatform.h"
#include <jni.h>
#include <SDL2/SDL_system.h>
#include <atomic>
namespace {std::atomic<bool> audioFocused{true};}
bool AndroidAudioFocused(){return audioFocused.load(std::memory_order_acquire);}
extern "C" JNIEXPORT void JNICALL
Java_org_projectjudas_runtime_JudasActivity_nativeAudioFocus(JNIEnv*,jclass,jboolean focused){
    audioFocused.store(focused==JNI_TRUE,std::memory_order_release);
}

bool AndroidVibration(int amplitude,int milliseconds){
    auto* env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if(!env)return false;
    auto activity=static_cast<jobject>(SDL_AndroidGetActivity());
    if(!activity)return false;
    auto type=env->GetObjectClass(activity);
    auto method=type?env->GetMethodID(type,"judasVibration","(II)Z"):nullptr;
    bool result=method&&env->CallBooleanMethod(activity,method,amplitude,milliseconds)==JNI_TRUE;
    if(env->ExceptionCheck()){env->ExceptionClear();result=false;}
    if(type)env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
    return result;
}
