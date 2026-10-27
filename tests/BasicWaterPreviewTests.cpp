#include "EngineHost.h"
#include "WorldPresentation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>
#include <cstdio>
#include <filesystem>
#include "ScreenshotWriter.h"

namespace {
constexpr int Width=256,Height=192;
unsigned checks=0,failures=0;
void Check(bool ok,const char* name){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",name);}
using Pixels=std::vector<unsigned char>;
Scene Fixture(bool waterFirst){
    Scene s;s.Settings().sunEnabled=false;s.Settings().ambientColor={1,1,1};
    auto water=[&](){auto& o=s.CreateObject("water");o.basicWater=BasicWaterSettings{};o.basicWater->halfExtents={3,2,2.4f};o.basicWater->waveAmplitude=0;o.basicWater->color={.02f,.4f,.8f};o.basicWater->opacity=.8f;};
    if(waterFirst)water();
    auto& floor=s.CreateObject("submerged ground");floor.transform.position={0,-1,0};floor.render=SceneRenderComponent{};floor.render->halfExtents={4,.2f,3};floor.render->color={.8f,.1f,.04f};
    auto& post=s.CreateObject("above-water occluder");post.transform.position={1,1,0};post.render=SceneRenderComponent{};post.render->halfExtents={.35f,1.5f,.35f};post.render->color={.2f,.9f,.15f};
    if(!waterFirst)water();
    return s;
}
float Difference(const Pixels& a,const Pixels& b){double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::abs(int(a[i])-int(b[i]));return float(sum/a.size());}
glm::vec3 Probe(const Pixels& pixels,const glm::vec3& point,const glm::mat4& view,const glm::mat4& projection){
    auto p=projection*view*glm::vec4(point,1);auto uv=glm::vec2(p)/p.w*.5f+.5f;
    int x=int(uv.x*Width),y=int((1-uv.y)*Height);glm::vec3 sum(0);
    for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i){auto index=size_t((y+j)*Width+x+i)*3;sum+=glm::vec3(pixels.at(index),pixels.at(index+1),pixels.at(index+2));}
    return sum/9.f;
}
}
int main(int argc,char** argv){
    std::string error;EngineHost host;if(!host.Init("Basic Water editor depth",Width,Height,false,error)){std::puts(error.c_str());return 1;}
    auto& r=host.GetRenderer();auto& assets=host.Resources();
    const auto out=std::filesystem::absolute(argc>1?argv[1]:".cache/water-preview");std::filesystem::create_directories(out);
    for(unsigned orientation=0;orientation<2;++orientation){
        const auto q=orientation?glm::angleAxis(.9f,glm::normalize(glm::vec3(1,2,3))):glm::quat(1,0,0,0);
        const glm::vec3 shift=orientation?glm::vec3(3,-2,1):glm::vec3(0);
        const auto view=glm::lookAt(shift+q*glm::vec3(0,8,0),shift,q*glm::vec3(0,0,-1));
        const auto projection=glm::ortho(-4.f,4.f,-3.f,3.f,.1f,30.f);
        auto first=Fixture(true),last=Fixture(false);
        for(auto* s:{&first,&last})for(auto& o:s->Objects()){o.transform.position=shift+q*o.transform.position;o.transform.rotation=q*o.transform.rotation;}
        auto frame=[&](const Scene& s,bool water){r.BeginFrame(Width,Height);r.SetCamera(view,projection);DrawAuthoredScene(r,s,assets,water);r.EndFrame();Pixels pixels;r.CaptureFrame(Width,Height,pixels);return pixels;};
        auto a=frame(first,true),b=frame(last,true),dry=frame(first,false);
        Check(Difference(a,b)<.1f,"authored object order cannot hide water behind submerged ground");
        auto p=shift+q*glm::vec3(-1,0,0);auto wet=Probe(a,p,view,projection),bare=Probe(dry,p,view,projection);
        std::printf("orientation=%u order_difference=%.6f water_difference=%.6f\n",orientation,Difference(a,b),glm::length(wet-bare));
        Check(glm::length(wet-bare)>30,"water is visible over real submerged geometry");
        p=shift+q*glm::vec3(1,2.5f,0);
        Check(glm::length(Probe(a,p,view,projection)-Probe(dry,p,view,projection))<2,"nearer above-water geometry still occludes water");
        Scene local,region;local.Settings()=first.Settings();region.Settings()=first.Settings();
        for(const auto& o:first.Objects())(o.basicWater?local:region).InsertObject(o);
        r.BeginFrame(Width,Height);r.SetCamera(view,projection);
        DrawAuthoredScene(r,local,assets,false);
        DrawAuthoredScene(r,region,assets,false);
        DrawAuthoredWaterSurfaces(r,local,assets);
        DrawAuthoredWaterSurfaces(r,region,assets);
        r.EndFrame();Pixels composed;r.CaptureFrame(Width,Height,composed);
        Check(Difference(composed,a)<.1f,"neighbouring-region opaque geometry is complete before local water");
        GLboolean depth;glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);Check(depth==GL_TRUE&&!glIsEnabled(GL_BLEND),"preview restores ordinary depth-write and blend state");
        auto hidden=first;for(auto& o:hidden.Objects())if(o.basicWater)o.renderVisible=false;
        Check(Difference(frame(hidden,true),dry)<.1f,"hidden water does not draw");
        auto disabled=first;for(auto& o:disabled.Objects())if(o.basicWater)o.basicWater->enabled=false;
        Check(Difference(frame(disabled,true),dry)<.1f,"disabled water does not draw");
        auto masked=first;masked.Settings().mainCameraRenderMask=CategoryBit(0);
        for(auto& o:masked.Objects())if(o.basicWater)o.renderLayer=1;
        Check(Difference(frame(masked,true),dry)<.1f,"water still respects authored camera layer masks");
        Check(WriteRgbPng((out/(std::string("water-first-")+std::to_string(orientation)+".png")).string(),Width,Height,a),"save actual preview reproduction");
    }
    std::printf("BasicWaterPreviewTests: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
