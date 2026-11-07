#include "WaterPass.h"
#include "WaterShaders.h"
#include "ShaderLibrary.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace JudasRender {
namespace {
// This pass must coexist with portable material, UI and secondary-camera passes.
struct State {
    GLint program=0,vao=0,active=0,read=0,draw=0,viewport[4]{},unpack=4;
    GLint unpackBuffer=0,rowLength=0,skipRows=0,skipPixels=0,depthFunc=GL_LESS,frontFace=GL_CCW;
    GLint sourceRGB=0,destRGB=0,sourceAlpha=0,destAlpha=0;
    GLboolean depth=0,cull=0,blend=0,scissor=0,write=0;
    std::array<GLint,4> texture2D{},samplers{};
    State(){
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);glGetBooleanv(GL_DEPTH_WRITEMASK,&write);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpackBuffer);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skipRows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skipPixels);
        glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetIntegerv(GL_FRONT_FACE,&frontFace);
        glGetIntegerv(GL_BLEND_SRC_RGB,&sourceRGB);glGetIntegerv(GL_BLEND_DST_RGB,&destRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA,&sourceAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&destAlpha);
        depth=glIsEnabled(GL_DEPTH_TEST);cull=glIsEnabled(GL_CULL_FACE);
        blend=glIsEnabled(GL_BLEND);scissor=glIsEnabled(GL_SCISSOR_TEST);
        for(unsigned i=0;i<4;++i){
            glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture2D[i]);
            glGetIntegeri_v(GL_SAMPLER_BINDING,i,&samplers[i]);glBindSampler(i,0);
        }
    }
    ~State(){
        glUseProgram(program);glBindVertexArray(vao);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glPixelStorei(GL_UNPACK_ALIGNMENT,unpack);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpackBuffer);glPixelStorei(GL_UNPACK_ROW_LENGTH,rowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,skipRows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skipPixels);
        glDepthFunc(depthFunc);glFrontFace(frontFace);
        glDepthMask(write);glBlendFuncSeparate(sourceRGB,destRGB,sourceAlpha,destAlpha);
        for(const auto &p:{std::pair<GLenum,GLboolean>{GL_DEPTH_TEST,depth},{GL_CULL_FACE,cull},{GL_BLEND,blend},{GL_SCISSOR_TEST,scissor}})
            if(p.second)glEnable(p.first);else glDisable(p.first);
        for(unsigned i=0;i<4;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,texture2D[i]);glBindSampler(i,samplers[i]);}
        glActiveTexture(active);
    }
};
bool Program(const char* vertex,const char* fragment,GLuint &program){
    GLuint vs=0,fs=0;
    bool okay=Shaders::CompileShader(GL_VERTEX_SHADER,vertex,vs,Shaders::PortableProfile(),"grid-water vertex")&&
        Shaders::CompileShader(GL_FRAGMENT_SHADER,fragment,fs,Shaders::PortableProfile(),"grid-water fragment")&&
        Shaders::LinkProgram(vs,fs,program,"grid-water presentation");
    if(vs)glDeleteShader(vs);
    if(fs)glDeleteShader(fs);
    return okay;
}
GLint Location(GLuint p,const char* name){return glGetUniformLocation(p,name);}
void Vector(GLuint p,const char* name,glm::vec3 v){glUniform3fv(Location(p,name),1,glm::value_ptr(v));}
void Matrix(GLuint p,const char* name,const glm::mat4& m){glUniformMatrix4fv(Location(p,name),1,GL_FALSE,glm::value_ptr(m));}
}
bool WaterPass::Prepare(std::string &error){
    if(surface_&&underwater_)return true;
    if(!Program(WaterShaders::SurfaceVertex,WaterShaders::SurfaceFragment,surface_)||
       !Program(WaterShaders::FullscreenVertex,WaterShaders::UnderwaterFragment,underwater_)){
        error="Grid water presentation shader compilation failed";Shutdown();return false;
    }
    glGenVertexArrays(1,&vao_);return true;
}
bool WaterPass::Capture(bool linear,std::string &error){
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    GLint viewport[4],source=0;glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&source);
    const int width=viewport[2],height=viewport[3];
    if(width<1||height<1){error="Grid water requires a nonempty viewport";return false;}
    if(!fbo_)glGenFramebuffers(1,&fbo_);
    if(!colour_)glGenTextures(1,&colour_);
    if(!depth_)glGenTextures(1,&depth_);
    if(width>width_||height>height_||linear_!=linear){
        const int allocationWidth=std::max(width_,(width+127)/128*128);
        const int allocationHeight=std::max(height_,(height+127)/128*128);
        glActiveTexture(GL_TEXTURE0);
        for(auto texture:{colour_,depth_}){
            glBindTexture(GL_TEXTURE_2D,texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,texture==colour_?GL_LINEAR:GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,texture==colour_?GL_LINEAR:GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            if(texture==colour_)glTexImage2D(GL_TEXTURE_2D,0,linear?GL_RGBA16F:GL_RGBA8,allocationWidth,allocationHeight,0,GL_RGBA,linear?GL_HALF_FLOAT:GL_UNSIGNED_BYTE,nullptr);
            else glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,allocationWidth,allocationHeight,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo_);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colour_,0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth_,0);
        if(glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){error="Grid water refraction framebuffer incomplete";glBindFramebuffer(GL_DRAW_FRAMEBUFFER,source);return false;}
        width_=allocationWidth;height_=allocationHeight;linear_=linear;
    }
    glDisable(GL_SCISSOR_TEST);glBindFramebuffer(GL_READ_FRAMEBUFFER,source);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,fbo_);
    glBlitFramebuffer(viewport[0],viewport[1],viewport[0]+width,viewport[1]+height,0,0,width,height,GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,source);return true;
}
bool WaterPass::Draw(std::uint64_t key,std::uint64_t revision,GLuint mesh,GLsizei count,
                     const JudasWater::SurfaceField& data,const View& camera,std::string& error){
    if(!mesh||!count||data.intervals.empty())return true;
    State state;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    if(!Prepare(error)||!Capture(camera.linear,error))return false;
    auto &field=fields_[key];
    if(!field.texture)glGenTextures(1,&field.texture);
    if(state.scissor)glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,field.texture);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    const bool resized=field.cells!=data.cells;
    if(resized){
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1024,int((data.intervals.size()+1023)/1024),0,GL_RGBA,GL_FLOAT,nullptr);
        field.cells=data.cells;
    }
    if(resized||field.revision!=revision){
        // Upload complete rows plus a bounded partial row; never read padding
        // beyond the input vector. All cells retain their actual 3-D identity.
        const auto rows=data.intervals.size()/1024,remainder=data.intervals.size()%1024;
        if(rows)glTexSubImage2D(GL_TEXTURE_2D,0,0,0,1024,int(rows),GL_RGBA,GL_FLOAT,data.intervals.data());
        if(remainder)glTexSubImage2D(GL_TEXTURE_2D,0,0,int(rows),int(remainder),1,GL_RGBA,GL_FLOAT,data.intervals.data()+rows*1024);
        field.revision=revision;
    }
    const glm::mat4 vp=camera.projection*camera.view,inverse=glm::inverse(vp);
    const glm::mat3 basis=glm::mat3_cast(data.rotation);
    const auto eye=glm::vec3(glm::inverse(camera.view)[3]);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,colour_);
    glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,depth_);
    glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,camera.environment);
    glUseProgram(surface_);
    Matrix(surface_,"vp",vp);Matrix(surface_,"inverseVP",inverse);Matrix(surface_,"view",camera.view);
    glUniformMatrix3fv(Location(surface_,"basis"),1,GL_FALSE,glm::value_ptr(basis));
    Vector(surface_,"origin",data.origin);glUniform3i(Location(surface_,"cells"),data.cells.x,data.cells.y,data.cells.z);
    glUniform1f(Location(surface_,"spacing"),data.spacing);glUniform1i(Location(surface_,"field"),2);
    Vector(surface_,"eye",eye);Vector(surface_,"sun",camera.sun);Vector(surface_,"sunColour",camera.sunColour);Vector(surface_,"waterColour",camera.colour);
    Vector(surface_,"skyColour",camera.background);Vector(surface_,"ambientColour",camera.ambient);
    glUniform1i(Location(surface_,"environmentMap"),3);glUniform1i(Location(surface_,"hasEnvironment"),camera.environment!=0);
    glUniform1f(Location(surface_,"environmentIntensity"),camera.environmentIntensity);
    glUniformMatrix3fv(Location(surface_,"environmentInverse"),1,GL_FALSE,glm::value_ptr(camera.environmentInverse));
    glUniform2f(Location(surface_,"resolution"),float(state.viewport[2]),float(state.viewport[3]));
    glUniform2f(Location(surface_,"captureScale"),float(state.viewport[2])/width_,float(state.viewport[3])/height_);
    glUniform2f(Location(surface_,"viewportOrigin"),float(state.viewport[0]),float(state.viewport[1]));
    glUniform1f(Location(surface_,"seconds"),camera.seconds);glUniform1f(Location(surface_,"opacity"),camera.opacity);
    glUniform1i(Location(surface_,"linearOutput"),camera.linear);glUniform1i(Location(surface_,"sceneColour"),0);glUniform1i(Location(surface_,"sceneDepth"),1);
    glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);glFrontFace(GL_CCW);
    glBindVertexArray(mesh);glDrawArrays(GL_TRIANGLES,0,count);
    ++draws_;
    // Post optics are needed only when this camera can intersect the medium.
    const auto local=glm::transpose(basis)*(eye-data.origin),extent=glm::vec3(data.cells)*data.spacing;
    const auto nearCorner=inverse*glm::vec4(-1,-1,-1,1);
    const float nearReach=glm::length(glm::vec3(nearCorner)/nearCorner.w-eye)+.02f;
    bool nearWater=false;
    if(local.x>=-nearReach&&local.z>=-nearReach&&local.x<extent.x+nearReach&&local.z<extent.z+nearReach&&local.y>=-nearReach&&local.y<=extent.y+nearReach){
        const int x=std::clamp(int(std::floor(local.x/data.spacing)),0,data.cells.x-1),z=std::clamp(int(std::floor(local.z/data.spacing)),0,data.cells.z-1);
        for(int y=0;y<data.cells.y;++y){const auto v=data.intervals[data.Index(x,y,z)];
            if(v.z>0&&local.y>=v.x-nearReach&&local.y<=v.y+nearReach){nearWater=true;break;}}
    }
    if(nearWater){
        const Pending p{key,camera,data.origin,data.cells,basis,data.spacing};
        auto existing=std::find_if(pending_.begin(),pending_.end(),[&](const auto& v){return v.key==key;});
        if(existing==pending_.end())pending_.push_back(p);else *existing=p;
    }

    return true;
}
bool WaterPass::Composite(std::string& error){
    bool okay=true;
    for(const auto& pending:pending_){
        const auto field=fields_.find(pending.key);if(field==fields_.end())continue;
        State state;
        if(!Capture(pending.camera.linear,error)){okay=false;break;}
        if(state.scissor)glEnable(GL_SCISSOR_TEST);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,colour_);
        glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,depth_);
        glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,field->second.texture);
        glUseProgram(underwater_);
        const auto inverse=glm::inverse(pending.camera.projection*pending.camera.view);
        Matrix(underwater_,"inverseVP",inverse);
        const auto inverseBasis=glm::transpose(pending.basis);
        glUniformMatrix3fv(Location(underwater_,"inverseBasis"),1,GL_FALSE,glm::value_ptr(inverseBasis));
        Vector(underwater_,"origin",pending.origin);Vector(underwater_,"extent",glm::vec3(pending.cells)*pending.spacing);
        glUniform3i(Location(underwater_,"cells"),pending.cells.x,pending.cells.y,pending.cells.z);
        glUniform1f(Location(underwater_,"spacing"),pending.spacing);
        glUniform1f(Location(underwater_,"seconds"),pending.camera.seconds);
        Vector(underwater_,"ambientColour",pending.camera.ambient);Vector(underwater_,"sunColour",pending.camera.sunColour);
        Vector(underwater_,"waterColour",pending.camera.colour);glUniform1f(Location(underwater_,"opacity"),pending.camera.opacity);
        glUniform1i(Location(underwater_,"field"),2);glUniform1i(Location(underwater_,"sceneDepth"),1);
        glUniform2f(Location(underwater_,"captureScale"),float(state.viewport[2])/width_,float(state.viewport[3])/height_);
        glUniform1i(Location(underwater_,"sceneColour"),0);glUniform1i(Location(underwater_,"linearOutput"),pending.camera.linear);
        glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);
        glBindVertexArray(vao_);glDrawArrays(GL_TRIANGLES,0,3);++draws_;++underwaterDraws_;
    }
    pending_.clear();return okay;
}

