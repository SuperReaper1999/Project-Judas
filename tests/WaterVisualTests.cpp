#include "EngineHost.h"
#include "InteractivePlay.h"
#include "PerformanceProfiler.h"
#include "Project.h"
#include "ProjectFeaturePolicy.h"
#include "RuntimeWorld.h"
#include "SceneSerialization.h"
#include "ScreenshotWriter.h"
#include "ScriptSystem.h"
#include "WorldPresentation.h"
#include "fluid/GameWaterRuntime.h"
#include "fluid/GameWaterSurface.h"
#include "WaterVisualGpuControls.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>

namespace {
int checks=0,failures=0;
void Check(bool result,const std::string& message){++checks;failures+=!result;std::cout<<(result?"PASS ":"FAIL ")<<message<<'\n';}
using Clock=std::chrono::steady_clock;
void Measure(std::vector<double> values,const std::string& label){
    std::sort(values.begin(),values.end());if(values.empty())return;
    std::cout<<"MEASURE "<<label<<" n="<<values.size()<<" median="<<values[values.size()/2]
             <<" p95="<<values[std::size_t(std::ceil(values.size()*.95))-1]<<" worst="<<values.back()<<" ms\n";
}
}
int main(int argc,char** argv){
    if(AdvancedFluidDynamicsRestriction()){std::cout<<"Water visuals require Experimental source preview\n";return 0;}
    const std::filesystem::path out=std::filesystem::absolute(argc>1?argv[1]:".cache/water-visual-check");
    const bool measured=argc>2&&std::string(argv[2])=="--measure";
    std::filesystem::create_directories(out);
    // Numerical controls are independent of screenshots or the new shaders.
    JudasWater::Settings config;config.cells={8,8,8};config.cellSize=.4f;config.origin={-1.6f,0,-1.6f};
    JudasWater::Grid grid;std::string error;Check(grid.Initialize(config,error),"initialize physical reference pool");
    grid.FillBox({-2,0,-2},{2,1.6f,2});
    const auto quantity=grid.Quantity();const auto field=JudasWater::BuildSurfaceField(grid);
    const auto a=field.At(.8f,1.2f,1.6f),b=field.At(.80001f,1.2f,1.6f);
    Check(std::abs(a.height-1.6f)<1e-5f&&glm::length(a.gradient)<1e-5f,"continuous reconstruction reproduces a level pool");
    Check(std::abs(a.height-b.height)<1e-5f,"no height seam at a render patch boundary");
    const auto fine=JudasWater::BuildPoolSurface(field,4),coarse=JudasWater::BuildPoolSurface(field,1);
    Check(fine.vertices.size()>coarse.vertices.size()*8,"visual refinement is independent of simulation resolution");
    Check(std::all_of(fine.vertices.begin(),fine.vertices.end(),[](const auto& v){return std::isfinite(v.position.y)&&std::isfinite(v.normal.x)&&std::isfinite(v.normal.y)&&(v.tangent.z>.5f||v.normal.y>.999f);}),"quiet pool has smooth shared upward normals and finite outline faces");
    Check(grid.Quantity()==quantity,"reconstruction leaves physical water quantity unchanged");
    JudasWater::SolidBox body;body.center={0,1,0};body.kind=JudasWater::SolidBox::Kind::Capsule;body.radius=.3f;body.halfHeight=.6f;body.half={.3f,.9f,.3f};
    grid.SetSolids({body});grid.Step(1.f/60,error);
    const auto afterMotion=grid.Quantity();
    const auto carved=JudasWater::BuildSurfaceField(grid);
    const auto near=carved.At(1.6f,1.6f,1.6f);
    Check(near.coverage>.9f&&near.height>1.5f,"moving-solid exclusion does not punch a render hole around a swimmer");
    Check(grid.Quantity()==afterMotion,"optical continuation does not manufacture physical water");
    const auto displacedSurface=JudasWater::BuildPoolSurface(carved);
    const float extent=config.cells.x*config.cellSize;
    Check(std::none_of(displacedSurface.vertices.begin(),displacedSurface.vertices.end(),[&](const auto& v){
        return v.tangent.z>.5f&&v.uv.x>.001f&&v.uv.y>.001f&&v.uv.x<extent-.001f&&v.uv.y<extent-.001f;
    }),"displacement waves do not create internal closing faces in a fully wet pool footprint");
    JudasWater::Settings broad=config;broad.cells={12,8,12};broad.origin={-2.4f,0,-2.4f};
    JudasWater::Grid held;held.Initialize(broad,error);held.FillBox({-3,0,-3},{3,1.6f,3});
    JudasWater::SolidBox parcel;parcel.center={0,.8f,-.6f};parcel.half={.85f,.8f,.65f};
    body.center={0,1,.6f};
    held.SetSolids({body,parcel});held.Step(1.f/60,error);
    const auto heldQuantity=held.Quantity();const auto heldField=JudasWater::BuildSurfaceField(held);
    const auto covered=heldField.intervals[heldField.Index(6,2,4)];
    Check(covered.z>0&&covered.y>1.5f,
          "multi-cell held-body interior continues the optical medium from connected wet witnesses");
    bool entry=true;
    for(int i=0;i<45;++i){
        body.center={0,2.3f-i*.04f,.6f};held.SetSolids({body,parcel});held.Step(1.f/60,error);
        const auto optical=JudasWater::BuildSurfaceField(held);
        const float eye=body.center.y+.65f; // Actual Workshop camera height.
        if(eye<1.58f){const auto interval=optical.intervals[optical.Index(6,int(eye/.4f),7)];entry=entry&&interval.z>0&&interval.y>=eye;}
    }
    Check(entry,"entering the water with an adjacent held object does not open an optical camera pocket");
    Check(std::abs(held.Quantity()-heldQuantity)<1e-9,"held-object optical reconstruction preserves physical quantity");
    JudasWater::Grid divided;divided.Initialize(config,error);
    divided.BakeStatic([](glm::vec3 p){return std::abs(p.x)<.06f;});
    divided.Fill([](glm::vec3 p){return p.x<0&&p.y<1.6f;});
    body.center={.2f,1,.2f};body.radius=.2f;body.half={.2f,.8f,.2f};
    divided.SetSolids({body});divided.Step(1.f/60,error);
    const auto drySide=JudasWater::BuildSurfaceField(divided);
    Check(drySide.intervals[drySide.Index(4,2,4)].z==0,
          "a dry swimmer behind a closed static wall does not acquire an optical water cavity");
    auto changed=field;
    for(auto& v:changed.intervals)if(v.z>0)v.y=1.7f;
    for(int z=0;z<8;++z)for(int x=0;x<8;++x)changed.intervals[changed.Index(x,4,z)]={0,1.7f,1,0};
    Check(changed.SameTopology(field),"changing level across a cell boundary reuses surface topology");
    // Two pools in one column must retain the dry air gap.
    JudasWater::Grid layers;layers.Initialize(config,error);
    layers.Fill([](glm::vec3 p){return p.y<.7f||(p.y>1.6f&&p.y<2.3f);});
    const auto layered=JudasWater::BuildSurfaceField(layers);
    const auto gap=layered.intervals[layered.Index(3,3,3)];
    Check(gap.z==0,"render intervals preserve an actual dry gap between separate layers");
    JudasWater::Grid exactLayers;exactLayers.Initialize(config,error);
    // Put analytic boundaries between seed sample planes, rather than exactly
    // on a float-rounded .7/2.3 quadrature point from the separate gap fixture.
    exactLayers.Fill([](glm::vec3 p){return p.y<.8f||(p.y>1.6f&&p.y<2.4f);});
    const auto layerMesh=JudasWater::BuildPoolSurface(JudasWater::BuildSurfaceField(exactLayers));
    bool lowerLayer=false,upperLayer=false,distinct=true;
    for(const auto& v:layerMesh.vertices)if(v.tangent.z<.5f){
        const bool low=std::abs(v.position.y-.8f)<1e-5f,high=std::abs(v.position.y-2.4f)<1e-5f;
        lowerLayer|=low;upperLayer|=high;distinct=distinct&&(low||high);
    }
    Check(lowerLayer&&upperLayer&&distinct,"separate water layers retain both known surface heights without bridging their air gap");

    const auto root=std::filesystem::path(__FILE__).parent_path().parent_path()/"projects/water_v2_workshop";
    Project project;if(!project.Load((root/"water_v2_workshop.judasproj").string(),error)){std::cerr<<error;return 1;}
    EngineHost host;if(!host.Init("Water visual qualification",1280,800,false,error)){std::cerr<<error;return 1;}
    host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
    const auto gpuControl=RunWaterVisualGpuControls(host.GetRenderer(),out.string());Check(gpuControl.passed,gpuControl.details);
    if(argc>2&&std::string(argv[2])=="--gpu-only"){
        std::cout<<checks<<" checks, "<<failures<<" failures\n";
        return failures?1:0;
    }
    auto& window=host.GetWindow();window.SetTestInputMode(true);window.Input().SetMap(project.Settings().input,error);
    auto& renderer=host.GetRenderer();auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);
    std::ofstream rows(out/"timings.csv");rows<<"scene,frame,simulation_ms,render_cpu_ms,rendered\n";
    for(const std::string name:{"pool","rotated","radial","planet-basin"}){
        auto owned=std::make_unique<RuntimeWorld>();auto& world=*owned;world.ConfigureProject(project.Settings());
        Scene scene;if(!LoadSceneFromFile((root/"Scenes"/(name+".judas")).string(),scene,error)||!world.Build(scene,&host.Resources(),error)){std::cerr<<error;return 1;}
        host.Resources().WaitForAll();InteractivePlay play;
        if(!play.Begin(world,WorldCoordinates(world.Settings().worldOrigin),error)){std::cerr<<error;return 1;}
        auto* water=world.GridWater();const auto owners=water->Owners();const auto id=owners[0];
        const auto& settings=water->Find(id)->Configuration();
        const auto origin=water->Find(id)->ToWorld(glm::vec3(settings.cells.x*.5f*settings.cellSize,0,settings.cells.z*.5f*settings.cellSize));
        const auto rotation=settings.rotation;const auto up=rotation*glm::vec3(0,1,0);
        const auto definition=world.RuntimeDefinition(id);const float level=definition->fluidVolume->fillHeight;
        auto worldPoint=[&](glm::vec3 p){return origin+rotation*p;};
        auto step=[&](){window.Input().BeginFrame();host.PumpResources();play.Frame(window,renderer,1.f/60,true,false);};
        for(int frame=0;frame<40;++frame)step();
        const auto before=water->Find(id)->Quantity();
        auto render=[&](glm::vec3 eye,glm::vec3 target,const std::string& label,bool save){
            profiler.BeginFrame("Water visual "+name+" "+label);
            RenderWorldFrame(renderer,1280,800,world,&play.Session(),glm::lookAt(worldPoint(eye),worldPoint(target),up),
                glm::perspective(glm::radians(70.f),1.6f,.05f,300.f),worldPoint(eye),1,1.f/60,WorldRenderPurpose::Observer);
            if(save){std::vector<unsigned char> pixels;renderer.CaptureFrame(1280,800,pixels);
                Check(WriteRgbPng((out/(name+"-"+label+".png")).string(),1280,800,pixels),"capture "+name+" "+label);}
            window.SwapBuffers();profiler.EndFrame();
        };
        render({1000,100,0},{2000,100,0},"offscreen",false);
        const auto hidden=profiler.History().back();
        Check(std::none_of(hidden.scopes.begin(),hidden.scopes.end(),[](const auto& scope){
            return std::string_view(scope.name).find("Grid water ")==0;
        }),name+" unseen water performs no visual field, surface, upload or optics work");
        render({-3,level+2,5.8f},{-1,level-.4f,0},"above",true);
        render({-3,level+.2f,3},{1,level-.05f,-2},"grazing",true);
        render({-3,level+.015f,2},{-1,level,-2},"waterline",true);
        render({-3,level-.55f,2},{1,level-.55f,-2},"under-horizontal",true);
        const float bank=-settings.cells.x*settings.cellSize*.5f;
        render({bank+.4f,level-.45f,0},{bank,level-.4f,-1},"wall-close",true);
        render({-3,level-.55f,2},{-2,level+1.5f,0},"under-up",true);
        render({-3,level-.55f,2},{-2,.2f,0},"under-down",true);
        render({-.8f,level+.45f,3.6f},{.1f,level-1.2f,2.6f},"shore",true);
        for(int i=0;i<7;++i)render({-3,level+.12f-i*.04f,2},{-1,level,-2},"cross-"+std::to_string(i),true);
        std::vector<double> simulation,submission;
        renderer.FinishForDiagnostics();profiler.BeginFrame("drain before measurement");
        renderer.PollProfileGPU();profiler.EndFrame();profiler.Clear();
        for(int frame=0;frame<180;++frame){
            if(frame==30)water->AddImpulse(id,worldPoint({-3,level-.1f,0}),rotation*glm::vec3(0,500,0),.6f);
            profiler.BeginFrame("Water measured "+name);
            auto start=Clock::now();step();
            simulation.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            const bool visible=measured||frame%30==0;
            double renderTime=0;
            if(visible){
                start=Clock::now();const auto eye=worldPoint({-3,level-.45f,2});
                RenderWorldFrame(renderer,1280,800,world,&play.Session(),glm::lookAt(eye,worldPoint({1,level-.35f,-2}),up),
                    glm::perspective(glm::radians(70.f),1.6f,.05f,300.f),eye,1,1.f/60,WorldRenderPurpose::Observer);
                window.SwapBuffers();renderTime=std::chrono::duration<double,std::milli>(Clock::now()-start).count();submission.push_back(renderTime);
            }
            rows<<name<<','<<frame<<','<<simulation.back()<<','<<renderTime<<','<<visible<<'\n';
            profiler.EndFrame();
        }
        Check(!water->Paused(id),name+" disturbance remains physically healthy");
        Check(std::abs(water->Find(id)->Quantity()-before)<1e-5,name+" presentation and impulses conserve physical quantity");
        render({-3,level+1,3},{-2,level-.5f,-1},"disturbed",true);
        const auto added=water->ChangeWater(id,worldPoint({-3,level+.4f,0}),2,4);
        for(int i=0;i<30;++i)step();
        render({-3,level+1,3},{-2,level-.5f,-1},"rising-level",true);
        Check(added>0&&std::abs(water->Find(id)->Quantity()-before-added)<1e-5,name+" changing levels follow real source receipts");
        Measure(simulation,name+" simulation");Measure(submission,name+(measured?" render-submit-and-present":" sampled render (quick gate; not a benchmark)"));
        renderer.FinishForDiagnostics();renderer.PollProfileGPU();
        Check(profiler.Export((out/(name+"-profile.json")).string(),error),"retain named CPU/GPU intervals");
        Check(glGetError()==GL_NO_ERROR,name+" water rendering has no GL errors");
        const auto* scripts=world.Scripts();Check(scripts&&scripts->Diagnostics().empty(),name+" normal game scripts remain healthy");
        play.End();owned.reset();
    }
    const auto reused=RunWaterVisualGpuControls(renderer);
    Check(reused.passed,"small-camera optics remain correct after larger views: "+reused.details);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
