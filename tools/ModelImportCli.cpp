#include "ModelImport.h"
#include "PerformanceProfiler.h"
#include "ModelLoader.h"
#include "PoseComposition.h"
#include "ModelCook.h"
#include "ModelArchive.h"
#include "ModelCollision.h"
#include "AssetDatabase.h"
#include "SceneFingerprint.h"
#include "../third_party/nlohmann/json.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>
using Json=nlohmann::json;
namespace {
Json Report(const ModelImportReport& r){Json j={{"sourceBones",r.sourceBones},{"hierarchyNodes",r.hierarchyNodes},{"paletteEntries",r.skinJoints},{"parts",r.parts},{"vertices",r.vertices},{"sourceUnitMeters",r.sourceUnitMeters},{"diagnostics",Json::array()}};for(auto& d:r.diagnostics)j["diagnostics"].push_back({{"severity",d.severity},{"code",d.code},{"source",d.source},{"node",d.node},{"face",d.face},{"vertex",d.vertex},{"message",d.message},{"action",d.action}});return j;}
}
int main(int argc,char** argv){
  ProfileRun run("Model import CLI");ProfileFrame frame("Model import CLI");
 try{
  std::string error;
  if(argc==3&&std::string(argv[1])=="--inspect") {MeshData mesh;if(!LoadModelMesh(argv[2],mesh,error))throw std::runtime_error(error);Json j={{"parts",Json::array()},{"clips",Json::array()},{"joints",Json::array()},{"materials",mesh.materialKeys}};for(size_t i=0;i<mesh.primitives.size();++i){auto& p=mesh.primitives[i];j["parts"].push_back({{"ordinal",i},{"identity",p.part},{"triangles",p.count/3},{"material",p.material}});}j["materialDefaults"]=Json::array();for(auto& m:mesh.materials){int low=255,high=0;for(size_t k=3;k<m.maps[0].embedded.pixels.size();k+=4){low=std::min(low,int(m.maps[0].embedded.pixels[k]));high=std::max(high,int(m.maps[0].embedded.pixels[k]));}j["materialDefaults"].push_back({{"factor",{m.baseColor.r,m.baseColor.g,m.baseColor.b,m.baseColor.a}},{"alpha",int(m.alpha)},{"imageAlphaMin",low},{"imageAlphaMax",high}});}if(mesh.skeletal){auto& a=*mesh.skeletal;for(size_t i=0;i<a.skeleton.names.size();++i)j["joints"].push_back(SkeletonJointKey(a.skeleton,int(i)));for(auto& c:a.clips)j["clips"].push_back({{"name",c.name},{"duration",c.duration},{"rootMotion",!c.motion.empty()}});}std::cout<<j.dump(2)<<'\n';return 0;}
  if(argc==5&&std::string(argv[1])=="--create") {std::string recipe;bool ok=CreateModelRecipe(argv[2],argv[3],argv[4],recipe,error);std::cout<<Json{{"success",ok},{"recipe",recipe},{"error",error}}.dump(2)<<'\n';return ok?0:1;}
  if(argc==3&&std::string(argv[1])=="--recipe") {ModelCookTask task;task.previewRequired=false;bool ok=CookModelRecipe(argv[2],task)&&PublishModelImport(task,error);auto j=Report(task.report);j["success"]=ok;j["unchanged"]=task.unchanged;j["receiptHit"]=task.receiptHit;j["decodedProducts"]=task.decodedProducts;j["output"]=task.output;j["assetId"]=task.assetId;j["error"]=error.empty()?task.error:error;std::cout<<j.dump(2)<<'\n';return !ok?1:task.report.diagnostics.empty()?0:2;}
  if(argc>=5&&std::string(argv[1])=="--collision"){
   MeshData source,cleaned;ModelCollisionCleanup settings;ModelCollisionCleanupReport cleanup;CollisionCookSettings cook;CollisionAsset asset;CollisionDiagnostic diagnostic;
   unsigned part=unsigned(std::stoul(argv[3]));cook.primitive=part;for(int i=5;i<argc;++i){std::string option=argv[i];if(option=="--remove-degenerates")settings.removeDegenerates=true;else if(option=="--orient")settings.orientPatches=true;else if(option=="--convex")cook.convex=true;else if(option=="--two-sided")cook.twoSided=true;else if(option.rfind("--weld=",0)==0)settings.weldTolerance=std::stod(option.substr(7));else throw std::runtime_error("unknown collision option: "+option);}
   AssetType type;std::string sourceId,provenance;AssetDatabase::ReadMeta(std::string(argv[2])+".judasmeta",sourceId,type,provenance,error);error.clear();
   bool ok=CookImportedCollisionFile(argv[2],sourceId,cook,settings,argv[4],asset,cleanup,diagnostic,error);
   Json report={{"success",ok},{"source",argv[2]},{"output",argv[4]},{"error",error},{"cleanup",{{"removeDegenerates",settings.removeDegenerates},{"weldTolerance",settings.weldTolerance},{"orientPatches",settings.orientPatches},{"removed",cleanup.removed},{"welded",cleanup.welded},{"flipped",cleanup.flipped}}},{"diagnostic",{{"code",diagnostic.code},{"node",diagnostic.node},{"face",diagnostic.sourceFace},{"vertices",diagnostic.sourceVertices},{"point",{diagnostic.point.x,diagnostic.point.y,diagnostic.point.z}},{"action",diagnostic.action}}}};std::cout<<report.dump(2)<<'\n';std::ofstream(std::string(argv[4])+".report.json")<<report.dump(2)<<'\n';return ok?0:1;
  }
  // Direct source inspection/cook is useful for fixtures. Project publication
  // uses --create / --recipe; this path does not create a second asset registry.
  if(argc<3){std::cerr<<"Usage: --create PROJECT SOURCE Assets/name.judasmodel | --recipe Imports/name.judasimport | SOURCE OUTPUT [MOTIONS...] | --collision MODEL PART OUTPUT [--orient --remove-degenerates --weld=0.00001 --convex]\n";return 64;}
  MeshData mesh;ModelImportReport report;ModelImportSettings settings;
  if(!ImportModelSource(argv[1],settings,mesh,report,error)){auto j=Report(report);j["error"]=error;std::cout<<j.dump(2)<<'\n';return 1;}
  if(argc>3&&mesh.skeletal){auto a=std::make_shared<SkeletalAsset>(*mesh.skeletal);for(int i=3;i<argc;++i){std::vector<AnimationClip> clips;ModelImportReport motion;if(!ImportCompatibleMotion(argv[i],settings,a->skeleton,clips,motion,error)){std::cerr<<error<<'\n';return 1;}for(auto& c:clips){c.name=std::filesystem::path(argv[i]).stem().string()+"/"+c.name;a->clips.push_back(std::move(c));}}mesh.skeletal=a;}
  std::vector<uint8_t> bytes;if(!EncodeModelArchive(mesh,bytes,error)){std::cerr<<error<<'\n';return 1;}std::filesystem::create_directories(std::filesystem::path(argv[2]).parent_path());std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!out)throw std::runtime_error("output write failed");auto j=Report(report);j["clips"]=Json::array();if(mesh.skeletal)for(auto& c:mesh.skeletal->clips)j["clips"].push_back({{"name",c.name},{"duration",c.duration},{"tracks",c.tracks.size()}});std::cout<<j.dump(2)<<'\n';return report.diagnostics.empty()?0:2;
 }catch(const std::exception& e){std::cout<<Json{{"success",false},{"severity","error"},{"code","cli-operation"},{"message",e.what()}}.dump(2)<<'\n';return 1;}
}
