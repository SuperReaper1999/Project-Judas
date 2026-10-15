// Portal views: render-target camera off-axis frustum and oblique near plane (portal views).
#include <cmath>
#include <cstdio>
#include <string>
#include <glm/gtc/matrix_transform.hpp>
#include "CameraProjection.h"
#include "EngineHost.h"
#include "Renderer.h"
#include "RuntimeWorld.h"
#include "Scene.h"
#include "WorldPresentation.h"
namespace {
int checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
glm::vec3 Ndc(const glm::mat4& p,glm::vec3 v){auto c=p*glm::vec4(v,1);return glm::vec3(c)/c.w;}
bool Close(float a,float b,float e=1e-4f){return std::abs(a-b)<=e;}

void MathChecks(){
    // Off-axis frustum: the near-plane window corners land exactly on the NDC corners.
    const glm::vec4 f{-.1f,.3f,-.1f,.2f};
    const auto p=SecondaryCameraProjection(60,1,.1f,50,&f,nullptr);
    auto a=Ndc(p,{.3f,.2f,-.1f}),b=Ndc(p,{-.1f,-.1f,-.1f});
    Check(Close(a.x,1)&&Close(a.y,1)&&Close(a.z,-1)&&Close(b.x,-1)&&Close(b.y,-1),"off-axis frustum maps the near window corners to the NDC corners");
    // The same window seen further away scales with distance (a real perspective, not a shifted image).
    auto far=Ndc(p,{3.f,2.f,-1.f});Check(Close(far.x,1,1e-3f)&&Close(far.y,1,1e-3f),"off-axis frustum keeps perspective through the window");
    Check(!ValidCameraFrustum({.2f,.1f,0,1})&&!ValidCameraFrustum({0,1,1,1})&&ValidCameraFrustum(f),"inverted or empty frustums are rejected");
    // Oblique near plane: camera at the origin looking down -Z; keep everything beyond z=-2.
    const glm::mat4 view(1);
    const auto plane=ViewSpacePlane(view,{0,0,-1},{0,0,-2});
    Check(Close(plane.x,0)&&Close(plane.y,0)&&Close(plane.z,-1)&&Close(plane.w,-2),"world plane converts to view space with the kept side positive");
    const auto base=glm::perspective(glm::radians(60.f),1.f,.1f,50.f);
    const auto o=SecondaryCameraProjection(60,1,.1f,50,nullptr,&plane);
    Check(Close(Ndc(o,{0,0,-2}).z,-1,1e-3f)&&Close(Ndc(o,{.5f,.3f,-2}).z,-1,1e-3f),"points on the clip plane sit exactly on the new near plane");
    Check(Ndc(o,{0,0,-1.5f}).z<-1,"geometry between the eye and the plane is clipped");
    const float z=Ndc(o,{0,0,-10}).z;Check(z>-1&&z<1,"geometry beyond the plane stays inside the depth range");
    const auto px=Ndc(o,{1,.5f,-4}),bx=Ndc(base,{1,.5f,-4});
    Check(Close(px.x,bx.x)&&Close(px.y,bx.y),"oblique clipping leaves the image (x/y) unchanged");
    const auto behind=ViewSpacePlane(view,{0,0,1},{0,0,-2});
    Check(SecondaryCameraProjection(60,1,.1f,50,nullptr,&behind)==base,"a plane with the eye on its kept side leaves the projection unchanged");
}

Scene Fixture(){
    Scene s;s.Settings().name="Portal camera";s.Settings().ambientColor=glm::vec3(1);s.Settings().sunColor=glm::vec3(0);
    auto box=[&](const char* name,glm::vec3 at,glm::vec3 colour){auto& o=s.CreateObject(name);o.transform.position=at;o.render=SceneRenderComponent{};o.render->halfExtents={1,1,.1f};o.render->color=colour;};
    box("Red occluder",{0,0,-3},{1,0,0});
    box("Green beyond",{0,0,-8},{0,1,0});
    box("Blue to the right",{6,0,-6},{0,0,1});
    auto& c=s.CreateObject("Portal view");c.renderCamera=SceneRenderCameraComponent{};
    c.renderCamera->width=64;c.renderCamera->height=64;c.renderCamera->verticalFovDegrees=60;c.renderCamera->farPlane=50;
    return s;
}
glm::vec3 Centre(Renderer& r,RuntimeWorld& w){
    RenderWorldFrame(r,64,64,w,nullptr,glm::lookAt(glm::vec3(0,20,0),glm::vec3(0),glm::vec3(0,0,-1)),glm::perspective(1.f,1.f,.1f,50.f),{0,1,0},1);
    TextureData t;if(!r.ReadTextureForDiagnostics(w.CameraTexture(4),t)||t.width!=64)return glm::vec3(-1);
    const int k=(32*64+32)*4;return {t.pixels[k]/255.f,t.pixels[k+1]/255.f,t.pixels[k+2]/255.f};
}
bool Is(glm::vec3 c,glm::vec3 want){return glm::length(c-want)<.35f;}
}

int main(){
    MathChecks();
    std::string error;EngineHost host;
    Check(host.Init("Portal cameras",64,64,false,error),"EngineHost creates a GL context");if(failures)return 1;
    Renderer& renderer=host.GetRenderer();
    RuntimeWorld world;Check(world.Build(Fixture(),nullptr,error),"portal camera fixture builds");
    auto& cam=world.PresentationCameras()[0];
    Check(Is(Centre(renderer,world),{1,0,0}),"plain camera sees the near occluder");
    cam.hasClipPlane=true;cam.clipNormal={0,0,-1};cam.clipPoint={0,0,-4};
    Check(Is(Centre(renderer,world),{0,1,0}),"oblique clip plane removes the occluder in front of the window");
    cam.hasClipPlane=false;cam.hasFrustum=true;cam.frustum={.05f,.15f,-.05f,.05f};
    Check(Is(Centre(renderer,world),{0,0,1}),"off-axis frustum looks through a window to the right");
    cam.hasFrustum=false;
    Check(Is(Centre(renderer,world),{1,0,0}),"clearing both restores the ordinary symmetric view");
    world.Destroy();
    std::printf("%d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
