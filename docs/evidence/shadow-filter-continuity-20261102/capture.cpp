#include "EngineHost.h"
#include "SceneSerialization.h"
#include "ShadowTransforms.h"
#include "ScreenshotWriter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    Scene scene;std::string error;
    if(!LoadSceneFromFile(argv[1],scene,error)){std::cerr<<error;return 1;}
    EngineHost host;
    if(!host.Init("Shadow diagnosis",1000,760,false,error)){std::cerr<<error;return 1;}
    auto& r=host.GetRenderer();r.SetBatchingEnabled(false);
    auto draw=[&](){r.SetMaterialBindings({});for(const auto& o:scene.Objects())if(o.render&&o.render->shape==SceneShape::Box&&o.render->alpha==1)
        r.DrawBox(o.transform.position,o.transform.rotation,o.render->halfExtents*o.transform.scale,o.render->color);};
    std::filesystem::create_directories(argv[2]);
    for(int camera=0;camera<2;++camera)for(int shadow=0;shadow<2;++shadow){
        glm::vec3 eye=camera?glm::vec3(-2.3f,3.2f,4.2f):glm::vec3(-3.f,4.8f,6.8f);
        glm::vec3 target=camera?glm::vec3(0,1.1f,1.6f):glm::vec3(0,1.3f,1.7f);
        r.SetCamera(glm::lookAt(eye,target,{0,1,0}),glm::perspective(glm::radians(65.f),1000.f/760,.1f,100.f));
        if(shadow){r.BeginShadowPass(0,ComputeDirectionalShadowMatrix(eye,scene.Settings().sunDirection,25,40));draw();r.EndShadowPass();}
        else r.InvalidateShadowSlot(0);
        r.BeginFrame(1000,760);r.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.08f,.09f,.11f});
        r.SetLighting(scene.Settings().sunDirection,scene.Settings().sunColor,scene.Settings().ambientColor);r.SetDynamicLights({});
        draw();r.EndFrame();std::vector<unsigned char> pixels;r.CaptureFrame(1000,760,pixels);
        WriteRgbPng((std::filesystem::path(argv[2])/("camera"+std::to_string(camera)+(shadow?"-on.png":"-off.png"))).string(),1000,760,pixels);
    }
    auto e=glGetError();std::cout<<"GL error "<<e<<'\n';return e?1:0;
}
