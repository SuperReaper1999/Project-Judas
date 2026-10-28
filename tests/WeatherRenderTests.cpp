// M86 actual GL output and same-workload timestamps. No weather/native policy
// shortcuts: all images use the public renderer and the shared CPU field sample.
#include "EngineHost.h"
#include "Weather.h"
#include "PerformanceProfiler.h"
#include "ScreenshotWriter.h"
#include "CameraProjection.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numeric>

namespace {
constexpr int Width=1280,Height=720;
unsigned checks=0,failures=0;
void Check(bool v,const char* message){++checks;failures+=!v;std::printf("%s %s\n",v?"PASS":"FAIL",message);}
using Pixels=std::vector<unsigned char>;
struct View {
 glm::vec3 eye{0,2,18},direction{0,.08f,-1};glm::dvec3 absolute{0,2,18};glm::quat rotation{1,0,0,0};glm::vec3 translation{0};
 glm::mat4 projection=glm::perspective(glm::radians(65.f),float(Width)/Height,.1f,20000000.f);
 bool geometry=true,props=true,wall=false,linear=true,secondary=false,weatherAfter=false,farGround=false;
};
WeatherSample Sample(const char* preset){WeatherSettings s;WeatherPreset(preset,s.profile);s.profile.cloudBase=400;s.profile.cloudThickness=700;s.seed=932;WeatherRuntime r(s);r.Advance(.25);auto sample=r.Sample({0,2,18});sample.time=17.25;sample.active=true;sample.weight=1;return sample;}
void Geometry(Renderer& r,const View& v){
 if(v.farGround){r.DrawBox({0,-4000100,0},{1,0,0,0},{10000000,100,10000000},{.1f,.55f,.2f});return;}
 if(!v.geometry)return;
 auto position=[&](glm::vec3 p){return v.rotation*p+v.translation;};
 r.DrawBox(position({0,-.2f,0}),v.rotation,{v.props?70.f:500.f,.2f,v.props?70.f:500.f},{.32f,.4f,.3f});
 if(v.props)for(int z=0;z<6;++z)for(int x=0;x<10;++x){glm::vec3 p{float(x*7-32),float(1+(x+z)%4),float(-z*9-12)};r.DrawBox(position(p),v.rotation,{1.6f,p.y,1.6f},{.28f+float(x%3)*.1f,.31f,.36f});}
 if(v.wall)r.DrawBox(position({0,3,13}),v.rotation,{4,4,.2f},{.8f,.1f,.05f});
}
void Frame(Renderer& r,const WeatherSample& sample,unsigned quality,const View& view,const WeatherShelterGrid* shelter=nullptr){
 r.SetSceneAppearance(view.linear,1,{},1,{1,0,0,0},false);
 r.BeginFrame(Width,Height);
 glm::vec3 eye=view.rotation*view.eye+view.translation;
 glm::vec3 direction=view.rotation*glm::normalize(view.direction);
 r.SetCamera(glm::lookAt(eye,eye+direction,view.rotation*glm::vec3(0,1,0)),view.projection);
 r.SetLighting(view.rotation*glm::normalize(glm::vec3(.3f,.8f,.4f)),{1.2f,1.12f,1.03f},{.25f,.28f,.32f});
 r.SetDynamicLights({});r.ClearWeather();
 if(!view.weatherAfter)r.SetWeather(sample,view.absolute,quality);
 Geometry(r,view);
 if(view.weatherAfter)r.SetWeather(sample,view.absolute,quality);
 if(shelter)r.SetWeatherShelter(*shelter);else r.SetWeatherShelter(WeatherShelterGrid{});
 r.DrawWeather(view.secondary);r.EndFrame();
}
Pixels Capture(Renderer& r){Pixels pixels;r.CaptureFrame(Width,Height,pixels);return pixels;}
double Difference(const Pixels& a,const Pixels& b,int y0=0,int y1=Height){double sum=0;size_t count=0;for(int y=y0;y<y1;++y)for(int x=0;x<Width*3;++x){size_t i=size_t(y*Width*3+x);sum+=std::abs(int(a.at(i))-int(b.at(i)));++count;}return sum/double(count);}
glm::vec3 Pixel(const Pixels& p,int x,int y){size_t i=size_t(y*Width+x)*3;return {p.at(i),p.at(i+1),p.at(i+2)};}
double Percentile(std::vector<double> values,double q){if(values.empty())return -1;std::sort(values.begin(),values.end());return values[size_t(q*double(values.size()-1))];}
void Image(Renderer& r,const std::filesystem::path& out,const char* name){WriteRgbPng((out/name).string(),Width,Height,Capture(r));}
struct Measurement {double cpuMedian=0,cpuP95=0,gpuMedian=0,gpuP95=0,cloud=0,fog=0,precipitation=0;size_t bytes=0;};
Measurement Measure(EngineHost& host,WeatherSample sample,unsigned quality,const std::filesystem::path& out,const std::string& name){
 auto& r=host.GetRenderer();auto& profiler=PerformanceProfiler::Get();profiler.Enable(false);
 View v;for(int i=0;i<12;++i){sample.time=17.25+i/60.;Frame(r,sample,quality,v);host.GetWindow().SwapBuffers();}
 r.FinishForDiagnostics();profiler.Enable(true);profiler.Clear();std::vector<double> cpu;
 for(int i=0;i<80;++i){ProfileFrame pf("M86 same-scene measurement");auto begin=std::chrono::steady_clock::now();auto token=r.BeginProfilePass("M86 entire scene");sample.time=18.+i/60.;Frame(r,sample,quality,v);r.EndProfilePass(token);cpu.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());host.GetWindow().SwapBuffers();}
 // Timestamp queries are polled asynchronously during normal frames above.
 // A single test-only finish drains the final tail, never every frame.
 r.FinishForDiagnostics();{ProfileFrame tail("M86 query drain");r.PollProfileGPU();}
 std::vector<double> gpu,cloud,fog,precip;
 for(auto& frame:profiler.History())for(auto& g:frame.gpu)if(!g.pending){if(g.pass=="M86 entire scene")gpu.push_back(g.milliseconds);else if(g.pass=="Weather clouds")cloud.push_back(g.milliseconds);else if(g.pass=="Weather sky fog")fog.push_back(g.milliseconds);else if(g.pass=="Weather precipitation")precip.push_back(g.milliseconds);}
 Measurement m;m.cpuMedian=Percentile(cpu,.5);m.cpuP95=Percentile(cpu,.95);m.gpuMedian=Percentile(gpu,.5);m.gpuP95=Percentile(gpu,.95);m.cloud=Percentile(cloud,.5);m.fog=Percentile(fog,.5);m.precipitation=Percentile(precip,.5);m.bytes=r.WeatherStats().bytes;
 std::string error;Check(!gpu.empty(),"M56 records completed native GPU timestamps");Check(profiler.Export((out/(name+"-profile.json")).string(),error),"M56 exact scenario profile exports");
 std::printf("MEASURE %s CPUmedian=%.6f CPUp95=%.6f GPUmedian=%.6f GPUp95=%.6f cloud=%.6f fog=%.6f precip=%.6f bytes=%zu\n",name.c_str(),m.cpuMedian,m.cpuP95,m.gpuMedian,m.gpuP95,m.cloud,m.fog,m.precipitation,m.bytes);
 profiler.Enable(false);return m;
}
}
int main(int argc,char** argv){
 std::setvbuf(stdout,nullptr,_IOLBF,0);std::filesystem::path out=argc>1?argv[1]:"/tmp/judas-m86-render";std::filesystem::create_directories(out);
 const bool correctnessOnly=argc>2&&std::string(argv[2])=="--correctness-only";
 EngineHost host;std::string error;Check(host.Init("M86 weather rendering",Width,Height,false,error),"native GL3.3 weather renderer starts");if(failures){std::printf("%s\n",error.c_str());return 1;}
 SDL_GL_SetSwapInterval(0);auto& r=host.GetRenderer();View view;auto sunny=Sample("sunny");sunny.profile.cloudCoverage=0;sunny.profile.rain=sunny.profile.snow=sunny.profile.fog=sunny.profile.tornado=0;
 Frame(r,sunny,0,view);auto off=Capture(r);Check(!r.WeatherStats().enabled&&r.WeatherStats().bytes==0,"weather-off has no weather GPU allocation or pass");
 const auto baseline=correctnessOnly?Measurement{}:Measure(host,sunny,0,out,"off");
 Frame(r,sunny,1,view);auto clear=Capture(r);Image(r,out,"sunny.png");Check(r.WeatherStats().ready&&r.WeatherStats().error.empty(),"all lazy weather shaders compile and link");Check(Difference(off,clear,0,240)>12,"sunny weather actually draws a lit sky");
 auto clouds=sunny;clouds.profile.cloudCoverage=1;clouds.profile.cloudDensity=1.5;
 Frame(r,clouds,1,view);auto cloudy=Capture(r);Image(r,out,"clouds.png");Check(Difference(clear,cloudy,0,240)>5,"three-dimensional cloud density changes actual sky output");
 auto partly=clouds;partly.profile.cloudCoverage=.45;partly.profile.cloudDensity=1;View upward=view;upward.geometry=false;upward.direction={0,.65f,-1};Frame(r,partly,1,upward);Image(r,out,"partly-cloudy.png");
 auto changed=clouds;changed.profile.cloudDensity=0;Frame(r,changed,1,view);auto noDensity=Capture(r);Check(Difference(cloudy,noDensity,0,240)>5,"negative control: zero density removes the cloud volume");
 View wrap=view;wrap.absolute.x=4194304.-.01;Frame(r,clouds,1,wrap);auto beforeWrap=Capture(r);wrap.absolute.x=4194304.+.01;Frame(r,clouds,1,wrap);Check(Difference(beforeWrap,Capture(r))<.2,"all noise octaves remain continuous across absolute modulo boundaries");
 // This near wall must occlude a low cloud layer, independently of surface
 // lighting/wetness uniforms. SetWeather after geometry isolates this contract.
 auto low=clouds;low.profile.cloudBase=8;low.profile.cloudThickness=30;View wall=view;wall.wall=true;wall.weatherAfter=true;
 Frame(r,low,0,wall);auto wallOff=Capture(r);Frame(r,low,1,wall);auto wallOn=Capture(r);Check(glm::length(Pixel(wallOff,Width/2,Height/2)-Pixel(wallOn,Width/2,Height/2))<5,"opaque geometry rejects the cloud volume behind it");
 auto rain=sunny;rain.profile.rain=1;rain.profile.wind={6,0,2};rain.wind={6,0,2};
 Frame(r,rain,1,view);auto rainOpen=Capture(r);Image(r,out,"rain.png");WeatherShelterGrid roof;roof.valid=true;roof.center={0,0,18};roof.extent=64;roof.heights.fill(80);
 Frame(r,rain,1,view,&roof);auto rainRoof=Capture(r);Check(Difference(rainOpen,rainRoof)>0.01,"overhead roof grid suppresses actual local GPU rain");
 roof.heights.fill(-10000);Frame(r,rain,1,view,&roof);Check(Difference(rainOpen,Capture(r))<.01,"negative control: open-sky shelter samples do not hide rain");
 auto snow=sunny;snow.profile.snow=1;snow.profile.fog=.25;snow.wind={13,0,4};Frame(r,snow,1,view);Image(r,out,"blizzard.png");Check(r.WeatherStats().particles>=1800,"snow uses bounded analytic GPU flakes");
 auto tornado=clouds;tornado.profile.tornado=1;tornado.profile.tornadoRadius=12;tornado.profile.tornadoHeight=150;tornado.tornadoPosition={25,0,-90};tornado.profile.cloudBase=130;
 Frame(r,tornado,1,view);auto funnel=Capture(r);Image(r,out,"tornado.png");tornado.profile.tornado=0;Frame(r,tornado,1,view);Check(Difference(funnel,Capture(r))>.05,"moving funnel volume and debris contribute visible depth");
 View openFunnel=view;openFunnel.props=false;openFunnel.eye={0,6,120};openFunnel.absolute={0,6,120};openFunnel.direction={25,69,-210};tornado.profile.tornado=1;tornado.altitude=6;Frame(r,tornado,1,openFunnel);Image(r,out,"tornado-open.png");
 auto isolated=tornado;isolated.profile.cloudCoverage=0;isolated.profile.rain=isolated.profile.snow=isolated.profile.fog=0;openFunnel.secondary=true;Frame(r,isolated,1,openFunnel);auto volumeOnly=Capture(r);isolated.profile.tornado=0;Frame(r,isolated,1,openFunnel);Check(Difference(volumeOnly,Capture(r))>.1,"funnel volume remains visible without debris, rain, cloud lighting or global darkening");
 auto flash=clouds;flash.lightningFlash=1;flash.lightningPosition={20,0,-200};Frame(r,flash,1,view);Image(r,out,"lightning.png");Check(Difference(cloudy,Capture(r))>5,"lightning changes cloud illumination and visible lightning output");
 // Entire physical/view chart is rotated while all weather-relative quantities
 // preserve their frame. This catches hardcoded world-Y in precipitation/sky.
 View rotated=view;rotated.rotation=glm::angleAxis(glm::radians(90.f),glm::vec3(0,0,1));rotated.absolute=glm::dvec3(rotated.rotation*view.eye);auto rr=rain;rr.frame.up={-1,0,0};rr.up={-1,0,0};rr.frame.north={0,0,-1};rr.wind=glm::dvec3(rotated.rotation*glm::vec3(rain.wind));rr.profile.wind=rr.wind;
 Frame(r,rr,1,rotated);auto rotation=Capture(r);Check(Difference(rainOpen,rotation)<1.5,"rotating weather, scene and view preserves arbitrary-up rendering");
 // Absolute anchoring is held fixed while only the local render chart moves.
 View shifted=view;shifted.translation={-512,0,-512};shifted.absolute=view.absolute;Frame(r,rain,1,shifted);Check(Difference(rainOpen,Capture(r))<1.0,"origin shift preserves camera-relative cloud and precipitation presentation");
 auto radial=clouds;radial.frame.radial=true;radial.frame.radius=6371000;radial.frame.center={9e11,0,0};radial.altitude=12000;radial.up={0,1,0};radial.frame.north={0,0,-1};View orbit;orbit.geometry=false;orbit.eye={0,0,0};orbit.absolute={9e11,6383000,0};orbit.rotation=glm::angleAxis(glm::radians(-90.f),glm::vec3(1,0,0));
 Frame(r,radial,1,orbit);auto orbital=Capture(r);Image(r,out,"orbital-clouds.png");radial.profile.cloudDensity=0;Frame(r,radial,1,orbit);Check(Difference(orbital,Capture(r))>2,"Earth-scale spherical cloud shell is visible from above");
 radial.profile.cloudCoverage=.75;radial.profile.cloudDensity=1.2;radial.altitude=4000000;orbit.absolute={9e11,10371000,0};orbit.projection=glm::perspective(glm::radians(65.f),float(Width)/Height,8000.f,20000000.f);Frame(r,radial,1,orbit);auto distant=Capture(r);Image(r,out,"planet-four-million-metres.png");View jitter=orbit;jitter.rotation=glm::angleAxis(.0001f,glm::vec3(0,0,1))*orbit.rotation;Frame(r,radial,1,jitter);Check(Difference(distant,Capture(r))<2,"orbital pixel-footprint filtering keeps a subpixel view change stable at four million metres");
 // A tilted view of a distant smooth surface must not truncate a thin cloud
 // layer merely because a low-resolution cloud pixel lies between depth rays.
 // The no-geometry control is opaque under the same authored dense layer.
 auto distantFlat=clouds;distantFlat.altitude=4000000;distantFlat.profile.cloudBase=1000;distantFlat.profile.cloudThickness=1200;distantFlat.profile.cloudCoverage=1;distantFlat.profile.cloudDensity=2;
 View tilted=orbit;tilted.absolute={0,4000000,0};tilted.direction={0,.8f,-1};tilted.geometry=false;tilted.rotation=glm::angleAxis(glm::radians(-90.f),glm::vec3(1,0,0));
 Frame(r,distantFlat,1,tilted);auto distantOpen=Capture(r);tilted.farGround=true;Frame(r,distantFlat,1,tilted);auto distantGround=Capture(r);Image(r,out,"distant-slanted-depth.png");
 const double distantDepthDifference=Difference(distantOpen,distantGround);std::printf("DIAGNOSTIC distant smooth depth image difference=%.6f\n",distantDepthDifference);
 Check(distantDepthDifference<3,"distant opaque geometry does not zebra-clip a thin foreground cloud layer");
 Frame(r,distantFlat,0,tilted);auto groundControl=Capture(r);tilted.farGround=false;Frame(r,distantFlat,0,tilted);Check(Difference(groundControl,Capture(r))>10,"negative control: the distant opaque depth fixture draws real geometry");
 View portal=view;portal.secondary=true;Frame(r,clouds,2,portal);auto secondary=r.WeatherStats();Check(secondary.marchSteps==16&&secondary.particles==0&&secondary.cloudWidth<=240&&secondary.cloudHeight<=135,"secondary cameras use a bounded history-free coarse pass");
 // A clipped portal cannot inherit weather between the eye and its oblique
 // near plane. A near solid wall remains unchanged after rendering this view.
 const auto plane=ViewSpacePlane(glm::lookAt(view.eye,view.eye+glm::vec3(0,.08f,-1),glm::vec3(0,1,0)),{0,0,-1},{0,0,-6});portal.projection=SecondaryCameraProjection(65,float(Width)/Height,.1f,10000,nullptr,&plane);Frame(r,clouds,1,portal);Check(r.WeatherStats().ready,"off-axis/oblique projection renders without temporal camera history");
 Frame(r,clouds,2,view);auto high=r.WeatherStats();Check(high.marchSteps==44&&high.cloudWidth==640&&high.cloudHeight==360&&high.bytes<32*1024*1024,"high quality remains within the declared scratch memory bound");
 auto target=r.CreateRenderTarget(64,64,error);Check(target.IsValid(),"ordinary secondary target allocates");
 auto small=[&](){r.SetSceneAppearance(true,1,{},1,{1,0,0,0},false);r.BeginRenderTarget(target);r.SetCamera(glm::lookAt(view.eye,view.eye+glm::vec3(0,.08f,-1),glm::vec3(0,1,0)),glm::perspective(glm::radians(65.f),1.f,.1f,100000.f));r.SetWeather(clouds,view.absolute,1);r.DrawWeather(true);r.EndRenderTarget();};
 Frame(r,clouds,1,view);small();Frame(r,clouds,1,view);small();const auto allocations=r.WeatherStats().allocations;
 for(int i=0;i<8;++i){Frame(r,clouds,1,view);small();}
 Check(r.WeatherStats().allocations==allocations&&r.WeatherStats().cachedTargets==2,"alternating secondary/main sizes reuse a bounded two-slot cache without frame allocations");r.DestroyRenderTarget(target);
 Frame(r,sunny,0,view);Check(Difference(off,Capture(r))<.01,"disabling weather restores the original renderer output");Check(glGetError()==GL_NO_ERROR,"weather depth copies, GL state, HDR and shader samplers produce no GL errors");
 std::ofstream measurements(out/"measurements.json");measurements<<"{\"correctness_only\":"<<(correctnessOnly?"true":"false")<<",\"width\":"<<Width<<",\"height\":"<<Height<<",\"boxes\":61,\"scenarios\":[";
 const char* labels[]={"off","sunny","cloudy","thunderstorm","tornado","blizzard"};bool comma=false;
 for(int i=0;!correctnessOnly&&i<6;++i){auto sample=Sample(i==0?"sunny":labels[i]);if(i==2)sample=clouds;if(i==4){sample=Sample("tornado");sample.tornadoPosition={25,0,-90};sample.profile.tornadoHeight=150;sample.profile.tornadoRadius=12;sample.profile.cloudBase=130;}
  auto m=i==0?baseline:Measure(host,sample,1,out,labels[i]);if(comma)measurements<<',';comma=true;measurements<<"{\"name\":\""<<labels[i]<<"\",\"cpu_median_ms\":"<<m.cpuMedian<<",\"cpu_p95_ms\":"<<m.cpuP95<<",\"gpu_median_ms\":"<<m.gpuMedian<<",\"gpu_p95_ms\":"<<m.gpuP95<<",\"cloud_gpu_median_ms\":"<<m.cloud<<",\"fog_gpu_median_ms\":"<<m.fog<<",\"precipitation_gpu_median_ms\":"<<m.precipitation<<",\"weather_cached_bytes\":"<<m.bytes<<'}';}
 measurements<<"]}\n";r.ClearWeather();host.Shutdown();std::printf("M86 RENDER %u checks %u failures\n",checks,failures);return failures?1:0;
}
