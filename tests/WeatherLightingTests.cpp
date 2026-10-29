#include "EngineHost.h"
#include "Weather.h"
#include "ScreenshotWriter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {
constexpr int Width=320,Height=180;
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
using Pixels=std::vector<unsigned char>;
double Brightness(const Pixels& p){double sum=0;for(auto c:p)sum+=c;return sum/p.size();}
double Difference(const Pixels& a,const Pixels& b){double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(int(a[i])-int(b[i]));return sum/a.size();}
// Pair the exact same sky/time with a high shelter mask. Only local precipitation
// is rejected; setting rain=0 would also change storm darkening and is not a
// valid rain-visibility oracle.
void PrecipitationContrast(Renderer& r,const std::filesystem::path& output) {
 struct Conditions {const char* name;float coverage,density,rain;glm::vec3 wind,sky,sun,ambient;float exposure;};
 const Conditions cases[]={
  {"squalls",.9f,1.3f,.75f,{11,0,6},{.42f,.46f,.5f},.3f*glm::vec3(.8f,.84f,.9f),{.3f,.32f,.36f},1.1f},
  {"storm",1,1.6f,1,{16,0,-8},{.2f,.22f,.26f},.15f*glm::vec3(.7f,.75f,.85f),{.16f,.17f,.2f},1.2f},
  {"unlit",1,1.6f,1,{16,0,-8},{0,0,0},{0,0,0},{0,0,0},1.2f}
 };
 for(bool linear:{false,true})for(const auto& conditions:cases){
  WeatherSettings settings;WeatherPreset("sunny",settings.profile);
  settings.profile.cloudCoverage=conditions.coverage;settings.profile.cloudDensity=conditions.density;
  settings.profile.rain=conditions.rain;settings.profile.snow=0;settings.profile.fog=std::string(conditions.name)=="squalls"?.25:.35;
  settings.profile.cloudBase=400;settings.profile.cloudThickness=700;settings.profile.wind=conditions.wind;
  WeatherRuntime runtime(settings);auto sample=runtime.Sample({0,70,0});sample.active=true;sample.weight=1;sample.wind=conditions.wind;
  double contrast=0;int minimum=0;
  for(unsigned tick=0;tick<3;++tick){sample.time=17.25+tick/60.;
   auto draw=[&](bool roof){
    r.SetSceneAppearance(linear,conditions.exposure,{},1,{1,0,0,0},false,conditions.sky);r.BeginFrame(Width,Height);
    r.SetCamera(glm::lookAt(glm::vec3(0),glm::vec3(0,0,-1),glm::vec3(0,1,0)),glm::perspective(glm::radians(65.f),float(Width)/Height,.1f,100000.f));
    r.SetLighting(glm::normalize(glm::vec3(.3f,.85f,.4f)),conditions.sun,conditions.ambient);r.ClearWeather();r.SetWeather(sample,{0,70,0},1);
    WeatherShelterGrid shelter;shelter.valid=roof;shelter.center={0,70,0};shelter.extent=256;shelter.heights.fill(100);
    r.SetWeatherShelter(shelter);r.DrawWeather(false);
    if(linear&&!roof&&tick==0){
     std::vector<float> rgba(size_t(Width)*Height*4);glReadPixels(0,0,Width,Height,GL_RGBA,GL_FLOAT,rgba.data());
     double alphaError=0;for(size_t i=3;i<rgba.size();i+=4)alphaError=std::max(alphaError,std::abs(double(rgba[i])-1));
     Check(alphaError<.00001,"rain scattering preserves the opaque sky target alpha");
    }
    r.EndFrame();Pixels pixels;r.CaptureFrame(Width,Height,pixels);
    Check(glGetError()==GL_NO_ERROR,"precipitation contrast has no GL error");
    if(tick==0){const auto path=output/(std::string(conditions.name)+(linear?"-hdr":"-legacy")+(roof?"-sheltered.png":"-rain.png"));Check(WriteRgbPng(path.string(),Width,Height,pixels),"capture paired precipitation control");}
    return pixels;
   };
   const auto rain=draw(false),sheltered=draw(true);contrast+=Difference(rain,sheltered)/3;
   for(size_t i=0;i<rain.size();++i)minimum=std::min(minimum,int(rain[i])-int(sheltered[i]));
  }
  std::printf("PRECIPITATION %s linear=%d paired_contrast=%.9f minimum_channel_delta=%d\n",conditions.name,linear,contrast,minimum);
  if(std::string(conditions.name)=="unlit")Check(contrast<.001,"negative control: rain has no emissive brightness floor in darkness");
  else {Check(contrast>.08,"dim daytime rain retains visible contrast against its unchanged sky");Check(minimum>=-1,"thin rain highlights do not replace matching sky with opaque albedo");}
 }
}

}
int main(int argc,char** argv){
 const auto output=std::filesystem::path(argc>1?argv[1]:"build/weather-lighting");std::filesystem::create_directories(output);
 EngineHost host;std::string error;Check(host.Init("Weather lighting regression",Width,Height,false,error),"native renderer starts");if(failures){std::puts(error.c_str());return 1;}
 auto& r=host.GetRenderer();WeatherSettings settings;WeatherPreset("sunny",settings.profile);settings.profile.cloudCoverage=0;settings.profile.fog=settings.profile.rain=settings.profile.snow=0;WeatherRuntime weather(settings);auto sample=weather.Sample({0,2,0});sample.active=true;sample.weight=1;sample.profile.cloudBase=400;sample.profile.cloudThickness=700;
 bool blackWall=false;
 auto frame=[&](const char* name,glm::vec3 background,glm::vec3 sunlight,glm::vec3 ambient,bool enabled=true,bool linear=false,glm::quat rotation=glm::quat(1,0,0,0)){
  r.SetSceneAppearance(linear,1,{},1,{1,0,0,0},false,background);r.BeginFrame(Width,Height);
  r.SetCamera(glm::lookAt(glm::vec3(0),rotation*glm::vec3(0,.35f,-1),rotation*glm::vec3(0,1,0)),glm::perspective(glm::radians(65.f),float(Width)/Height,.1f,100000.f));r.SetLighting(rotation*glm::normalize(glm::vec3(.3f,.7f,.2f)),sunlight,ambient);r.ClearWeather();auto rotated=sample;rotated.up=glm::dmat3(glm::mat3_cast(rotation))*sample.up;rotated.frame.north=glm::dmat3(glm::mat3_cast(rotation))*sample.frame.north;r.SetWeather(rotated,{0,2,0},enabled?1:0);if(blackWall)r.DrawBox({0,0,-20},{1,0,0,0},{50,50,1},{0,0,0});r.DrawWeather(false);r.EndFrame();Pixels p;r.CaptureFrame(Width,Height,p);Check(glGetError()==GL_NO_ERROR,"weather lighting has no GL error");Check(WriteRgbPng((output/(std::string(name)+".png")).string(),Width,Height,p),"capture actual weather output");return p;
 };
 const glm::vec3 daySky(.52f,.72f,.9f),nightSky(.012f,.018f,.04f),daySun(1,.96f,.88f),nightSun=.12f*glm::vec3(.55f,.65f,.95f),dayAmbient(.3f,.34f,.4f),nightAmbient(.035f,.045f,.08f);
 for(bool linear:{false,true}){auto day=frame(linear?"day-hdr":"day",daySky,daySun,dayAmbient,true,linear);auto night=frame(linear?"night-hdr":"night",nightSky,nightSun,nightAmbient,true,linear);Check(Brightness(day)>Brightness(night)*3,"night weather respects authored darkness even with an above-horizon moon light");auto dusk=frame(linear?"dusk-hdr":"dusk",{.86f,.52f,.38f},{.8f,.44f,.24f},{.24f,.17f,.19f},true,linear);double red=0,blue=0;for(size_t i=0;i<dusk.size();i+=3){red+=dusk[i];blue+=dusk[i+2];}Check(red>blue,"dusk retains authored warm sky rather than fixed daylight blue");std::printf("LIGHTING linear=%d day=%g night=%g\n",linear,Brightness(day),Brightness(night));}
 auto off=frame("weather-off",nightSky,nightSun,nightAmbient,false);Check(!r.WeatherStats().enabled,"weather off stays disabled");Check(Brightness(off)<12,"disabled weather preserves the ordinary dark background");
 sample.profile.cloudCoverage=1;sample.profile.cloudDensity=1.5;auto litCloud=frame("day-cloud",daySky,daySun,dayAmbient);auto darkCloud=frame("night-cloud",nightSky,nightSun,nightAmbient);Check(Brightness(litCloud)>Brightness(darkCloud),"cloud ambient/direct illumination follows scene night lighting");auto unlitCloud=frame("unlit-cloud",{0,0,0},{0,0,0},{0,0,0});auto ambientCloud=frame("ambient-cloud",{0,0,0},{0,0,0},dayAmbient);Check(Brightness(unlitCloud)<.01,"negative control: unlit clouds do not emit fixed daylight");Check(Brightness(ambientCloud)>Brightness(unlitCloud)+10,"clouds receive actual scene ambient illumination");sample.lightningFlash=1;sample.lightningPosition={20,0,-200};auto flash=frame("night-flash",nightSky,nightSun,nightAmbient);Check(Brightness(flash)>Brightness(darkCloud)+5,"lightning still illuminates a night storm");
 sample.lightningFlash=0;sample.profile.cloudCoverage=0;sample.profile.fog=.5;blackWall=true;auto fogDay=frame("fog-day",nightSky,{0,0,0},dayAmbient);auto fogNight=frame("fog-night",nightSky,{0,0,0},nightAmbient);Check(Brightness(fogDay)>Brightness(fogNight)*2,"fog illumination follows scene ambient without a fixed daylight term");blackWall=false;sample.profile.fog=0;auto upright=frame("upright-sky",daySky,daySun,dayAmbient);auto q=glm::angleAxis(.8f,glm::normalize(glm::vec3(1,2,3)));auto rotated=frame("rotated-sky",daySky,daySun,dayAmbient,true,false,q);Check(Difference(upright,rotated)<1,"lighting is covariant under arbitrary-up rotation");
 PrecipitationContrast(r,output);
 std::printf("WEATHER_LIGHTING checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
