#include "ModelCook.h"
#include "ProjectExporter.h"
#include "ModelArchive.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Material.h"
#include "NamedAuthoring.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "SaveService.h"
#include "editor/EditorDocument.h"
#include "../third_party/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <sys/resource.h>
using J=nlohmann::json;namespace fs=std::filesystem;
namespace {
unsigned checks=0;
void Check(bool ok,const std::string& label){++checks;if(!ok)throw std::runtime_error(label);std::cout<<"PASS "<<label<<'\n';}
std::string Hash(const fs::path& p){std::string h,e;Check(SceneFingerprintSha256File(p.string(),h,e),e);return h;}
J Read(const fs::path& p){std::ifstream f(p);return J::parse(f);}
void Write(const fs::path& p,const J& j){std::ofstream(p)<<j.dump(2)<<'\n';}
bool Cook(const std::string& p,ModelCookTask& t){std::string e;bool ok=CookModelRecipe(p,t)&&PublishModelImport(t,e);if(!ok)std::cerr<<t.error<<' '<<e<<'\n';return ok;}
}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"usage: judas_scalability_pipeline_tests NEW-output Release-runtime\n";return 2;}
 try{
  auto out=fs::absolute(argv[1]);Check(!fs::exists(out),"fresh owned output required");fs::create_directories(out);
  std::string error,recipe;auto root=out/"import";
  Check(CreateModelRecipe(root.string(),"tests/fixtures/m66/external.gltf","Assets/model.judasmodel",recipe,error),error);
  ModelCookTask first;Check(Cook(recipe,first),"initial validated recipe generation");auto accepted=Hash(first.output);
  ModelCookTask fast;fast.previewRequired=false;Check(Cook(recipe,fast)&&fast.unchanged&&fast.receiptHit&&fast.decodedProducts==0&&!fast.preview,"unchanged content receipt requires zero product decodes");
  ModelCookTask preview;Check(Cook(recipe,preview)&&preview.unchanged&&preview.decodedProducts==1&&preview.preview,"editor unchanged preview needs one decode");
  std::ofstream(first.output+".import-report.json")<<"invalid receipt";
  ModelCookTask fallback;fallback.previewRequired=false;Check(Cook(recipe,fallback)&&fallback.unchanged&&fallback.decodedProducts==1,"corrupt disposable receipt uses validated embedded provenance");
  auto settings=Read(recipe);auto source=root/settings.at("source").get<std::string>();auto original=Read(source);std::ifstream sourceFile(source,std::ios::binary);std::string originalBytes((std::istreambuf_iterator<char>(sourceFile)),{});auto restoreSource=[&]{std::ofstream(source,std::ios::binary)<<originalBytes;};
  auto sameTime=fs::last_write_time(source);auto altered=original;altered["nodes"][1]["translation"]={0.1,0,0};Write(source,altered);fs::last_write_time(source,sameTime);
  ModelCookTask change;change.previewRequired=false;Check(Cook(recipe,change)&&!change.unchanged&&Hash(first.output)!=accepted,"same-mtime content edit invalidates generation");
  restoreSource();ModelCookTask restored;Check(Cook(recipe,restored)&&Hash(first.output)==accepted,"restored content preserves exact cooked representation");
  fs::remove(first.output);
  for(auto& f:fs::directory_iterator(root/".cache/model-import"))if(f.path().extension()==".judasmodel")std::ofstream(f.path(),std::ios::binary)<<"corrupt cache";
  ModelCookTask repaired;Check(Cook(recipe,repaired)&&Hash(first.output)==accepted,"corrupt base cache reconstructs from author-owned sources");
  ModelCookTask staged;Check(CookModelRecipe(recipe,staged),"accepted unchanged generation stages");Write(source,altered);fs::last_write_time(source,sameTime);Check(!PublishModelImport(staged,error)&&Hash(first.output)==accepted,"post-worker same-mtime edit cannot publish stale generation");restoreSource();
  auto options=Read(recipe);options["settings"]["sampleRate"]=61;Write(recipe,options);ModelCookTask earlier;Check(CookModelRecipe(recipe,earlier),"first changed generation staged privately");options["settings"]["sampleRate"]=62;Write(recipe,options);ModelCookTask newer;Check(Cook(recipe,newer)&&!PublishModelImport(earlier,error),"newer import wins without older staging overwrite");
  std::string motionRecipe;Check(CreateModelRecipe(root.string(),"tests/fixtures/m66/capacity256.gltf","Assets/motion.judasmodel",motionRecipe,error),error);
  auto motionOptions=Read(motionRecipe);auto motionSource=motionOptions.at("source").get<std::string>();
  motionOptions["motions"]=J::array({{{"source",motionSource},{"take","Translate"},{"name","External"}}});Write(motionRecipe,motionOptions);
  ModelCookTask motionFirst;Check(Cook(motionRecipe,motionFirst),"compatible motion uses its own validated cache");auto motionHash=Hash(motionFirst.output);fs::remove(motionFirst.output);
  bool foundMotion=false;for(auto& file:fs::directory_iterator(root/".cache/model-import"))if(file.path().string().find(".motion.judasmodel")!=std::string::npos&&file.path().extension()==".judasmodel"){std::ofstream(file.path(),std::ios::binary)<<"corrupt motion cache";foundMotion=true;}
  ModelCookTask motionRepair;Check(foundMotion&&Cook(motionRecipe,motionRepair)&&Hash(motionFirst.output)==motionHash,"corrupt motion cache safely reconstructs unchanged authored clips");
  MaterialDefinition material;material.maps[0].embedded.pixels.resize(4096);material.maps[0].encodedImage.resize(1000);material.doubleSided=true;material.maps[0].uvSet=1;
  auto lean=MaterialSettings(material);Check(SerializeMaterial(lean)==SerializeMaterial(material)&&lean.maps[0].embedded.pixels.empty()&&lean.maps[0].encodedImage.empty(),"GPU settings retain meaning without image payload copies");
  Project project;Check(Project::CreateNew((out/"project").string(),"Closure test",project,error),error);
  auto assets=fs::path(project.AssetsDir());const std::string mesh="11111111111111111111111111111111",texture="22222222222222222222222222222222",prefab="33333333333333333333333333333333",shared="44444444444444444444444444444444",junk="55555555555555555555555555555555";
  auto asset=[&](const char* name,const std::string& id,AssetType type,const char* input){fs::copy_file(input,assets/name);Check(AssetDatabase::WriteMeta((assets/name).string()+kAssetMetaExtension,id,type,"owned",error),error);};
  asset("mesh.obj",mesh,AssetType::Mesh,"assets/models/beacon.obj");asset("texture.png",texture,AssetType::Texture,"assets/textures/beacon.png");asset("shared.png",shared,AssetType::Texture,"assets/textures/beacon.png");asset("unreferenced.png",junk,AssetType::Texture,"assets/textures/beacon.png");
  Scene prop;auto& object=prop.CreateObject("Prop");object.render=SceneRenderComponent{};object.render->meshAsset=mesh;object.render->textureAsset=texture;
  Check(SaveSceneToFile(prop,(assets/"prop.judasprefab").string(),error)&&AssetDatabase::WriteMeta((assets/"prop.judasprefab").string()+kAssetMetaExtension,prefab,AssetType::Prefab,"owned",error),error);
  project.Settings().startupScene="Scenes/start.judas";Scene main;main.CreateObject("root");Check(SaveSceneToFile(main,project.StartupScenePath(),error),error);
  ProjectExportOptions exportOptions;exportOptions.runtimeExecutable=fs::absolute(argv[2]).string();exportOptions.destination=(out/"all").string();ProjectExportResult result;
  Check(ExportProject(project,exportOptions,result,error)&&result.assetCount==5,"legacy default retains all dynamic registered assets: "+error);
  project.Settings().exportAssetPolicy="closure";project.Settings().runtimeAssets={prefab,shared};Check(project.Save(error),error);
  ProjectSettings parsed;std::string named;Check(LegacyToNamed(Project::SerializeToString(project.Settings()),"project",named,error)&&Project::ParseFromString(named,parsed,error)&&parsed.runtimeAssets==project.Settings().runtimeAssets&&parsed.exportAssetPolicy=="closure","named/legacy export roots round trip");
  exportOptions.destination=(out/"closure").string();Check(ExportProject(project,exportOptions,result,error)&&result.assetCount==4,"explicit dynamic/save root transitive closure: "+error);
  Check(!fs::exists(out/"closure/Assets/unreferenced.png")&&fs::exists(out/"closure/Assets/prop.judasprefab"),"eligible unreferenced asset excluded, dynamic prefab retained");
  Check(result.deduplicatedBytes>0&&fs::equivalent(out/"closure/Assets/texture.png",out/"closure/Assets/shared.png"),"identical immutable bytes share physical storage with distinct asset IDs");
  AssetDatabase before,after;before.Scan(project.RootDir(),project.AssetsDir());after.Scan((out/"closure").string(),(out/"closure/Assets").string());
  Check(before.Find(texture)&&after.Find(texture)&&after.Find(shared)&&after.Find(texture)->id!=after.Find(shared)->id,"dedup preserves logical identity");
  const auto acceptedReport=Read(out/"closure/DEPENDENCIES.json");
  {std::string sourceIdentity,packageIdentity;Project packaged;
   Check(packaged.Load((out/"closure/game.judasproj").string(),error)&&ComputeSaveContentFingerprint(project,before,{"Scenes/start.judas"},sourceIdentity,error)&&ComputeSaveContentFingerprint(packaged,after,{"Scenes/start.judas"},packageIdentity,error)&&sourceIdentity==packageIdentity,"pruned closure and packaging settings preserve source save content identity: "+error);
   auto legacySettings=project.Settings();legacySettings.saveIdentity.clear();legacySettings.exportAssetPolicy="all";legacySettings.runtimeAssets.clear();
   std::string legacyBytes="Judas.SaveContent.1:"+Project::SerializeToString(legacySettings);legacyBytes+="Scenes/start.judas:"+Hash(project.StartupScenePath());
   for(const auto& [id,record]:before.Records())legacyBytes+=id+":"+AssetTypeName(record.type)+":"+Hash(record.path);
   Check(sourceIdentity==SceneFingerprintSha256(legacyBytes),"independent pre-M68 content formula remains byte-compatible");
   std::ofstream(out/"closure/Assets/mesh.obj",std::ios::app)<<"\n# changed content\n";
   Check(ComputeSaveContentFingerprint(packaged,after,{"Scenes/start.judas"},packageIdentity,error)&&packageIdentity!=sourceIdentity,"packaged file edits remain visible despite inclusion report hashes");
   auto changed=Read(out/"closure/DEPENDENCIES.json");changed["excluded"][0]["sha256"]="bad";Write(out/"closure/DEPENDENCIES.json",changed);
   Check(!ComputeSaveContentFingerprint(packaged,after,{"Scenes/start.judas"},packageIdentity,error),"malformed excluded identity cannot silently pass save compatibility");}
  {ResourceManager packaged(nullptr,&after);RuntimeWorld world;Check(world.Build(main,&packaged,error),error);SceneTransform placement;
   auto a=world.SpawnPrefab(prefab,placement,error);placement.position={4,0,0};auto b=world.SpawnPrefab(prefab,placement,error);
   Check(a&&b&&a!=b,"deduplicated package spawns independent mutable prefab instances");
   auto changed=world.RuntimeDefinition(a)->transform;changed.position={8,0,0};
   Check(world.SetRuntimeTransform(a,changed)&&world.RuntimeDefinition(b)->transform.position==placement.position,"mutating one packaged instance does not couple shared resources");}
  Check(ExportProject(project,exportOptions,result,error)&&Read(out/"closure/DEPENDENCIES.json")==acceptedReport,"repeat export dependency report deterministic after deliberate corruption");
  fs::remove(assets/"texture.png");Check(!ExportProject(project,exportOptions,result,error)&&fs::exists(out/"closure/Assets/texture.png"),"missing transitive asset leaves previous valid package intact");fs::copy_file("assets/textures/beacon.png",assets/"texture.png");
  const std::string external="66666666666666666666666666666666";
  // Runtime glTF compatibility is deliberately self-contained. External source
  // dependencies are cooked into the archive by M66, not imported by a game.
  asset("embedded.gltf",external,AssetType::Mesh,"tests/fixtures/m66/multipart.gltf");
  project.Settings().runtimeAssets.push_back(external);exportOptions.destination=(out/"embedded-closure").string();
  const bool embeddedExport=ExportProject(project,exportOptions,result,error);
  Check(embeddedExport&&fs::exists(out/"embedded-closure/Assets/embedded.gltf"),"self-contained runtime glTF retained through closure: "+error);
  fs::copy_file("tests/fixtures/m66/external.gltf",assets/"embedded.gltf",fs::copy_options::overwrite_existing);
  Check(!ExportProject(project,exportOptions,result,error)&&error.find("Dependency "+external)!=std::string::npos&&error.size()>50&&fs::exists(out/"embedded-closure/Assets/embedded.gltf"),"uncooked external source fails with detail and retains last-good package");
  fs::copy_file("tests/fixtures/m66/multipart.gltf",assets/"embedded.gltf",fs::copy_options::overwrite_existing);
  // Exercise deferred admission, not just three submitted tiny requests.
  std::vector<AssetId> burst;
  for(int i=0;i<12;++i){std::string id=std::string(30,'a')+"0123456789abcdef"[i]+"0";auto file=assets/("burst-"+std::to_string(i)+".png");fs::copy_file("assets/textures/beacon.png",file);Check(AssetDatabase::WriteMeta(file.string()+kAssetMetaExtension,id,AssetType::Texture,"owned",error),error);burst.push_back(id);}
  before.Scan(project.RootDir(),project.AssetsDir());
  JobSystem jobs(2);ResourceManager resources(nullptr,&before,&jobs);resources.SetHeadlessResidency(true);resources.SetBudgetBytes(1000000);
  for(auto& id:{texture,shared,junk}){resources.AddRef(id);resources.RequestTexture(id);}Check(resources.Stats().admittedRequests<=3,"bounded resource admission");resources.ReleaseRef(shared);resources.WaitForAll();Check(resources.StateOf(texture)==ResourceState::Ready&&resources.StateOf(junk)==ResourceState::Ready&&resources.StateOf(shared)!=ResourceState::Ready,"cancel retires only own shared-payload request");for(auto& id:burst){resources.AddRef(id);resources.RequestTexture(id);}Check(resources.Stats().loading==12&&resources.Stats().admittedRequests<=3,"burst is queued behind bounded decode admission");
  resources.ReleaseRef(burst[8]);resources.Invalidate(burst[10]);resources.RequestTexture(burst[10]);resources.WaitForAll();
  bool ready=true;for(size_t i=0;i<burst.size();++i)ready &= i==8?resources.StateOf(burst[i])!=ResourceState::Ready:resources.StateOf(burst[i])==ResourceState::Ready;Check(ready,"deferred cancellation and generation replacement make progress safely");
  resources.ReleaseAll();Check(resources.Stats().loading==0&&resources.Stats().bytesResident==0,"resource shutdown clears admitted and deferred work");
  Scene workshop;Check(LoadSceneFromFile("projects/world_workshop/Scenes/workshop.judas",workshop,error),error);EditorDocument doc;doc.GetScene()=workshop;doc.Select(workshop.Objects().back().id);auto initial=doc.GetScene().Find(doc.Selected())->transform.position;
  auto historyStart=std::chrono::steady_clock::now();
  for(int i=0;i<200;++i)Check(doc.BatchTransform({.01f,0,0},{1,0,0,0},{1,1,1},error,true,true),"one action retains one undo");
  for(int i=0;i<200;++i)doc.Undo();
  Check(!doc.CanUndo()&&glm::length(doc.GetScene().Find(doc.Selected())->transform.position-initial)<1e-5f,"all 200 exact history revisions retained");for(int i=0;i<200;++i)doc.Redo();Check(!doc.CanRedo(),"all 200 revisions redo independently");
  struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
  std::ofstream(out/"results.json")<<J{{"checks",checks},{"failures",0},{"schema",1},{"historyObjects",workshop.Objects().size()},{"historyRevisions",200},{"historyEditUndoRedoMs",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-historyStart).count()},{"peakRSSKiB",usage.ru_maxrss}}.dump(2)<<'\n';std::cout<<"SUMMARY "<<checks<<" checks\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
}
