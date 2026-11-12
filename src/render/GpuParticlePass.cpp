#include "GpuParticlePass.h"
#include "../GraphicsApi.h"
#include "../particles/GpuParticles.h"
#include "ShaderLibrary.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <glm/gtc/type_ptr.hpp>

namespace JudasRender {
struct GpuParticlePass::Storage {
    unsigned program=0,vao=0;
    // Vulkan-native variant: per-particle records are fetched from shader storage
    // by instance index instead of instance-rate vertex attributes.
    bool pull=false;
    std::uintptr_t context=0;
    std::thread::id thread;
    GpuParticlePassStats stats;
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    bool Own(std::string& error){
        const auto active=GraphicsContextIdentity();
        if(!active){error="GPU particle drawing needs its owning graphics context.";return false;}
        if(context&&(context!=active||thread!=std::this_thread::get_id())){error="GPU particle draw resources belong to another context/thread.";return false;}
        context=active;thread=std::this_thread::get_id();return true;
    }
    bool Initialize(std::string& error){
        if(program&&vao)return true;
        constexpr auto vertex=R"GLSL(#version 450 core
layout(location=0) in vec4 centerSize;layout(location=1) in vec4 particleColour;
uniform mat4 vp;uniform vec3 cameraRight,cameraUp;out vec2 texcoord;out vec4 tint;
void main(){vec2 corners[4]=vec2[4](vec2(-.5,-.5),vec2(.5,-.5),vec2(.5,.5),vec2(-.5,.5));int indices[6]=int[6](0,1,2,0,2,3);vec2 corner=corners[indices[gl_VertexID]];vec3 world=centerSize.xyz+centerSize.w*(cameraRight*corner.x+cameraUp*corner.y);gl_Position=vp*vec4(world,1);texcoord=corner+vec2(.5);tint=particleColour;}
)GLSL";
        constexpr auto fragment=R"GLSL(#version 450 core
in vec2 texcoord;in vec4 tint;uniform sampler2D image;uniform bool modern,imageLinear;out vec4 result;
vec3 linear(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
void main(){vec4 sampleColour=texture(image,texcoord);result=sampleColour*tint;if(modern)result.rgb=(imageLinear?sampleColour.rgb:linear(sampleColour.rgb))*linear(tint.rgb);}
)GLSL";
        constexpr auto pullVertex=R"GLSL(#version 450 core
struct Record {vec4 centerSize;vec4 color;};
layout(std430,binding=4) readonly buffer Records {Record records[];};
uniform mat4 vp;uniform vec3 cameraRight,cameraUp;out vec2 texcoord;out vec4 tint;
void main(){Record record=records[gl_InstanceID];vec2 corners[4]=vec2[4](vec2(-.5,-.5),vec2(.5,-.5),vec2(.5,.5),vec2(-.5,.5));int indices[6]=int[6](0,1,2,0,2,3);vec2 corner=corners[indices[gl_VertexID]];vec3 world=record.centerSize.xyz+record.centerSize.w*(cameraRight*corner.x+cameraUp*corner.y);gl_Position=vp*vec4(world,1);texcoord=corner+vec2(.5);tint=record.color;}
)GLSL";
        if(!program){
            const char* policy=std::getenv("JUDAS_VULKAN_PARTICLE_PULL");
            pull=JudasVulkan::Active()&&!(policy&&std::strcmp(policy,"0")==0);
            unsigned vs=0,fs=0;
            if(!Shaders::CompileShader(GL_VERTEX_SHADER,pull?pullVertex:vertex,vs,Shaders::Profile::Desktop450,"ParticlesV2/resident-billboard.vert")){error="Resident particle vertex shader failed.";return false;}
            if(!Shaders::CompileShader(GL_FRAGMENT_SHADER,fragment,fs,Shaders::Profile::Desktop450,"ParticlesV2/resident-billboard.frag")){glDeleteShader(vs);error="Resident particle fragment shader failed.";return false;}
            const bool linked=Shaders::LinkProgram(vs,fs,program,"ParticlesV2/resident-billboard");glDeleteShader(vs);glDeleteShader(fs);
            if(!linked){error="Resident particle draw program failed.";return false;}
        }
        if(!vao)glGenVertexArrays(1,&vao);
        if(!vao){error="Cannot create resident particle draw VAO.";return false;}
        return true;
    }
