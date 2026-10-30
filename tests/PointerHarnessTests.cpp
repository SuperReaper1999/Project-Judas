#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "TestHarness.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
unsigned checks=0,failures=0;
void Check(bool ok,const char* text){++checks;failures+=!ok;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
}
int main(int argc,char** argv){
 const auto output=std::filesystem::absolute(argc>1?argv[1]:"build/pointer-harness");std::filesystem::create_directories(output);
 EngineHost host;std::string error;Check(host.Init("Pointer harness regression",320,240,false,error),"native window starts");if(failures)return 1;
 auto& window=host.GetWindow();window.SetTestInputMode(true);window.SetTestPointer(80,60);window.QueueTestPhysical("mouse:Left",1);window.PollEvents();auto p=window.Input().Pointer();Check(p.x==.25f&&p.y==.25f&&p.down,"queued primary press synchronizes gameplay pointer");window.QueueTestPhysical("mouse:Left",0);window.PollEvents();Check(!window.Input().Pointer().down&&window.Input().Action("ui_click").released,"queued release synchronizes pointer and logical edge");
 window.Input().Reset();window.BeginTestFrame();window.ApplyTestPhysical("mouse:Left",1);window.ApplyTestPhysical("mouse:Left",0);auto edge=window.Input().Action("ui_click");window.Input().BeginFixedStep();Check(edge.pressed&&edge.released&&!edge.held&&!window.Input().Pointer().down&&window.Input().FixedAction("ui_click").pressed,"short click retains edges without latching the held snapshot");
 window.SetTestPointer(80,60);window.QueueTestPhysical("mouse:Left",1);window.SetInputClaimed(false,true);window.PollEvents();Check(!window.Input().Pointer().down&&!window.Input().Action("ui_click").held,"editor/UI physical ownership clears injected held pointer");window.SetInputClaimed(false,false);window.Input().Reset();
 RuntimeWorld world;world.legacyGameplay=false;Scene scene;Check(world.Build(scene,&host.Resources(),error),"ordinary empty scripted world builds");InteractivePlay play;Check(play.Begin(world,WorldCoordinates{},error),"ordinary play session begins");if(failures){std::puts(error.c_str());return 1;}
 auto run=[&](const char* name,const std::string& contents,const std::function<void()>& after={}){auto path=output/name;std::ofstream(path)<<contents;window.Input().Reset();return RunTestHarness(window,host.GetRenderer(),play,path.string(),{},after);};
 unsigned observations=0;int result=run("pointer-touch.txt",R"TXT(FRAMES 8
LOG_EVERY 0
POINTER_DOWN 80 60 0
EXPECT_POINTER .25 .25 1 0
POINTER 160 120 1
EXPECT_POINTER .5 .5 1 1
POINTER_UP 160 120 2
EXPECT_POINTER .5 .5 0 2
POINTER_DOWN 80 60 3
POINTER_UP 80 60 3
EXPECT_POINTER .25 .25 0 3
TOUCH_DOWN 7 .2 .3 .8 4
EXPECT_TOUCHES 1 4
TOUCH_MOVE 7 .4 .5 .6 5
TOUCH_DOWN 8 .1 .2 .4 5
EXPECT_TOUCHES 2 5
TOUCH_UP 7 .4 .5 6
EXPECT_TOUCHES 1 6
TOUCH_UP 8 .1 .2 7
EXPECT_TOUCHES 0 7
)TXT",[&](){if(observations==3){auto a=window.Input().Action("ui_click");Check(a.pressed&&a.released&&!a.held,"actual harness preserves same-frame press/release");}if(observations==5){const auto& t=window.Input().Touches();Check(t.size()==2&&t[0].x==.4f&&t[0].y==.5f&&t[0].pressure==.6f,"actual harness retains independent touch motion/pressure");}++observations;});Check(result==0&&observations==8,"new directives traverse real play frames with public pointer/touch assertions");
 Check(run("bad-pressure.txt","FRAMES 1\nTOUCH_DOWN 1 .5 .5 2 0\n")!=0,"out-of-range pressure rejects");Check(run("bad-contact.txt","FRAMES 1\nTOUCH_MOVE 99 .5 .5 .5 0\n")!=0,"move cannot silently invent a missing contact");Check(run("bad-expectation.txt","FRAMES 1\nEXPECT_POINTER .5 .5 2 0\n")!=0,"invalid boolean pointer expectation rejects");Check(run("wrong-oracle.txt","FRAMES 1\nPOINTER_DOWN 80 60 0\nEXPECT_POINTER .75 .75 1 0\n")!=0,"negative control detects an incorrect public pointer position");
 unsigned resizeFrames=0;
 Check(run("resize.txt",R"TXT(FRAMES 4
LOG_EVERY 0
RESIZE 800 360 0
EXPECT_VIEWPORT 800 360 0
POINTER_DOWN 400 180 0
EXPECT_POINTER .5 .5 1 0
RESIZE 360 800 1
EXPECT_VIEWPORT 360 800 1
POINTER_UP 180 400 1
EXPECT_POINTER .5 .5 0 1
RESIZE 320 240 2
EXPECT_VIEWPORT 320 240 2
)TXT",[&](){++resizeFrames;})==0&&resizeFrames==4,"native landscape/portrait resize reaches gameplay viewport and pointer without polling devices");
 Check(run("bad-size.txt","FRAMES 1\nRESIZE 0 800 0\n")!=0,"zero resize rejects before execution");
 Check(run("huge-size.txt","FRAMES 1\nRESIZE 9000 800 0\n")!=0,"oversized resize rejects before allocation");
 Check(run("bad-resize-frame.txt","FRAMES 1\nRESIZE 360 800 1\n")!=0,"out-of-range resize frame rejects");
 Check(run("wrong-size.txt","FRAMES 1\nEXPECT_VIEWPORT 123 456 0\n")!=0,"negative control catches an incorrect native/gameplay viewport");
 // Logical window sizes alone are not evidence that GL storage was resized.
 // Cover both axes growing/shrinking, repeated calls and odd RGB row lengths.
 for(const auto size:{std::pair<int,int>{800,360},{360,800},{1200,540},{540,1200},{321,241},{321,241},{320,240}}){
  Check(window.Resize(size.first,size.second,error),"native presentation surface resizes");
  glDisable(GL_SCISSOR_TEST);glClearColor(.25f,.5f,.75f,1);glClear(GL_COLOR_BUFFER_BIT);
  glPixelStorei(GL_PACK_ALIGNMENT,8);std::vector<unsigned char> pixels;
  host.GetRenderer().CaptureFrame(size.first,size.second,pixels);GLint packing=0;glGetIntegerv(GL_PACK_ALIGNMENT,&packing);
  bool complete=pixels.size()==std::size_t(size.first)*std::size_t(size.second)*3u;
  std::size_t bad=0;
  for(std::size_t i=0;i<pixels.size();i+=3)if(pixels[i]<63||pixels[i]>65||pixels[i+1]<127||pixels[i+1]>129||pixels[i+2]<190||pixels[i+2]>192)++bad;
  std::printf("RESIZE_PIXELS size=%dx%d bad=%zu pack=%d\n",size.first,size.second,bad,int(packing));
  Check(complete&&bad==0&&packing==8,"entire resized drawable is captured and odd-width pack alignment is preserved");
  glPixelStorei(GL_PACK_ALIGNMENT,4);
 }
 play.End();std::printf("POINTER_HARNESS checks=%u failures=%u\n",checks,failures);return failures?1:0;
}
