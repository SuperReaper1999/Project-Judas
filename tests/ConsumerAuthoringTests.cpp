#include "editor/EditorDocument.h"
#include "SceneSerialization.h"
#include "ScriptSystem.h"
#include "Project.h"
#include "RuntimeUI.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
namespace {unsigned count=0,failed=0;void check(bool ok,const std::string& message){++count;failed+=!ok;std::cout<<(ok?"PASS ":"FAIL ")<<message<<'\n';}}
int main(int argc,char** argv){
    if(argc!=3){std::cerr<<"usage: "<<argv[0]<<" <consumers-project-dir> <output-dir>\n";return 2;}
    std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);std::string error;
    Project project;check(project.Load((root/"post_m65_consumers.judasproj").string(),error),"combined ordinary project loads");
    check(!project.Settings().legacyGameplay,"combined project has no legacy gameplay path");
    UIDocument menu;check(LoadUIDocument((root/"Assets/collection/menu.judasui").string(),menu,error)&&menu.elements.size()==7,"named menu loads through normal UI serializer");
    for(auto game:{"skate/park","rooftop/rooftops","void/system"}){
        EditorDocument doc;check(doc.Load((root/"Scenes"/(std::string(game)+".judas")).string(),error),std::string(game)+" editor document loads");
        std::string before;SaveSceneToString(doc.GetScene(),before);
        std::vector<SceneObjectId> render,bodies;for(const auto& o:doc.GetScene().Objects()){if(o.render)render.push_back(o.id);if(o.body&&o.body->motion==SceneBodyMotion::Static)bodies.push_back(o.id);}
        check(render.size()>1&&bodies.size()>1,"actual consumer has editable render/body batches");
        doc.Select(render[0]);doc.Select(render[1],true);check(doc.Selection().size()==2,"actual consumer multi-selection");
        check(doc.BatchProperties({{"render.color","0.1 0.2 0.3"}},error),"batch-edit colour on actual consumer geometry");
        check(doc.GetScene().Find(render[0])->render->color==glm::vec3(.1f,.2f,.3f)&&doc.GetScene().Find(render[1])->render->color==glm::vec3(.1f,.2f,.3f),"both selected items edited");
        doc.Undo();std::string restored;SaveSceneToString(doc.GetScene(),restored);check(restored==before,"batch undo preserves authored consumer byte serialization");
        doc.Select(bodies[0]);doc.CopyComponent("body");doc.Select(bodies[1]);check(doc.PasteComponent(error),"body component copy/paste on actual scene");doc.Undo();
        doc.Select(render[0]);doc.Select(render[1],true);auto size=doc.GetScene().Objects().size();check(doc.DuplicateSelection(error)&&doc.GetScene().Objects().size()>=size+2,"duplicate batch including hierarchy");doc.Undo();
        check(doc.SaveAs((out/(std::filesystem::path(game).filename().string()+".judas")).string(),error),"edited/undo scene saves to isolated output");
    }
    EditorDocument skate;check(skate.Load((root/"Scenes/skate/park.judas").string(),error),"Skate typed-reference fixture loads");
    const auto old=skate.GetScene().Objects().size();skate.Select(10);skate.Select(11,true);skate.Select(12,true);
    check(ScriptSystem::PropertyEntities(skate.GetScene().Find(10)->scripts[0].properties)==std::vector<SceneObjectId>({11,12}),"Skate actually authors deck/rider entity references");
    check(skate.DuplicateSelection(error),"Skate board/deck/rider duplicated together");
    const auto selected=skate.Selection();check(selected.size()==3&&skate.GetScene().Objects().size()==old+3,"three independent copied consumer entities");
    if(selected.size()==3){auto refs=ScriptSystem::PropertyEntities(skate.GetScene().Find(selected[0])->scripts[0].properties);check(refs==std::vector<SceneObjectId>({selected[1],selected[2]}),"copy remaps typed references to copied visual rig");}
    std::cout<<"SUMMARY "<<count<<" checks "<<failed<<" failures\n";return failed?1:0;
}
