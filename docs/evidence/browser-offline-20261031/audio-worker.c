#define MA_NO_RESOURCE_MANAGER
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <emscripten.h>
#include <pthread.h>
#include <stdio.h>
static ma_engine engine;
static ma_waveform wave;
static ma_sound sound;
static void* run(void* unused){
 ma_engine_config config=ma_engine_config_init();config.channels=2;config.sampleRate=48000;
 ma_result r=ma_engine_init(&config,&engine);printf("AUDIO_INIT %d backend=%d\n",r,r==MA_SUCCESS?(int)ma_device_get_context(ma_engine_get_device(&engine))->backend:-1);fflush(stdout);if(r!=MA_SUCCESS)return NULL;
 ma_waveform_config w=ma_waveform_config_init(ma_format_f32,1,48000,ma_waveform_type_sine,.1,440);
 r=ma_waveform_init(&w,&wave);if(r==MA_SUCCESS)r=ma_sound_init_from_data_source(&engine,&wave,MA_SOUND_FLAG_NO_SPATIALIZATION,NULL,&sound);
 if(r==MA_SUCCESS)r=ma_sound_start(&sound);printf("AUDIO_PLAY %d\n",r);fflush(stdout);return NULL;
}
EMSCRIPTEN_KEEPALIVE void start_audio(){pthread_t t;pthread_create(&t,NULL,run,NULL);pthread_detach(t);}
int main(){EM_ASM({let b=document.createElement('button');b.id='start';b.textContent='Start actual worker audio';b.onclick=()=>_start_audio();document.body.appendChild(b);});emscripten_exit_with_live_runtime();}
