#include "NamedAuthoring.h"
#include "WorldBuilder.h"
#include "AuthoringDocument.h"
#include "SceneSerialization.h"
#include "editor/EditorDocument.h"
#include "RuntimeUI.h"
#include "PerformanceProfiler.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
using J=nlohmann::json;
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::filesystem::create_directories(argv[1]);std::string error;
    Scene scene;if(!LoadSceneFromFile("projects/world_workshop/Scenes/workshop.judas",scene,error)){std::cerr<<error;return 1;}
    EditorDocument doc;doc.GetScene()=scene;for(auto& o:scene.Objects())if(o.id>=100&&doc.Selection().size()<32)doc.Select(o.id,true);
    auto& profiler=PerformanceProfiler::Get();profiler.Enable(true);profiler.RegisterThread("authoring measurements");
    J result={{"objects",scene.Objects().size()},{"batchSelection",doc.Selection().size()},{"units","milliseconds"},{"measurements",J::object()}};
    auto measure=[&](const char* label,auto operation){std::vector<double> samples;double cold=0;for(int i=0;i<32;++i){auto start=PerformanceProfiler::Now();profiler.BeginFrame("M67 authoring",i==0);{ProfileScope scope(profiler.Intern(label));operation();}profiler.EndFrame();double ms=double(PerformanceProfiler::Now()-start)/1e6;if(!i)cold=ms;else samples.push_back(ms);}std::sort(samples.begin(),samples.end());result["measurements"][label]={{"cold",cold},{"median",samples[samples.size()/2]},{"p95",samples[size_t(samples.size()*.95)]},{"maximum",samples.back()}};};
    measure("Selection",[&]{doc.Select(3);doc.Select(6,true);});
    measure("Filter cached",[&]{doc.Search("gallery-1");});
    measure("Filter changed query",[&]{static unsigned n=0;doc.Search(n++%2?"gallery-1":"Step");});
    doc.Select(0);for(auto& o:scene.Objects())if(o.id>=100&&doc.Selection().size()<32)doc.Select(o.id,true);
    measure("Batch translation plus undo",[&]{if(!doc.BatchTransform({.25f,0,0},{1,0,0,0},{1,1,1},error,true,true))throw std::runtime_error(error);doc.Undo();});
    WorldRecipe recipe;ParseWorldRecipe(scene.Settings().authoringRecipes.at("gallery-0"),recipe,error);AssetDatabase assets;
    measure("Array regeneration",[&]{RecipeProduct product;if(!EvaluateWorldRecipe(recipe,scene,&assets,product,error))throw std::runtime_error(error);});
    ParseWorldRecipe(scene.Settings().authoringRecipes.at("quarter"),recipe,error);
    measure("Arc generation plus actual cook",[&]{RecipeProduct product;if(!EvaluateWorldRecipe(recipe,scene,&assets,product,error))throw std::runtime_error(error);});
    std::string source;SaveSceneToString(scene,source);
    measure("Source normalize and reload",[&]{std::string named;if(!LegacyToNamed(source,"scene",named,error))throw std::runtime_error(error);Scene decoded;if(!LoadSceneFromString(named,decoded,error))throw std::runtime_error(error);});
    UIDocument ui;LoadUIDocument("projects/world_workshop/Assets/ui/workshop.judasui",ui,error);RuntimeUI preview;auto h=preview.Add(ui,"preview",0,error);preview.Layout(1280,720);
    measure("Runtime layout mutation",[&]{static unsigned layoutTick=0;preview.SetElementLayout(h,"marker",{{"offset",{float(10+(layoutTick++%2)),20}},{"size",{150,35}}},error);preview.Layout(1280,720);});
    profiler.Export((std::filesystem::path(argv[1])/"m56-profile.json").string(),error);
    std::ofstream(std::filesystem::path(argv[1])/"authoring-performance.json")<<result.dump(2)<<'\n';std::cout<<result.dump(2)<<'\n';return 0;
}
