#include "RuntimeTerrain.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "CollisionAsset.h"
#include "SceneFingerprint.h"
#include "SaveArchive.h"
#include "WorldPresentation.h"
#include <algorithm>
#include <chrono>
#include <cmath>
namespace {
using Clock=std::chrono::steady_clock;
double Milliseconds(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
constexpr size_t MaxSamples=32768,MaxVolumes=8,TotalSamples=262144;
}
RuntimeTerrain::RuntimeTerrain(RuntimeWorld& w):world(w){}
RuntimeTerrain::~RuntimeTerrain(){while(!records.empty())Remove(records.begin()->first);}
bool RuntimeTerrain::Has(uint64_t id)const{auto i=records.find(id);return i!=records.end()&&bool(i->second.committed);}
const RuntimeTerrainStatus* RuntimeTerrain::Status(uint64_t id)const{auto i=records.find(id);return i==records.end()?nullptr:&i->second.status;}
bool RuntimeTerrain::ReadyToSave()const{for(auto& [_,r]:records)if(r.pending)return false;return true;}
std::vector<uint64_t> RuntimeTerrain::Owners()const{std::vector<uint64_t> out;for(auto& [id,r]:records)if(r.committed)out.push_back(id);return out;}
bool RuntimeTerrain::Prepare(Product& p,const ProjectNavigation& config,const JobContext* context){
 auto start=Clock::now();auto cancel=[&]{return context&&context->CancelRequested();};
 if(!BuildVolumeTerrainMesh(p.source,p.mesh,p.error,cancel))return false;
 if(p.mesh.indices.size()/3>16384){p.error="runtime terrain exceeds 16384 triangles; committed geometry retained";return false;}
 p.collision=std::make_shared<CollisionAsset>();CollisionCookSettings cook;
 if(cancel()||!CookCollision(p.mesh,cook,*p.collision,p.error))return false;
 p.collision->sourceAsset=p.source.meshId;
 p.collision->sourceFingerprint=SceneFingerprintSha256(SerializeVolumeTerrain(p.source));
 p.collision->settingsFingerprint=CollisionSettingsFingerprint(cook);
 if(p.navigated){
  NavigationGeometry geometry;for(const auto& v:p.mesh.vertices)geometry.vertices.insert(geometry.vertices.end(),{v.position.x,v.position.y,v.position.z});
  for(auto i:p.mesh.indices)geometry.triangles.push_back(int(i));
  geometry.fingerprint=p.collision->sourceFingerprint;
  NavigationSurfaceSettings settings;settings.halfExtents=p.source.extent*.5f+glm::vec3(1);settings.profile=p.profile;
  settings.cellSize=std::max(.2f,std::max({p.source.extent.x,p.source.extent.y,p.source.extent.z})/128.f);settings.minRegion=0;
  p.navigation=std::make_shared<NavigationData>();
  if(cancel()||!BakeNavigationGeometry(geometry,settings,config.profiles.at(p.profile),*p.navigation,p.error))return false;
 }
 p.ms=Milliseconds(start);return !cancel();
}
bool RuntimeTerrain::Submit(uint64_t id,VolumeTerrainSource source,bool navigation,unsigned profile,std::string& error,bool privateOwner){
 const auto definition=world.m_scriptDefinitions.find(id);
 const auto* d=privateOwner&&definition!=world.m_scriptDefinitions.end()?&definition->second:world.RuntimeDefinition(id);auto* resources=world.Resources();
 const auto category=world.m_entityCategories.find(id);
 const auto body=privateOwner&&category!=world.m_entityCategories.end()?category->second.body:world.RuntimeBody(id);
 if(!d||!d->body||d->body->motion!=SceneBodyMotion::Static||d->deformable||d->planet||d->navigationSurface||d->parent||d->transform.scale!=glm::vec3(1)||!body.IsValid()){
  error="terrain requires a root, unit-scale static body without another geometry/navigation owner";return false;}
 if(!resources||!resources->Jobs()||!world.Navigation().Configuration().profiles.count(profile)){error="terrain requires jobs and a registered navigation profile";return false;}
 if(!ValidateVolumeTerrain(source,error)||source.density.size()>MaxSamples||std::max({source.extent.x,source.extent.y,source.extent.z})>64){if(error.empty())error="runtime terrain limits: 32768 samples, 64 metres per axis";return false;}
 size_t samples=source.density.size();for(auto& [owner,r]:records){if(r.pending){error="terrain preparation busy; retry after publication";return false;}if(owner!=id&&r.committed)samples+=r.committed->source.density.size();}
 if((!records.count(id)&&records.size()>=MaxVolumes)||samples>TotalSamples){error="runtime terrain world budget exceeded";return false;}
 auto product=std::make_shared<Product>();product->source=std::move(source);product->navigated=navigation;product->profile=profile;
 auto config=world.Navigation().Configuration();auto job=resources->Jobs()->Submit([product,config](JobContext& context){if(!Prepare(*product,config,&context)){if(context.CancelRequested())context.ReportCancelled();else context.SetError(product->error);}},JobPriority::Normal,"runtime terrain geometry");
 if(!job.IsValid()){error="terrain job queue unavailable";return false;}
 auto& record=records[id];record.pending=product;record.privateOwner=privateOwner;record.job=job;record.status.busy=true;record.status.error.clear();return true;
}
bool RuntimeTerrain::Create(uint64_t id,VolumeTerrainSource source,bool navigation,unsigned profile,std::string& error){
 if(auto i=records.find(id);i!=records.end()){if(i->second.committed||i->second.pending){error="terrain already initialized; use brush";return false;}records.erase(i);}
 return Submit(id,std::move(source),navigation,profile,error);
}
bool RuntimeTerrain::Brush(uint64_t id,glm::vec3 point,const VolumeBrush& brush,std::string& error){
 auto i=records.find(id);auto* d=world.RuntimeDefinition(id);
 if(i==records.end()||!i->second.committed||!d){error="terrain not ready";return false;}
 auto source=i->second.committed->source;
 auto local=glm::inverse(d->transform.rotation)*(point-d->transform.position);
 if(!ApplyVolumeBrush(source,local,brush,error))return false;
 return Submit(id,std::move(source),i->second.committed->navigated,i->second.committed->profile,error);
}
bool RuntimeTerrain::Publish(uint64_t id,Record& record,std::shared_ptr<Product> product,std::string& error,bool privateOwner){
 auto start=Clock::now();auto definition=world.m_scriptDefinitions.find(id);auto* d=privateOwner&&definition!=world.m_scriptDefinitions.end()?&definition->second:world.RuntimeDefinition(id);auto category=world.m_entityCategories.find(id);auto body=privateOwner&&category!=world.m_entityCategories.end()?category->second.body:world.RuntimeBody(id);if(!d||!body.IsValid()){error="terrain owner disappeared";return false;}
 // Stage native navigation first. Failure leaves all current geometry intact.
 if(product->navigation&&!world.Navigation().LoadSurface(id,d->transform,product->navigation,error,false))return false;
 if(!world.Physics().ReplaceStaticCollision(body,product->collision)){world.Navigation().DiscardStagedSurface(id);error="terrain collision publication rejected";return false;}
 if(product->navigation&&world.IsPublished(id))world.Navigation().PublishSurfaces({id});
 record.committed=std::move(product);record.status.ready=true;record.status.navigation=record.committed->navigated;++record.status.revision;
 record.status.samples=record.committed->source.density.size();record.status.triangles=record.committed->mesh.indices.size()/3;
 record.status.bytes=record.status.samples*sizeof(float)+record.committed->mesh.vertices.size()*sizeof(MeshVertex)+record.committed->mesh.indices.size()*sizeof(unsigned);
 record.status.prepareMs=record.committed->ms;record.status.publishMs=Milliseconds(start);record.status.error.clear();return true;
}
void RuntimeTerrain::Advance(){
 auto* resources=world.Resources();if(!resources||!resources->Jobs())return;
 for(auto& [id,r]:records)if(r.pending&&resources->Jobs()->IsFinished(r.job)){
  auto state=resources->Jobs()->StateOf(r.job);std::string error;
  if(state!=JobState::Completed)error=resources->Jobs()->ErrorOf(r.job);
  else if(!Publish(id,r,r.pending,error,r.privateOwner)){}
  resources->Jobs()->Forget(r.job);r.job={};r.pending.reset();r.status.busy=false;r.status.error=error;
 }
}
void RuntimeTerrain::Remove(uint64_t id){auto it=records.find(id);if(it==records.end())return;auto& r=it->second;
 if(r.job.IsValid()&&world.Resources()&&world.Resources()->Jobs()){world.Resources()->Jobs()->Cancel(r.job);world.Resources()->Jobs()->Forget(r.job);}
 if(world.Resources()&&world.Resources()->GetRenderer()&&r.mesh.IsValid())world.Resources()->GetRenderer()->DestroyMesh(r.mesh);
 if(r.committed&&r.committed->navigated)world.Navigation().RemoveSurface(id);
 records.erase(it);
}
void RuntimeTerrain::Draw(Renderer& renderer)const{
 for(auto& [id,r]:records)if(r.committed&&world.IsPublished(id)&&world.RenderVisible(id)){
  auto* d=world.RuntimeDefinition(id);if(!d||!d->render)continue;
  if(r.drawnRevision!=r.status.revision){if(r.mesh.IsValid())renderer.DestroyMesh(r.mesh);r.mesh=renderer.CreateMesh(r.committed->mesh);r.drawnRevision=r.status.revision;}
  renderer.SetRenderLayer(world.RenderLayerOf(id));renderer.SetMaterialBindings(BuildRenderMaterialBindings(world.Resources(),*d->render));
  auto texture=world.Resources()?world.Resources()->TryGetTexture(d->render->textureAsset):TextureHandle{};
  renderer.DrawMesh(r.mesh,d->transform.position,d->transform.rotation,glm::vec3(1),texture,d->render->color,d->render->alpha);
 }
}
bool RuntimeTerrain::Capture(uint64_t id,std::string& out,std::string& error)const{
 out.clear();auto i=records.find(id);if(i==records.end())return true;if(i->second.pending){error="terrain edit pending";return false;}if(!i->second.committed)return true;
 auto& p=*i->second.committed;SaveArchive archive;auto source=SerializeVolumeTerrain(p.source);bool nav=p.navigated;unsigned profile=p.profile;archive(source,nav,profile);out.assign(archive.bytes.begin(),archive.bytes.end());return true;
}
bool RuntimeTerrain::Restore(uint64_t id,const std::string& bytes,std::string& error,bool asynchronous){
 if(bytes.empty())return true;
 try{SaveArchive archive(bytes);std::string source;bool nav=false;unsigned profile=0;archive(source,nav,profile);archive.Finish();
  VolumeTerrainSource parsed;if(!ParseVolumeTerrain(source,parsed,error))return false;
  const bool privateOwner=world.m_regionPending.count(id)!=0;
  if(!Submit(id,std::move(parsed),nav,profile,error,privateOwner))return false;
  if(asynchronous)return true;
  auto& r=records.at(id);world.Resources()->Jobs()->Wait(r.job);auto state=world.Resources()->Jobs()->StateOf(r.job);auto product=r.pending;
  world.Resources()->Jobs()->Forget(r.job);r.job={};r.pending.reset();r.status.busy=false;
  if(state!=JobState::Completed){error=product->error;return false;}return Publish(id,r,product,error,privateOwner);
 }catch(const std::exception& e){error=e.what();return false;}
}
