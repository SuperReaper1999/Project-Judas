#include "EngineHost.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "Simulation.h"
#include "WorldGraphicsState.h"
#include "WorldPersistence.h"
#include "WorldPresentation.h"
#include "lighting/ShadowSystem.h"
#include "ScreenshotWriter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

namespace {
unsigned failures=0;
void Check(bool yes,const char* message) {
    std::cout<<(yes?"PASS ":"FAIL ")<<message<<'\n';
    failures+=!yes;
}
glm::vec3 Tip(const JudasHair::Guides& state,const JudasHair::Groom& groom) {
    return state.strands.front().points[groom.segments];
}
void Numerics() {
    using namespace JudasHair;
    auto groom=MakePreset(Preset::Hair);
    groom.stiffness=.1f;
    std::string error;
    Groom parsed;
    Check(Decode(Encode(groom),parsed,error)&&Encode(groom)==Encode(parsed),"groom fields round-trip exactly");
    Check(!Decode(R"({"version":1,"guides":-1})",parsed,error),"invalid guide count is rejected");
    Check(!Decode(R"({"version":1,"segments":32768})",parsed,error),"invalid segment count is rejected");
    Guides a,b,control;
    const Frame frame;
    Seed(a,groom,frame);Seed(b,groom,frame);Seed(control,groom,frame);
    const auto initial=Tip(a,groom);
    const float dt=1.f/60;
    for(unsigned i=0;i<45;++i) {
        Step(a,groom,frame,{9.8f,0,0},{0,0,15},dt);
        Step(b,groom,frame,{9.8f,0,0},{0,0,15},dt);
        Step(control,groom,frame,{9.8f,0,0},{0,0,0},dt);
    }
    const auto tip=Tip(a,groom);
    Check(tip==Tip(b,groom),"equal fixed inputs reproduce the same guide state");
    Check(tip.x>initial.x+.015f&&tip.z>Tip(control,groom).z+.005f,"sideways gravity and wind bend hair independently");
    bool lengths=true;
    for(const auto& guide:a.strands)
        for(unsigned j=1;j<=groom.segments;++j)
            lengths &= glm::length(guide.points[j]-guide.points[j-1])<=guide.length/groom.segments+1e-5f;
    Check(lengths,"all segments stay inside the stretch bound");
    const Frame jump{{25,1,-2}};
    Step(a,groom,jump,{0,-9.8f,0},{0,0,0},dt);
    Check(Tip(a,groom)==a.strands.front().previous[groom.segments]&&
          glm::length(Tip(a,groom)-jump.position)<1,"teleport has no inherited velocity or far tip");
    const auto worldTip=Tip(a,groom);
    Shift(a,{20,0,0});
    Check(glm::length(Tip(a,groom)+glm::vec3(20,0,0)-worldTip)<1e-4f,
          "origin shift leaves finite local state");

    groom.comb=0;
    groom.center=glm::vec3(0);
    groom.extent={.002f,0,.002f};
    groom.colliders={{{0,-.18f,0},{0,-.05f,0},.06f}};
    Guides collision;
    Seed(collision,groom,frame);
    unsigned sweeps=0;
    for(unsigned i=0;i<90;++i)
        Step(collision,groom,frame,{1,-28,0},{0,0,0},dt,[&](auto,auto&){++sweeps;});
    bool outside=true;
    for(const auto& guide:collision.strands)
        for(unsigned j=1;j<=groom.segments;++j) {
            const auto p=guide.points[j];
            const glm::vec3 spine{0,std::clamp(p.y,-.18f,-.05f),0};
            outside &= glm::length(p-spine) >= .045f;
        }
    Check(outside&&sweeps==90*groom.guides,"local capsules keep guide nodes out and world queries are bounded per tip");
    std::vector<MeshVertex> dense,sparse;
    const auto reduced=VisualStrands(groom,2,.5f);
    BuildVertices(collision,groom,frame,1,groom.strands,dense);
    BuildVertices(collision,groom,frame,1,reduced,sparse);
    const bool prefix=!sparse.empty()&&sparse.size()<dense.size()&&
        std::equal(sparse.begin(),sparse.end(),dense.begin(),[](const auto& a,const auto& b){return a.position==b.position;});
    Check(prefix&&VisualStrands(groom,groom.lodEnd+1,1)==0,"quality selects a stable strand prefix and respects the far bound");

    const auto coat=MakePreset(Preset::Coat);
    Guides surface;
    Seed(surface,coat,frame);
    std::array<bool,32> rootBins{};
    const float bandHeight=1.f-std::cos(glm::radians(coat.coverageDegrees));
    for(size_t i=0;i<64&&i<surface.fibres.size();++i) {
        const auto root=(surface.fibres[i].root-coat.center)/coat.extent;
        const unsigned band=unsigned(std::clamp((1.f-root.y)/bandHeight*4.f,0.f,3.f));
        const float azimuth=std::atan2(root.z,root.x)/6.2831853f+.5f;
        const unsigned sector=std::min(7u,unsigned(azimuth*8.f));
        rootBins[band*8+sector]=true;
    }
    Check(std::count(rootBins.begin(),rootBins.end(),true)>=24,
          "the low-detail coat covers the growth patch instead of tracing a winding line");
}
}

