#include "ProjectStarter.h"
#include "Project.h"
#include "Scene.h"
#include "AssetDatabase.h"
#include "EnginePaths.h"
#include <filesystem>
#include <fstream>

bool PopulateVisibleProjectStarter(Project& project,Scene& scene,std::string& error) {
    const auto source=ResolveEngineDataPath("docs/judasjs/examples/first-project.js");
    std::ifstream input(source,std::ios::binary);
    if(!input){error="Visible starter script is missing: "+source+". Keep the SDK docs with the editor, or choose Empty scene.";return false;}
    const std::string bytes{std::istreambuf_iterator<char>(input),{}};
    const auto destination=std::filesystem::u8path(project.AssetsDir())/"game.js";
    if(std::filesystem::exists(destination)){error="Starter would overwrite "+destination.u8string();return false;}
    std::ofstream output(destination,std::ios::binary);output.write(bytes.data(),bytes.size());output.close();
    if(!output){error="Cannot write starter script: "+destination.u8string();return false;}
    AssetDatabase assets;assets.Scan(project.RootDir(),project.AssetsDir());AssetRecord record;
    if(!assets.Track(destination.u8string(),record,error))return false;
    auto& object=scene.CreateObject("My rotating box - edit Assets/game.js");
    object.transform.position={0,2,0};object.render=SceneRenderComponent{};
    object.render->color={.2f,.7f,.85f};object.scripts.push_back({1,record.id,true,"{}"});
    error.clear();return true;
}
