#include "EngineHost.h"
#include "render/MaterialUniformBatch.h"
#include "RuntimeWorld.h"
#include "WorldPresentation.h"
#include "PerformanceProfiler.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cmath>

namespace {
unsigned checks=0,failures=0;
void Check(bool okay,const char* message) {
    ++checks; failures+=!okay;
    std::printf("%s %s\n",okay?"PASS":"FAIL",message);
}
}
int main() {
    EngineHost host; std::string error;
    if(!host.Init("Material uniform publication",384,128,false,error)) {
        std::puts(error.c_str()); return 1;
    }
    auto& renderer=host.GetRenderer();
    MaterialDefinition material; material.model=MaterialModel::Unlit;
    material.baseColor={.05f,.8f,.05f,1};
    const auto green=renderer.CreateMaterial(material);
    material.baseColor={.05f,.05f,.8f,1};
    const auto blue=renderer.CreateMaterial(material);
    renderer.SetSceneAppearance(true,1,{},1,{1,0,0,0},false);
    renderer.ResetStats(); renderer.BeginFrame(384,128);
    renderer.SetCamera(glm::lookAt(glm::vec3(0,0,8),glm::vec3(0),glm::vec3(0,1,0)),
                       glm::ortho(-3.f,3.f,-1.f,1.f,.1f,20.f));
    for(int i=0;i<3;++i) {
        renderer.SetMaterialBindings({{i==2?blue:green,{}}});
        renderer.DrawBox({float(i-1)*2,0,0},{1,0,0,0},{.7f,.7f,.2f},{1,1,1});
    }
    const auto stats=renderer.Stats();
    Check(stats.materialUniformReuses>0&&stats.materialUniformPublishes>=2,
          "identical consecutive materials reuse uniforms; changed material publishes");
    GLint program=0; glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    JudasRender::MaterialUniformCache cache;
    JudasRender::MaterialUniformBatch packet;
    packet.Int("uAlphaMode",0); packet.Float("uRoughness",.375f);
    const GLfloat colour[]={.2f,.3f,.4f,1},emission[]={.05f,.1f,.15f};
    const GLfloat matrix[]={1,0,0,0,0,-1,0,1,0};
    packet.Vec4("uBaseFactor",colour); packet.Vec3("uEmission",emission);
    packet.Mat3("uEnvironmentInverse",matrix);
    const auto location=[&](const char* name){return glGetUniformLocation(GLuint(program),name);};
    Check(cache.Publish(packet,GLuint(program),location)&&!cache.Publish(packet,GLuint(program),location),
          "complete identical packet avoids repeated GL publication");
    GLfloat value=0; glGetUniformfv(GLuint(program),location("uRoughness"),&value);
    Check(value==.375f,"actual GPU scalar has the requested value");
    GLfloat actual[9]{};glGetUniformfv(GLuint(program),location("uEnvironmentInverse"),actual);
    bool same=true;for(unsigned i=0;i<9;++i)same&=actual[i]==matrix[i];
    Check(same,"actual GPU matrix retains all nine values");
    GLint integer=-1;glGetUniformiv(GLuint(program),location("uAlphaMode"),&integer);
    Check(integer==0,"integer uniform is published without float reinterpretation");
    glUniform1f(location("uRoughness"),.9f);cache.Invalidate();
    Check(cache.Publish(packet,GLuint(program),location),"external writer invalidation forces restoration");
    glGetUniformfv(GLuint(program),location("uRoughness"),&value);
    Check(value==.375f,"restoration corrects actual GPU state after invalidation");
    renderer.EndFrame();

    // A new ordinary frame must restore the renderer's own values after the
    // isolated packet control modified the shader above.
    renderer.BeginFrame(384,128);
    renderer.SetCamera(glm::lookAt(glm::vec3(0,0,8),glm::vec3(0),glm::vec3(0,1,0)),
                       glm::ortho(-3.f,3.f,-1.f,1.f,.1f,20.f));
    for(int i=0;i<3;++i){renderer.SetMaterialBindings({{i==2?blue:green,{}}});
        renderer.DrawBox({float(i-1)*2,0,0},{1,0,0,0},{.7f,.7f,.2f},{1,1,1});}
    renderer.EndFrame();std::vector<unsigned char> pixels;
    renderer.CaptureFrame(384,128,pixels);
    Check(pixels.size()==384*128*3,"capture actual repeated-material frame");
    if(pixels.size()!=384*128*3){host.Shutdown();return 1;}
    const auto sample=[&](unsigned x,unsigned channel){return pixels[(64*384+x)*3+channel];};
    Check(sample(64,1)>sample(64,0)+40&&sample(192,1)==sample(64,1),
          "reused green draws retain identical correct GPU colour");
    Check(sample(320,2)>sample(320,1)+40,"changed blue material is visible in the same frame");
    material.baseColor={1,1,1,1};const auto white=renderer.CreateMaterial(material);
    const auto redTexture=renderer.CreateTexture({1,1,{255,0,0,255}},true);
    const auto greenTexture=renderer.CreateTexture({1,1,{0,255,0,255}},true);
    renderer.BeginFrame(384,128);
    renderer.SetCamera(glm::lookAt(glm::vec3(0,0,8),glm::vec3(0),glm::vec3(0,1,0)),
                       glm::ortho(-3.f,3.f,-1.f,1.f,.1f,20.f));
    renderer.SetMaterialBindings({{white,{}}});
    renderer.DrawBox({-2,0,0},{1,0,0,0},{.7f,.7f,.2f},{1,1,1},1,redTexture);
    renderer.DrawBox({0,0,0},{1,0,0,0},{.7f,.7f,.2f},{1,1,1},1,greenTexture);
    renderer.EndFrame();renderer.CaptureFrame(384,128,pixels);
    Check(sample(64,0)>sample(64,1)+80&&sample(192,1)>sample(192,0)+80,
          "identical uniform state still binds each draw's actual texture");
    Check(glGetError()==GL_NO_ERROR,"publication and frame restoration produce no GL errors");
    renderer.DestroyTexture(redTexture);renderer.DestroyTexture(greenTexture);renderer.DestroyMaterial(white);
    renderer.DestroyMaterial(green);renderer.DestroyMaterial(blue);
    Scene scene;SceneObject box;box.id=1;box.name="Counter fixture";box.render=SceneRenderComponent{};
    scene.InsertObject(box);RuntimeWorld world;
    const bool built=world.Build(scene,&host.Resources(),error);
    Check(built,"ordinary world for frame counter validation");
    if(!built){std::puts(error.c_str());host.Shutdown();return 1;}
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);
    for(unsigned frame=0;frame<3;++frame){
        // Runtime leaves statistics accumulated; editor explicitly resets.
        // Both must publish just this world's work, including queued draws.
        if(frame==2)renderer.ResetStats();
        const auto before=renderer.Stats();profiler.BeginFrame("counter fixture");
        const auto id=profiler.FrameId();
        RenderWorldFrame(renderer,384,128,world,
            glm::lookAt(glm::vec3(0,0,8),glm::vec3(0),glm::vec3(0,1,0)),
            glm::ortho(-3.f,3.f,-1.f,1.f,.1f,20.f),glm::vec3(0),1);
        profiler.EndFrame();const auto snapshot=profiler.Snapshot(id);const auto after=renderer.Stats();
        auto counter=[&](const char* name){
            for(const auto& value:snapshot.counters){
                if(std::string(value.name.c_str())==name)return value.value;
            }
            return -1.;
        };
        Check(counter("Material applications")==double(after.materialApplications-before.materialApplications)&&
              counter("Presentation upload calls")==double(after.uploadCalls-before.uploadCalls)&&
              counter("Presentation upload bytes (view/light/instances/billboards)")==double(after.uploadedBytes-before.uploadedBytes),
              "world counters measure this submission with accumulated or reset renderer statistics");
    }
    profiler.Enable(false);world.Destroy();host.Shutdown();
    std::printf("SUMMARY %u checks %u failures\n",checks,failures);
    return failures?1:0;
}
