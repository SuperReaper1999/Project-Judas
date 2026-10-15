#include "Weather.h"
#include "../third_party/nlohmann/json.hpp"
#include <glm/gtx/quaternion.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
using V=glm::dvec3;using J=nlohmann::ordered_json;
unsigned checks=0,failures=0;
void Check(bool value,const char* label){++checks;failures+=!value;std::printf("%s %s\n",value?"PASS":"FAIL",label);}
bool Near(V a,V b,double epsilon=1e-9){return glm::length(a-b)<epsilon;}
template<class F> bool Rejects(F f){try{f();return false;}catch(const std::invalid_argument&){return true;}}
WeatherSettings Rotated(const WeatherSettings& a,const glm::dquat& q,V center){WeatherSettings b=a;b.frame.center=center+q*a.frame.center;b.frame.up=q*a.frame.up;b.frame.north=q*a.frame.north;b.profile.wind=q*a.profile.wind;b.tornadoDirection=q*a.tornadoDirection;b.regionCenter=center+q*a.regionCenter;b.regionRotation=q*a.regionRotation;return b;}
}
int main(){std::string error;WeatherSettings settings;
 Check(ValidateWeatherSettings(settings,error),"default profile/frame validates");
 for(const char* name:{"sunny","cloudy","rain","snow","blizzard","thunderstorm","tornado","fog"}){WeatherProfile profile;Check(WeatherPreset(name,profile)&&ValidateWeatherProfile(profile,error),name);}
 WeatherProfile sentinel;sentinel.rain=.345;Check(!WeatherPreset("made-up",sentinel)&&sentinel.rain==.345,"unknown preset preserves output instead of inventing mode");
 auto encoded=EncodeWeatherSettings(settings);WeatherSettings decoded;
 Check(DecodeWeatherSettings(encoded,decoded,error)&&EncodeWeatherSettings(decoded)==encoded,"settings strict JSON roundtrip");
 Check(DecodeWeatherSettings("{\"version\":1}",decoded,error)&&decoded.profile.cloudCoverage==settings.profile.cloudCoverage,"partial authored descriptor uses documented defaults");
 auto bad=J::parse(encoded);bad["cloudsActuallyNotExposed"]=true;
 Check(!DecodeWeatherSettings(bad.dump(),decoded,error),"unknown top-level property rejected");bad=J::parse(encoded);bad["profile"]["cloudCoverage"]=2;
 auto before=EncodeWeatherSettings(decoded);Check(!DecodeWeatherSettings(bad.dump(),decoded,error)&&EncodeWeatherSettings(decoded)==before,"invalid intensity cannot partially publish output");
 bad=J::parse(encoded);bad["frame"]["up"]={0,0,0};Check(!DecodeWeatherSettings(bad.dump(),decoded,error),"undefined up rejected");
 bad=J::parse(encoded);bad["frame"]["north"]={0,1,0};Check(!DecodeWeatherSettings(bad.dump(),decoded,error),"parallel north/up rejected");
 bad=J::parse(encoded);bad["enabled"]=1;Check(!DecodeWeatherSettings(bad.dump(),decoded,error),"numeric enabled cannot masquerade as Boolean");
 bad=J::parse(encoded);bad["seed"]=-1;Check(!DecodeWeatherSettings(bad.dump(),decoded,error),"negative seed rejected without wrapping");
 WeatherResponseSettings response,roundtrip;response.occlusionMask=(uint64_t{1}<<63)|3;
 Check(DecodeWeatherResponseSettings(EncodeWeatherResponseSettings(response),roundtrip,error)&&roundtrip.occlusionMask==response.occlusionMask,"full M39 64-bit occlusion mask preserved");
 Check(!DecodeWeatherResponseSettings("{\"version\":1,\"occlusionMask\":-1}",roundtrip,error),"negative occlusion mask rejected");
 Check(!DecodeWeatherResponseSettings("{\"version\":1,\"area\":-1}",roundtrip,error),"negative drag area rejected");

 WeatherRuntime runtime(settings);WeatherProfile rain;WeatherPreset("rain",rain);runtime.Transition(rain,2);
 Check(runtime.Current().rain==0,"transition begins at previous evaluated profile");runtime.Advance(1);
 Check(std::abs(runtime.Current().rain-rain.rain*.5)<1e-12,"smooth transition midpoint interpolates actual profile");
 const auto middle=runtime.Current();WeatherProfile snow;WeatherPreset("snow",snow);runtime.Transition(snow,1);
 Check(runtime.Current().rain==middle.rain&&runtime.Current().snow==middle.snow,"interruption preserves continuous current profile");runtime.Advance(1);
 Check(runtime.Current().snow==snow.snow&&runtime.Current().rain==0,"interrupted transition reaches requested endpoint");
 runtime.Transition(rain,0);Check(runtime.Current().rain==rain.rain,"zero-duration transition immediate");
 const auto stateBefore=runtime.Capture();runtime.Advance(0);Check(runtime.Capture()==stateBefore,"pause advances no state");
 Check(Rejects([&]{runtime.Advance(1.1);})&&runtime.Capture()==stateBefore,"oversized timestep rejected atomically");
 Check(Rejects([&]{runtime.Advance(std::numeric_limits<double>::quiet_NaN());})&&runtime.Capture()==stateBefore,"nonfinite timestep rejected atomically");
 Check(Rejects([&]{runtime.Transition(rain,-1);})&&runtime.Capture()==stateBefore,"negative transition rejected atomically");
 WeatherProfile badProfile=rain;badProfile.wind={1e9,0,0};Check(Rejects([&]{runtime.Transition(badProfile,1);}),"unsafe wind strength rejected");

 WeatherSettings region;region.regionShape=WeatherRegionShape::Sphere;region.regionCenter={20,0,0};region.regionRadius=5;region.blendDistance=4;
 Check(WeatherRegionWeight(region,{20,0,0})==1&&WeatherRegionWeight(region,{25,0,0})==1,"region owns interior and exact boundary");
 Check(std::abs(WeatherRegionWeight(region,{27,0,0})-.5)<1e-12&&WeatherRegionWeight(region,{29,0,0})==0,"region feather is bounded continuous outside its authored shape");
 region.regionShape=WeatherRegionShape::Box;region.regionHalfExtents={4,1,2};region.blendDistance=0;region.regionRotation=glm::angleAxis(.5*3.141592653589793,V(0,0,1));
 Check(WeatherRegionWeight(region,{20,3,0})==1&&WeatherRegionWeight(region,{23,0,0})==0,"box region uses authored orientation, not world-up");
 region.enabled=false;Check(WeatherRegionWeight(region,{20,0,0})==0,"disabled field has no spatial influence");

 WeatherSettings wind;wind.profile.wind={9,0,-4};wind.profile.gust=7;wind.seed=9182;WeatherRuntime a(wind),b(wind);
 for(unsigned i=0;i<120;++i){a.Advance(1./60);b.Advance(1./60);}
 Check(a.Capture()==b.Capture()&&Near(a.Sample({4,8,-3}).wind,b.Sample({4,8,-3}).wind),"independent instances reproduce seeded fixed-step field");
 auto stable=a.Capture();for(unsigned i=0;i<100;++i)(void)a.Sample({double(i),2,3});Check(a.Capture()==stable,"visual/spatial samples never consume authoritative RNG or advance weather");
 wind.seed++;WeatherRuntime different(wind);different.Advance(1);different.Advance(1);Check(!Near(different.Sample({4,8,-3}).wind,b.Sample({4,8,-3}).wind,.001),"different seeds generate different coherent gusts");
 const auto q=glm::angleAxis(.87,glm::normalize(V(1,2,3)));const V translation{480,-25,700};wind.seed=9182;
 WeatherRuntime normal(wind),rotated(Rotated(wind,q,translation));normal.Advance(.7);rotated.Advance(.7);const V point{12,7,-3};const auto sn=normal.Sample(point),sr=rotated.Sample(translation+q*point);
 Check(Near(sr.wind,q*sn.wind,1e-8)&&Near(sr.up,q*sn.up)&&std::abs(sr.altitude-sn.altitude)<1e-9,"entire prescribed field rotates equivalently under arbitrary gravity frame");
 const V farOrigin{9e11,-8e11,7e11};WeatherSettings translated=wind;translated.frame.center+=farOrigin;translated.regionCenter+=farOrigin;WeatherRuntime absolute(translated);absolute.Advance(.7);
 Check(Near(normal.Sample({12,7,-3}).wind,absolute.Sample(farOrigin+V(12,7,-3)).wind,1e-8),"absolute double anchoring preserves fine local wind at large scene origins");
 WeatherSettings radial=wind;radial.frame.radial=true;radial.frame.center={1e8,2e8,-3e8};radial.frame.radius=6371000;radial.profile.gust=0;radial.profile.wind={9,3,-4};WeatherRuntime sphere(radial);
 auto local=sphere.Sample(radial.frame.center+V(radial.frame.radius+2,0,0));
 Check(Near(local.up,{1,0,0})&&std::abs(local.altitude-2)<1e-8&&std::abs(glm::dot(local.wind,local.up))<1e-8,"radial wind follows local tangent and altitude without universal up");
 auto orbit=sphere.Sample(radial.frame.center+V(0,radial.frame.radius+100000,0));
 Check(orbit.active&&orbit.weight==1&&orbit.profile.cloudDensity==radial.profile.cloudDensity&&glm::length(orbit.wind)==0&&orbit.airDensity==0,"orbital view keeps cloud shell while local air/forces fade to vacuum");
 Check(Near(WeatherDragForce(orbit,{100,0,0},10,response),V(0)),"no phantom drag above atmosphere");

 WeatherSettings vortex;vortex.profile.wind=V(0);vortex.profile.gust=0;vortex.profile.tornado=1;vortex.profile.tornadoSpeed=0;WeatherRuntime tornado(vortex);
 auto left=tornado.Sample({-12,10,0}),right=tornado.Sample({12,10,0}),core=tornado.Sample({0,10,0});
 Check(left.wind.z*right.wind.z<0&&left.wind.y>0&&right.wind.y>0,"tornado produces opposed rotational flow and updraft");
 Check(std::isfinite(glm::length(core.wind))&&core.wind.y>0,"tornado core finite with no inverse-distance singularity");
 Check(glm::length(tornado.Sample({1000,10,0}).wind)==0,"tornado wind influence bounded in space");
 vortex.profile.tornadoSpeed=8;WeatherRuntime moving(vortex);moving.Advance(1);
 Check(Near(moving.Sample({0,1,0}).tornadoPosition,{8,0,0}),"moving tornado follows authored physical speed");
 vortex.frame.radial=true;vortex.frame.radius=100;vortex.tornadoOffset={10,15};WeatherRuntime curved(vortex);curved.Advance(1);
 Check(std::abs(glm::length(curved.Sample({0,100,0}).tornadoPosition-vortex.frame.center)-100)<1e-9,"moving tornado remains on curved planetary ground reference");
 WeatherRuntime movedRotated(Rotated(vortex,q,translation));movedRotated.Advance(1);Check(Near(movedRotated.Sample(translation+q*V(0,100,0)).tornadoPosition,translation+q*curved.Sample({0,100,0}).tornadoPosition,1e-8),"curved tornado track rotates with frame");
 WeatherSettings track;track.profile.tornadoSpeed=2;WeatherRuntime coarse(track),fine(track);WeatherProfile accelerated=track.profile;accelerated.tornadoSpeed=22;accelerated.wind={17,0,4};coarse.Transition(accelerated,2);fine.Transition(accelerated,2);for(unsigned i=0;i<3;++i)coarse.Advance(1);for(unsigned i=0;i<120;++i)fine.Advance(.025);
 Check(Near(coarse.Sample({0,1,0}).tornadoPosition,fine.Sample({0,1,0}).tornadoPosition,1e-8),"transitioned tornado travel uses analytic integral independent of step partition");
 Check(Near(coarse.State().cloudOffset,fine.State().cloudOffset,1e-8),"cloud advection integrates transition wind independent of step partition");const auto offset=coarse.State().cloudOffset;coarse.Transition(track.profile,1);Check(Near(coarse.Sample({0,1,0}).cloudOffset,offset),"changing wind never jumps the existing cloud advection offset");

 WeatherSettings forceSettings;forceSettings.profile.wind={100,0,0};forceSettings.profile.gust=0;WeatherRuntime forceRuntime(forceSettings);auto forceSample=forceRuntime.Sample({0,0,0});response.area=20;response.maxAcceleration=7;auto force=WeatherDragForce(forceSample,V(0),2,response);
 Check(Near(force,{14,0,0}),"generic weather force respects authored maximum acceleration");
 Check(Near(WeatherDragForce(forceSample,V(0),2,response,0),V(0)),"shelter exposure suppresses opt-in wind load");
 Check(Near(WeatherDragForce(forceSample,{100,0,0},2,response),V(0)),"co-moving body has zero relative-air drag");response.enabled=false;
 Check(Near(WeatherDragForce(forceSample,V(0),2,response),V(0)),"opt-out body receives no weather force");response.enabled=true;response.area=1;response.maxAcceleration=1000;
 auto half=WeatherDragForce(forceSample,V(0),100,response,.5),full=WeatherDragForce(forceSample,V(0),100,response,1);Check(Near(half,full*.5),"exposure scales finite pressure drag without changing weather state");

 WeatherSettings storm;storm.profile.lightningRate=120;WeatherRuntime thunder(storm);thunder.Advance(1);
 Check(thunder.State().lightning.sequence==2&&thunder.State().lightning.time==1,"bounded deterministic lightning slots produce actual events");
 Check(thunder.Sample({0,1,0}).lightningFlash>0,"lightning event and rendered flash share authoritative clock");
 auto saved=thunder.Capture();WeatherRuntime fresh(storm);Check(fresh.Restore(saved,error)&&fresh.Capture()==saved,"weather cold continuation restores exact event/time/transition state");thunder.Advance(.5);fresh.Advance(.5);Check(thunder.Capture()==fresh.Capture(),"new-process weather continuation preserves next strike and movement");
 before=fresh.Capture();bad=J::parse(before);bad["transitionElapsed"]=999;Check(!fresh.Restore(bad.dump(),error)&&fresh.Capture()==before,"failed continuation restore leaves runtime untouched");
 auto mismatched=storm;mismatched.seed++;WeatherRuntime mismatch(mismatched);Check(!mismatch.Restore(before,error),"wrong-seed continuation rejected");
 fresh.SetEnabled(false);fresh.Advance(1);Check(!fresh.Sample({0,1,0}).active&&fresh.State().lightning.sequence==thunder.State().lightning.sequence,"disabled field advances time without emitting storm events");
 WeatherRuntime disabled(storm);Check(disabled.Restore(fresh.Capture(),error)&&!disabled.Settings().enabled,"runtime enabled state survives cold restoration");
 fresh.SetEnabled(true);fresh.TriggerLightning({6,0,-5},.8);Check(fresh.State().lightning.position==V(6,0,-5)&&fresh.State().lightning.time==fresh.State().time,"generic manual strike publishes same event path");
 before=fresh.Capture();Check(Rejects([&]{fresh.TriggerLightning({std::numeric_limits<double>::infinity(),0,0});})&&fresh.Capture()==before,"invalid strike cannot corrupt event state");

 auto start=std::chrono::steady_clock::now();double checksum=0;for(unsigned i=0;i<100000;++i)checksum+=tornado.Sample({double(i%103),double(i%41),double(i%59)}).wind.x;
 const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::printf("MEASURE 100000 weather samples %.6f ms checksum %.9f\n",ms,checksum);
 std::printf("Weather %u checks, %u failures\n",checks,failures);return failures?1:0;
}
