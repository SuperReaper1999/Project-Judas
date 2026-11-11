#include "StartupScreen.h"
#include "EngineHost.h"
#include "EnginePaths.h"
#include <algorithm>
#include <cstdio>

StartupScreen::StartupScreen(EngineHost& host):m_host(host),m_begin(std::chrono::steady_clock::now()){
    std::string error;m_movieAvailable=m_movie.Open(ResolveEngineDataPath("assets/branding/judas-startup.jsplash"),error);
    if(!m_movieAvailable)std::fprintf(stderr,"%s; using text loading screen.\n",error.c_str());
    Present("Preparing game",true);
}
StartupScreen::~StartupScreen(){Finish();}
void StartupScreen::Finish(){
    if(m_finished)return;
    m_host.GetRenderer().DestroyTexture(m_texture);m_texture={};m_movie.Close();m_finished=true;
}
bool StartupScreen::Present(const std::string& stage,bool force,const std::function<void()>& beforeSwap){
    if(m_finished)return false;
    auto& window=m_host.GetWindow();window.PollEvents();
    if(window.ShouldClose())return false;
    const auto now=std::chrono::steady_clock::now();
    if(!force&&std::chrono::duration<double>(now-m_last).count()<1./30)return true;
    m_last=now;auto& renderer=m_host.GetRenderer();
    const double elapsed=std::chrono::duration<double>(now-m_begin).count();
    const auto index=m_movie.FrameIndex(elapsed);
    if(m_movieAvailable&&index!=m_frame){
        TextureData image;std::string error;
        if(m_movie.Frame(elapsed,image,error)){
            auto next=renderer.CreateTexture(image);
            if(next.IsValid()){renderer.DestroyTexture(m_texture);m_texture=next;m_frame=index;}
        }else{std::fprintf(stderr,"%s; using text loading screen.\n",error.c_str());m_movieAvailable=false;}
    }
    const int w=window.Width(),h=window.Height();
    renderer.BeginFrame(w,h);renderer.EndFrame();renderer.BeginUIFrame(w,h);
    renderer.DrawUIRect({0,0},{w,h},{0,0,0,1});
    const float side=std::max(1.f,std::min(float(w)*.85f,float(h)*.8f));
    if(m_texture.IsValid())renderer.DrawUIImage({(w-side)/2,(h-side)/2-20},{side,side},m_texture,{1,1,1,1},true);
    const float labelScale=std::min(1.f,std::max(.25f,float(h)/720.f));
    const auto title=renderer.MeasureUIText("JUDAS ENGINE",labelScale);
    renderer.DrawUIText("JUDAS ENGINE",{(w-title.x)/2,h*.88f},labelScale,{.1f,.95f,.9f,1});
    const auto size=renderer.MeasureUIText(stage,.7f*labelScale);
    renderer.DrawUIText(stage,{(w-size.x)/2,h*.94f},.7f*labelScale,{.8f,.8f,.85f,1});
    renderer.EndUIFrame();if(beforeSwap)beforeSwap();window.SwapBuffers();return true;
}
