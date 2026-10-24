#include "RuntimeWorld.h"
#include "Project.h"
#include "WorldStreaming.h"
#include "AssetDatabase.h"
#include "SceneSerialization.h"
#include <iostream>
int main(){
 std::string e;Project project;if(!Project::CreateNew("/tmp/judas-final-prealpha-audit-20261024/stream-project","Policy probe",project,e))return 2;
 AssetDatabase assets;assets.Scan(project.RootDir(),project.Settings().assetsDir);
 for(bool authored:{false,true}){
  Scene scene;auto& o=scene.CreateObject("Region fixture");o.render=SceneRenderComponent{};
  if(authored){o.liquidBasin=LiquidBasinSettings{};o.liquidBasin->geometry=std::string(32,'a');o.liquidBasin->asset=std::string(32,'b');}
  RuntimeWorld world;world.ConfigureProject(project.Settings());e.clear();bool root=world.Build(scene,nullptr,e);std::cout<<"ROOT advanced="<<world.AdvancedFluidsEnabled()<<" authored="<<authored<<" accepted="<<root<<" error="<<e<<"\n";
  const auto path=project.Resolve("Scenes/region.judas");if(!SaveSceneToFile(scene,path,e))return 3;
  WorldRegion region;region.id="region";region.scene="Scenes/region.judas";PreparedWorldRegion prepared;e.clear();bool okay=PrepareWorldRegion(region,project,assets,prepared,e);
  std::cout<<"REGION advanced="<<project.Settings().advancedFluidDynamics<<" authored="<<authored<<" accepted="<<okay<<" error="<<e<<"\n";
 }
 return 0;
}