void WaterPass::Forget(std::uint64_t key){pending_.erase(std::remove_if(pending_.begin(),pending_.end(),[&](const auto& p){return p.key==key;}),pending_.end());auto it=fields_.find(key);if(it==fields_.end())return;if(it->second.texture)glDeleteTextures(1,&it->second.texture);fields_.erase(it);}
void WaterPass::Shutdown(){
    pending_.clear();
    for(const auto &[key,f]:fields_){(void)key;if(f.texture)glDeleteTextures(1,&f.texture);}fields_.clear();
    if(surface_)glDeleteProgram(surface_);
    if(underwater_)glDeleteProgram(underwater_);
    if(vao_)glDeleteVertexArrays(1,&vao_);
    if(fbo_)glDeleteFramebuffers(1,&fbo_);
    if(colour_)glDeleteTextures(1,&colour_);
    if(depth_)glDeleteTextures(1,&depth_);
    surface_=underwater_=vao_=fbo_=colour_=depth_=0;width_=height_=0;
}
std::size_t WaterPass::Bytes() const {
    std::size_t size=std::size_t(width_)*height_*(linear_?12:8);
    for(const auto &[key,f]:fields_){(void)key;size+=((std::size_t(f.cells.x)*f.cells.y*f.cells.z+1023)/1024)*1024*16;}return size;
}
} // namespace JudasRender
