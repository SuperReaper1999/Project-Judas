#include "editor/EditorScriptWorkflow.h"
#include "editor/EditorDocument.h"
#include "editor/EditorPanels.h"
#include "editor/EditorPrimitiveAssets.h"
#include "Project.h"
#include "RuntimeWorld.h"
#include "ScriptSystem.h"
#include "SceneSerialization.h"
#include "ProjectStarter.h"
#include "CollisionAsset.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
int main() {
    unsigned checks=0;
    auto check=[&](bool okay,const std::string& label){++checks;if(!okay)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';};
    try {
        const auto root=fs::temp_directory_path()/("judas-script-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Project project;std::string error;
        check(Project::CreateNew(root.string(),"Scripts",project,error),"create ordinary project");
        AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());
        EditorDocument doc;const auto owner=doc.GetScene().CreateObject("Script owner").id;doc.Select(owner);
        AssetRecord record;
        check(CreateAndAttachEditorScript(doc,owner,project,assets,"MyScript",record,error),"create, register and attach script in one workflow");
        check(record.type==AssetType::Script&&record.relativePath=="Assets/Scripts/MyScript.js"&&fs::is_regular_file(record.path+".judasmeta"),"ordinary project asset and metadata exist");
        check(doc.SelectedObject()->scripts.size()==1&&doc.SelectedObject()->scripts[0].asset==record.id,"selected object holds the actual script reference");
        check(ScriptSystem::CheckProjectSources(assets).empty(),"new script compiles and links through public JudasJS API");
        doc.Undo();check(doc.SelectedObject()->scripts.empty()&&fs::exists(record.path),"Undo detaches without deleting authored source");
        doc.Redo();check(doc.SelectedObject()->scripts.size()==1,"Redo restores attachment");
        check(!CreateAndAttachEditorScript(doc,owner,project,assets,"MyScript.js",record,error),"existing file is never overwritten");
        check(!CreateAndAttachEditorScript(doc,owner,project,assets,"../escape.js",record,error)&&!fs::exists(root/"escape.js"),"path traversal is rejected before writing");
        check(!CreateAndAttachEditorScript(doc,owner,project,assets,"bad.txt",record,error),"wrong source extension rejects");
        check(!AttachEditorScript(doc,owner,assets,doc.SelectedObject()->scripts[0].asset,error),"duplicate attachment is rejected");
        check(!AttachEditorScript(doc,999999,assets,record.id,error),"missing owner is rejected");
        check(!AttachEditorScript(doc,owner,assets,"missing",error),"missing asset is rejected");
        const auto second=doc.GetScene().CreateObject("Second owner").id;
        check(AttachEditorScript(doc,second,assets,doc.GetScene().Find(owner)->scripts[0].asset,error),"existing script attaches to another object without copying source");
        project.Settings().startupScene="Scenes/main.judas";
        check(SaveSceneToFile(doc.GetScene(),project.StartupScenePath(),error),"scene attachment saves normally");
        Scene scene;check(LoadSceneFromFile(project.StartupScenePath(),scene,error)&&scene.Find(owner)->scripts[0].asset==scene.Find(second)->scripts[0].asset,"reload preserves actual shared asset references");
        ResourceManager resources(nullptr,&assets);RuntimeWorld world;world.legacyGameplay=false;
        check(world.Build(scene,&resources,error),"ordinary runtime builds both attached scripts");
        InputSystem input;world.UpdateScripts(&input,.25f);
        check(world.Scripts()&&world.Scripts()->Diagnostics().empty()&&world.Scripts()->Capture().size()==2,"new scripts execute independently without VM faults");
        world.Destroy();
        EditorDocument playerDoc;
        check(PopulateVisibleProjectStarter(project,playerDoc.GetScene(),error),"ordinary unedited starter is present before player setup");
        assets.Scan(project.RootDir(),project.AssetsDir());
        const auto starterId=playerDoc.GetScene().Objects().front().id;
        auto& floor=playerDoc.GetScene().CreateObject("Ground");floor.transform.position={0,-.5f,0};floor.body=SceneBodyComponent{};floor.body->halfExtents={20,.5f,20};
        auto& gravity=playerDoc.GetScene().CreateObject("Gravity");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionRadius=100;
        const auto player=CreateEditorFirstPersonPlayer(playerDoc,project,assets,{0,0,0},0,error);
        check(player&&playerDoc.Selected()==player&&playerDoc.SelectedObject()->characterMotor&&playerDoc.SelectedObject()->scripts.size()==1,"player setup authors selected motor plus ordinary project script: "+error);
        check(!playerDoc.GetScene().Find(starterId)->scripts[0].enabled,"only exact unedited starter camera is superseded");
        playerDoc.Undo();check(!playerDoc.GetScene().Find(player)&&playerDoc.GetScene().Find(starterId)->scripts[0].enabled,"player setup and starter handoff undo together");
        playerDoc.Redo();
        assets.Scan(project.RootDir(),project.AssetsDir());
        check(ScriptSystem::CheckProjectSources(assets).empty(),"generated player uses documented public APIs");
        check(world.Build(playerDoc.GetScene(),&resources,error),"generated player builds through normal runtime");
        input.SetMap(project.Settings().input,error);
        for(int i=0;i<120;++i){input.BeginFrame();input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.Physics().Step(1.f/60);world.UpdateCharacters(1.f/60);world.UpdateScripts(&input,1.f/60);}
        auto* motor=world.RuntimeCharacter(player);check(motor&&motor->result.supported,"real player motor settles on authored ground");
        const auto start=motor->position;input.SetPhysical("key:W",1);
        for(int i=0;i<60;++i){input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.Physics().Step(1.f/60);world.UpdateCharacters(1.f/60);world.UpdateScripts(&input,1.f/60);input.BeginFrame();}
        check(motor->position.z<start.z-2&&std::abs(motor->position.y-start.y)<.05f,"logical WASD drives actual collision-aware motor displacement");
        input.SetPhysical("key:W",0);input.SetPhysical("key:Space",1);input.BeginFixedStep();world.FixedScripts(&input,1.f/60);world.Physics().Step(1.f/60);world.UpdateCharacters(1.f/60);
        check(motor->velocity.y>3,"logical jump applies actual motor velocity");
        world.PresentationScripts(&input,1.f/60,.5f);check(world.view.has_value()&&world.pointerCapture,"player publishes a presentation camera and capture request");
        const auto firstView=world.view->pose.position;const auto savedVelocity=motor->velocity;
        input.BeginFrame();input.SetPhysical("key:V",1);world.UpdateUIScripts(&input,1.f/60);world.PresentationScripts(&input,1.f/60,.5f);
        check(glm::length(world.view->pose.position-firstView)>3&&world.RuntimeCharacter(player)==motor&&motor->velocity==savedVelocity,"V switches to third person without replacing player or changing physics state");
        check(world.RuntimeDefinition(player)->renderVisible,"third-person view shows the authored capsule body");
        const auto obstruction=world.Physics().CreateStaticBox(firstView+glm::vec3(0,0,2),{1,1,.1f},.6f,0);
        world.PresentationScripts(&input,1.f/60,.5f);
        check(glm::length(world.view->pose.position-firstView)>.5f&&glm::length(world.view->pose.position-firstView)<2,
              "third-person camera sphere-cast pulls in before a real physical wall");
        world.Physics().DestroyBody(obstruction);
        input.BeginFrame();input.SetPhysical("key:V",0);world.UpdateUIScripts(&input,1.f/60);input.BeginFrame();input.SetPhysical("key:V",1);world.UpdateUIScripts(&input,1.f/60);world.PresentationScripts(&input,1.f/60,.5f);
        check(glm::length(world.view->pose.position-firstView)<.001f&&!world.RuntimeDefinition(player)->renderVisible,"switching back restores first-person camera and hides own mesh");
        check(world.Scripts()->Diagnostics().empty(),"player lifecycle and public calls remain fault-free");
        world.EndScripts();check(!world.view&&!world.pointerCapture,"Stop releases player camera and capture");world.Destroy();
        EditorDocument thirdDoc;const auto thirdPlayer=CreateEditorFirstPersonPlayer(thirdDoc,project,assets,{},0,error,true);
        check(thirdPlayer&&thirdDoc.SelectedObject()->scripts[0].properties=="{\"thirdPerson\":true}","third-person preset authors the same player with a view property");
        thirdDoc.Undo();check(!thirdDoc.GetScene().Find(thirdPlayer),"third-person player creation remains one Undo transaction");
        for(const auto* kind:{"capsule","cylinder"}){
            const auto primitive=CreateEditorPrimitive(playerDoc,project,assets,kind,{3,2,0},0,error);
            check(primitive!=0,"primitive creation authors selected ordinary mesh and collision");
            const auto* object=playerDoc.GetScene().Find(primitive);
            check(playerDoc.Selected()==primitive&&object->render&&object->body&&object->body->shape==SceneShape::ConvexHull,"primitive has real convex collision and immediate selection");
            const auto mesh=object->render->meshAsset,shape=object->body->collisionAsset;
            CollisionAsset collision;check(LoadCollisionAsset(assets.Find(shape)->path,collision,error)&&collision.convex&&collision.volume>1.1,"primitive collision contains a genuine closed convex volume");
            Scene isolated;auto& physical=isolated.CreateObject("Primitive collision");physical.transform=object->transform;physical.body=object->body;
            check(world.Build(isolated,&resources,error),"generated convex body builds through normal runtime");
            const auto center=world.Physics().Raycast({3,2,-2},{0,0,1},4);
            const auto corner=world.Physics().Raycast({3.49f,2.9f,-2},{0,0,1},4);
            check(center.hit&&center.distance>1.4f&&center.distance<1.6f,"actual runtime ray hits the curved primitive surface");
            check(std::string(kind)=="capsule"?!corner.hit:corner.hit,"capsule cap differs physically from cylinder and box bounds");
            world.Destroy();
            const auto count=assets.Records().size();
            const auto child=CreateEditorPrimitive(playerDoc,project,assets,kind,{},primitive,error);
            check(child&&assets.Records().size()==count&&playerDoc.GetScene().Find(child)->render->meshAsset==mesh&&playerDoc.GetScene().Find(child)->body->collisionAsset==shape,"repeated primitives share the same authored render/collision assets");
            check(playerDoc.GetScene().Find(child)->parent==primitive&&playerDoc.GetScene().Find(child)->transform.position==glm::vec3(0),"primitive child creation uses local origin");
            playerDoc.Undo();check(!playerDoc.GetScene().Find(child)&&assets.Find(mesh),"primitive Undo removes only the object and preserves reusable assets");
            {std::ofstream changed(assets.Find(mesh)->path,std::ios::app);changed<<"# user edit\n";}
            check(!CreateEditorPrimitive(playerDoc,project,assets,kind,{},0,error)&&error.find("refusing to overwrite")!=std::string::npos,
                  "edited primitive mesh is preserved and reuse fails with an actionable error");
        }
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1000,800};io.DeltaTime=1.f/60;
        unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        EditorPanelState state;state.project=&project;state.assets=&assets;state.newScriptName="FromInspector.js";doc.Select(owner);
        auto frame=[&](){ImGui::NewFrame();ImGui::SetNextWindowSize({400,400});ImGui::Begin("Workflow");DrawEditorScriptWorkflow(doc,*doc.SelectedObject(),state);ImGui::End();ImGui::Render();};
        frame();frame();auto* window=ImGui::FindWindowByName("Workflow");
        ImGui::ActivateItemByID(window->GetID("Create script & attach"));frame();frame();
        auto* popup=ImGui::FindWindowByName("Create script");check(popup&&state.newScriptOwner==owner,"inspector button opens named create dialog for the exact object");
        ImGui::ActivateItemByID(popup->GetID("Create & edit"));frame();frame();
        check(!state.editScriptAsset.empty()&&assets.Find(state.editScriptAsset)&&state.revealComponent=="Scripts","actual dialog creates attachment and requests source editor");
        check(doc.SelectedObject()->scripts.size()==2,"UI creates exactly one additional attachment");
        check(ImGui::GetCurrentContext()->OpenPopupStack.empty(),"successful creation closes its modal");
        state.mode=EditorMode::Play;ImGui::ActivateItemByID(window->GetID("Create script & attach"));frame();frame();
        check(ImGui::GetCurrentContext()->OpenPopupStack.empty(),"create/attach is disabled during Play");
        ImGui::DestroyContext();fs::remove_all(root);
        std::cout<<"Editor script workflow: "<<checks<<" checks, 0 failures\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
