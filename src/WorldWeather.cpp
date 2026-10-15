#include "RuntimeWorld.h"
#include "PerformanceProfiler.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {
using V=glm::dvec3;using J=nlohmann::json;
bool Finite(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
J Vector(V v){return J::array({v.x,v.y,v.z});}
V Vector(const J& j){if(!j.is_array()||j.size()!=3)throw std::invalid_argument("weather saved vector shape");V v(j.at(0).get<double>(),j.at(1).get<double>(),j.at(2).get<double>());if(!Finite(v))throw std::invalid_argument("weather saved vector finite");return v;}
}
WeatherSettings RuntimeWorld::ResolveWeatherSettings(const SceneObject& o,V origin){
    auto s=*o.weather;const auto q=glm::dquat(glm::normalize(o.transform.rotation));
    const V at=origin+V(o.transform.position),scale(o.transform.scale);
    s.regionCenter=at+q*(s.regionCenter*scale);
    s.regionRotation=glm::normalize(q*s.regionRotation);
    s.regionHalfExtents*=glm::abs(scale);s.regionRadius*=std::max({std::abs(scale.x),std::abs(scale.y),std::abs(scale.z)});
    if(!s.frame.radial)s.frame.center=at+q*s.frame.center;
    s.frame.up=q*s.frame.up;s.frame.north=q*s.frame.north;
    s.profile.wind=q*s.profile.wind;s.tornadoDirection=q*s.tornadoDirection;
    return s;
}
bool RuntimeWorld::RegisterWeather(const SceneObject& o,std::string& error){
    if(o.weather){
        if(!ValidateWeatherSettings(*o.weather,error))return false;
        if(!m_weather.count(o.id)&&m_weather.size()>=16){error="at most 16 resident weather regions";return false;}
        if(!Finite(V(o.transform.scale))||glm::any(glm::lessThanEqual(glm::abs(o.transform.scale),glm::vec3(1e-6f)))){error="weather region requires nonsingular scale";return false;}
        auto resolved=ResolveWeatherSettings(o,m_settings.worldOrigin);
        if(!ValidateWeatherSettings(resolved,error))return false;
        if(m_weather.empty())m_weatherListener=m_settings.worldOrigin+V(o.transform.position);
        WeatherInstance record;record.runtime=std::make_unique<WeatherRuntime>(std::move(resolved));record.ownerAbsolute=m_settings.worldOrigin+V(o.transform.position);record.ownerRotation=o.transform.rotation;record.ownerScale=o.transform.scale;
        m_weather[o.id]=std::move(record);
    }
    if(o.weatherResponse){
        if(!ValidateWeatherResponseSettings(*o.weatherResponse,error))return false;
        if(!o.body||o.body->motion!=SceneBodyMotion::Dynamic){error="weather response needs an ordinary dynamic body";return false;}
        if(!m_weatherResponses.count(o.id)&&m_weatherResponses.size()>=64){error="at most 64 opted-in resident wind response bodies";return false;}
        m_weatherResponses.insert(o.id);
    }
    error.clear();return true;
}
WeatherRuntime* RuntimeWorld::RuntimeWeather(EntityId id){
    if(!RuntimeDefinition(id))return nullptr;
    auto it=m_weather.find(id);return it==m_weather.end()?nullptr:it->second.runtime.get();
}
const WeatherRuntime* RuntimeWorld::Weather(EntityId id)const{
    if(!RuntimeDefinition(id))return nullptr;
    auto it=m_weather.find(id);return it==m_weather.end()?nullptr:it->second.runtime.get();
}
WeatherSample RuntimeWorld::SampleWeather(V point)const{
    WeatherSample out;if(!Finite(point)||glm::any(glm::greaterThan(glm::abs(point),V(1.01e12))))return out;
    std::vector<std::pair<EntityId,const WeatherRuntime*>> regions;
    for(const auto& [id,r]:m_weather)if(IsPublished(id)&&RuntimeDefinition(id)&&r.runtime->Settings().enabled)regions.emplace_back(id,r.runtime.get());
    std::sort(regions.begin(),regions.end(),[](const auto& a,const auto& b){auto p=a.second->Settings().priority,q=b.second->Settings().priority;return p==q?a.first<b.first:p<q;});
    // Each higher-priority region smoothly replaces the resolved lower stack.
    // Stable entity order resolves equal-priority overlaps without hash order.
    WeatherProfile clear;clear.cloudCoverage=0;clear.cloudDensity=0;clear.wind=V(0);clear.gust=0;out.profile=clear;
    for(const auto& [id,r]:regions){(void)id;const auto sample=r->Sample(point);if(!sample.active||sample.weight<=0)continue;const double w=sample.weight;
        const auto old=out;out=sample;out.profile=BlendWeatherProfiles(old.profile,sample.profile,w);out.cloudOffset=glm::mix(old.cloudOffset,sample.cloudOffset,w);out.wind=glm::mix(old.wind,sample.wind/w,w);out.airDensity=glm::mix(old.airDensity,sample.airDensity/w,w);out.lightningFlash=glm::mix(old.lightningFlash,sample.lightningFlash,w);out.weight=old.weight+(1-old.weight)*w;out.active=true;
    }
    return out;
}
void RuntimeWorld::UpdateWeather(float dt){
    if(m_weather.empty())return;
    JUDAS_PROFILE_SCOPE("Weather fixed fields and forces");
    for(auto& [id,record]:m_weather){
        if(!IsPublished(id))continue;
        const auto* o=RuntimeDefinition(id);if(!o)continue;
        auto spatial=*o;EntityPhysicalState pose;
        if(GetEntityState(id,pose)){spatial.transform.position=pose.position;spatial.transform.rotation=pose.rotation;}
        const auto absolute=m_settings.worldOrigin+V(spatial.transform.position);
        if(absolute!=record.ownerAbsolute||spatial.transform.rotation!=record.ownerRotation||spatial.transform.scale!=record.ownerScale){
            // Rebind only the spatial frame; retain explicit runtime intent and
            // already emitted absolute strikes. An origin shift alone cancels.
            const auto saved=record.runtime->Capture();auto next=std::make_unique<WeatherRuntime>(ResolveWeatherSettings(spatial,m_settings.worldOrigin));std::string error;
            if(!next->Restore(saved,error))throw std::runtime_error(error);
            record.runtime=std::move(next);record.ownerAbsolute=absolute;record.ownerRotation=spatial.transform.rotation;record.ownerScale=spatial.transform.scale;
        }
        record.runtime->Advance(dt);const auto& event=record.runtime->State().lightning;
        if(event.sequence!=record.observedLightning){
            record.observedLightning=event.sequence;
            if(event.time>=0&&record.thunder.size()<8){const double delay=glm::length(event.position-m_weatherListener)/343.3;record.thunder.push_back({event.time+delay,event.position,event.energy});}
        }
    }
    for(auto id:m_weatherResponses){
        const auto* o=RuntimeDefinition(id);if(!o||!o->weatherResponse||!o->weatherResponse->enabled||!IsPublished(id))continue;
        const auto body=RuntimeBody(id);if(!m_physics.IsDynamicBody(body)||!m_physics.IsBodyEnabled(body))continue;
        // M26 already applies whole-body atmosphere drag to its celestial
        // vehicle. Never add a second whole-body drag force to that body.
        if(m_vehicle&&m_atmosphere&&m_vehicle->handle.id==body.id&&m_vehicle->component.gravity==SceneVehicleGravity::Celestial)continue;
        const auto position=m_physics.GetTransform(body).position;const auto sample=SampleWeather(m_settings.worldOrigin+V(position));const auto velocity=m_physics.GetLinearVelocity(body);const auto relative=sample.wind-V(velocity);if(!sample.active||glm::length(relative)<1e-5)continue;
        PhysicsQueryFilter filter;filter.includeLayers=o->weatherResponse->occlusionMask;filter.ignoredBodies.push_back(body);
        const auto incoming=glm::vec3(glm::normalize(relative));const auto hit=m_physics.Raycast(position,-incoming,12,filter);++m_weatherWork.forceQueries;
        const auto force=WeatherDragForce(sample,V(velocity),m_physics.GetMass(body),*o->weatherResponse,hit.hit?.1:1);
        if(glm::length(force)>1e-6){m_physics.ApplyForce(body,glm::vec3(force));++m_weatherWork.forces;}
    }
}
void RuntimeWorld::PrepareWeather(Renderer& renderer,V eye,uint64_t camera)const{
    const auto sample=SampleWeather(eye);m_weatherListener=camera?m_weatherListener:eye;
    if(!sample.active||weatherQuality==0){renderer.ClearWeather();return;}
    renderer.SetWeather(sample,eye,camera?std::min(1u,weatherQuality):weatherQuality);
    if(camera||sample.profile.rain+sample.profile.snow<.001){renderer.SetWeatherShelter({});return;}
    JUDAS_PROFILE_SCOPE("Weather shelter queries");
    auto& cache=m_weatherShelter;const auto up=glm::vec3(sample.up);
    if(!cache.grid.valid||glm::distance(cache.grid.center,eye)>8||glm::dot(cache.grid.up,up)<.9999f){
        const auto oldRevision=cache.grid.revision;cache={};cache.grid.center=eye;cache.grid.up=up;
        auto north=glm::vec3(sample.frame.north);north-=up*glm::dot(north,up);if(glm::length(north)<.01f)north=glm::cross(up,std::abs(up.x)<.8f?glm::vec3(1,0,0):glm::vec3(0,0,1));
        cache.grid.forward=glm::normalize(north);cache.grid.right=glm::cross(cache.grid.forward,up);cache.grid.extent=32;cache.grid.valid=true;cache.grid.revision=oldRevision+1;
    }
    for(unsigned i=0;i<16;++i){const auto cell=cache.cursor++%256;const double x=(double(cell%16)+.5)/16*2-1,z=(double(cell/16)+.5)/16*2-1;
        const auto absolute=cache.grid.center+V(cache.grid.right)*(x*cache.grid.extent*.5)+V(cache.grid.forward)*(z*cache.grid.extent*.5)+V(up)*80.;
        const auto hit=m_physics.Raycast(glm::vec3(absolute-m_settings.worldOrigin),-up,160);
        cache.grid.heights[cell]=hit.hit?80-hit.distance:-10000; ++m_weatherWork.shelterQueries;
    }
    ++cache.grid.revision;renderer.SetWeatherShelter(cache.grid);
}
bool RuntimeWorld::CaptureWeather(EntityId id,std::string& text,std::string& error){
    try{auto it=m_weather.find(id);if(it==m_weather.end()){text.clear();error.clear();return true;}J queue=J::array();for(const auto& t:it->second.thunder)queue.push_back({{"due",t.due},{"position",Vector(t.position)},{"energy",t.energy}});
        text=J{{"version",1},{"runtime",it->second.runtime->Capture()},{"observed",it->second.observedLightning},{"thunder",queue}}.dump();error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool RuntimeWorld::RestoreWeather(EntityId id,const std::string& text,std::string& error,bool staged){
    try{if(text.empty())throw std::runtime_error("weather continuation missing");if(!staged&&!RuntimeDefinition(id))throw std::runtime_error("stale weather owner");auto it=m_weather.find(id);if(it==m_weather.end())throw std::runtime_error("weather component absent");
        if(text.size()>65536)throw std::runtime_error("weather continuation byte bound");
        const auto j=J::parse(text);if(j.at("version")!=1||j.size()!=4||!j.at("thunder").is_array()||j.at("thunder").size()>8)throw std::runtime_error("weather continuation version/shape");
        auto next=std::make_unique<WeatherRuntime>(it->second.runtime->Settings());if(!next->Restore(j.at("runtime").get<std::string>(),error))return false;
        std::vector<WeatherThunder> queue;for(const auto& t:j.at("thunder")){const auto due=t.at("due").get<double>(),energy=t.at("energy").get<double>();if(t.size()!=3||!std::isfinite(due)||due<0||!std::isfinite(energy)||energy<0||energy>1)throw std::runtime_error("invalid pending thunder");queue.push_back({due,Vector(t.at("position")),energy});}
        const auto observed=j.at("observed").get<uint64_t>();if(observed>next->State().lightning.sequence)throw std::runtime_error("weather observed event sequence exceeds source");
        it->second.runtime=std::move(next);it->second.observedLightning=observed;it->second.thunder=std::move(queue);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void RuntimeWorld::EndWeatherAudio(){
    if(m_audioSystem){for(auto voice:m_weatherVoices)m_audioSystem->DestroyVoice(voice.voice);if(m_weatherThunderClip.IsValid())m_audioSystem->DestroyClip(m_weatherThunderClip);}
    m_weatherVoices.clear();m_weatherThunderClip={};
}
void RuntimeWorld::UpdateWeatherAudio(V listener,bool valid){
    if(!valid||!m_audioSystem||!m_audioRunning||m_weather.empty())return;
    m_weatherListener=listener;
    for(auto it=m_weatherVoices.begin();it!=m_weatherVoices.end();){AudioVoiceSnapshot s;if(!m_audioSystem->Snapshot(it->voice,s)||s.state==AudioPlaybackState::Finished){m_audioSystem->DestroyVoice(it->voice);it=m_weatherVoices.erase(it);}else {m_audioSystem->SetPosition(it->voice,glm::vec3(it->absolute-m_settings.worldOrigin));++it;}}
    for(auto& [id,r]:m_weather){if(!IsPublished(id))continue;
        for(auto it=r.thunder.begin();it!=r.thunder.end();){if(it->due>r.runtime->State().time){++it;continue;}
            if(m_weatherVoices.size()<8){
                if(!m_weatherThunderClip.IsValid()){
                    // Original bounded procedural rumble through the existing
                    // audio mixer. It is not a new audio backend or asset type.
                    AudioData data;data.channels=1;data.sampleRate=48000;data.samples.resize(144000);uint32_t seed=0x8d72354a;float low=0;
                    for(size_t i=0;i<data.samples.size();++i){seed=1664525*seed+1013904223;const float noise=float(seed>>8)/float(0x7fffff)-1;low=low*.985f+noise*.015f;const float t=float(i)/data.sampleRate,envelope=std::min(1.f,t*35)*std::exp(-t*1.4f);data.samples[i]=(low*4+noise*.035f)*envelope;}
                    m_weatherThunderClip=m_audioSystem->CreateClip(std::move(data));
                }
                AudioSettings settings;settings.spatial=true;settings.referenceDistance=80;settings.maximumDistance=16000;settings.volume=float(std::clamp(it->energy,0.,1.));settings.group="master";std::string error;
                auto voice=m_audioSystem->PlayOneShot(m_weatherThunderClip,settings,glm::vec3(it->position-m_settings.worldOrigin),error);if(voice.IsValid()){m_weatherVoices.push_back({voice,it->position});++m_weatherWork.thunderPlayed;}
            }
            it=r.thunder.erase(it);
        }
    }
}
