#include "Weather.h"
#include "../third_party/nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
using V=glm::dvec3;using J=nlohmann::ordered_json;
constexpr double pi=3.14159265358979323846;
bool Finite(V p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
void Require(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
bool Range(double v,double low,double high){return std::isfinite(v)&&v>=low&&v<=high;}
V Unit(V p){double n=glm::length(p);Require(Finite(p)&&std::isfinite(n)&&n>1e-12,"undefined weather direction");return p/n;}
V Tangent(V v,V up){return v-up*glm::dot(v,up);}
V North(const WeatherFrame& f){return Unit(Tangent(f.north,Unit(f.up)));}
double Smooth(double v){v=std::clamp(v,0.,1.);return v*v*(3-2*v);}
// Integral of smoothstep from 0 to x. This makes tornado travel independent
// of how a smooth authored transition is divided into fixed timesteps.
double SmoothIntegral(double x){x=std::clamp(x,0.,1.);return x*x*x-.5*x*x*x*x;}
uint64_t Hash(uint64_t x){x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
double Random(uint32_t seed,uint64_t n,unsigned channel){return double(Hash(n^(uint64_t(seed)<<32)^Hash(channel))>>11)*0x1.0p-53;}
J Array(V v){return J::array({v.x,v.y,v.z});}
V Vector(const J& j){Require(j.is_array()&&j.size()==3,"weather vector requires three numbers");V v;for(unsigned i=0;i<3;++i){Require(j[i].is_number(),"weather vector requires numbers");v[i]=j[i].get<double>();}Require(Finite(v),"weather vector is not finite");return v;}
void Known(const J& input,const J& example){Require(input.is_object(),"weather descriptor requires object");for(auto it=input.begin();it!=input.end();++it)Require(example.contains(it.key()),"unknown weather descriptor field");}
void Number(const J& j,const char* key,double& value){if(j.contains(key)){Require(j[key].is_number(),"weather scalar requires number");value=j[key].get<double>();}}
void Boolean(const J& j,const char* key,bool& value){if(j.contains(key)){Require(j[key].is_boolean(),"weather switch requires Boolean");value=j[key].get<bool>();}}
uint64_t Unsigned(const J& j){Require(j.is_number_unsigned()||(j.is_number_integer()&&j.get<int64_t>()>=0),"weather count requires unsigned integer");return j.get<uint64_t>();}
J ProfileJSON(const WeatherProfile& p){return {{"cloudCoverage",p.cloudCoverage},{"cloudDensity",p.cloudDensity},{"cloudBase",p.cloudBase},{"cloudThickness",p.cloudThickness},{"rain",p.rain},{"snow",p.snow},{"fog",p.fog},{"wind",Array(p.wind)},{"gust",p.gust},{"lightningRate",p.lightningRate},{"tornado",p.tornado},{"tornadoRadius",p.tornadoRadius},{"tornadoHeight",p.tornadoHeight},{"tornadoSpeed",p.tornadoSpeed}};}
WeatherProfile Profile(const J& j){WeatherProfile p;Known(j,ProfileJSON(p));Number(j,"cloudCoverage",p.cloudCoverage);Number(j,"cloudDensity",p.cloudDensity);Number(j,"cloudBase",p.cloudBase);Number(j,"cloudThickness",p.cloudThickness);Number(j,"rain",p.rain);Number(j,"snow",p.snow);Number(j,"fog",p.fog);Number(j,"gust",p.gust);Number(j,"lightningRate",p.lightningRate);Number(j,"tornado",p.tornado);Number(j,"tornadoRadius",p.tornadoRadius);Number(j,"tornadoHeight",p.tornadoHeight);Number(j,"tornadoSpeed",p.tornadoSpeed);if(j.contains("wind"))p.wind=Vector(j["wind"]);std::string error;if(!ValidateWeatherProfile(p,error))throw std::invalid_argument(error);return p;}
WeatherProfile At(const WeatherState& state,double elapsed){return BlendWeatherProfiles(state.from,state.to,state.transitionDuration>0?Smooth(elapsed/state.transitionDuration):1);}
double TravelIntegral(const WeatherState& s,double a,double b){if(s.transitionDuration<=0)return (b-a)*s.to.tornadoSpeed;const double d=s.transitionDuration;
 auto integral=[&](double t){const double inside=std::min(t,d);return s.from.tornadoSpeed*inside+(s.to.tornadoSpeed-s.from.tornadoSpeed)*d*SmoothIntegral(inside/d)+std::max(0.,t-d)*s.to.tornadoSpeed;};return integral(b)-integral(a);}
V CloudIntegral(const WeatherState& s,double a,double b){if(s.transitionDuration<=0)return (b-a)*s.to.wind;const double d=s.transitionDuration;
 auto integral=[&](double t){const double inside=std::min(t,d);return s.from.wind*inside+(s.to.wind-s.from.wind)*(d*SmoothIntegral(inside/d))+s.to.wind*std::max(0.,t-d);};return integral(b)-integral(a);}
V Ground(const WeatherFrame& f,V p){const auto up=WeatherUp(f,p);return f.radial?f.center+up*f.radius:p-up*WeatherAltitude(f,p);}
V Start(const WeatherSettings& s){V up=Unit(s.frame.up),north=North(s.frame),east=glm::cross(north,up);V offset=north*s.tornadoOffset.x+east*s.tornadoOffset.y;if(!s.frame.radial)return s.frame.center+offset;
 double distance=glm::length(offset);if(distance<1e-12)return s.frame.center+up*s.frame.radius;double a=distance/s.frame.radius;return s.frame.center+(up*std::cos(a)+offset/distance*std::sin(a))*s.frame.radius;}
J SettingsJSON(const WeatherSettings& s){return {{"version",1},{"enabled",s.enabled},{"seed",s.seed},{"priority",s.priority},{"profile",ProfileJSON(s.profile)},
 {"frame",{{"radial",s.frame.radial},{"center",Array(s.frame.center)},{"up",Array(s.frame.up)},{"north",Array(s.frame.north)},{"radius",s.frame.radius}}},
 {"regionShape",s.regionShape==WeatherRegionShape::Global?"global":s.regionShape==WeatherRegionShape::Sphere?"sphere":"box"},{"regionCenter",Array(s.regionCenter)},{"regionHalfExtents",Array(s.regionHalfExtents)},{"regionRotation",J::array({s.regionRotation.w,s.regionRotation.x,s.regionRotation.y,s.regionRotation.z})},{"regionRadius",s.regionRadius},{"blendDistance",s.blendDistance},{"topAltitude",s.topAltitude},{"tornadoOffset",J::array({s.tornadoOffset.x,s.tornadoOffset.y})},{"tornadoDirection",Array(s.tornadoDirection)}};}
}

bool ValidateWeatherProfile(const WeatherProfile& p,std::string& error){try{
 Require(Range(p.cloudCoverage,0,1)&&Range(p.cloudDensity,0,2)&&Range(p.cloudBase,0,50000)&&Range(p.cloudThickness,10,50000),"weather cloud settings outside supported bounds");
 Require(Range(p.rain,0,1)&&Range(p.snow,0,1)&&Range(p.fog,0,1),"weather intensity outside [0,1]");
 Require(Finite(p.wind)&&glm::length(p.wind)<=100&&Range(p.gust,0,100),"weather wind/gust exceeds 100 m/s authored bound");
 Require(Range(p.lightningRate,0,120)&&Range(p.tornado,0,1)&&Range(p.tornadoRadius,1,1000)&&Range(p.tornadoHeight,10,10000)&&Range(p.tornadoSpeed,0,200),"weather storm settings outside supported bounds");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool ValidateWeatherSettings(const WeatherSettings& s,std::string& error){try{
 if(!ValidateWeatherProfile(s.profile,error))throw std::invalid_argument(error);
 Require(s.priority>=-1000000&&s.priority<=1000000,"weather priority outside supported bounds");
 Require(Finite(s.frame.center)&&glm::all(glm::lessThanEqual(glm::abs(s.frame.center),V(1e12)))&&Range(s.frame.radius,1,1e10),"weather frame exceeds absolute positioning bounds");
 Require(Finite(s.frame.up)&&Finite(s.frame.north)&&glm::length(s.frame.up)>1e-12&&glm::length(s.frame.up)<1e6&&glm::length(s.frame.north)<1e6&&glm::length(Tangent(s.frame.north,Unit(s.frame.up)))>1e-8,"weather frame needs independent finite up and north directions");
 Require(s.regionShape==WeatherRegionShape::Global||s.regionShape==WeatherRegionShape::Sphere||s.regionShape==WeatherRegionShape::Box,"invalid weather region shape");
 Require(Finite(s.regionCenter)&&glm::all(glm::lessThanEqual(glm::abs(s.regionCenter),V(1e12)))&&Finite(s.regionHalfExtents)&&glm::all(glm::greaterThanEqual(s.regionHalfExtents,V(.01)))&&glm::all(glm::lessThanEqual(s.regionHalfExtents,V(1e8)))&&Range(s.regionRadius,.01,1e8),"weather region exceeds supported bounds");
 Require(std::isfinite(glm::length(s.regionRotation))&&glm::length(s.regionRotation)>1e-12,"invalid weather region rotation");
 Require(Range(s.blendDistance,0,1e7)&&Range(s.topAltitude,1,1e6)&&s.topAltitude>=s.profile.cloudBase+s.profile.cloudThickness,"weather atmosphere must contain authored cloud layer");
 Require(std::isfinite(s.tornadoOffset.x)&&std::isfinite(s.tornadoOffset.y)&&glm::length(s.tornadoOffset)<=1e7&&Finite(s.tornadoDirection)&&glm::length(s.tornadoDirection)>1e-12&&glm::length(s.tornadoDirection)<1e6,"invalid weather tornado track");error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
bool ValidateWeatherResponseSettings(const WeatherResponseSettings& s,std::string& error){if(!Range(s.area,0,10000)||!Range(s.dragCoefficient,0,10)||!Range(s.maxAcceleration,0,1000)){error="weather response requires area [0,10000] m2, Cd [0,10] and acceleration [0,1000] m/s2";return false;}error.clear();return true;}
std::string EncodeWeatherSettings(const WeatherSettings& s){return SettingsJSON(s).dump();}
bool DecodeWeatherSettings(const std::string& text,WeatherSettings& out,std::string& error){try{
 Require(text.size()<=32768,"weather descriptor exceeds text budget");const auto j=J::parse(text);WeatherSettings s;const auto prototype=SettingsJSON(s);Known(j,prototype);Require(j.contains("version")&&j["version"]==1,"unsupported weather descriptor version");
 Boolean(j,"enabled",s.enabled);if(j.contains("seed")){auto n=Unsigned(j["seed"]);Require(n<=std::numeric_limits<uint32_t>::max(),"weather seed outside uint32");s.seed=uint32_t(n);}if(j.contains("priority")){Require(j["priority"].is_number_integer(),"weather priority requires integer");auto n=j["priority"].get<int64_t>();Require(n>=-1000000&&n<=1000000,"weather priority outside supported bounds");s.priority=int(n);}
 if(j.contains("profile"))s.profile=Profile(j["profile"]);
 if(j.contains("frame")){const auto& f=j["frame"];Known(f,prototype["frame"]);Boolean(f,"radial",s.frame.radial);if(f.contains("center"))s.frame.center=Vector(f["center"]);if(f.contains("up"))s.frame.up=Vector(f["up"]);if(f.contains("north"))s.frame.north=Vector(f["north"]);Number(f,"radius",s.frame.radius);}
 if(j.contains("regionShape")){Require(j["regionShape"].is_string(),"weather regionShape requires string");const auto shape=j["regionShape"].get<std::string>();Require(shape=="global"||shape=="sphere"||shape=="box","unknown weather region shape");s.regionShape=shape=="global"?WeatherRegionShape::Global:shape=="sphere"?WeatherRegionShape::Sphere:WeatherRegionShape::Box;}
 if(j.contains("regionCenter"))s.regionCenter=Vector(j["regionCenter"]);
 if(j.contains("regionHalfExtents"))s.regionHalfExtents=Vector(j["regionHalfExtents"]);
 if(j.contains("regionRotation")){const auto& q=j["regionRotation"];Require(q.is_array()&&q.size()==4,"weather rotation requires [w,x,y,z]");for(const auto& n:q)Require(n.is_number(),"weather rotation requires numbers");s.regionRotation={q[0].get<double>(),q[1].get<double>(),q[2].get<double>(),q[3].get<double>()};}
 Number(j,"regionRadius",s.regionRadius);Number(j,"blendDistance",s.blendDistance);Number(j,"topAltitude",s.topAltitude);
 if(j.contains("tornadoOffset")){const auto& v=j["tornadoOffset"];Require(v.is_array()&&v.size()==2&&v[0].is_number()&&v[1].is_number(),"weather tornado offset requires two numbers");s.tornadoOffset={v[0].get<double>(),v[1].get<double>()};}if(j.contains("tornadoDirection"))s.tornadoDirection=Vector(j["tornadoDirection"]);
 if(!ValidateWeatherSettings(s,error))throw std::invalid_argument(error);
 out=std::move(s);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}}
std::string EncodeWeatherResponseSettings(const WeatherResponseSettings& s){return J{{"version",1},{"enabled",s.enabled},{"area",s.area},{"dragCoefficient",s.dragCoefficient},{"maxAcceleration",s.maxAcceleration},{"occlusionMask",s.occlusionMask}}.dump();}
bool DecodeWeatherResponseSettings(const std::string& text,WeatherResponseSettings& out,std::string& error){try{Require(text.size()<=4096,"weather response exceeds text budget");auto j=J::parse(text);WeatherResponseSettings s;Known(j,J::parse(EncodeWeatherResponseSettings(s)));Require(j.contains("version")&&j["version"]==1,"unsupported weather response version");Boolean(j,"enabled",s.enabled);Number(j,"area",s.area);Number(j,"dragCoefficient",s.dragCoefficient);Number(j,"maxAcceleration",s.maxAcceleration);if(j.contains("occlusionMask"))s.occlusionMask=Unsigned(j["occlusionMask"]);if(!ValidateWeatherResponseSettings(s,error))throw std::invalid_argument(error);out=s;error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}

bool WeatherPreset(const std::string& name,WeatherProfile& out){WeatherProfile p;
 if(name=="sunny"){p.cloudCoverage=.12;p.cloudDensity=.6;}
 else if(name=="cloudy"){p.cloudCoverage=.75;p.cloudDensity=1;p.gust=3;}
 else if(name=="rain"){p.cloudCoverage=.9;p.cloudDensity=1.35;p.rain=.7;p.fog=.2;p.wind={7,0,2};p.gust=4;}
 else if(name=="thunderstorm"){p.cloudCoverage=.98;p.cloudDensity=1.8;p.rain=1;p.fog=.35;p.wind={13,0,6};p.gust=10;p.lightningRate=16;}
 else if(name=="snow"){p.cloudCoverage=.8;p.cloudDensity=1.1;p.snow=.7;p.fog=.22;p.wind={3,0,2};p.gust=3;}
 else if(name=="blizzard"){p.cloudCoverage=1;p.cloudDensity=1.5;p.snow=1;p.fog=.85;p.wind={19,0,8};p.gust=12;}
 else if(name=="tornado"){p.cloudCoverage=.98;p.cloudDensity=1.9;p.rain=.65;p.fog=.35;p.wind={14,0,5};p.gust=13;p.lightningRate=12;p.tornado=1;}
 else if(name=="fog"){p.cloudCoverage=.45;p.fog=.85;p.wind={1,0,0};p.gust=.5;}
 else return false;
 out=p;return true;
}
WeatherProfile BlendWeatherProfiles(const WeatherProfile& a,const WeatherProfile& b,double t){Require(std::isfinite(t),"nonfinite weather blend");t=std::clamp(t,0.,1.);WeatherProfile p;
#define WEATHER_MIX(field) p.field=glm::mix(a.field,b.field,t)
 WEATHER_MIX(cloudCoverage);WEATHER_MIX(cloudDensity);WEATHER_MIX(cloudBase);WEATHER_MIX(cloudThickness);WEATHER_MIX(rain);WEATHER_MIX(snow);WEATHER_MIX(fog);WEATHER_MIX(wind);WEATHER_MIX(gust);WEATHER_MIX(lightningRate);WEATHER_MIX(tornado);WEATHER_MIX(tornadoRadius);WEATHER_MIX(tornadoHeight);WEATHER_MIX(tornadoSpeed);
#undef WEATHER_MIX
 return p;}
double WeatherRegionWeight(const WeatherSettings& s,V p){if(!s.enabled||!Finite(p))return 0;if(s.regionShape==WeatherRegionShape::Global)return 1;double outside=0;if(s.regionShape==WeatherRegionShape::Sphere)outside=glm::length(p-s.regionCenter)-s.regionRadius;else{V q=glm::abs(glm::conjugate(glm::normalize(s.regionRotation))*(p-s.regionCenter))-s.regionHalfExtents;outside=glm::length(glm::max(q,V(0)))+std::min(0.,std::max({q.x,q.y,q.z}));}if(outside<=0)return 1;if(s.blendDistance<=0)return 0;return 1-Smooth(outside/s.blendDistance);}
V WeatherUp(const WeatherFrame& f,V p){if(!f.radial)return Unit(f.up);const V d=p-f.center;return glm::length(d)>1e-12?Unit(d):Unit(f.up);}
double WeatherAltitude(const WeatherFrame& f,V p){return f.radial?glm::length(p-f.center)-f.radius:glm::dot(p-f.center,Unit(f.up));}
WeatherRuntime::WeatherRuntime(WeatherSettings s):m_settings(std::move(s)){std::string error;if(!ValidateWeatherSettings(m_settings,error))throw std::invalid_argument(error);m_settings.frame.up=Unit(m_settings.frame.up);m_settings.frame.north=North(m_settings.frame);m_settings.regionRotation=glm::normalize(m_settings.regionRotation);m_state.from=m_state.to=m_settings.profile;}
WeatherProfile WeatherRuntime::Current()const{return At(m_state,m_state.transitionElapsed);}
void WeatherRuntime::Transition(const WeatherProfile& p,double duration){std::string error;if(!ValidateWeatherProfile(p,error))throw std::invalid_argument(error);Require(Range(duration,0,86400),"weather transition duration outside [0,86400] seconds");Require(p.cloudBase+p.cloudThickness<=m_settings.topAltitude,"weather clouds exceed frame atmosphere");const auto previous=Current();m_state.from=previous;m_state.to=p;m_state.transitionElapsed=0;m_state.transitionDuration=duration;}
V WeatherRuntime::TornadoPosition(double travel)const{const V start=Start(m_settings),up=WeatherUp(m_settings.frame,start);V direction=Tangent(m_settings.tornadoDirection,up);if(glm::length(direction)<1e-10)direction=Tangent(m_settings.frame.north,up);if(glm::length(direction)<1e-10){const V axis=std::abs(up.x)<.8?V(1,0,0):V(0,1,0);direction=glm::cross(up,axis);}direction=Unit(direction);if(!m_settings.frame.radial)return start+direction*travel;const double angle=std::remainder(travel/m_settings.frame.radius,2*pi);return m_settings.frame.center+(up*std::cos(angle)+direction*std::sin(angle))*m_settings.frame.radius;}
void WeatherRuntime::TriggerLightning(V p,double energy){Require(Finite(p)&&glm::all(glm::lessThanEqual(glm::abs(p),V(1.01e12)))&&Range(energy,0,1),"invalid weather lightning position or energy");Require(m_state.lightning.sequence<std::numeric_limits<uint64_t>::max(),"weather lightning sequence exhausted");m_state.lightning={m_state.lightning.sequence+1,m_state.time,p,energy};}
void WeatherRuntime::Advance(double dt){Require(Range(dt,0,1),"weather Advance requires finite [0,1] seconds");if(dt==0)return;Require(m_state.time+dt<=1e12,"weather clock exceeds supported lifetime");const double before=m_state.time,elapsed=m_state.transitionElapsed;
 m_state.cloudOffset+=CloudIntegral(m_state,elapsed,elapsed+dt);m_state.tornadoTravel+=TravelIntegral(m_state,elapsed,elapsed+dt);m_state.transitionElapsed=m_state.transitionDuration>0?std::min(elapsed+dt,m_state.transitionDuration):0;m_state.time+=dt;
 // Fixed half-second slots bound work regardless of storm intensity. A slot
 // has a deterministic independent probability rate/120; there is no hidden
 // graphics-dependent RNG or variable-rate particle feedback into physics.
 const uint64_t first=uint64_t(std::floor(before*2))+1,last=uint64_t(std::floor(m_state.time*2));
 if(!m_settings.enabled)return;
 for(uint64_t slot=first;slot<=last;++slot){const double eventTime=double(slot)*.5;const auto p=At(m_state,elapsed+eventTime-before);if(Random(m_settings.seed,slot,0)>=p.lightningRate/120)continue;
  const V anchor=m_settings.regionShape==WeatherRegionShape::Global?Start(m_settings):Ground(m_settings.frame,m_settings.regionCenter);const V up=WeatherUp(m_settings.frame,anchor);V north=Tangent(m_settings.frame.north,up);if(glm::length(north)<1e-10)north=Tangent(m_settings.frame.up,up);north=Unit(north);const V east=glm::cross(north,up);double extent=1800;if(m_settings.regionShape==WeatherRegionShape::Sphere)extent=std::min(extent,m_settings.regionRadius*.8);if(m_settings.regionShape==WeatherRegionShape::Box)extent=std::min(extent,std::min({m_settings.regionHalfExtents.x,m_settings.regionHalfExtents.y,m_settings.regionHalfExtents.z})*.8);const double distance=std::sqrt(Random(m_settings.seed,slot,1))*extent,angle=Random(m_settings.seed,slot,2)*2*pi;V point=anchor+(north*std::cos(angle)+east*std::sin(angle))*distance;point=Ground(m_settings.frame,point);if(WeatherRegionWeight(m_settings,point)<=0)continue;
  TriggerLightning(point,.7+.3*Random(m_settings.seed,slot,3));m_state.lightning.time=eventTime;
 }
}
WeatherSample WeatherRuntime::Sample(V p)const{Require(Finite(p)&&glm::all(glm::lessThanEqual(glm::abs(p),V(1.01e12))),"weather query exceeds finite absolute positioning bounds");WeatherSample s;s.profile=Current();s.frame=m_settings.frame;s.seed=m_settings.seed;s.time=m_state.time;s.cloudOffset=m_state.cloudOffset;s.up=WeatherUp(s.frame,p);s.altitude=WeatherAltitude(s.frame,p);s.weight=WeatherRegionWeight(m_settings,p);const double airFade=1-Smooth((s.altitude-m_settings.topAltitude*.8)/(m_settings.topAltitude*.2));s.airDensity=1.225*std::exp(-std::max(0.,s.altitude)/8500.)*airFade*s.weight;s.active=s.weight>0;s.tornadoPosition=TornadoPosition(m_state.tornadoTravel);s.lightningPosition=m_state.lightning.position;s.lightningSequence=m_state.lightning.sequence;
 const double age=m_state.time-m_state.lightning.time;s.lightningFlash=m_state.lightning.time>=0&&age>=0&&age<.22?m_state.lightning.energy*std::exp(-age*22)*(1+.25*std::sin(age*170)):0;
 if(!s.active)return s;
 V background=s.profile.wind;if(s.frame.radial)background=Tangent(background,s.up);
 const V relative=p-s.frame.center,frameUp=Unit(s.frame.up),frameNorth=North(s.frame),frameEast=glm::cross(frameNorth,frameUp);const V noisePoint{glm::dot(relative,frameEast),glm::dot(relative,frameUp),glm::dot(relative,frameNorth)};const double phase=Random(m_settings.seed,0,4)*2*pi,t=m_state.time;
 const double g=.55*std::sin(glm::dot(noisePoint,V(.013,.009,.007))+t*.83+phase)+.3*std::sin(glm::dot(noisePoint,V(-.006,.017,.011))+t*1.37+phase*2)+.15*std::sin(glm::dot(noisePoint,V(.031,.003,-.021))+t*2.19);
 V direction=glm::length(background)>1e-10?Unit(background):Tangent(m_settings.frame.north,s.up);if(glm::length(direction)<1e-10)direction=Tangent(m_settings.frame.up,s.up);if(glm::length(direction)<1e-10)direction=glm::cross(s.up,std::abs(s.up.x)<.8?V(1,0,0):V(0,1,0));direction=Unit(direction);
 V wind=background+direction*(s.profile.gust*g);
 if(s.profile.tornado>0){const V axis=WeatherUp(s.frame,s.tornadoPosition),delta=p-s.tornadoPosition;const double h=glm::dot(delta,axis),r=glm::length(Tangent(delta,axis)),radius=s.profile.tornadoRadius;const double height=Smooth((h+2)/2)*(1-Smooth((h-s.profile.tornadoHeight*.6)/(s.profile.tornadoHeight*.4)));if(h>=-2&&height>0&&r<radius*6){const double q=r/radius,core=q/(.25+q*q),envelope=1-Smooth((q-3)/3),strength=s.profile.tornado*height*envelope;V toward=r>1e-8?-Tangent(delta,axis)/r:V(0);wind+=strength*(glm::cross(toward,axis)*core*42.0+toward*(12*q/(.5+q*q))+axis*22.0/(1+q*q));}}
 s.wind=wind*(s.weight*airFade);return s;
}
std::string WeatherRuntime::Capture()const{return J{{"version",1},{"seed",m_settings.seed},{"enabled",m_settings.enabled},{"time",m_state.time},{"transitionElapsed",m_state.transitionElapsed},{"transitionDuration",m_state.transitionDuration},{"tornadoTravel",m_state.tornadoTravel},{"cloudOffset",Array(m_state.cloudOffset)},{"from",ProfileJSON(m_state.from)},{"to",ProfileJSON(m_state.to)},{"lightning",{{"sequence",m_state.lightning.sequence},{"time",m_state.lightning.time},{"position",Array(m_state.lightning.position)},{"energy",m_state.lightning.energy}}}}.dump();}
bool WeatherRuntime::Restore(const std::string& text,std::string& error){try{Require(text.size()<=32768,"weather continuation exceeds text budget");auto j=J::parse(text);Known(j,J::parse(Capture()));Require(j.at("version")==1&&Unsigned(j.at("seed"))==m_settings.seed,"weather continuation version/seed differs");WeatherState state;bool enabled=m_settings.enabled;Boolean(j,"enabled",enabled);state.time=j.at("time").get<double>();state.transitionElapsed=j.at("transitionElapsed").get<double>();state.transitionDuration=j.at("transitionDuration").get<double>();state.tornadoTravel=j.at("tornadoTravel").get<double>();state.cloudOffset=Vector(j.at("cloudOffset"));Require(glm::length(state.cloudOffset)<=2e14,"weather saved cloud advection exceeds supported lifetime");state.from=Profile(j.at("from"));state.to=Profile(j.at("to"));Require(state.from.cloudBase+state.from.cloudThickness<=m_settings.topAltitude&&state.to.cloudBase+state.to.cloudThickness<=m_settings.topAltitude,"saved weather clouds exceed frame atmosphere");Require(Range(state.time,0,1e12)&&Range(state.transitionDuration,0,86400)&&Range(state.transitionElapsed,0,state.transitionDuration)&&Range(state.tornadoTravel,0,2e14),"invalid weather continuation clock/track");const auto& l=j.at("lightning");Known(l,J{{"sequence",0},{"time",0},{"position",J::array()},{"energy",0}});state.lightning.sequence=Unsigned(l.at("sequence"));state.lightning.time=l.at("time").get<double>();state.lightning.position=Vector(l.at("position"));state.lightning.energy=l.at("energy").get<double>();Require(Range(state.lightning.time,-1,state.time)&&Range(state.lightning.energy,0,1)&&((state.lightning.sequence==0&&state.lightning.time==-1)||(state.lightning.sequence>0&&state.lightning.time>=0)),"invalid weather saved lightning");m_state=state;m_settings.enabled=enabled;error.clear();return true;}catch(const std::exception& e){error=e.what();return false;}}
V WeatherDragForce(const WeatherSample& sample,V velocity,double mass,const WeatherResponseSettings& response,double exposure){std::string error;if(!ValidateWeatherResponseSettings(response,error))throw std::invalid_argument(error);Require(Finite(velocity)&&glm::length(velocity)<=1e12&&Range(mass,1e-12,1e12)&&Range(exposure,0,1),"invalid weather force input");if(!response.enabled||!sample.active||response.area==0||response.dragCoefficient==0||exposure==0)return V(0);V relative=sample.wind-velocity;const double speed=glm::length(relative);if(speed<1e-12)return V(0);Require(Range(sample.airDensity,0,1.225001)&&Finite(sample.wind),"invalid weather sample for drag");const double force=.5*sample.airDensity*response.dragCoefficient*response.area*speed*speed*exposure;return relative/speed*std::min(force,mass*response.maxAcceleration);}
