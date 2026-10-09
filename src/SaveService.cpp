#include "SaveService.h"
#include "SceneSession.h"
#include "WorldPersistence.h"
#include "WorldStreaming.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "InteractivePlay.h"
#include "SceneFingerprint.h"
#include "GamePackage.h"
#include "../third_party/nlohmann/json.hpp"
#include "PerformanceProfiler.h"
#include <filesystem>
#include <chrono>
#include <atomic>
#include <fstream>
#include <stdexcept>
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
std::atomic<uint64_t> nextRequest{1};
}
bool ComputeSaveContentFingerprint(const Project& project,const AssetDatabase& assets,
 const std::vector<std::string>& scenes,std::string& result,std::string& error,const JobContext* cancel){
 try{
  auto settings=project.Settings();settings.saveIdentity.clear();settings.exportAssetPolicy="all";settings.runtimeAssets.clear();
  std::string bytes="Judas.SaveContent.1:"+Project::SerializeToString(settings),digest;
  for(const auto& scene:scenes){if(cancel&&cancel->CancelRequested())return false;if(!SceneFingerprintSha256File(project.Resolve(scene),digest,error))return false;bytes+=scene+":"+digest;}
  std::map<AssetId,std::pair<std::string,std::string>> records;
  for(const auto& [id,a]:assets.Records()){
   if(cancel&&cancel->CancelRequested())return false;
   if(a.missing||!SceneFingerprintSha256File(a.path,digest,error)){error="save content dependency "+id+": "+error;return false;}
   records.emplace(id,std::make_pair(AssetTypeName(a.type),digest));
  }
  const auto report=std::filesystem::path(project.RootDir())/"DEPENDENCIES.json";
  if(std::filesystem::exists(std::filesystem::path(project.RootDir())/kGamePackageMarker)&&std::filesystem::exists(report)){
   GamePackage marker;if(!ReadGamePackage(project.RootDir(),marker,error))return false;
   if(marker.projectFile!=std::filesystem::path(project.ProjectFile()).filename())throw std::runtime_error("package project differs from save identity source");
   if(std::filesystem::file_size(report)>32*1024*1024)throw std::runtime_error("package dependency report exceeds 32 MiB");
   std::ifstream file(report);auto data=nlohmann::json::parse(file,[](int depth,nlohmann::json::parse_event_t,const nlohmann::json&){if(depth>48)throw std::runtime_error("package dependency report nesting bound");return true;});
   if(data.at("version")!=1||!data.at("excluded").is_array()||data.at("excluded").size()>65536)throw std::runtime_error("invalid package exclusion report");
   for(const auto& entry:data.at("excluded")){
    auto id=entry.at("id").get<std::string>(),type=entry.at("type").get<std::string>(),hash=entry.at("sha256").get<std::string>();bool known=false;
    for(int i=0;i<=int(AssetType::PhysicalMaterial);++i)known|=type==AssetTypeName(AssetType(i));
    if(!IsValidAssetId(id)||!known||hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=std::string::npos||!records.emplace(id,std::make_pair(type,hash)).second)throw std::runtime_error("invalid/duplicate package excluded asset identity");
   }
  }
  for(const auto& [id,record]:records)bytes+=id+":"+record.first+":"+record.second;
  result=SceneFingerprintSha256(bytes);error.clear();return true;
 }catch(const std::exception& e){error=std::string("save content: ")+e.what();return false;}
}
struct SaveService::Product {GameSnapshot snapshot;std::vector<SaveSlotInfo> slots;std::string error,digest;bool recovered=false;double elapsed=0;size_t bytes=0;};
SaveService::SaveService(SceneSession& session,ResourceManager& resources,bool editor):m_session(session),m_resources(resources){
 auto project=session.m_project;auto settings=project.Settings();m_identity=settings.saveIdentity.empty()?SceneFingerprintSha256(settings.name+"\n"+std::filesystem::path(project.ProjectFile()).filename().string()):settings.saveIdentity;
 GamePackage package{"",m_identity};std::string root;if(!PackageSaveDirectory(package,root,m_identityError))return;
 m_root=root+(editor?"/EditorSlots":"/Slots");
 RefreshContent();
}
void SaveService::RefreshContent(){
 m_content.clear();m_identityError.clear();
 auto project=m_session.m_project;auto assets=*m_resources.Assets();auto scenes=m_session.Scenes();
 auto product=m_identityProduct=std::make_shared<Product>();
 m_identityJob=m_resources.Jobs()->Submit([product,project,assets,scenes](JobContext& ctx){
  ComputeSaveContentFingerprint(project,assets,scenes,product->digest,product->error,&ctx);
  if(ctx.CancelRequested())ctx.ReportCancelled();
 },JobPriority::Normal,"save project identity");
}
SaveService::~SaveService(){if(auto* jobs=m_resources.Jobs()){jobs->Cancel(m_job);jobs->Cancel(m_identityJob);jobs->Forget(m_job);jobs->Forget(m_identityJob);}}
uint64_t SaveService::Request(const std::string& operation,const std::string& slot,const std::string& name,const std::string& metadata,std::string& error){
 if(Busy()||m_session.Pending()||!m_session.Accepting()){error="save/scene service busy";return 0;}
 if(operation!="save"&&operation!="load"&&operation!="delete"&&operation!="list"){error="invalid save operation";return 0;}
 if(operation!="list"&&!SaveStorage::ValidSlot(slot)){error="invalid slot ID";return 0;}
 if(name.size()>1024||!ScriptSystem::ValidateJson(metadata,error)){if(error.empty())error="invalid slot display name/metadata";return 0;}
 if(!m_identityError.empty()){error=m_identityError;return 0;}
 if(!m_identityJob.IsValid())RefreshContent();
 auto id=nextRequest.fetch_add(1);m_restoreStart=Clock::now();m_active=id;m_name=name;m_metadata=metadata;m_requests[id]={id,operation,slot,"queued",""};while(m_requests.size()>32)m_requests.erase(m_requests.begin());return id;
}
bool SaveService::Cancel(uint64_t id){if(id!=m_active||!id)return false;auto& s=m_requests.at(id);if(s.state!="queued"&&s.state!="preparing")return false;m_staged.reset();m_stagedSession.reset();s.state="cancelled";m_active=0;return true;}
const SaveRequestStatus* SaveService::Status(uint64_t id)const{auto it=m_requests.find(id);return it==m_requests.end()?nullptr:&it->second;}
void SaveService::Finish(const std::string& error){auto& s=m_requests.at(m_active);s.error=error;s.state=error.empty()?"completed":"failed";m_staged.reset();m_stagedSession.reset();m_product.reset();m_active=0;}
void SaveService::Advance(std::unique_ptr<RuntimeWorld>& world,InteractivePlay& play,std::string& error){
 auto* jobs=m_resources.Jobs();if(m_identityJob.IsValid()&&jobs->IsFinished(m_identityJob)){m_content=m_identityProduct->digest;m_identityError=m_identityProduct->error;if(m_content.empty()&&m_identityError.empty())m_identityError="save content identity failed";jobs->Forget(m_identityJob);m_identityJob={};m_identityProduct.reset();}
 if(!m_active)return;
 auto& s=m_requests.at(m_active);
 if(s.state=="queued"){
  if(!m_identityError.empty()){error=m_identityError;Finish(error);return;}if(m_content.empty())return;
  GameSnapshot snapshot;auto start=Clock::now();
  if(s.operation=="save"){
   if(m_session.m_project.Settings().legacyGameplay){error="modern slots require legacy-gameplay false; built-in legacy player/control state uses the separate .judasstate contract";Finish(error);return;}
   if(m_session.m_streaming&&!m_session.m_streaming->SaveReady())return;
   if(!WorldPersistence::Prepare(*world,error)){if(error=="loading"&&ms(m_restoreStart)<=30000){error.clear();return;}if(error=="loading")error="required save participants failed to prepare within 30 seconds";Finish(error);return;}
   snapshot.project=m_identity;snapshot.scene=m_session.Current();snapshot.content=m_content;snapshot.displayName=m_name;snapshot.metadata=m_metadata;snapshot.timestamp=uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
   if(!WorldPersistence::Capture(*world,snapshot.participants,error)){Finish(error);return;}
   try{SaveArchive a;auto values=m_session.m_values;auto locale=m_session.Localization(&m_resources).Locale();a(values,locale);snapshot.participants["session"]={1,std::move(a.bytes)};
    SaveArchive stream;unsigned streamVersion=1;bool composed=m_session.ComposedProject();stream(composed);if(composed){std::string e;auto* coordinator=m_session.Streaming(*world,e);if(!coordinator)throw std::runtime_error(e);streamVersion=coordinator->ArchiveVersion();coordinator->Persist(stream,streamVersion);}snapshot.participants["streaming"]={streamVersion,std::move(stream.bytes)};
   }catch(const std::exception& e){error=e.what();Finish(error);return;}
   s.captureMs=ms(start);
  }
  size_t snapshotBytes=0;for(const auto& [_,chunk]:snapshot.participants)snapshotBytes+=chunk.data.size()+128;if(snapshotBytes>8*1024*1024){error="snapshot exceeds 8 MiB payload limit";Finish(error);return;}JUDAS_PROFILE_COUNTER("Save snapshot bytes",double(snapshotBytes),ProfileCounterMode::Latest);
  auto product=m_product=std::make_shared<Product>();product->snapshot=std::move(snapshot);auto root=m_root,id=s.slot,operation=s.operation,identity=m_identity,content=m_content;
  s.state=operation=="load"?"reading":operation=="save"?"writing":"working";
  m_job=jobs->Submit([product,root,id,operation,identity,content](JobContext&){auto start=Clock::now();SaveStorage storage(root);bool ok=true;
   if(operation=="save"){product->bytes=EncodeGameSnapshot(product->snapshot).size();ok=storage.Write(id,product->snapshot,product->error);}
   else if(operation=="load"){ok=storage.Read(id,product->snapshot,product->recovered,product->error);if(ok)product->bytes=EncodeGameSnapshot(product->snapshot).size();}
   else if(operation=="delete")ok=storage.Delete(id,product->error);
   if(ok){std::string listingError;product->slots=storage.List(identity,content,listingError);if(!listingError.empty())product->error=(operation=="save"?"save committed, metadata refresh failed: ":"")+listingError;}
   product->elapsed=ms(start);
  },JobPriority::Normal,"save slot "+operation);return;
 }
 if(m_job.IsValid()){
  if(!jobs->IsFinished(m_job))return;
  auto jobError=jobs->ErrorOf(m_job);jobs->Forget(m_job);m_job={};if(m_product->error.empty())m_product->error=jobError;
  s.workerMs=m_product->elapsed;s.bytes=m_product->bytes;s.recovered=m_product->recovered;m_slots=m_product->slots;
  if(!m_product->error.empty()){error=m_product->error;Finish(error);return;}
  if(s.operation!="load"){Finish("");return;}
  auto& saved=m_product->snapshot;
  if(saved.project!=m_identity||saved.content!=m_content||std::find(m_session.Scenes().begin(),m_session.Scenes().end(),saved.scene)==m_session.Scenes().end()){error="save incompatible with registered project/content";Finish(error);return;}
  if(saved.gameVersion!=1||(saved.participants.size()<11||saved.participants.size()>16)){error="unsupported save game/participant schema";Finish(error);return;}
  Scene scene;if(!WorldPersistence::SceneFrom(saved.participants,scene,error)){Finish(error);return;}
  const bool hasDeformable=std::any_of(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){return bool(o.deformable);});
  const bool hasKinematic=std::any_of(scene.Objects().begin(),scene.Objects().end(),[](const auto& o){return o.body&&o.body->motion==SceneBodyMotion::Kinematic;});
  if(bool(saved.participants.count("kinematic-motion"))!=hasKinematic){error="kinematic motion participant does not match snapshot components";Finish(error);return;}
  if(saved.participants.size()!=size_t(11+hasDeformable+hasKinematic+saved.participants.count("sleep")+saved.participants.count("view-projection")+saved.participants.count("physical-animation"))||bool(saved.participants.count("deformables"))!=hasDeformable){error="deformable participant does not match snapshot components";Finish(error);return;}
  m_restoreStart=Clock::now();m_stagedSession=std::make_shared<SceneSession>(m_session.m_project,saved.scene);m_staged=std::make_unique<RuntimeWorld>();m_staged->SetSceneControl(m_stagedSession);m_staged->audioGroups=m_session.m_project.Settings().audio;m_staged->legacyGameplay=m_session.m_project.Settings().legacyGameplay;
  if(!m_staged->Build(scene,&m_resources,error,&m_session.m_project.Settings().classification,&m_session.m_project.Settings().navigation,true)){Finish(error);return;}s.state="preparing";
 }
 if(s.state!="preparing"&&s.state!="restoring")return;
 if(ms(m_restoreStart)>30000){error="save required resources failed to prepare within 30 seconds";Finish(error);return;}
 if(s.state=="preparing"){
 JUDAS_PROFILE_SCOPE("Save private preparation");
 if(!WorldPersistence::Prepare(*m_staged,error)){if(error=="loading"){error.clear();return;}Finish(error);return;}
 auto& saved=m_product->snapshot;
 if(!WorldPersistence::Restore(*m_staged,saved.participants,error)){Finish(error);return;}
 try{
  auto it=saved.participants.find("session");if(it==saved.participants.end()||it->second.version!=1)throw std::runtime_error("required session participant missing/newer");SaveArchive a(it->second.data);std::map<std::string,std::string> values;std::string locale;a(values,locale);a.Finish();for(auto& [key,json]:values)if(!m_stagedSession->Set(key,json,error))throw std::runtime_error(error);if(!m_stagedSession->Localization(&m_resources).Configuration().locales.empty()&&!m_stagedSession->Localization(&m_resources).SetLocale(locale,error))throw std::runtime_error(error);
  it=saved.participants.find("streaming");if(it==saved.participants.end()||(it->second.version<1||it->second.version>4))throw std::runtime_error("required streaming participant missing/newer");SaveArchive b(it->second.data);bool composed=false;b(composed);if(composed!=m_session.ComposedProject())throw std::runtime_error("saved composition mismatch");if(composed){auto* stream=m_stagedSession->Streaming(*m_staged,error);if(!stream)throw std::runtime_error(error);stream->Persist(b,it->second.version);}b.Finish();
 }catch(const std::exception& e){error=e.what();Finish(error);return;}
 s.state="restoring";
 }
 auto& localeControl=m_stagedSession->Localization(&m_resources);localeControl.Refresh();
 if(!localeControl.Configuration().locales.empty()){auto sessionIt=m_product->snapshot.participants.find("session");SaveArchive localeData(sessionIt->second.data);std::map<std::string,std::string> values;std::string wanted;localeData(values,wanted);if(localeControl.Locale()!=wanted)return;}
 if(!WorldPersistence::PrepareAudio(*m_staged,error)){if(error=="loading"){error.clear();return;}Finish(error);return;}
 auto& saved=m_product->snapshot;
 GameSession validation;if(!validation.Begin(*m_staged,error)){Finish(error);return;}validation.End();
 // Everything fallible above operated on a private world. The validated normal
 // session is published once; no legacy authored delta is overlaid on it.
 JUDAS_PROFILE_SCOPE("Save activation");
 auto control=world->SceneControl();m_session.m_accepting=false;play.End();m_session.m_streaming.reset();m_session.m_streamWorld=nullptr;
 world=std::move(m_staged);m_session.m_values=std::move(m_stagedSession->m_values);std::string locale=m_stagedSession->Localization(&m_resources).Locale();if(!m_session.Localization(&m_resources).Configuration().locales.empty())m_session.Localization(&m_resources).SetLocale(locale,error);
 m_session.m_streaming=std::move(m_stagedSession->m_streaming);m_session.m_streamWorld=world.get();m_session.m_current=saved.scene;m_session.m_accepting=true;if(m_session.m_streaming)m_session.m_streaming->ResumeOwnership();world->SetSceneControl(std::move(control));
 if(!play.Begin(*world,WorldCoordinates(world->Settings().worldOrigin),error)){error="published save session start failed: "+error;Finish(error);return;}play.SetWorldStatePath("",false);WorldPersistence::Published(*world);
 s.restoreMs=ms(m_restoreStart);Finish("");
}
