#include "Application.h"
#include "EngineHost.h"
#include "RuntimeWorld.h"
#include "InteractivePlay.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "LocalMultiplayer.h"
#include "ScreenshotWriter.h"
#include <SDL2/SDL.h>
#include <filesystem>
#include <cstdio>
int checks=0,failures=0;
void Check(bool ok,const char* name){++checks;failures+=!ok;printf("%s %s\n",ok?"PASS":"FAIL",name);}
void Click(RuntimeWorld& w,uint32_t h,const std::string& id,int width,int height){auto& ui=w.UI();ui.Layout(width,height);auto* layout=ui.LayoutOf(h,id);if(!layout){Check(false,"click target exists");return;}auto point=layout->rect.position+layout->rect.size*.5f;InputSystem input;input.SetPhysical("mouse:Left",1);ui.Input(input,point,true,width,height);input.BeginFrame();input.SetPhysical("mouse:Left",0);ui.Input(input,point,true,width,height);w.DispatchUIEvents(&input,.016f);}
int main(int argc,char** argv){auto out=std::filesystem::absolute(argc>1?argv[1]:".cache/m96/application");std::filesystem::create_directories(out);ApplicationControl c;c.hidden=true;c.frameSeconds=[](float){return 1.f/60;};c.hostReady=[](EngineHost& h){std::string e;h.Audio().Init(e,true);};int frame=0,phase=0;bool done=false;RuntimeWorld* savedWorld=nullptr;std::string savedName;uint64_t savedRequest=0;
 c.afterFrame=[&](EngineHost& host,RuntimeWorld& w,InteractivePlay& play){++frame;auto h=w.UI().Find("showcase");int width=host.GetWindow().Width(),height=host.GetWindow().Height();
 if(w.Scripts()&&!w.Scripts()->Diagnostics().empty()){for(auto& d:w.Scripts()->Diagnostics())printf("FAULT %s\n",d.message.c_str());Check(false,"showcase callbacks fault-free");phase=99;}
 auto click=[&](const std::string& id){Click(w,h,id,width,height);};
 auto screenshot=[&](const std::string& name){std::vector<unsigned char> pixels;host.GetRenderer().CaptureFrame(width,height,pixels);Check(WriteRgbPng((out/(name+".png")).string(),width,height,pixels),"actual showcase framebuffer captured");};
 if(h&&w.UI().Focused(h)=="nav_menu"&&phase==0){Check(play.IsPaused(),"authored main interface owns pause");screenshot("welcome");click("nav_inventory");Check(w.UI().Element(h,"inventory")->visible,"ordinary JS navigates menu to inventory");phase=1;}
 else if(phase==1){Check(w.UI().Stats().listRows<12&&w.UI().Stats().listRows>0,"3000-row game list draws bounded visible interval");click("add");Check(w.UI().Element(h,"status")->text.find("3012")!=std::string::npos,"dynamic content appended through project JS");w.UI().Focus(h,"filter");phase=2;}
 else if(phase==2){SDL_Event text{};text.type=SDL_TEXTINPUT;SDL_strlcpy(text.text.text,"Battery",sizeof(text.text.text));SDL_PushEvent(&text);phase=3;}
 else if(phase==3){Check(w.UI().Element(h,"filter")->text=="Battery"&&w.UI().Element(h,"status")->text.find("377 supplies")!=std::string::npos,"native SDL text commit invokes real JS inventory filtering");screenshot("inventory");click("nav_dialogue");phase=4;}
 else if(phase==4){InputSystem input;w.UI().Focus(h,"choice");input.SetPhysical("key:Down",1);w.UI().Input(input,{-1,-1},false,width,height);w.DispatchUIEvents(&input,.016f);Check(w.UI().Element(h,"story")->text.find("YOU /")!=std::string::npos,"list selection drives real dialogue policy");click("nav_entry");phase=5;}
 else if(phase==5){SDL_Event text{};text.type=SDL_TEXTINPUT;SDL_strlcpy(text.text.text,"Conner / café",sizeof(text.text.text));SDL_PushEvent(&text);phase=6;}
 else if(phase==6){savedName=w.UI().Element(h,"name")->text;Check(savedName=="Conner / café","native text edits actual showcase field");screenshot("entry");click("save");savedWorld=&w;phase=7;}
 else if(phase==7){auto* service=w.SceneControl()->Saves(host.Resources());if(service&&!service->Busy()){bool saved=false;for(auto& slot:service->List())saved|=slot.id=="field-notes"&&slot.status=="compatible";if(saved){Check(true,"JS captures bounded UI state and normal SaveService commits");w.UI().Element(h,"name")->text="changed after save";click("load");phase=8;}}}
 else if(phase==8&& &w!=savedWorld){Check(w.UI().Element(h,"name")->text==savedName,"fresh world restores actual text from ordinary saved script state");Check(w.UI().Element(h,"filter")->text=="Battery","filter persists and rebuilds project list data");click("nav_local");click("start_local");phase=9;}
 else if(phase==9){auto* players=w.PlayersIfLoaded();Check(players&&players->All().size()==2,"ordinary JS creates two independent local UI owners");if(players){auto first=players->All().begin();auto second=std::next(first);auto a=w.UI().Find("desk-"+std::to_string(first->first)),b=w.UI().Find("desk-"+std::to_string(second->first));Check(a&&b&&w.UI().Player(a)==first->first&&w.UI().Player(b)==second->first,"HUD documents bind to distinct actual player identities");Check(w.UI().Focused(a)=="items"&&w.UI().Focused(b)=="items","both local interfaces retain their own focus");w.UI().Select(a,"items",12);w.UI().Select(b,"items",4);Check(w.UI().Selected(a,"items")==12&&w.UI().Selected(b,"items")==4,"local selected rows are independent");screenshot("local");}
 SDL_Event key{};key.type=SDL_KEYDOWN;key.key.keysym.scancode=SDL_SCANCODE_F1;key.key.keysym.sym=SDLK_F1;SDL_PushEvent(&key);phase=10;}
 else if(phase==10){Check(w.UI().Document(h)->visible&&w.PlayersIfLoaded()->All().empty(),"leaving local mode tears down owner documents and restores shared screen");Check(w.Scripts()->Diagnostics().empty(),"whole game sequence has no JS errors");SDL_SetWindowSize(host.GetWindow().NativeWindow(),720,1280);phase=11;}
 else if(phase==11&&width==720&&height==1280){Check(w.UI().Document(h)->reference==glm::vec2(720,1280),"portrait viewport drives public referenceSize and JS layout reflow");click("nav_inventory");phase=12;}
 else if(phase==12){w.UI().Layout(width,height);auto rect=w.UI().LayoutOf(h,"inventory")->rect;Check(rect.position.x>=0&&rect.position.x+rect.size.x<=width+.1f&&rect.position.y+rect.size.y<=height+.1f,"portrait inventory geometry stays inside live viewport");screenshot("portrait");click("nav_settings");phase=13;}
 else if(phase==13){auto& ui=w.UI();ui.Layout(width,height);auto rect=ui.LayoutOf(h,"scale")->rect;auto point=rect.position+glm::vec2(rect.size.x-1,rect.size.y*.5f);InputSystem input;input.SetPhysical("mouse:Left",1);ui.Input(input,point,true,width,height);input.BeginFrame();input.SetPhysical("mouse:Left",0);ui.Input(input,point,true,width,height);w.DispatchUIEvents(&input,.016f);Check(ui.Scale(h)>1.45f,"actual settings slider drives public readable scale and JS reflow");click("contrast");Check(ui.Element(h,"name")->focusColor==glm::vec4(1),"contrast preference configures actual generic focus styling");phase=14;}
 else if(phase==14){screenshot("settings-portrait");Check(w.Scripts()->Diagnostics().empty(),"responsive scaling and control configuration remain fault-free");done=true;phase=99;}
 if(phase==7||phase==8)SDL_Delay(1);
 if(frame>1200){Check(false,"bounded end-to-end workflow completes");phase=99;}
 if(phase==99){SDL_Event q{};q.type=SDL_QUIT;SDL_PushEvent(&q);}
 (void)savedRequest;
 };
 char name[]="judas",project[]="projects/ui_showcase/ui_showcase.judasproj";char* args[]={name,project};Application app;Check(app.Run(2,args,&c)==0&&done,"normal application startup, UI/save/player lifecycle and shutdown");printf("SUMMARY %d checks %d failures\n",checks,failures);return failures?1:0;
}