int main(int argc,char** argv) {
    Numerics();
    const auto out=std::filesystem::absolute(argc>1?argv[1]:".cache/hair-test");
    std::filesystem::create_directories(out);
    std::string error;
    Project project;
    Scene scene;
    EngineHost host;
    if(!project.Load("projects/hair_fur_lab/hair_fur_lab.judasproj",error)||
       !LoadSceneFromFile(project.StartupScenePath(),scene,error)||
       !host.Init("Hair and fur validation",480,320,false,error)) {
        std::cerr<<error<<'\n';return 1;
    }
    host.OpenProjectAssets(project.RootDir(),project.AssetsDir());
    host.Resources().SetBlockingMode(true);
    RuntimeWorld world;
    if(!world.Build(scene,&host.Resources(),error)) {std::cerr<<error<<'\n';return 1;}
    host.Resources().WaitForAll();
    world.UpdateAnimations(0);
    world.UpdateSockets();
    world.UpdateHair(1.f/60);
    const auto* mane=world.HairGuides(21);
    const auto* hair=world.HairGuides(31);
    if(!mane||!hair||mane->strands.empty()||hair->strands.empty())return 1;
    const auto before=mane->strands.front().points[0];
    const auto& settings=*scene.Find(21)->hairGroom;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<50;++i)StepPlayedWorld(world,host.GetWindow(),1.f/60);
    const auto fixedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/50;
    Check(glm::length(mane->strands.front().points[0]-before)>.01f&&
          glm::distance(mane->strands.front().points[0],world.PresentedTransform(21,scene.Find(21)->transform,1).position)<1,
          "the authored mane follows its animated neck socket under the real fixed step");
    auto draw=[&]{
        auto& r=host.GetRenderer();
        const auto view=glm::lookAt(glm::vec3(0,2.35f,7),glm::vec3(0,1,0),glm::vec3(0,1,0));
        auto projection=glm::perspective(glm::radians(52.f),1.5f,.075f,80.f);
        RenderWorldFrame(r,480,320,world,view,projection,{0,1,0},1);
        std::vector<unsigned char> pixels;
        r.CaptureFrame(480,320,pixels);
        return pixels;
    };
    auto image=draw();
    std::vector<JudasLighting::ShadowCaster> casters;
    world.AppendHairShadowCasters(casters);
    Check(casters.size()==6,"the body coat, head coat, mane, tail, scalp and beard register actual chart casters");
    const auto originalTip=Tip(*mane,settings);
    const auto old=host.GetRenderer().Quality();
    auto lower=old;lower.decorativeDensity=.3f;
    Check(host.GetRenderer().SetGraphicsQuality(lower,error),"lower visual strand density through existing quality policy");
    const auto sparse=draw();
    Check(sparse!=image&&Tip(*mane,settings)==originalTip,"visual density changes rendered pixels while guide state remains exact");
    Check(host.GetRenderer().SetGraphicsQuality(old,error),"restore authored density");
    Check(WriteRgbPng((out/"grooms.png").string(),480,320,draw()),"capture the authored groom workshop");
    WorldGraphicsState state;
    auto* script=world.Scripts();
    const auto previous=draw();
    if(host.GetWindow().UsesVulkan()) {
        const auto snapshot=Tip(*mane,settings);
        auto& assets=host.Resources();
        auto rebuild=[&](bool useVulkan) {
            if(!world.CaptureGraphicsState(state,error)||!world.ReleaseWorldGraphics(host.GetRenderer(),error)||
               !assets.DetachGraphics(error)||
               !host.RebuildDesktopGraphics("Hair and fur reconstruction",480,320,false,useVulkan,error))return false;
            assets.WaitForAll();
            bool ready=false;
            return assets.GraphicsRestorationReady(ready,error)&&ready&&
                   world.RestoreGraphicsState(state,host.GraphicsCaps(),error);
        };
        const bool switched=rebuild(false);
        Check(switched,"running world reconstructs groom graphics on the other backend");
        if(!switched) {std::cerr<<error<<'\n';return 1;}
        const auto rebuilt=draw();
        unsigned changed=0;
        for(size_t i=0;i<std::min(previous.size(),rebuilt.size());++i)changed+=std::abs(int(previous[i])-int(rebuilt[i]))>12;
        Check(!previous.empty()&&previous.size()==rebuilt.size()&&changed<previous.size()/20&&
              Tip(*mane,settings)==snapshot&&world.Scripts()==script,
              "recreated colour is within five percent of channels beyond 12 levels and guide state is exact");
        std::cout<<"backend channels with difference above 12: "<<changed<<'/'<<previous.size()<<'\n';
        const bool returned=rebuild(true);
        Check(returned&&draw()==previous&&Tip(*mane,settings)==snapshot,
              "returning to Vulkan reconstructs the same image without a simulation tick");
        if(!returned) {std::cerr<<error<<'\n';return 1;}
    }
    std::string authored;
    Scene reloaded;
    Check(SaveSceneToString(scene,authored)&&LoadSceneFromString(authored,reloaded,error)&&
          reloaded.Find(21)&&reloaded.Find(21)->hairGroom&&
          JudasHair::Encode(*reloaded.Find(21)->hairGroom)==JudasHair::Encode(settings),
          "author, save and reload preserve groom and bone reference");
    std::map<std::string,SaveChunk> snapshot;
    Scene restored;
    Check(WorldPersistence::Capture(world,snapshot,error)&&WorldPersistence::SceneFrom(snapshot,restored,error)&&
          restored.Find(21)&&restored.Find(21)->hairGroom&&restored.Find(21)->socket&&
          JudasHair::Encode(*restored.Find(21)->hairGroom)==JudasHair::Encode(settings),
          "save state records groom intent and socket rather than derived buffers");
    RuntimeWorld cold;
    Check(cold.Build(restored,&host.Resources(),error)&&cold.HairGuides(21)&&cold.HairGuides(21)->strands.empty(),
          "a cold load reconstructs the groom and starts without stale guide points");
    world.HideRegion({21});
    world.UpdateHair(1.f/60);
    draw();
    casters.clear();
    world.AppendHairShadowCasters(casters);
    Check(mane->strands.empty()&&casters.size()==5,"region suspension drops guide state and its shadow caster");
    world.PublishRegion({21});
    world.UpdateHair(1.f/60);
    draw();
    casters.clear();
    world.AppendHairShadowCasters(casters);
    Check(!mane->strands.empty()&&casters.size()==6,"republication restores a fresh groom without stale buffers");
    Check(world.DestroyEntity(21,&error)&&world.HairGuides(21)==nullptr,"destroying the attached object retires its groom");
    std::cout<<"50 whole fixed steps averaged "<<fixedMs<<" ms with four simulated grooms\n";
    std::cout<<"HairGroomTests failures "<<failures<<'\n';
    return failures?1:0;
}
