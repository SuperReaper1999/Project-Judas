// Integration of the actual editor handlers, not a source/symbol inventory.
#include "Project.h"
#include "ProjectStarter.h"
#include "editor/EditorDocument.h"
#include "editor/EditorScriptWorkflow.h"
#include "editor/SpriteSheetEditor.h"
#include "editor/TilemapEditor.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "ScriptSystem.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
int main(int argc,char** argv){unsigned checks=0;try{
 if(argc!=3)return 2;
 std::string error;auto check=[&](bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(std::string(text)+": "+error);std::cout<<"PASS "<<text<<'\n';};
 const auto root=fs::absolute(argv[2]);check(!fs::exists(root),"fresh output protects existing projects");Project p;
 check(Project::CreateNew(root.string(),"Fresh 2D Workflow",p,error),"create brand-new project");EditorDocument doc;
 check(PopulatePlanarProjectStarter(p,doc.GetScene(),false,error),"same New 2D Game starter as editor");p.Settings().startupScene="Scenes/main.judas";
 AssetDatabase assets;assets.Scan(p.RootDir(),p.AssetsDir());AssetRecord image,sheet,script;
 check(assets.Import(argv[1],"Imported/tiles.png",image,error),"import external PNG with ordinary asset metadata");
 check(!assets.Import(argv[1],"Imported/tiles.png",image,error),"duplicate destination never overwrites source art");error.clear();
 check(CreateSpriteSheetAsset(assets,image.id,sheet,error),"same right-click Create sprite sheet handler");SpriteSheetEditor editor;
 check(editor.Open(sheet,error),"open actual editable sheet draft");
 check(SliceSpriteSheet(editor.draft,{32,32},{0,0},{0,0},error),"slice native palette using actual image dimensions");
 check(editor.draft.frames.size()>=2,"workflow input image must provide at least two 32x32 frames");
 SpriteClip clip;clip.name="test-loop";clip.frames={{editor.draft.frames[0].id,.1},{editor.draft.frames[1].id,.1}};clip.events={{"pulse","pulse","",.1}};editor.draft.clips.push_back(clip);
 check(editor.Save(error),"save authored frame animation and event content");
 TilemapEditor painter;check(painter.Create(doc,assets,sheet.id,{0,0,0},error),"create selected editable tile grid");painter.layer=0;painter.frame=0;painter.painting=true;
 check(painter.Stroke(doc,assets,{2.5f,2.5f,5},{0,0,-1},true,true,false,true,false,false,error),"start viewport brush through native ray");
 check(painter.Stroke(doc,assets,{2.5f,2.5f,5},{0,0,-1},true,false,false,false,false,false,error),"release commits real sprite and collision");
 check(painter.recipe.cells.size()==1,"painted cell really exists");doc.Undo();painter.Sync(doc);check(painter.recipe.cells.empty(),"Undo removes authored cell");doc.Redo();painter.Sync(doc);check(painter.recipe.cells.size()==1,"Redo restores cell");
 auto owner=doc.Selected();const std::string source="export default class {constructor(){this.state={ticks:0};} update(){this.state.ticks++;}}";
 check(CreateAndAttachEditorScript(doc,owner,p,assets,"LevelLogic",script,error,source),"create and attach ordinary JavaScript from inspector workflow");
 check(ScriptSystem::CheckProjectSources(assets).empty(),"complete fresh project compiles public scripts");
 std::ofstream(script.path)<<"export default class { update( {";
 check(!ScriptSystem::CheckProjectSources(assets).empty(),"broken edit is detected rather than accepted silently");
 std::ofstream(script.path)<<source;
 check(ScriptSystem::CheckProjectSources(assets).empty(),"corrected source compiles without restarting editor");
 check(doc.SaveAs(p.StartupScenePath(),error)&&p.Save(error),"save project and scene through actual document path");EditorDocument reopened;check(reopened.Load(p.StartupScenePath(),error),"reopen saved authored scene");
 auto* actor=reopened.GetScene().Find(1);actor->render->spritePlayback=SpritePlayback{sheet.id,"test-loop","",0,1,true,true};
 ResourceManager resources(nullptr,&assets);resources.SetBlockingMode(true);RuntimeWorld world;world.legacyGameplay=false;
 check(world.Build(reopened.GetScene(),&resources,error),"reopened composed sprites, animation, scripts and physical grid build");InputSystem input;world.UpdateScripts(&input,.016f);
 check(world.Scripts()->Diagnostics().empty(),"newly attached source executes in ordinary world");bool ticked=false;for(const auto& state:world.Scripts()->Capture())if(state.entity==owner)ticked=state.json.find("\"ticks\":1")!=std::string::npos;
 check(ticked,"script actually executes instead of merely being present");
 std::cout<<"TWO_D_WORKFLOW checks="<<checks<<" failures=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
