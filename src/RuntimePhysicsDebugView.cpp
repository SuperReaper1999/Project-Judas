#include "RuntimePhysicsDebugView.h"
#include "Window.h"
#include "Renderer.h"
#include "PerformanceProfiler.h"
#include "CameraProjection.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <map>
#include <tuple>
#include <glm/gtc/matrix_transform.hpp>
using Command=PhysicsEventDebugger::Command;
RuntimePhysicsDebugView::RuntimePhysicsDebugView(Window& window,Renderer& renderer):m_window(window),m_renderer(renderer){SDL_SetWindowTitle(window.NativeWindow(),"Judas Physics Event Debugger");}
bool RuntimePhysicsDebugView::PollPauseKey(){const bool value=SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_F9];const bool edge=value&&!m_pauseKey;m_pauseKey=value;return edge;}
Command RuntimePhysicsDebugView::Inspect(const PhysicsEventDebugger::Snapshot& snapshot){
    ProfileRecordingPause quietInspector;
    const bool captured=m_window.IsMouseCaptured();if(captured)m_window.SetMouseCaptured(false);
    // Do not route live device events into an in-flight world's input hub.
    // Coalesce deferred levels; publish them only at the next outer boundary.
    std::map<std::pair<unsigned,int>,std::pair<unsigned,SDL_Event>> deferred;unsigned order=0;
    const auto defer=[&](unsigned category,int control,SDL_Event event){if(deferred.size()<1024||deferred.count({category,control}))deferred[{category,control}]={++order,event};};
    const auto finish=[&](Command command){std::vector<std::pair<unsigned,SDL_Event>> events;for(auto& item:deferred)events.push_back(item.second);std::sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.first<b.first;});for(auto& e:events)SDL_PushEvent(&e.second);if(captured)m_window.SetMouseCaptured(true);return command;};
    for(;;){
        bool quit=false;
        if(snapshot.inFlight){SDL_Event event;while(SDL_PollEvent(&event)){
            if(event.type==SDL_QUIT){defer(SDL_QUIT,0,event);quit=true;}
            else if(event.type==SDL_WINDOWEVENT){defer(SDL_WINDOWEVENT,event.window.event,event);quit|=event.window.event==SDL_WINDOWEVENT_CLOSE;}
            else if(event.type==SDL_MOUSEMOTION){m_inspectDx+=event.motion.xrel;m_inspectDy+=event.motion.yrel;}
            else if(event.type==SDL_MOUSEBUTTONUP)defer(SDL_MOUSEBUTTONUP,event.button.button,event);
            else if(event.type==SDL_KEYDOWN||event.type==SDL_KEYUP){
                const auto key=event.key.keysym.scancode;const bool reserved=(key>=SDL_SCANCODE_F7&&key<=SDL_SCANCODE_F12)||key==SDL_SCANCODE_ESCAPE||key==SDL_SCANCODE_C||key==SDL_SCANCODE_L||key==SDL_SCANCODE_EQUALS||key==SDL_SCANCODE_MINUS;
                if(!event.key.repeat&&(!reserved||event.type==SDL_KEYUP))defer(SDL_KEYDOWN,int(key),event);
            }else if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED)defer(event.type,event.cdevice.which,event);
        }}
        if(quit||m_window.ShouldClose())return finish(Command::Fixed);
        const auto command=Paint(snapshot);
        if(command!=Command::Hold||!snapshot.inFlight)return finish(command);
        m_window.SwapBuffers();SDL_Delay(16);
    }
}

