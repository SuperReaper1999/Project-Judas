#include "AudioSystem.h"
#ifdef __ANDROID__
#include "AndroidPlatform.h"
#endif
#include "AudioStream.h"
#include "PerformanceProfiler.h"
#include "miniaudio.h"
#include "verblib.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
extern "C" float judas_audio_distance_gain(int,float,float,float,float);
namespace {
glm::vec3 Vector(ma_vec3f v){return {v.x,v.y,v.z};}
bool Finite(glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
ma_attenuation_model Model(AudioAttenuation m){return m==AudioAttenuation::Linear?ma_attenuation_model_linear:ma_attenuation_model_inverse;}
}
float AudioSystem::DistanceGainForDiagnostics(const AudioSettings& s,float distance){return s.spatial?judas_audio_distance_gain(int(s.attenuation),distance,s.referenceDistance,s.maximumDistance,s.rolloff):1;}
bool ValidAudioSettings(const AudioSettings& s){
 return std::isfinite(s.volume)&&s.volume>=0&&s.volume<=1&&std::isfinite(s.pitch)&&s.pitch>=.125f&&s.pitch<=8&&
 std::isfinite(s.referenceDistance)&&s.referenceDistance>0&&std::isfinite(s.maximumDistance)&&s.maximumDistance>s.referenceDistance&&
 std::isfinite(s.rolloff)&&s.rolloff>=0&&int(s.attenuation)>=0&&int(s.attenuation)<=2&&int(s.loading)>=0&&int(s.loading)<=1&&
 s.streamPageFrames>=1024&&s.streamPageFrames<=16384&&s.group.size()<=64&&std::isfinite(s.doppler)&&s.doppler>=0&&s.doppler<=4&&
 std::isfinite(s.send)&&s.send>=0&&s.send<=1&&std::isfinite(s.occludedGain)&&s.occludedGain>=0&&s.occludedGain<=1&&
 std::isfinite(s.occludedCutoff)&&s.occludedCutoff>=40&&s.occludedCutoff<=24000;
}
float AudioSystem::DopplerRatio(const glm::vec3& source,const glm::vec3& listener,const glm::vec3& sv,const glm::vec3& lv,float factor,float speed){
 auto d=listener-source;float n=glm::length(d);if(n<1e-5f||factor<=0||!Finite(d)||!Finite(sv)||!Finite(lv))return 1;
 d/=n;const float a=glm::clamp(factor*glm::dot(sv,d),-.75f*speed,.75f*speed),b=glm::clamp(factor*glm::dot(lv,d),-.75f*speed,.75f*speed);
 return glm::clamp((speed-b)/(speed-a),.5f,2.f);
}
struct AudioSystem::Impl {
 ma_engine engine{};bool initialized=false,noDevice=false,applicationSuspended=false,deviceSuspended=false;std::string initFailure;JobSystem* jobs=nullptr;
 std::uint64_t nextClip=1,nextVoice=1;
 std::map<std::uint64_t,std::shared_ptr<const AudioData>> clips;
 std::map<std::string,std::shared_ptr<AudioStreamMetadata>> metadata;
 struct Group {std::string name;AudioGroupSettings settings;std::atomic<float> gain{1};std::atomic<std::uint64_t> fade{0};};
 std::array<Group,17> groups;unsigned groupCount=1;
 glm::vec3 listenerVelocity{0};AudioListenerSnapshot listener;
 struct Output {
  ma_node_base base{};Impl* owner=nullptr;float gain=1,target=1;std::uint64_t left=0;
  static void Process(ma_node* n,const float** in,ma_uint32*,float** out,ma_uint32* count){auto& o=*reinterpret_cast<Output*>(n);
#ifdef __ANDROID__
   // Focus-only interruption mutes the final mix while consuming media. This
   // prevents paused one-shots accumulating and bursting when focus returns.
   const float platformGain=AndroidAudioFocused()?1.f:0.f;
#else
   constexpr float platformGain=1.f;
#endif
   float next=o.owner->groups[0].gain.load();if(next!=o.target){o.target=next;o.left=o.owner->groups[0].fade.load();}for(unsigned i=0;i<*count;++i){if(o.left){o.gain+=(o.target-o.gain)/float(o.left);--o.left;}else o.gain=o.target;for(unsigned c=0;c<2;++c)out[0][i*2+c]=(in[0]?in[0][i*2+c]:0)*o.gain*platformGain;}}
 } output;
 bool outputReady=false;
 struct Effect {
  ma_node_base base{};Impl* owner=nullptr;std::atomic<unsigned> group{0};
  std::atomic<float> occlusion{1},cutoff{24000},send{0};
  float smoothedGain=1,smoothedCut=24000,busGain=1,oldTarget=1,lp[2]{};std::uint64_t fadeLeft=0;
  std::atomic<float> publishedBus{1},resumeGain{1};std::atomic<uint64_t> publishedFade{0},resumeFade{0};std::atomic<bool> resumePending{false};
  static void Process(ma_node* n,const float** input,ma_uint32*,float** output,ma_uint32* count){
   auto& e=*reinterpret_cast<Effect*>(n);auto& a=*e.owner;const auto g=e.group.load();
   const float target=g?a.groups[g].gain.load():1.f;
   if(e.resumePending.exchange(false)){e.busGain=e.resumeGain.load();e.fadeLeft=e.resumeFade.load();e.oldTarget=target;}
   else if(target!=e.oldTarget){e.oldTarget=target;e.fadeLeft=a.groups[g].fade.load();}
   const float occ=e.occlusion.load(),cut=e.cutoff.load(),send=e.send.load();
   const float smooth=1-std::exp(-float(*count)/4800.f);e.smoothedGain+=(occ-e.smoothedGain)*smooth;e.smoothedCut+=(cut-e.smoothedCut)*smooth;
   const float pole=std::exp(-6.2831853f*std::min(e.smoothedCut,23999.f)/48000.f);
   for(unsigned i=0;i<*count;++i){
    if(e.fadeLeft){e.busGain+=(target-e.busGain)/float(e.fadeLeft);--e.fadeLeft;}else e.busGain=target;
    for(unsigned c=0;c<2;++c){const auto k=i*2+c;float x=input[0]?input[0][k]:0;
     if(e.smoothedCut<23900)e.lp[c]=(1-pole)*x+pole*e.lp[c];else e.lp[c]=x;
     x=e.lp[c]*e.smoothedGain*e.busGain;
     if(output[0])output[0][k]=x;
     if(output[1])output[1][k]=x*send;
    }
   }
   e.publishedBus.store(e.busGain);e.publishedFade.store(e.fadeLeft);
  }
 };
 struct Reverb {
  ma_node_base base{};verblib dsp{};
  std::atomic<float> room{.6f},damping{.4f},width{1},wet{0};float r=.6f,d=.4f,w=1,level=0;
  static void Process(ma_node* n,const float** input,ma_uint32*,float** out,ma_uint32* count){
   auto& a=*reinterpret_cast<Reverb*>(n);const float smooth=1-std::exp(-float(*count)/4800.f);
   a.r+=(a.room.load()-a.r)*smooth;a.d+=(a.damping.load()-a.d)*smooth;a.w+=(a.width.load()-a.w)*smooth;a.level+=(a.wet.load()-a.level)*smooth;
   verblib_set_room_size(&a.dsp,a.r);verblib_set_damping(&a.dsp,a.d);verblib_set_width(&a.dsp,a.w);verblib_set_wet(&a.dsp,a.level);verblib_set_dry(&a.dsp,0);
   verblib_process(&a.dsp,input[0],out[0],*count);
  }
 };
 std::unique_ptr<Reverb> reverb;
 // Immutable PCM with callback-owned cursor and atomic seek handoff.
 struct Buffered {
  ma_data_source_base base{};
  const AudioData* data=nullptr;
  std::atomic<ma_uint64> cursor{0},target{0},generation{1};
  std::atomic<bool> loop{false};
  ma_uint64 readCursor=0,readGeneration=0;
  static ma_result Read(ma_data_source* p,void* output,ma_uint64 count,ma_uint64* read){
   auto& b=*static_cast<Buffered*>(p);const auto gen=b.generation.load(std::memory_order_acquire);
   if(gen!=b.readGeneration){b.readCursor=b.target.load();b.readGeneration=gen;}
   auto* out=static_cast<float*>(output);ma_uint64 done=0;
   while(done<count){
    if(b.readCursor>=b.data->Frames()){if(b.loop.load())b.readCursor=0;else break;}
    const auto n=std::min<ma_uint64>(count-done,b.data->Frames()-b.readCursor);
    if(out)std::copy_n(b.data->samples.data()+b.readCursor*b.data->channels,n*b.data->channels,out+done*b.data->channels);
    b.readCursor+=n;done+=n;
   }
   if(gen==b.generation.load())b.cursor.store(b.readCursor);
   if(read)*read=done;
   return done<count?MA_AT_END:MA_SUCCESS;
  }
  static ma_result Seek(ma_data_source* p,ma_uint64 frame){auto& b=*static_cast<Buffered*>(p);if(frame>b.data->Frames())return MA_INVALID_ARGS;b.target.store(frame);b.cursor.store(frame);b.generation.fetch_add(1,std::memory_order_release);return MA_SUCCESS;}
  static ma_result BackendSeek(ma_data_source*,ma_uint64){return MA_SUCCESS;}
  static ma_result Format(ma_data_source* p,ma_format* f,ma_uint32* c,ma_uint32* r,ma_channel* map,size_t cap){auto& b=*static_cast<Buffered*>(p);if(f)*f=ma_format_f32;if(c)*c=b.data->channels;if(r)*r=48000;if(map)ma_channel_map_init_standard(ma_standard_channel_map_default,map,cap,b.data->channels);return MA_SUCCESS;}
  static ma_result Cursor(ma_data_source* p,ma_uint64* n){*n=static_cast<Buffered*>(p)->cursor.load();return MA_SUCCESS;}
  static ma_result Length(ma_data_source* p,ma_uint64* n){*n=static_cast<Buffered*>(p)->data->Frames();return MA_SUCCESS;}
  static ma_result Loop(ma_data_source* p,ma_bool32 v){static_cast<Buffered*>(p)->loop.store(v!=0);return MA_SUCCESS;}
  bool Init(const AudioData* d){data=d;static const ma_data_source_vtable table={Read,BackendSeek,Format,Cursor,Length,Loop,MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT};auto c=ma_data_source_config_init();c.vtable=&table;return ma_data_source_init(&c,&base)==MA_SUCCESS;}
 };
 struct Voice {
  std::shared_ptr<const AudioData> data;std::shared_ptr<PreparedAudioStream> stream;
  Buffered buffer;ma_sound sound{};Effect effect;
  bool paused=false,stopped=true,requested=false,autoRelease=false,bufferReady=false,soundReady=false,effectReady=false,groupPaused=false;
  AudioSettings settings;glm::vec3 velocity{0};float dopplerRatio=1,obstruction=0;
  ~Voice(){if(soundReady)ma_sound_uninit(&sound);if(effectReady)ma_node_uninit(&effect.base,nullptr);if(bufferReady)ma_data_source_uninit(&buffer.base);}
 };
 std::map<std::uint64_t,std::unique_ptr<Voice>> voices;
 struct Retirement {std::shared_ptr<PreparedAudioStream> stream;bool closing=false;};std::vector<Retirement> retired;
 std::size_t streamHighWater=0;double maxDetach=0;
 unsigned GroupIndex(const std::string& name)const{if(name.empty()||name=="master")return 0;for(unsigned i=1;i<groupCount;++i)if(groups[i].name==name)return i;return 17;}
 bool Sound(Voice& v,ma_data_source* source,std::string& error){
  if(ma_sound_init_from_data_source(&engine,source,MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT,nullptr,&v.sound)!=MA_SUCCESS){error="Audio voice initialization failed";return false;}v.soundReady=true;
  static const ma_node_vtable table={Effect::Process,nullptr,1,2,0};const ma_uint32 in[]={2},out[]={2,2};auto c=ma_node_config_init();c.vtable=&table;c.pInputChannels=in;c.pOutputChannels=out;
  v.effect.owner=this;
  if(ma_node_init(ma_engine_get_node_graph(&engine),&c,nullptr,&v.effect.base)!=MA_SUCCESS){error="Audio DSP node initialization failed";return false;}v.effectReady=true;
  ma_node_attach_output_bus(&v.sound,0,&v.effect.base,0);ma_node_attach_output_bus(&v.effect.base,0,&output.base,0);
  if(reverb)ma_node_attach_output_bus(&v.effect.base,1,&reverb->base,0);
  ma_sound_set_pinned_listener_index(&v.sound,0);ma_sound_set_doppler_factor(&v.sound,0);return true;
 }
};
AudioSystem::AudioSystem():m(std::make_unique<Impl>()){m->groups[0].name="master";}
AudioSystem::~AudioSystem(){Shutdown();}
void AudioSystem::SetJobSystem(JobSystem* jobs){m->jobs=jobs;}
bool AudioSystem::Init(std::string& error,bool noDevice){
 if(m->initialized){error.clear();return true;}if(!m->initFailure.empty()){error=m->initFailure;return false;}
 auto config=ma_engine_config_init();config.listenerCount=1;config.channels=2;config.sampleRate=48000;config.noDevice=noDevice;
 auto result=ma_engine_init(&config,&m->engine);if(result!=MA_SUCCESS){error="audio device/engine initialization failed: "+std::to_string(result);m->initFailure=error;return false;}
 m->output.owner=m.get();static const ma_node_vtable table={Impl::Output::Process,nullptr,1,1,0};const ma_uint32 channels[]={2};auto node=ma_node_config_init();node.vtable=&table;node.pInputChannels=channels;node.pOutputChannels=channels;
 if(ma_node_init(ma_engine_get_node_graph(&m->engine),&node,nullptr,&m->output.base)!=MA_SUCCESS){ma_engine_uninit(&m->engine);error="Master output node failed";return false;}m->outputReady=true;ma_node_attach_output_bus(&m->output.base,0,ma_node_graph_get_endpoint(ma_engine_get_node_graph(&m->engine)),0);
 m->initialized=true;m->noDevice=noDevice;m->deviceSuspended=false;SetApplicationSuspended(m->applicationSuspended);error.clear();return true;
}
void AudioSystem::Shutdown(){
 StopAll();m->clips.clear();
 // Shutdown is the only blocking drain; normal region removal never waits on jobs.
 if(m->jobs)while(!m->retired.empty()){Update();for(auto& r:m->retired)if(r.stream->job.IsValid())m->jobs->Wait(r.stream->job);}
 m->metadata.clear();if(m->outputReady){ma_node_uninit(&m->output.base,nullptr);m->outputReady=false;}if(m->initialized)ma_engine_uninit(&m->engine);m->initialized=false;m->initFailure.clear();
}
AudioClipHandle AudioSystem::CreateClip(AudioData data){if(!data.Frames()||data.channels>2||data.sampleRate!=48000)return {};const auto id=m->nextClip++;m->clips[id]=std::make_shared<const AudioData>(std::move(data));return {id};}
void AudioSystem::DestroyClip(AudioClipHandle c){m->clips.erase(c.id);}
AudioVoiceHandle AudioSystem::CreateVoice(AudioClipHandle c,const AudioSettings& s,std::string& error){
 auto data=m->clips.find(c.id);if(data==m->clips.end()){error="invalid or stale audio clip";return {};}if(!ValidAudioSettings(s)){error="invalid audio settings";return {};}if(m->GroupIndex(s.group)>16){error="missing audio group: "+s.group;return {};}if(m->voices.size()>=128){error="audio voice capacity reached (128)";return {};}
 if(!Init(error))return {};
 auto v=std::make_unique<Impl::Voice>();v->data=data->second;
 if(!v->buffer.Init(v->data.get())){error="Audio buffer initialization failed";return {};}v->bufferReady=true;
 if(!m->Sound(*v,&v->buffer,error))return {};
 auto id=m->nextVoice++;m->voices[id]=std::move(v);SetSettings({id},s);error.clear();return {id};
}
AudioVoiceHandle AudioSystem::CreateStreamVoice(const std::string& path,const AudioSettings& s,std::string& error){
 if(!m->jobs){error="audio stream requires Judas jobs";return {};}if(!ValidAudioSettings(s)){error="invalid audio settings";return {};}if(m->GroupIndex(s.group)>16){error="missing audio group: "+s.group;return {};}if(m->voices.size()>=128){error="audio voice capacity reached (128)";return {};}if(Diagnostics().streams>=32||m->retired.size()+Diagnostics().streams>=64){error="audio stream capacity reached (32 streams/64 retirements)";return {};}
 if(!Init(error))return {};
 auto& meta=m->metadata[path];if(!meta)meta=std::make_shared<AudioStreamMetadata>(path);
 auto v=std::make_unique<Impl::Voice>();v->stream=std::make_shared<PreparedAudioStream>(meta,s.streamPageFrames);
 if(!m->Sound(*v,v->stream.get(),error))return {};
 const auto id=m->nextVoice++;m->voices[id]=std::move(v);SetSettings({id},s);Update();m->streamHighWater=std::max(m->streamHighWater,Diagnostics().streamBytes);error.clear();return {id};
}
AudioVoiceHandle AudioSystem::PlayOneShot(AudioClipHandle c,const AudioSettings& s,const glm::vec3& p,std::string& error){AudioGroupSettings group,master;
 if(GetGroup(s.group,group)&&GetGroup("master",master)&&(group.paused||master.paused)){error.clear();return {};} // Ephemeral requests are dropped, never queued for resume.
 auto once=s;once.loop=false;auto h=CreateVoice(c,once,error);if(h.IsValid()){m->voices[h.id]->autoRelease=true;SetPosition(h,p);Play(h);}return h;}
void AudioSystem::DestroyVoice(AudioVoiceHandle h){
 auto it=m->voices.find(h.id);if(it==m->voices.end())return;const auto start=std::chrono::steady_clock::now();
 auto stream=it->second->stream;if(stream)stream->retired.store(true);m->voices.erase(it); // graph detach waits for current bounded mixer read, never for decoder jobs
 m->maxDetach=std::max(m->maxDetach,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
 if(stream)m->retired.push_back({std::move(stream),false});
}
bool AudioSystem::Play(AudioVoiceHandle h){auto it=m->voices.find(h.id);if(it==m->voices.end())return false;auto& v=*it->second;ma_sound_stop(&v.sound);
 if(v.stream)v.stream->Seek(0);else Impl::Buffered::Seek(&v.buffer,0);v.paused=false;v.stopped=false;v.requested=true;
 if(!v.stream&&!v.groupPaused)return ma_sound_start(&v.sound)==MA_SUCCESS;
 return true;}
bool AudioSystem::Stop(AudioVoiceHandle h){auto it=m->voices.find(h.id);if(it==m->voices.end())return false;auto& v=*it->second;ma_sound_stop(&v.sound);if(v.stream)v.stream->Seek(0);else Impl::Buffered::Seek(&v.buffer,0);v.paused=false;v.stopped=true;v.requested=false;return true;}
bool AudioSystem::Pause(AudioVoiceHandle h){auto it=m->voices.find(h.id);if(it==m->voices.end()||it->second->stopped)return false;ma_sound_stop(&it->second->sound);it->second->paused=true;return true;}
bool AudioSystem::Resume(AudioVoiceHandle h){auto it=m->voices.find(h.id);if(it==m->voices.end()||!it->second->paused)return false;it->second->paused=false;Update();return true;}
bool AudioSystem::Seek(AudioVoiceHandle h,double seconds){auto it=m->voices.find(h.id);if(it==m->voices.end()||!std::isfinite(seconds)||seconds<0||seconds>1e8)return false;
 AudioVoiceSnapshot snap;Snapshot(h,snap);if(snap.durationKnown&&seconds>snap.durationSeconds)return false;auto& v=*it->second;const auto frames=ma_uint64(seconds*48000);
 if(v.stream){ma_sound_stop(&v.sound);v.stream->Seek(frames);return true;}return Impl::Buffered::Seek(&v.buffer,frames)==MA_SUCCESS;}
bool AudioSystem::RestorePlayback(AudioVoiceHandle h,double seconds,AudioPlaybackState state,float gain,uint64_t fade){
 auto it=m->voices.find(h.id);if(it==m->voices.end()||gain<0||gain>1||fade>60*48000||!Seek(h,seconds))return false;auto& v=*it->second;
 ma_sound_stop(&v.sound);v.paused=state==AudioPlaybackState::Paused;v.stopped=state==AudioPlaybackState::Stopped||state==AudioPlaybackState::Finished;v.requested=state==AudioPlaybackState::Playing||state==AudioPlaybackState::Paused;
 v.effect.resumeGain.store(gain);v.effect.resumeFade.store(fade);v.effect.resumePending.store(true);return true;
}
bool AudioSystem::SetSettings(AudioVoiceHandle h,const AudioSettings& s){auto it=m->voices.find(h.id);if(it==m->voices.end()||!ValidAudioSettings(s)||m->GroupIndex(s.group)>16)return false;auto& v=*it->second;v.settings=s;auto* sound=&v.sound;
 if(v.stream){v.stream->loop.store(s.loop);ma_sound_set_looping(sound,s.loop);}else {v.buffer.loop.store(s.loop);ma_sound_set_looping(sound,s.loop);}
 ma_sound_set_spatialization_enabled(sound,s.spatial);ma_sound_set_volume(sound,s.volume);ma_sound_set_pitch(sound,glm::clamp(s.pitch*v.dopplerRatio,.125f,8.f));
 ma_sound_set_attenuation_model(sound,Model(s.attenuation));ma_sound_set_min_distance(sound,s.referenceDistance);ma_sound_set_max_distance(sound,s.maximumDistance);ma_sound_set_rolloff(sound,s.attenuation==AudioAttenuation::None?0:s.rolloff);
 v.effect.group.store(m->GroupIndex(s.group));v.effect.send.store(s.bypass?0:s.send);return SetOcclusion(h,v.obstruction);}
bool AudioSystem::SetPosition(AudioVoiceHandle h,const glm::vec3& p){auto it=m->voices.find(h.id);if(it==m->voices.end()||!Finite(p))return false;if(it->second->settings.spatial)ma_sound_set_position(&it->second->sound,p.x,p.y,p.z);return true;}
bool AudioSystem::SetMotion(AudioVoiceHandle h,const glm::vec3& velocity){auto it=m->voices.find(h.id);if(it==m->voices.end()||!Finite(velocity))return false;auto& v=*it->second;v.velocity=velocity;
 v.dopplerRatio=v.settings.spatial?DopplerRatio(Vector(ma_sound_get_position(&v.sound)),m->listener.position,velocity,m->listenerVelocity,v.settings.doppler):1;
 ma_sound_set_pitch(&v.sound,glm::clamp(v.settings.pitch*v.dopplerRatio,.125f,8.f));return true;}
bool AudioSystem::SetOcclusion(AudioVoiceHandle h,float obstruction){auto it=m->voices.find(h.id);if(it==m->voices.end()||!std::isfinite(obstruction))return false;auto& v=*it->second;v.obstruction=glm::clamp(obstruction,0.f,1.f);
 const float mix=v.settings.occlusion&&v.settings.spatial&&!v.settings.bypass?v.obstruction:0;
 v.effect.occlusion.store(1+mix*(v.settings.occludedGain-1));v.effect.cutoff.store(24000+mix*(v.settings.occludedCutoff-24000));return true;}
bool AudioSystem::SetListener(const glm::vec3& p,const glm::quat& orientation){const auto length=glm::dot(orientation,orientation);if(!m->initialized||!Finite(p)||!std::isfinite(length)||length<=0)return false;
 auto q=glm::normalize(orientation);auto forward=q*glm::vec3(0,0,-1),up=q*glm::vec3(0,1,0);m->listener={p,forward,up};
 ma_engine_listener_set_position(&m->engine,0,p.x,p.y,p.z);ma_engine_listener_set_direction(&m->engine,0,forward.x,forward.y,forward.z);ma_engine_listener_set_world_up(&m->engine,0,up.x,up.y,up.z);return true;}
bool AudioSystem::SetListenerMotion(const glm::vec3& v){if(!Finite(v))return false;m->listenerVelocity=v;return true;}
AudioListenerSnapshot AudioSystem::Listener()const{return m->listener;}
bool AudioSystem::Snapshot(AudioVoiceHandle h,AudioVoiceSnapshot& out)const{auto it=m->voices.find(h.id);if(it==m->voices.end())return false;const auto& v=*it->second;const auto* s=&v.sound;out={};
 out.state=v.stopped?AudioPlaybackState::Stopped:(v.stream?v.stream->ended.load():ma_sound_at_end(s))?AudioPlaybackState::Finished:v.paused||v.groupPaused?AudioPlaybackState::Paused:ma_sound_is_playing(s)?AudioPlaybackState::Playing:AudioPlaybackState::Stopped;
 out.groupGain=v.effect.publishedBus.load();out.groupFadeFrames=v.effect.publishedFade.load();out.streamed=bool(v.stream);out.ready=!v.stream||(v.stream->ready.load()&&v.stream->readyGeneration.load()==v.stream->generation.load());out.seeking=v.stream&&v.stream->readyGeneration.load()>0&&v.stream->generation.load()!=v.stream->readyGeneration.load();
 if(v.stream){out.cursorAvailable=true;out.cursorFrames=v.stream->cursor.load();out.bufferBytes=v.stream->BufferBytes();out.underruns=v.stream->underruns.load();out.durationKnown=v.stream->metadata->length.load()>0;out.durationSeconds=v.stream->metadata->length.load()/48000.;out.starved=v.stream->starved.load();if(v.stream->failed.load(std::memory_order_acquire))out.error=v.stream->error;}
 else{out.cursorAvailable=true;out.cursorFrames=v.buffer.cursor.load();out.durationKnown=true;out.durationSeconds=v.data->Frames()/48000.;}
 out.positionSeconds=out.cursorFrames/48000.;out.position=Vector(ma_sound_get_position(s));out.spatial=v.settings.spatial;out.loop=v.settings.loop;out.volume=v.settings.volume;out.pitch=v.settings.pitch;out.referenceDistance=v.settings.referenceDistance;out.maximumDistance=v.settings.maximumDistance;out.rolloff=v.settings.attenuation==AudioAttenuation::None?0:v.settings.rolloff;out.attenuation=v.settings.attenuation;out.spatialPanningEnabled=out.spatial;
 out.dopplerRatio=v.dopplerRatio;out.occlusionGain=v.effect.occlusion.load();out.cutoff=v.effect.cutoff.load();out.distanceGain=DistanceGainForDiagnostics(v.settings,glm::distance(out.position,m->listener.position));return true;
}
bool AudioSystem::SetGroup(const std::string& name,const AudioGroupSettings& s,float seconds){unsigned i=m->GroupIndex(name);if(i>16||!std::isfinite(s.gain)||s.gain<0||s.gain>1||!std::isfinite(seconds)||seconds<0||seconds>60)return false;
 m->groups[i].settings=s;m->groups[i].fade.store(std::uint64_t(seconds*48000));m->groups[i].gain.store(s.mute||(i==0&&s.paused)?0:s.gain);Update();return true;}
bool AudioSystem::GetGroup(const std::string& name,AudioGroupSettings& s)const{auto i=m->GroupIndex(name);if(i>16)return false;s=m->groups[i].settings;return true;}
void AudioSystem::ConfigureGroups(const ProjectAudioSettings& s){m->groupCount=1;SetGroup("master",{});for(auto& [name,g]:s.groups){auto i=m->groupCount++;m->groups[i].name=name;m->groups[i].settings=g;m->groups[i].gain.store(g.mute?0:g.gain);}}
void AudioSystem::SetEnvironment(const AudioEnvironmentSettings& s,float weight){if(!m->initialized||!ValidAudioEnvironment(s)||!std::isfinite(weight))return;
 if(!m->reverb&&weight>0){auto r=std::make_unique<Impl::Reverb>();if(!verblib_initialize(&r->dsp,48000,2))return;
 static const ma_node_vtable table={Impl::Reverb::Process,nullptr,1,1,MA_NODE_FLAG_CONTINUOUS_PROCESSING};const ma_uint32 channels[]={2};auto config=ma_node_config_init();config.vtable=&table;config.pInputChannels=channels;config.pOutputChannels=channels;
 if(ma_node_init(ma_engine_get_node_graph(&m->engine),&config,nullptr,&r->base)!=MA_SUCCESS)return;
 ma_node_attach_output_bus(&r->base,0,&m->output.base,0);m->reverb=std::move(r);
 for(auto& [id,v]:m->voices){(void)id;ma_node_attach_output_bus(&v->effect.base,1,&m->reverb->base,0);}
 }
 if(m->reverb){m->reverb->room.store(s.roomSize);m->reverb->damping.store(s.damping);m->reverb->width.store(s.width);m->reverb->wet.store(s.wet*glm::clamp(weight,0.f,1.f));}
}
void AudioSystem::ResetEnvironment(){if(m->reverb){ma_node_uninit(&m->reverb->base,nullptr);m->reverb.reset();}}
void AudioSystem::Update(){
 JUDAS_PROFILE_SCOPE("Audio control and retirement");
 for(auto it=m->retired.begin();it!=m->retired.end();){auto& r=*it;auto& s=*r.stream;
  if(!m->jobs){++it;continue;}
  if(s.job.IsValid()&&!m->jobs->IsFinished(s.job)){++it;continue;}
  if(s.job.IsValid())m->jobs->Forget(s.job);
  if(r.closing){it=m->retired.erase(it);continue;}
  auto stream=r.stream;s.job=m->jobs->Submit([stream](JobContext&){JUDAS_PROFILE_SCOPE("Audio stream retirement worker");stream->Close();},JobPriority::Normal,"audio stream retirement");r.closing=true;++it;
 }
 for(auto it=m->voices.begin();it!=m->voices.end();){auto h=AudioVoiceHandle{it->first};auto& v=*it->second;
  if(v.autoRelease&&ma_sound_at_end(&v.sound)){++it;DestroyVoice(h);continue;}
  if(v.stream&&m->jobs&&!v.stream->failed.load()&&!v.stream->retired.load()){
   auto s=v.stream;if(!s->job.IsValid()||m->jobs->IsFinished(s->job)){if(s->job.IsValid()){m->jobs->Forget(s->job);s->job={};}
    bool needed=s->workerGeneration!=s->generation.load();
    if(!s->workerAtEnd)for(auto& page:s->pages)needed|=page.state.load()==0;
    if(needed)s->job=m->jobs->Submit([s](JobContext& c){s->Fill(c);},JobPriority::High,"audio stream pages");}
  }
  const auto g=m->GroupIndex(v.settings.group);v.groupPaused=m->groups[0].settings.paused||(g<17&&m->groups[g].settings.paused);
  if(v.groupPaused||v.paused||v.stopped||(v.stream&&v.stream->failed.load())){if(ma_sound_is_playing(&v.sound))ma_sound_stop(&v.sound);}
  else if(v.requested&&(!v.stream||(v.stream->ready.load()&&v.stream->readyGeneration.load()==v.stream->generation.load()))&&!(v.stream?v.stream->ended.load():ma_sound_at_end(&v.sound))&&!ma_sound_is_playing(&v.sound))ma_sound_start(&v.sound);
  ++it;
 }
 for(auto it=m->metadata.begin();it!=m->metadata.end();)if(it->second.use_count()==1)it=m->metadata.erase(it);else ++it;
 auto stats=Diagnostics();JUDAS_PROFILE_COUNTER("Audio voices",double(stats.voices),ProfileCounterMode::Latest);JUDAS_PROFILE_COUNTER("Audio stream PCM bytes",double(stats.streamBytes),ProfileCounterMode::Latest);JUDAS_PROFILE_COUNTER("Audio pending retirements",double(stats.pendingRetirements),ProfileCounterMode::Latest);
}
void AudioSystem::StopAll(){while(!m->voices.empty())DestroyVoice({m->voices.begin()->first});ResetEnvironment();}
bool AudioSystem::IsInitialized()const{return m->initialized;}
std::size_t AudioSystem::VoiceCount()const{return m->voices.size();}
std::size_t AudioSystem::ClipCount()const{return m->clips.size();}
AudioDiagnostics AudioSystem::Diagnostics()const{AudioDiagnostics d;d.voices=m->voices.size();d.pendingRetirements=m->retired.size();d.reverbProcessors=m->reverb?1:0;d.maximumDetachMilliseconds=m->maxDetach;d.streamHighWater=m->streamHighWater;
 for(auto& [id,v]:m->voices){(void)id;if(v->stream){++d.streams;d.streamBytes+=v->stream->BufferBytes();d.underruns+=v->stream->underruns.load();d.decodedFrames+=v->stream->decodedFrames.load();}}
 std::set<const AudioData*> data;for(auto& [id,clip]:m->clips){(void)id;data.insert(clip.get());}for(auto& [id,v]:m->voices){(void)id;if(v->data)data.insert(v->data.get());}for(auto* clip:data)d.bufferedBytes+=clip->samples.size()*sizeof(float);for(auto& retired:m->retired)d.streamBytes+=retired.stream->BufferBytes();return d;}
bool AudioSystem::ReadWithoutDevice(float* out,std::uint64_t frames){return m->initialized&&m->noDevice&&out&&ma_engine_read_pcm_frames(&m->engine,out,frames,nullptr)==MA_SUCCESS;}
bool AudioSystem::AdvanceWithoutDevice(std::uint64_t frames){float scratch[512*2];while(frames){auto n=std::min<std::uint64_t>(frames,512);if(!ReadWithoutDevice(scratch,n))return false;frames-=n;}return true;}
bool AudioSystem::SetStreamDecodeDelay(AudioVoiceHandle h,unsigned ms){auto it=m->voices.find(h.id);if(it==m->voices.end()||!it->second->stream||ms>500)return false;it->second->stream->delayMilliseconds.store(ms);return true;}

void AudioSystem::SetApplicationSuspended(bool suspended){
 m->applicationSuspended=suspended;
 if(!m->initialized||suspended==m->deviceSuspended)return;
 const auto result=suspended?ma_engine_stop(&m->engine):ma_engine_start(&m->engine);
 if(result==MA_SUCCESS)m->deviceSuspended=suspended;
 else std::fprintf(stderr,"Audio device suspend/resume failed: %d\n",int(result));
}