#endif
};
GpuParticlePass::GpuParticlePass():storage(std::make_unique<Storage>()){}
GpuParticlePass::~GpuParticlePass(){if(storage->program||storage->vao)std::fprintf(stderr,"[Particles GPU] Draw owner omitted Shutdown; destructor performs no GL calls.\n");}
GpuParticlePassStats GpuParticlePass::Stats()const{return storage->stats;}
bool GpuParticlePass::Draw(JudasParticles::GpuParticles& particles,const glm::mat4& view,const glm::mat4& projection,
                          unsigned texture,bool imageLinear,bool sceneLinear,std::string& error){
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    (void)particles;(void)view;(void)projection;(void)texture;(void)imageLinear;(void)sceneLinear;
    error="Resident compute particles are unavailable on this portable backend; select CPU particles.";return false;
#else
    error.clear();auto& s=*storage;
    if(!particles.Initialized()){error="Resident GPU particle owner is not initialized.";return false;}
    if(!s.Own(error)||!s.Initialize(error)||!particles.PrepareView(view,projection,true,error))return false;
    const auto inverse=glm::inverse(view);const auto right=glm::vec3(inverse[0]),up=glm::vec3(inverse[1]);const auto vp=projection*view;
    GLboolean cull=glIsEnabled(GL_CULL_FACE),blend=glIsEnabled(GL_BLEND),depthMask;
    GLint sourceRgb,destinationRgb,sourceAlpha,destinationAlpha,indirect;
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetIntegerv(GL_BLEND_SRC_RGB,&sourceRgb);glGetIntegerv(GL_BLEND_DST_RGB,&destinationRgb);glGetIntegerv(GL_BLEND_SRC_ALPHA,&sourceAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&destinationAlpha);glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING,&indirect);
    glUseProgram(s.program);glBindVertexArray(s.vao);
    if(!(s.pull?particles.BindDrawStorage(4,error):particles.BindDrawBuffers(error))){glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_DRAW_INDIRECT_BUFFER,GLuint(indirect));return false;}
    glUniformMatrix4fv(glGetUniformLocation(s.program,"vp"),1,GL_FALSE,glm::value_ptr(vp));glUniform3fv(glGetUniformLocation(s.program,"cameraRight"),1,glm::value_ptr(right));glUniform3fv(glGetUniformLocation(s.program,"cameraUp"),1,glm::value_ptr(up));
    glUniform1i(glGetUniformLocation(s.program,"image"),0);glUniform1i(glGetUniformLocation(s.program,"modern"),sceneLinear);glUniform1i(glGetUniformLocation(s.program,"imageLinear"),imageLinear);
    glActiveTexture(GL_TEXTURE0);glBindSampler(0,0);glBindTexture(GL_TEXTURE_2D,texture);
    if(!s.pull){
    glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,32,nullptr);glEnableVertexAttribArray(0);glVertexAttribDivisor(0,1);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,32,reinterpret_cast<void*>(16));glEnableVertexAttribArray(1);glVertexAttribDivisor(1,1);
    }
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);
    glDrawArraysIndirect(GL_TRIANGLES,nullptr);++s.stats.draws;particles.RecordDraw();
    // The emitter owns this VBO. Do not retain its storage through our VAO
    // after the draw: Play/Stop or a CPU transition may retire that emitter.
    if(!s.pull){glVertexArrayVertexBuffer(s.vao,0,0,0,32);glVertexArrayVertexBuffer(s.vao,1,0,0,32);}
    glDepthMask(depthMask);if(cull)glEnable(GL_CULL_FACE);if(!blend)glDisable(GL_BLEND);glBlendFuncSeparate(sourceRgb,destinationRgb,sourceAlpha,destinationAlpha);
    glBindVertexArray(0);glBindBuffer(GL_ARRAY_BUFFER,0);glBindBuffer(GL_DRAW_INDIRECT_BUFFER,GLuint(indirect));return true;
#endif
}
bool GpuParticlePass::Shutdown(std::string* error){
    if(error)error->clear();
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
    std::string reason;
    if((storage->program||storage->vao)&&!storage->Own(reason)){if(error)*error=reason;return false;}
    if(storage->program){GLint current=0;glGetIntegerv(GL_CURRENT_PROGRAM,&current);if(GLuint(current)==storage->program)glUseProgram(0);}
    if(storage->program)glDeleteProgram(storage->program);
    if(storage->vao)glDeleteVertexArrays(1,&storage->vao);
#endif
    storage->program=storage->vao=0;storage->context=0;storage->thread={};return true;
}
}
