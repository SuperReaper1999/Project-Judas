#include "editor/EditorPauseExample.h"
#include "editor/EditorScriptWorkflow.h"
#include "editor/EditorDocument.h"
#include "Project.h"
#include "EngineHost.h"
#include "InteractivePlay.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "SceneSerialization.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(){
    unsigned checks=0;
    auto check=[&](bool okay,const std::string& label){++checks;if(!okay)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';};
    try{
        namespace fs=std::filesystem;
        const auto root=fs::temp_directory_path()/("judas-pause-example-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::string error;Project project;check(Project::CreateNew(root.string(),"Pause example",project,error),"ordinary project created");
        EngineHost host;check(host.Init("Pause menu application",1280,720,false,error),"native graphics host");
        host.OpenProjectAssets(project.RootDir(),project.AssetsDir());host.Resources().SetBlockingMode(true);
        EditorDocument doc;
        auto& ground=doc.GetScene().CreateObject("Ground");ground.transform.position={0,-.5f,0};ground.body=SceneBodyComponent{};ground.body->halfExtents={30,.5f,30};
        auto& gravity=doc.GetScene().CreateObject("Gravity");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionRadius=100;
        const auto player=CreateEditorFirstPersonPlayer(doc,project,host.Assets(),{},0,error);
        check(player!=0,"ordinary script-owned player created: "+error);
        const auto menu=CreateEditorPauseExample(doc,project,host.Assets(),error);
        check(menu!=0,"optional pause UI and script created: "+error);
        const auto objects=doc.GetScene().Objects().size();
        check(!CreateEditorPauseExample(doc,project,host.Assets(),error)&&doc.GetScene().Objects().size()==objects,"duplicate pause example rejects without changing scene");
        auto& probe=doc.GetScene().CreateObject("Moving physics probe");const auto probeId=probe.id;probe.transform.position={4,8,0};probe.body=SceneBodyComponent{};probe.body->motion=SceneBodyMotion::Dynamic;
        project.Settings().startupScene="Scenes/main.judas";
        check(SaveSceneToFile(doc.GetScene(),project.StartupScenePath(),error)&&project.Save(error),"save the actual composed scene for export");
        RuntimeWorld world;world.ConfigureProject(project.Settings());check(world.Build(doc.GetScene(),&host.Resources(),error),"normal runtime builds authored player, UI and physical probe");host.Resources().WaitForAll();
        auto& window=host.GetWindow();window.SetTestInputMode(true);window.Input().SetMap(project.Settings().input,error);
        InteractivePlay play;check(play.Begin(world,WorldCoordinates{},error),"ordinary Play begins");
        auto frame=[&](const std::string& key="",float value=0){window.Input().BeginFrame();if(!key.empty())window.Input().SetPhysical(key,value);host.PumpResources();play.Frame(window,host.GetRenderer(),1.f/60,true,true);};
        for(int i=0;i<10;++i)frame();
        auto& ui=world.UI();const auto handle=ui.Find("pause_menu");
        check(handle&&!ui.Document(handle)->visible&&!play.IsPaused(),"example starts hidden without pausing gameplay");
        const auto before=world.Physics().GetTransform(world.RuntimeBody(probeId)).position;
        frame("key:Escape",1);frame("key:Escape",0);
        check(play.IsPaused()&&ui.Document(handle)->modal&&!window.IsMouseCaptured(),"Escape opens real modal pause and releases mouse");
        const auto paused=world.Physics().GetTransform(world.RuntimeBody(probeId)).position;
        for(int i=0;i<6;++i)frame();
        check(paused==world.Physics().GetTransform(world.RuntimeBody(probeId)).position&&paused.y<before.y+.01f,"physical world remains unchanged throughout pause");
        auto activate=[&](const char* id){check(ui.Focus(handle,id),std::string("focus ")+id);frame("key:Return",1);frame("key:Return",0);};
        activate("tab_settings");check(ui.Element(handle,"settings")->visible,"Settings tab reveals project control list");
        int row=0;for(const auto& entry:project.Settings().input.entries){if(entry.name=="move_y"){row+=1;break;}row+=int(std::max(size_t(1),entry.bindings.size()));}
        check(ui.Select(handle,"bindings",row-1)&&ui.Focus(handle,"bindings"),"select the actual project's positive move_y binding");
        frame("key:Down",1);frame("key:Down",0);activate("change");
        check(ui.Element(handle,"choices")->visible,"Change opens physical control choices");
        check(ui.Select(handle,"choices",9)&&ui.Focus(handle,"choices"),"navigate control choices");
        frame("key:Down",1);frame("key:Down",0);activate("apply");
        check(window.Input().Map().Find("move_y")->bindings[1].control=="key:K","real UI applies the selected physical binding to main gameplay input");
        const auto captured=world.Scripts()->Capture();
        RuntimeWorld restored;restored.ConfigureProject(project.Settings());check(restored.Build(doc.GetScene(),&host.Resources(),error),"fresh world builds independently for script-state restoration");
        check(restored.RestoreScriptState(captured,error,true),"restore saved menu/player JSON without an input callback");
        InputSystem freshInput;freshInput.SetMap(project.Settings().input,error);restored.UpdateUIScripts(&freshInput,1.f/60);freshInput.BeginFrame();
        check(freshInput.Map().Find("move_y")->bindings[1].control=="key:K"&&restored.Scripts()->Diagnostics().empty(),"restored controls publish to fresh input on the next real input boundary");
        restored.Destroy();
        frame("key:K",1);check(play.IsPaused(),"rebinding while paused does not resume simulation");frame("key:K",0);
        activate("reset");check(window.Input().Map().Serialize()==project.Settings().input.Serialize(),"Reset restores exact authored project controls");
        activate("tab_pause");activate("resume");check(!play.IsPaused()&&window.IsMouseCaptured(),"Resume restores gameplay and player mouse ownership");
        const auto resume=world.Physics().GetTransform(world.RuntimeBody(probeId)).position;for(int i=0;i<8;++i)frame();
        check(world.Physics().GetTransform(world.RuntimeBody(probeId)).position.y<resume.y-.01f,"actual physical probe continues after resume");
        check(world.Scripts()&&world.Scripts()->Diagnostics().empty(),"composed player/menu scripts remain fault-free");
        play.End();world.Destroy();host.Shutdown();std::cout<<"FIXTURE "<<root.string()<<'\n';
        std::cout<<"Pause example application: "<<checks<<" checks, 0 failures\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
