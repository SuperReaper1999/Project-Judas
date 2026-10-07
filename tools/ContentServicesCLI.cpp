#include "ContentServicesCLI.h"
#include "ModelCook.h"
#include "ModelLoader.h"
#include "ModelCollision.h"
#include "SkeletonAuthoring.h"
#include "AuthoringDocument.h"
#include "NamedAuthoring.h"
#include "ProjectExporter.h"
#include "SceneSerialization.h"
#include "Project.h"
#include "Scene.h"
#include "AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
namespace {
std::string read(const std::string& p){std::ifstream f(p,std::ios::binary);
if(!f)throw std::runtime_error("Cannot read "+p);
return {std::istreambuf_iterator<char>(f),{}};
}
AuthoringOutput destination(const std::string& path,bool overwrite){
 bool exists=std::filesystem::exists(path);
 if(exists&&!overwrite)throw std::runtime_error("Destination exists: use --overwrite after review");
 return {path,"",exists?read(path):"",exists};
}
void publish(AuthoringOutput output,const std::string& bytes,std::string& error){
 output.bytes=bytes;
 if(!PublishAuthoringOutputs(std::filesystem::path(output.path).parent_path().string()+"/.authoring-recovery",{output},error))throw std::runtime_error(error);
}
}
int RunContentServicesCLI(int argc,char** argv){
 if(argc<2)return -1;
std::string command=argv[1];

 if(command!="--build-project"&&command!="--import-model"&&command!="--cook-collision"&&command!="--fit-skeleton"&&command!="--track-asset"&&command!="--object-template"&&command!="--create-import")return -1;

 try{
  std::string error;
bool overwrite=false;
for(int i=2;i<argc;++i)overwrite|=std::string(argv[i])=="--overwrite";

  if(command=="--build-project"){
   if(argc!=6)throw std::runtime_error("--build-project project destination releaseRuntime engineData");
Project project;
if(!project.Load(argv[2],error))throw std::runtime_error(error);
ProjectExportOptions options;
options.destination=argv[3];
options.runtimeExecutable=argv[4];
options.engineDataRoot=argv[5];
ProjectExportResult result;
if(!ExportProject(project,options,result,error))throw std::runtime_error(error);
std::cout<<result.packageDirectory<<" | "<<result.assetCount<<" assets | "<<result.bytes<<" bytes\n";
return 0;

  }
  if(command=="--create-import"){
   if(argc!=5)throw std::runtime_error("--create-import projectRoot source relativeOutput");

   std::string recipe;
if(!CreateModelRecipe(argv[2],argv[3],argv[4],recipe,error))throw std::runtime_error(error);
std::cout<<recipe<<'\n';
return 0;

  }
  if(command=="--import-model"){
   if(argc!=3)throw std::runtime_error("--import-model model.judasimport");
ModelCookTask task;
if(!CookModelRecipe(argv[2],task)||!PublishModelImport(task,error))throw std::runtime_error(task.error.empty()?error:task.error);
std::cout<<task.assetId<<" "<<task.output<<'\n';
return 0;

  }
  if(command=="--track-asset"){
   if(argc<4||argc>5)throw std::runtime_error("--track-asset project assetPath [stableAssetID]");
Project p;
if(!p.Load(argv[2],error))throw std::runtime_error(error);
AssetDatabase db;
db.Scan(p.RootDir(),p.AssetsDir());
AssetRecord r;
auto relative=std::filesystem::weakly_canonical(argv[3]).lexically_relative(p.RootDir()).generic_string();
if(auto* existing=db.FindByRelativePath(relative)){if(argc==5&&existing->id!=argv[4])throw std::runtime_error("Existing asset identity differs; no reminting");
if(!AssetDatabase::ValidateAssetFile(existing->path,existing->type,error))throw std::runtime_error(error);
std::cout<<existing->id<<'\n';
return 0;
}if(!db.Track(argv[3],r,error,argc==5?argv[4]:""))throw std::runtime_error(error);
std::cout<<r.id<<'\n';
return 0;

  }
  if(command=="--object-template"){
   if(argc<5)throw std::runtime_error("--object-template componentsCSV name output [--overwrite]");
auto output=destination(argv[4],overwrite);
Scene s;
auto& o=s.CreateObject(argv[3]);
std::istringstream list(argv[2]);
std::string component;

   while(std::getline(list,component,',')){
#define ADD(name,member,type) if(component==name)o.member=type{};else
    ADD("render",render,SceneRenderComponent) ADD("body",body,SceneBodyComponent) ADD("motor",characterMotor,CharacterMotorSettings) ADD("gravity",gravity,SceneGravityComponent) ADD("camera",renderCamera,SceneRenderCameraComponent) ADD("animation",animation,SceneAnimationComponent) ADD("ragdoll",ragdoll,RagdollDefinition) ADD("socket",socket,SceneSocketComponent) ADD("joint",joint,SceneJointComponent) ADD("particle",particleEmitter,ParticleEmitterSettings) ADD("audio",audioEmitter,SceneAudioEmitterComponent) ADD("ui",ui,SceneUIComponent)
    if(component!="empty")throw std::runtime_error("Unknown template component "+component);

#undef ADD
   }
   if(o.animation&&o.render)o.render->shape=SceneShape::Mesh;

   std::string legacy,named;
SaveSceneToString(s,legacy);
if(!LegacyToNamed(legacy,"scene",named,error))throw std::runtime_error(error);
publish(output,named,error);
return 0;

  }
  if(command=="--fit-skeleton"){
   if(argc<8)throw std::runtime_error("--fit-skeleton scene project entity jointKeysCSV thickness output [--overwrite]");
auto output=destination(argv[7],overwrite);
const auto source=read(argv[2]);
Project p;
Scene s;
if(!p.Load(argv[3],error)||!LoadSceneFromFile(argv[2],s,error))throw std::runtime_error(error);
auto* o=s.Find(std::stoull(argv[4]));
if(!o||!o->render)throw std::runtime_error("Mapped entity requires a mesh");
AssetDatabase db;
db.Scan(p.RootDir(),p.AssetsDir());
auto* asset=db.Find(o->render->meshAsset);
MeshData mesh;
if(!asset||!LoadModelMesh(asset->path,mesh,error)||!mesh.skeletal)throw std::runtime_error("Skeleton asset: "+error);
std::vector<std::string> joints;
std::istringstream list(argv[5]);
std::string key;
while(std::getline(list,key,','))joints.push_back(key);
RagdollDefinition fitted;
if(!FitSkeletonChain(mesh.skeletal->skeleton,o->ragdoll.value_or(RagdollDefinition{}),joints,std::stof(argv[6]),fitted,error))throw std::runtime_error(error);
o->ragdoll=fitted;
std::string legacy,named;
SaveSceneToString(s,legacy);
if(!LegacyToNamed(legacy,"scene",named,error))throw std::runtime_error(error);
if(read(argv[2])!=source)throw std::runtime_error("Scene changed during skeleton fitting; retry against reviewed source");
publish(output,named,error);
return 0;

  }
  if(argc<5)throw std::runtime_error("--cook-collision meshSource stableMeshID output [--convex] [--two-sided] [--overwrite]");

  auto output=destination(argv[4],overwrite);
const auto source=read(argv[2]);
CollisionCookSettings settings;
for(int i=5;i<argc;++i){settings.convex|=std::string(argv[i])=="--convex";
settings.twoSided|=std::string(argv[i])=="--two-sided";
}CollisionAsset cooked;
ModelCollisionCleanup cleanup;
ModelCollisionCleanupReport report;
CollisionDiagnostic diagnostic;

  auto stage=std::string(argv[4])+".cook-stage-"+MintAssetId();
try {
 if(!CookImportedCollisionFile(argv[2],argv[3],settings,cleanup,stage,cooked,report,diagnostic,error))throw std::runtime_error("Collision "+diagnostic.code+" source face "+std::to_string(diagnostic.sourceFace)+": "+error);
 if(read(argv[2])!=source)throw std::runtime_error("Collision source changed during cooking; retry against reviewed source");
 publish(output,read(stage),error);
 std::filesystem::remove(stage);
} catch(...) {std::error_code ignored;std::filesystem::remove(stage,ignored);throw;}
std::cout<<cooked.faces.size()<<" faces\n";
return 0;

 }catch(const std::exception& e){std::cerr<<"content service: "<<e.what()<<'\n';
return 1;
}
}