Command RuntimePhysicsDebugView::Paint(const PhysicsEventDebugger::Snapshot& snapshot){
    const auto* keys=SDL_GetKeyboardState(nullptr);
    const auto pressed=[&](SDL_Scancode key){return keys[key]&&!m_keys[unsigned(key)];};
    int drawableWidth=0,drawableHeight=0;SDL_GL_GetDrawableSize(m_window.NativeWindow(),&drawableWidth,&drawableHeight);
    const unsigned width=std::max(320,drawableWidth),height=std::max(240,drawableHeight);
    int mouseX=0,mouseY=0;const auto buttons=SDL_GetMouseState(&mouseX,&mouseY);const bool click=(buttons&SDL_BUTTON_LMASK)&&!m_mouse;
    const int rootDx=mouseX-m_previousMouseX,rootDy=mouseY-m_previousMouseY;
    m_previousMouseX=mouseX;m_previousMouseY=mouseY;
    int logicalWidth=1,logicalHeight=1;SDL_GetWindowSize(m_window.NativeWindow(),&logicalWidth,&logicalHeight);mouseX=int(double(mouseX)*width/std::max(1,logicalWidth));mouseY=int(double(mouseY)*height/std::max(1,logicalHeight));
    if((buttons&SDL_BUTTON_RMASK)&&m_rightMouse){const int dx=snapshot.inFlight?m_inspectDx:rootDx,dy=snapshot.inFlight?m_inspectDy:rootDy;m_yaw+=dx*.004f;m_pitch=std::clamp(m_pitch+dy*.004f,-1.2f,1.2f);}
    m_rightMouse=bool(buttons&SDL_BUTTON_RMASK);
    m_inspectDx=m_inspectDy=0;
    if(keys[SDL_SCANCODE_EQUALS])m_distance=std::max(2.f,m_distance*.98f);
    if(keys[SDL_SCANCODE_MINUS])m_distance=std::min(300.f,m_distance*1.02f);
    glm::vec3 target(0);const auto& p=snapshot.boundary;
    const auto focus=std::find_if(snapshot.bodies.begin(),snapshot.bodies.end(),[&](const auto& b){return b.dynamic&&(b.body.id==p.a||b.body.id==p.b);});
    const auto first=std::find_if(snapshot.bodies.begin(),snapshot.bodies.end(),[](const auto& b){return b.dynamic;});
    if(focus!=snapshot.bodies.end())m_focus=focus->body.id;
    const auto retained=std::find_if(snapshot.bodies.begin(),snapshot.bodies.end(),[&](const auto& b){return b.body.id==m_focus;});
    if(retained!=snapshot.bodies.end())target=retained->position;else if(first!=snapshot.bodies.end())target=first->position;
    const glm::vec3 eye=target+m_distance*glm::vec3(std::cos(m_pitch)*std::sin(m_yaw),std::sin(m_pitch),std::cos(m_pitch)*std::cos(m_yaw));
    m_renderer.BeginFrame(width,height);m_renderer.SetCamera(glm::lookAt(eye,target,glm::vec3(0,1,0)),glm::perspective(glm::radians(65.f),float(width)/height,.05f,2000.f));
    DebugLineList lines;
    for(const auto& b:snapshot.bodies){if(glm::length(b.position-target)>150||!std::isfinite(b.halfExtents.x))continue;const auto color=b.body.id==p.a||b.body.id==p.b?glm::vec3(1,.6,.1):b.dynamic?glm::vec3(.3,.8,1):glm::vec3(.3,.4,.4);lines.Box(b.position,b.rotation,b.halfExtents,color);}
    for(const auto& contact:p.manifold)lines.Arrow(contact.point,contact.point+contact.normal*.7f,{1,.2,.3});
    for(const auto& material:snapshot.materials)for(auto v:material.positions){const auto x=glm::vec3(v);lines.Line(x-glm::vec3(.025,0,0),x+glm::vec3(.025,0,0),{.4,1,.3});}
    m_renderer.DrawDebugLines(lines.Lines(),false);m_renderer.EndFrame();
    m_renderer.BeginUIFrame(width,height);m_renderer.DrawUIRect({8,8},{float(width)-16,232},{.02f,.025f,.04f,.94f});
    std::ostringstream text;text<<std::fixed<<std::setprecision(4);
    text<<"PHYSICS EVENTS | "<<PhysicsEventDebugger::PhaseName(p.phase)<<" | "<<(snapshot.inFlight?"LIVE SOLVER STACK PAUSED":"FIXED BOUNDARY")<<"\n"
        <<"Fixed "<<snapshot.fixedSteps<<" | accepted response "<<p.event<<" | time group "<<p.timeGroup<<" | macro t "<<p.macroTime<<" s\n"
        <<"Bodies "<<p.a<<":"<<p.partA<<" / "<<p.b<<":"<<p.partB<<" | revisions "<<p.revisionA<<" / "<<p.revisionB<<"\n"
        <<"Candidates "<<p.candidates<<" | response rows "<<p.contacts<<" | participants "<<p.participants<<" | queued "<<p.queuedPairs<<"\n"
        <<"CCD queries "<<p.stats.impactQueries<<" | sampling "<<p.stats.impactSamplingTests<<" | geometry "<<p.geometryRequests<<" (hits "<<p.geometryHits<<")\n"
        <<"Interval completed CPU: search "<<p.searchMs<<" ms | contact "<<p.contactSolveMs<<" ms | active gap "<<snapshot.activeMilliseconds<<" ms\n";
    std::size_t nodes=0,hinges=0,broken=0,released=0;double work=0;for(const auto& m:snapshot.materials){nodes+=m.nodes;hinges+=m.hinges;broken+=m.broken;released+=m.released;work+=m.plasticWork;}
    text<<"Materials "<<snapshot.materials.size()<<" | nodes "<<nodes<<" | hinges "<<hinges<<" | broken "<<broken<<" | released "<<released<<" | work "<<work<<"\n"
        <<"Trace omitted "<<snapshot.omittedRecords<<" | "<<m_status<<"\n";
    m_renderer.DrawUIText(text.str(),{18,14},.45f,{.9f,.95f,1,1});
    const char* labels[]={"F10 Fixed","F11 Event","F12 Phase","F7 Arm/play","F8 Live","F9 Pause","C Save","L Restore","Esc Detach"};
    const Command commands[]={Command::Fixed,Command::Event,Command::Phase,Command::Arm,Command::Realtime,Command::Pause,Command::Checkpoint,Command::Restore,Command::Detach};
    const SDL_Scancode codes[]={SDL_SCANCODE_F10,SDL_SCANCODE_F11,SDL_SCANCODE_F12,SDL_SCANCODE_F7,SDL_SCANCODE_F8,SDL_SCANCODE_F9,SDL_SCANCODE_C,SDL_SCANCODE_L,SDL_SCANCODE_ESCAPE};
    Command selected=Command::Hold;
    for(unsigned i=0;i<9;++i){float x=12+float(i%5)*float(width-24)/5,y=float(height)-100+float(i/5)*44,w=float(width-34)/5;
        const bool disabled=(snapshot.inFlight&&(i==5||i==6||i==7))||(!snapshot.inFlight&&m_busy&&i!=5&&i!=8);
        m_renderer.DrawUIRect({x,y},{w,35},disabled?glm::vec4(.12,.12,.12,1):glm::vec4(.12,.2,.3,1));m_renderer.DrawUIText(labels[i],{x+5,y+8},.43f,disabled?glm::vec4(.4,.4,.4,1):glm::vec4(1));
        if(!disabled&&(pressed(codes[i])||(click&&mouseX>=x&&mouseX<x+w&&mouseY>=y&&mouseY<y+35)))selected=commands[i];
    }
    m_renderer.DrawUIText("RMB orbit | +/- zoom | Enclosures are diagnostic bounds; red arrows are real manifold normals.",{15,251},.4f,{1,.9f,.5f,1});
    m_renderer.EndUIFrame();std::copy(keys,keys+m_keys.size(),m_keys.begin());m_mouse=(buttons&SDL_BUTTON_LMASK)!=0;
    return selected;
}
