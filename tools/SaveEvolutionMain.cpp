#include "BuildIdentity.h"
#include "Project.h"
#include "AssetDatabase.h"
#include "SceneSession.h"
#include "SaveService.h"
#include "SaveEvolution.h"
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
#include <cstdio>
#include <filesystem>
#include <chrono>
using J=nlohmann::json;
int main(int argc,char** argv){
    if (HandleBuildIdentityArguments(argc, argv, "save")) return 0;

 try{
  if(argc<3)throw std::runtime_error("Usage: judas_save identity project | inspect root slot | check project root slot | migrate project root source unused-destination | recover root source unused-destination | previous root source unused-destination. No command overwrites a source. check/migrate validate data conversion, not native restoration; load the destination in the game before trusting it.");
  std::string cmd=argv[1],error,content;Project project;AssetDatabase assets;
  if(cmd=="identity"||cmd=="check"||cmd=="migrate"){
   if(!project.Load(argv[2],error))throw std::runtime_error(error);
   assets.Scan(project.RootDir(),project.AssetsDir());if(!assets.Problems().empty())throw std::runtime_error("project asset diagnostics must be resolved first");SceneSession session(project,project.Settings().startupScene);
   if(!ComputeSaveContentFingerprint(project,assets,session.Scenes(),content,error))throw std::runtime_error(error);
   SaveEvolutionIdentity id;if(!SaveEvolutionCurrent(project.Settings().saveEvolution,id,error))throw std::runtime_error(error);
   if(cmd=="identity"){std::puts(J({{"content",content},{"compatibility",id.compatibility},{"version",id.version}}).dump(2).c_str());return 0;}
  }
  bool recovered=false;GameSnapshot source;auto start=std::chrono::steady_clock::now();
  if(cmd=="inspect"||cmd=="recover"||cmd=="previous"){
   if(argc!=(cmd=="inspect"?4:5))throw std::runtime_error("wrong argument count");
   SaveStorage storage(argv[2]);if(!(cmd=="previous"?storage.ReadPrevious(argv[3],source,error):storage.Read(argv[3],source,recovered,error)))throw std::runtime_error(error);
   if(cmd=="recover"||cmd=="previous"){
    auto slots=storage.List("","",error);if(!error.empty())throw std::runtime_error(error);for(auto& s:slots)if(s.id==argv[4])throw std::runtime_error("destination exists; choose a new slot");if(std::string(argv[3])==argv[4])throw std::runtime_error("source is never overwritten");if(!storage.Write(argv[4],source,error,true))throw std::runtime_error(error);
   }
  }else if(cmd=="check"||cmd=="migrate"){
   if(argc!=(cmd=="check"?5:6))throw std::runtime_error("wrong argument count");
   // Mod resolution/trust belongs to the normal game launcher. Offline conversion
   // deliberately supports unmodded games only, never auto-enables packages.
   if(!project.Settings().modContract.empty())throw std::runtime_error("offline migration of mod-enabled projects is unsupported; use the resolved in-game service");
   SaveStorage storage(argv[3]);if(!storage.Read(argv[4],source,recovered,error))throw std::runtime_error(error);
   auto identity=project.Settings().saveIdentity;if(identity.empty())identity=SceneFingerprintSha256(project.Settings().name+"\n"+std::filesystem::path(project.ProjectFile()).filename().string());if(source.project!=identity)throw std::runtime_error("different project identity");
   GameSnapshot result;std::string report;if(!EvolveSave(project.Settings().saveEvolution,content,"",source,true,result,report,error))throw std::runtime_error(error);
   if(cmd=="migrate"){
    auto slots=storage.List("","",error);if(!error.empty())throw std::runtime_error(error);for(auto& s:slots)if(s.id==argv[5])throw std::runtime_error("destination exists; choose a new slot");if(std::string(argv[4])==argv[5])throw std::runtime_error("source is never overwritten");if(!storage.Write(argv[5],result,error,true))throw std::runtime_error(error);
   }
   std::puts(report.c_str());
  }else throw std::runtime_error("unknown save command");
  std::puts(J({{"project",source.project},{"scene",source.scene},{"content",source.content},{"compatibility",source.compatibility},{"version",source.contentVersion},{"engineSchema",source.gameVersion},{"build",source.diagnosticBuild},{"participants",source.participants.size()},{"modContext",source.modContext},{"recoveredPrevious",recovered},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}}).dump(2).c_str());return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"Save tool: %s\n",e.what());return 1;}
}
