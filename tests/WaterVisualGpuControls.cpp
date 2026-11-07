#include "WaterVisualGpuControls.h"
#include "GraphicsApi.h"
#include "Renderer.h"
#include "BasicWater.h"
#include "ScreenshotWriter.h"
#include "fluid/GameWaterSurface.h"
#include "render/ShaderLibrary.h"
#include "render/WaterShaders.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>

// Capture the actual production vertex shader. The independent CPU interpolant
// supplies its known answer; neither a blue image nor a source substring is an oracle.
WaterGpuControls RunWaterVisualGpuControls(Renderer& renderer,const std::string& captureDirectory) {
    using namespace JudasRender;
    WaterGpuControls result;
    JudasWater::Settings settings;settings.cells={8,8,8};settings.cellSize=.4f;settings.origin={-1.6f,0,-1.6f};
    JudasWater::Grid water;std::string error;
    if(!water.Initialize(settings,error)){result.details=error;return result;}
    water.FillBox({-2,0,-2},{2,1.99f,2});auto field=JudasWater::BuildSurfaceField(water);
    // Independent affine render field: h(x)=1.6+.5*x, spanning several Y
    // cells. Both the height and slope have analytic answers. Fluid seeding
    // quadrature is deliberately not used as a height oracle.
    for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        const float height=1.6f+.5f*(x+.5f)*.4f;
        field.intervals[field.Index(x,y,z)]=y*.4f<height?glm::vec4(0,height,1,0):glm::vec4(-1,-1,0,8);
    }
    GLuint vertex=0,program=0,vao=0,input=0,output=0,texture=0;
    if(!Shaders::CompileShader(GL_VERTEX_SHADER,WaterShaders::SurfaceVertex,vertex)){
        result.details="production vertex shader did not compile";return result;
    }
    program=glCreateProgram();glAttachShader(program,vertex);
    const char* varying[]={"world","baseNormal"};
    glTransformFeedbackVaryings(program,2,varying,GL_INTERLEAVED_ATTRIBS);glLinkProgram(program);
    GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);
    if(linked){
        glGenVertexArrays(1,&vao);glBindVertexArray(vao);
        MeshVertex sample;sample.position={-.6f,1.6f,-.2f};sample.normal={0,1,0};sample.uv={1,1.4f};sample.tangent.w=1.6f;
        glGenBuffers(1,&input);glBindBuffer(GL_ARRAY_BUFFER,input);glBufferData(GL_ARRAY_BUFFER,sizeof(sample),&sample,GL_STATIC_DRAW);
        for(const auto& attribute:{glm::uvec3(0,3,offsetof(MeshVertex,position)),glm::uvec3(1,3,offsetof(MeshVertex,normal)),glm::uvec3(2,2,offsetof(MeshVertex,uv)),glm::uvec3(5,4,offsetof(MeshVertex,tangent))}){
            glEnableVertexAttribArray(attribute.x);glVertexAttribPointer(attribute.x,GLint(attribute.y),GL_FLOAT,GL_FALSE,sizeof(MeshVertex),reinterpret_cast<void*>(std::size_t(attribute.z)));
        }
        glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1024,1,0,GL_RGBA,GL_FLOAT,nullptr);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,512,1,GL_RGBA,GL_FLOAT,field.intervals.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glUseProgram(program);
        const auto identity=glm::mat4(1);const auto basis=glm::mat3(1);
        glUniformMatrix4fv(glGetUniformLocation(program,"vp"),1,GL_FALSE,glm::value_ptr(identity));
        glUniformMatrix3fv(glGetUniformLocation(program,"basis"),1,GL_FALSE,glm::value_ptr(basis));
        glUniform3fv(glGetUniformLocation(program,"origin"),1,glm::value_ptr(field.origin));
        glUniform3i(glGetUniformLocation(program,"cells"),8,8,8);glUniform1f(glGetUniformLocation(program,"spacing"),.4f);
        glUniform1i(glGetUniformLocation(program,"field"),2);glUniform1f(glGetUniformLocation(program,"seconds"),0);
        glGenBuffers(1,&output);glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,output);glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER,6*sizeof(float),nullptr,GL_STREAM_READ);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,output);glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_POINTS);glDrawArrays(GL_POINTS,0,1);glEndTransformFeedback();glDisable(GL_RASTERIZER_DISCARD);
        float captured[6]{};
        const auto* bytes=glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER,0,sizeof(captured),GL_MAP_READ_BIT);
        if(bytes){std::copy_n(static_cast<const float*>(bytes),6,captured);glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);}
        const auto expected=field.At(1,1.4f,1.6f,0);
        const float wave=.008f*std::sin(1.7f+1.4f*1.1f)+.005f*std::sin(-2.3f+1.4f*2.8f);
        const auto position=field.origin+glm::vec3(1,expected.height+wave,1.4f);
        const auto normal=glm::normalize(glm::vec3(-expected.gradient.x,1,-expected.gradient.y));
        const float positionError=glm::length(position-glm::vec3(captured[0],captured[1],captured[2]));
        const float normalError=glm::length(normal-glm::vec3(captured[3],captured[4],captured[5]));
        const float analyticError=std::abs(expected.height-2.1f)+glm::length(expected.gradient-glm::vec2(.5f,0));
        result.passed=bytes&&positionError<2e-5f&&normalError<2e-5f&&analyticError<2e-5f&&captured[1]>2.09f;
        std::ostringstream details;details<<"production GPU height/normal vs CPU: "<<positionError<<" m / "<<normalError<<"; affine oracle error="<<analyticError<<"; old mesh y=1.6, current GPU y="<<captured[1];result.details=details.str();
    }else result.details="transform-feedback program did not link";
    glUseProgram(0);glBindVertexArray(0);glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,0,0);
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER,0);glBindBuffer(GL_ARRAY_BUFFER,0);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,0);glActiveTexture(GL_TEXTURE0);
    if(texture)glDeleteTextures(1,&texture);
    if(input)glDeleteBuffers(1,&input);
    if(output)glDeleteBuffers(1,&output);
    if(vao)glDeleteVertexArrays(1,&vao);
    if(program)glDeleteProgram(program);
    glDeleteShader(vertex);
    result.passed=result.passed&&glGetError()==GL_NO_ERROR;
    // Independent optical control: white opaque geometry gets red attenuation
    // with distance, including a camera inside the motor's excluded volume.
    JudasWater::SolidBox swimmer;swimmer.kind=JudasWater::SolidBox::Kind::Capsule;
    swimmer.center={0,.8f,.6f};swimmer.radius=.3f;swimmer.halfHeight=.6f;swimmer.half={.3f,.9f,.3f};
    water.SetSolids({swimmer});water.Step(1.f/60,error);
    const auto optical=JudasWater::BuildSurfaceField(water);
    const auto surface=renderer.CreateMesh(JudasWater::BuildPoolSurface(optical));
    renderer.SetSceneAppearance(false,1,{},1,{1,0,0,0},false,{.5f,.6f,.7f});
    bool restored=true;
    GLuint foreignSampler=0;glGenSamplers(1,&foreignSampler);
    glSamplerParameteri(foreignSampler,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glSamplerParameteri(foreignSampler,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glSamplerParameteri(foreignSampler,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
    auto pixel=[&](float distance,bool wet,float opacity=.82f,bool drawAfterWater=false){
        renderer.BeginFrame(256,192);
        renderer.SetCamera(glm::lookAt(swimmer.center,swimmer.center+glm::vec3(0,0,-1),{0,1,0}),
                           glm::perspective(glm::radians(70.f),256.f/192,.05f,30.f));
        renderer.SetLighting({0,1,0},{0,0,0},{1,1,1});
        renderer.SetMaterialBindings({});
        auto box=[&]{renderer.DrawBox(swimmer.center+glm::vec3(0,0,-distance),{1,0,0,0},{.8f,.8f,.02f},{1,1,1});};
        if(!drawAfterWater)box();
        if(wet){
            glDepthMask(GL_FALSE);glEnable(GL_BLEND);glDisable(GL_CULL_FACE);glActiveTexture(GL_TEXTURE7);
            for(unsigned unit=0;unit<4;++unit)glBindSampler(unit,foreignSampler);
            BasicWaterSettings style;style.opacity=opacity;
            renderer.DrawGridWater(surface,optical,style,0,1);
            GLboolean mask=GL_TRUE;GLint active=0;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
            restored=restored&&!mask&&glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_CULL_FACE)&&active==GL_TEXTURE7;
            for(unsigned unit=0;unit<4;++unit){GLint sampler=0;glGetIntegeri_v(GL_SAMPLER_BINDING,unit,&sampler);restored=restored&&GLuint(sampler)==foreignSampler;glBindSampler(unit,0);}
            glDepthMask(GL_TRUE);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);glActiveTexture(GL_TEXTURE0);
        }
        if(drawAfterWater)box();
        renderer.EndFrame();std::vector<unsigned char> pixels;renderer.CaptureFrame(256,192,pixels);
        const auto index=(96*256+128)*3;
        return glm::vec3(pixels[index],pixels[index+1],pixels[index+2]);
    };
    const auto dry=pixel(.6f,false),nearPixel=pixel(.6f,true),farPixel=pixel(2.f,true),clear=pixel(2.f,true,0);
    const auto late=pixel(2.f,true,.82f,true);
    const bool optics=dry.r>245&&nearPixel.r<dry.r-10&&farPixel.r<nearPixel.r-20&&farPixel.b>farPixel.r+15&&clear.r>farPixel.r+20
        &&glm::length(late-farPixel)<2;
    // A liquid's finite-domain closing face may coincide with an authored pool
    // wall. The opaque wall must win, exactly as if that hidden water face were
    // absent; otherwise depth ties produce striped reflection/refraction.
    const auto closedMesh=JudasWater::BuildPoolSurface(optical);
    MeshData topMesh;
    for(std::size_t i=0;i+2<closedMesh.vertices.size();i+=3)
        if(closedMesh.vertices[i].tangent.z<.5f)
            topMesh.vertices.insert(topMesh.vertices.end(),closedMesh.vertices.begin()+i,closedMesh.vertices.begin()+i+3);
    const auto topSurface=renderer.CreateMesh(topMesh);
    auto wallImage=[&](bool closed,bool wall,glm::vec3 eye){
        renderer.BeginFrame(256,192);
        const auto target=wall?glm::vec3(eye.x+1.6f,.8f,eye.z-.4f):glm::vec3(eye.x,.8f,eye.z< -1.6f?0.f:-2.5f);
        renderer.SetCamera(glm::lookAt(eye,target,{0,1,0}),
                           glm::perspective(glm::radians(70.f),256.f/192,.05f,30.f));
        renderer.SetLighting({0,1,0},{0,0,0},{1,1,1});renderer.SetMaterialBindings({});
        // Dyadic thickness preserves the exact float boundary; decimal .05/.1
        // can put the wall one ULP in front and accidentally hide a depth tie.
        if(wall)renderer.DrawBox({0,.8f,optical.origin.z-.125f},{1,0,0,0},{1.6f,.8f,.125f},{1,1,1});
        std::vector<float> opaque(256*192);
        GLint read=0,draw=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,draw);glReadPixels(0,0,256,192,GL_DEPTH_COMPONENT,GL_FLOAT,opaque.data());glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        BasicWaterSettings style;renderer.DrawGridWater(closed?surface:topSurface,optical,style,0,1);
        renderer.EndFrame();
        std::vector<unsigned char> pixels;renderer.CaptureFrame(256,192,pixels);return std::make_pair(std::move(pixels),std::move(opaque));
    };
    unsigned wallPixels=0,wallSamples=0;std::ostringstream wallDetails;
    for(float x:{-.45f,0.f,.45f}){
        const auto withSides=wallImage(true,true,{x-.8f,.8f,-1.3f}),withoutSides=wallImage(false,true,{x-.8f,.8f,-1.3f});
        const auto centre=(96*256+128)*3;
        wallDetails<<" ["<<x<<": "<<int(withSides.first[centre])<<','<<int(withSides.first[centre+1])<<','<<int(withSides.first[centre+2])
                   <<" vs "<<int(withoutSides.first[centre])<<','<<int(withoutSides.first[centre+1])<<','<<int(withoutSides.first[centre+2])<<']';
        if(x==0&&!captureDirectory.empty()){
            WriteRgbPng(captureDirectory+"/wall-with-sides.png",256,192,withSides.first);
            WriteRgbPng(captureDirectory+"/wall-reference.png",256,192,withoutSides.first);
        }
        for(int y=76;y<116;++y)for(int xx=88;xx<168;++xx){
            const auto i=(y*256+xx)*3;
            // At grazing angles the finite wall ends inside the view. Compare
            // actual opaque-wall pixels; an exposed liquid edge against sky is
            // correctly different and is tested independently below.
            if(withoutSides.second[(191-y)*256+xx]<.99999f){
                ++wallSamples;
                for(int c=0;c<3;++c)wallPixels+=std::abs(int(withSides.first[i+c])-int(withoutSides.first[i+c]))>2;
            }
        }
    }
    const auto freeSide=wallImage(true,false,{0,.8f,-2.3f}),noSide=wallImage(false,false,{0,.8f,-2.3f});
    const auto centre=(96*256+128)*3;
    unsigned freeDifference=0;for(int c=0;c<3;++c)freeDifference+=std::abs(int(freeSide.first[centre+c])-int(noSide.first[centre+c]));
    std::ostringstream detail;detail<<result.details<<"; white/dry="<<dry.r<<", near RGB="<<nearPixel.r<<','<<nearPixel.g<<','<<nearPixel.b
        <<", far RGB="<<farPixel.r<<','<<farPixel.g<<','<<farPixel.b<<"; clear-water red="<<clear.r<<"; later-object colour error="<<glm::length(late-farPixel)<<"; GL state restored="<<restored
        <<"; coplanar wall channel mismatches="<<wallPixels<<" in "<<wallSamples<<" opaque pixels; free-side difference="<<freeDifference<<wallDetails.str();
    result.details=detail.str();result.passed=result.passed&&optics&&restored&&wallPixels==0&&wallSamples>1000&&freeDifference>20&&glGetError()==GL_NO_ERROR;
    glDeleteSamplers(1,&foreignSampler);
    renderer.DestroyMesh(surface);
    renderer.DestroyMesh(topSurface);
    return result;
}
