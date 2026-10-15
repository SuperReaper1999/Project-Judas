#pragma once
#include <memory>
#include <string>
#include <glm/gtc/quaternion.hpp>
#include "AudioTypes.h"
#include "AudioData.h"

// Single engine audio owner. Main-thread API; miniaudio owns device/mixing threads.
// No backend object crosses this boundary. Device opens lazily at first voice.
class JobSystem;
class AudioSystem {
public:
    AudioSystem(); ~AudioSystem();
    AudioSystem(const AudioSystem&)=delete;AudioSystem& operator=(const AudioSystem&)=delete;
    bool Init(std::string& error, bool noDevice=false);
    void Shutdown();
    void SetJobSystem(JobSystem* jobs);
    AudioVoiceHandle CreateStreamVoice(const std::string& path,const AudioSettings&,std::string& error);
    bool Seek(AudioVoiceHandle,double seconds);
    bool RestorePlayback(AudioVoiceHandle,double seconds,AudioPlaybackState,float groupGain=1,uint64_t fadeFrames=0);
    bool SetMotion(AudioVoiceHandle,const glm::vec3& velocity);
    bool SetListenerMotion(const glm::vec3& velocity);
    bool SetOcclusion(AudioVoiceHandle,float obstruction);
    bool SetGroup(const std::string&,const AudioGroupSettings&,float fadeSeconds=0);
    bool GetGroup(const std::string&,AudioGroupSettings&)const;
    void ConfigureGroups(const ProjectAudioSettings&);
    void SetEnvironment(const AudioEnvironmentSettings&,float weight);
    void ResetEnvironment();
    AudioDiagnostics Diagnostics()const;
    bool ReadWithoutDevice(float* stereo,std::uint64_t frames);
    bool SetStreamDecodeDelay(AudioVoiceHandle,unsigned milliseconds);
    static float DopplerRatio(const glm::vec3& source,const glm::vec3& listener,const glm::vec3& sourceVelocity,const glm::vec3& listenerVelocity,float scale,float speed=343.3f);

    AudioClipHandle CreateClip(AudioData data);
    void DestroyClip(AudioClipHandle clip); // existing voices retain immutable PCM
    AudioVoiceHandle CreateVoice(AudioClipHandle clip,const AudioSettings& settings,std::string& error);
    AudioVoiceHandle PlayOneShot(AudioClipHandle clip,const AudioSettings& settings,const glm::vec3& position,std::string& error);
    void DestroyVoice(AudioVoiceHandle voice);
    bool Play(AudioVoiceHandle voice);bool Stop(AudioVoiceHandle voice);
    void SetApplicationSuspended(bool suspended);
    bool Pause(AudioVoiceHandle voice);bool Resume(AudioVoiceHandle voice);
    bool SetSettings(AudioVoiceHandle voice,const AudioSettings& settings);
    bool SetPosition(AudioVoiceHandle voice,const glm::vec3& position);
    bool SetListener(const glm::vec3& position,const glm::quat& orientation);
    bool Snapshot(AudioVoiceHandle voice,AudioVoiceSnapshot& out) const;
    AudioListenerSnapshot Listener() const;
    void Update();void StopAll();
    static float DistanceGainForDiagnostics(const AudioSettings& settings,float distance);
    bool IsInitialized() const;
    std::size_t VoiceCount() const;std::size_t ClipCount() const;
    // Deterministic device-free test/offline seam. Same backend mixer/voices;
    // consumes samples without a recording or any perceptual assertion.
    bool AdvanceWithoutDevice(std::uint64_t frames);
private:
    struct Impl;std::unique_ptr<Impl> m;
};
