#include "EngineHost.h"
#include "Renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {
unsigned checks=0,failures=0;
void Check(bool okay,const char *name) {
    ++checks;failures+=!okay;std::printf("%s %s\n",okay?"PASS":"FAIL",name);
}
std::array<int,3> Pixel(Renderer &renderer,int x=48,int y=32) {
    std::vector<unsigned char> rgb;renderer.CaptureFrame(96,64,rgb);
    const auto offset=size_t((y*96+x)*3);
    return {rgb[offset],rgb[offset+1],rgb[offset+2]};
}
bool Near(const std::array<int,3> &a,const std::array<int,3> &b,int tolerance=2) {
    for(int i=0;i<3;++i)if(std::abs(a[i]-b[i])>tolerance)return false;
    return true;
}
std::array<int,3> Expected(glm::vec3 scene,float exposure) {
    scene=glm::max(scene,glm::vec3(0))*exposure;
    scene/=1+glm::dot(scene,glm::vec3(.2126f,.7152f,.0722f));
    std::array<int,3> result;
    for(int i=0;i<3;++i) {
        const float c=glm::clamp(scene[i],0.f,1.f);
        result[i]=int(std::round(255*(c<.0031308f?c*12.92f:1.055f*std::pow(c,1.f/2.4f)-.055f)));
    }
    return result;
}
}
int main() {
    EngineHost host;std::string error;
    Check(host.Init("HDR UI preview controls",96,64,false,error),"native renderer initializes");
    if(failures){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    auto &r=host.GetRenderer();
    for(const auto colour : {glm::vec3(.08f,.2f,.03f),glm::vec3(4,.8f,.15f)})
        for(float exposure : {0.f,.8f,2.f}) {
            r.SetSceneAppearance(true,exposure,{},1,glm::quat(1,0,0,0),false,colour);
            r.BeginFrame(96,64);r.EndFrame();const auto main=Pixel(r);
            Check(Near(main,Expected(colour,exposure)),"main resolve matches independent exposure/luminance-Reinhard/sRGB oracle");
            auto linear=r.CreateRenderTarget(24,16,error);
            Check(linear.IsValid()&&r.BeginRenderTarget(linear),"create and render scene-linear target");
            r.EndRenderTarget();
            r.BeginUIFrame(96,64);
            r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(linear),{1,1,1,1},false,true);
            r.EndUIFrame();const auto ui=Pixel(r);
            std::printf("COLOUR exposure %.2f main %d %d %d preview %d %d %d\n",exposure,
                        main[0],main[1],main[2],ui[0],ui[1],ui[2]);
            Check(Near(ui,main),"UI linear preview has exactly one display conversion");
            // Observe the sampled texture itself: displaying it must not turn
            // the shared world-material source into clamped/display RGB.
            GLint sampled=0,previousRead=0;GLuint probe=0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D,&sampled);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previousRead);
            glGenFramebuffers(1,&probe);glBindFramebuffer(GL_READ_FRAMEBUFFER,probe);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,GLuint(sampled),0);
            float raw[4]={};glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,raw);
            bool unchanged=true;
            for(int k=0;k<3;++k)unchanged &= std::abs(raw[k]-colour[k])<.003f;
            Check(unchanged,"shared camera texture retains raw HDR values after UI display");
            glBindFramebuffer(GL_READ_FRAMEBUFFER,GLuint(previousRead));glDeleteFramebuffers(1,&probe);
            r.BeginUIFrame(96,64);
            r.DrawUIRect({0,0},{96,64},{0,0,0,1});
            r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(linear),{.8f,.8f,.8f,.5f},false);
            r.EndUIFrame();auto tinted=main;for(auto &c:tinted)c=int(std::round(c*.4f));
            Check(Near(Pixel(r),tinted),"preview tint and opacity apply after display conversion");
            r.BeginUIFrame(96,64);
            r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(linear),{1,1,1,1},false);
            r.DrawUIRect({0,0},{96,64},{.2f,.4f,.8f,1});r.EndUIFrame();
            Check(Near(Pixel(r),{51,102,204}),"ordinary panels after HDR preview retain display colours");
            TextureData data;data.width=1;data.height=1;data.pixels={51,102,204,255};
            auto image=r.CreateTexture(data);
            r.BeginUIFrame(96,64);
            r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(linear),{1,1,1,1},false);
            r.DrawUIImage({0,0},{96,64},image,{1,1,1,1},false);r.EndUIFrame();
            Check(Near(Pixel(r),{51,102,204}),"ordinary image after HDR preview is not tone mapped");
            r.DestroyTexture(image);
            auto display=r.CreateRenderTarget(24,16,error,true);
            Check(display.IsValid()&&r.BeginRenderTarget(display),"create explicit display target");r.EndRenderTarget();
            r.BeginUIFrame(96,64);r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(display),{1,1,1,1},false);r.EndUIFrame();
            Check(Near(Pixel(r),main),"already-resolved display target is not converted twice");
            Check(r.ResizeRenderTarget(linear,47,31,error)&&r.BeginRenderTarget(linear),"resize linear preview target");
            r.EndRenderTarget();r.BeginUIFrame(96,64);
            r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(linear),{1,1,1,1},true,true);r.EndUIFrame();
            Check(Near(Pixel(r),main),"resized fit-mode preview retains colour ownership");
            r.DestroyRenderTarget(display);r.DestroyRenderTarget(linear);
        }
    r.SetSceneAppearance(false,1,{},1,glm::quat(1,0,0,0),false,{.2f,.4f,.8f});
    auto legacy=r.CreateRenderTarget(24,16,error);Check(r.BeginRenderTarget(legacy),"legacy camera target begins");r.EndRenderTarget();
    r.BeginFrame(96,64);r.EndFrame();r.BeginUIFrame(96,64);
    r.DrawUIImage({0,0},{96,64},r.RenderTargetTexture(legacy),{1,1,1,1},false);r.EndUIFrame();
    Check(Near(Pixel(r),{51,102,204}),"legacy camera target remains display-ready");r.DestroyRenderTarget(legacy);
    r.SetSceneAppearance(true,.8f,{},1,glm::quat(1,0,0,0),false,{.1f,.3f,.2f});
    auto textPreview=r.CreateRenderTarget(24,16,error);r.BeginRenderTarget(textPreview);r.EndRenderTarget();
    const auto font=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/fonts/DejaVuSans.ttf";
    Check(r.LoadFont(font.string().c_str(),14,error),"load ordinary UI font");
    TextOptions options;options.pixels=14;auto layout=r.LayoutText("Label",options);
    auto textFrame=[&](bool preview) {
        r.BeginFrame(96,64);r.EndFrame();r.BeginUIFrame(96,64);r.DrawUIRect({0,0},{96,64},{0,0,0,1});
        if(preview)r.DrawUIImage({0,0},{32,64},r.RenderTargetTexture(textPreview),{1,1,1,1},false);
        r.DrawTextLayout(*layout,{40,20},{.8f,.6f,.4f,1});r.EndUIFrame();
        std::vector<unsigned char> image;r.CaptureFrame(96,64,image);return image;
    };
    auto ordinaryText=textFrame(false),afterPreview=textFrame(true);bool sameText=true,visibleText=false;
    for(int y=0;y<64;++y)for(int x=36;x<96;++x)for(int channel=0;channel<3;++channel) {
        const auto index=size_t((y*96+x)*3+channel);
        sameText &= ordinaryText[index]==afterPreview[index];visibleText |= ordinaryText[index]!=0;
    }
    Check(sameText&&visibleText,"glyph colours after HDR preview match ordinary text byte-for-byte");
    r.DestroyRenderTarget(textPreview);
    Check(glGetError()==GL_NO_ERROR,"preview and mixed UI operations have no GL errors");
    Check(r.RenderTargetDiagnostics().liveTargets==0,"all test camera targets are released");
    host.Shutdown();std::printf("%u checks, %u failures\n",checks,failures);return failures?1:0;
}
