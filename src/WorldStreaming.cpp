#include "NamedAuthoring.h"
#include "WorldPersistence.h"
#include "ScriptSystem.h"
#include "WorldStreaming.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "SceneSerialization.h"
#include "SceneFingerprint.h"
#include "Prefab.h"
#include "NavigationAsset.h"
#include "PerformanceProfiler.h"
#include "SaveArchive.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <cmath>
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
bool finite(glm::dvec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool read(const std::string& path,std::string& text,std::string& error,size_t cap=8*1024*1024){std::ifstream f(path,std::ios::binary|std::ios::ate);auto size=f.tellg();if(!f||size<0||size>std::streamoff(cap)){error="missing/oversized world content: "+path;return false;}text.resize(size_t(size));f.seekg(0);if(!f.read(text.data(),size)){error="cannot read world content: "+path;return false;}return true;}
std::string orderKey(const WorldRegion& r,SceneObjectId local){std::ostringstream s;s<<"0:"<<std::setw(10)<<std::setfill('0')<<(1000000-r.priority)<<":"<<r.id<<":"<<std::setw(20)<<local;return s.str();}
void remap(SceneObject& o,const std::map<SceneObjectId,SceneObjectId>& ids){auto map=[&](SceneObjectId& id){if(id&&ids.count(id))id=ids.at(id);};for(auto& slot:o.scripts)slot.properties=ScriptSystem::RemapPropertyEntities(slot.properties,ids);map(o.id);map(o.parent);if(o.socket)map(o.socket->target);map(o.prefabRoot);if(o.render)map(o.render->textureCamera);if(o.joint){map(o.joint->bodyA);map(o.joint->bodyB);}if(o.deformable)for(auto& a:o.deformable->attachments)map(a.target);if(o.liquidConnection){map(o.liquidConnection->source);map(o.liquidConnection->destination);}for(auto& [source,id]:o.prefabIds){(void)source;map(id);}}
bool reference(SceneObject& o,const std::string& field,SceneObjectId id){if(field.rfind("script:",0)==0){auto split=field.find(':',7);if(split==std::string::npos)return false;uint64_t slot=0;try{slot=std::stoull(field.substr(7,split-7));}catch(...){return false;}for(auto& s:o.scripts)if(s.id==slot)return ScriptSystem::SetPropertyEntity(s.properties,field.substr(split+1),id);return false;}if(field.rfind("deformable:",0)==0&&o.deformable){for(auto& a:o.deformable->attachments)if(a.group==field.substr(11)){a.target=id;return true;}return false;}if(field=="socket"&&o.socket){o.socket->target=id;return true;}if(field=="parent"){o.parent=id;return true;}if(field=="textureCamera"&&o.render){o.render->textureCamera=id;return true;}if(field=="bodyA"&&o.joint){o.joint->bodyA=id;return true;}if(field=="bodyB"&&o.joint){o.joint->bodyB=id;return true;}if(field=="source"&&o.liquidConnection){o.liquidConnection->source=id;return true;}if(field=="destination"&&o.liquidConnection){o.liquidConnection->destination=id;return true;}return false;}
}
bool ParseWorldManifest(const std::string& text,WorldManifest& out,std::string& error){
    if(IsNamedDocument(text)){std::string legacy;if(!NamedToLegacy(text,"world",legacy,error))return false;return ParseWorldManifest(legacy,out,error);}
    if(text.size()>1024*1024||text.find('\0')!=std::string::npos){error="world manifest exceeds 1MiB or contains NUL";return false;}
    std::istringstream input(text);std::string line;WorldManifest m;unsigned n=0;bool header=false,budget=false;
    while(std::getline(input,line)){++n;std::istringstream s(line);s.imbue(std::locale::classic());std::string key;if(!(s>>key)||key[0]=='#')continue;
        auto bad=[&]{error="world manifest line "+std::to_string(n)+": invalid "+key;return false;};
        if(!header){int version;if(key!="JudasWorld"||!(s>>version)||version!=1)return bad();header=true;}
        else if(key=="budget"){if(budget||!(s>>m.installMilliseconds>>m.unitsPerFrame>>m.maxPreparing>>m.pendingBytes>>m.liveBytes>>m.retainedBytes>>m.resourceCacheBytes))return bad();budget=true;}
        else if(key=="region"){WorldRegion r;float x,y,z,w;std::string deps;
            if(!(s>>std::quoted(r.id)>>std::quoted(r.scene)>>r.origin.x>>r.origin.y>>r.origin.z>>x>>y>>z>>w>>r.halfExtents.x>>r.halfExtents.y>>r.halfExtents.z>>r.priority>>std::quoted(r.policy)>>r.estimatedBytes>>std::quoted(deps)))return bad();
            r.rotation={w,x,y,z};float norm=glm::dot(r.rotation,r.rotation);
            if(r.id.empty()||r.id.size()>128||r.id=="root"||!finite(r.origin)||!std::isfinite(norm)||std::abs(norm-1)>1e-4||r.halfExtents.x<=0||r.halfExtents.y<=0||r.halfExtents.z<=0||!finite(glm::dvec3(r.halfExtents))||(r.priority<-100000||r.priority>100000)||r.estimatedBytes<sizeof(SceneObject)||r.estimatedBytes>128*1024*1024||(r.policy!="snapshot"&&r.policy!="resident"))return bad();
            std::istringstream d(deps);std::string dep;while(d>>dep)r.dependencies.push_back(dep);
            if(m.regions.size()>=4096||!m.regions.emplace(r.id,r).second)return bad();
        }else if(key=="reference"){WorldReference r;std::string mode;if(!(s>>std::quoted(r.region)>>r.local>>std::quoted(r.field)>>std::quoted(r.target)>>r.targetLocal>>mode)||(mode!="hard"&&mode!="soft")||!r.local||!r.targetLocal)return bad();r.hard=mode=="hard";
            // Physical/hierarchy dependencies cannot disappear while active.
            if(!r.hard&&r.field!="textureCamera")return bad();
            m.references.push_back(r);
        }else return bad();
        s>>std::ws;if(!s.eof())return bad();
    }
    if(!header||m.installMilliseconds<=0||m.installMilliseconds>50||!m.unitsPerFrame||m.unitsPerFrame>1024||!m.maxPreparing||m.maxPreparing>16||m.pendingBytes<4096||m.liveBytes<4096||m.retainedBytes<4096){error="invalid world budget/header";return false;}
    for(auto& r:m.references){if(!m.regions.count(r.region)||!m.regions.count(r.target)){error="reference to unknown region";return false;}if(r.hard&&r.region!=r.target){auto& deps=m.regions.at(r.region).dependencies;if(std::find(deps.begin(),deps.end(),r.target)==deps.end())deps.push_back(r.target);}}
    std::set<std::string> visiting,done;std::function<bool(const std::string&)> visit=[&](const std::string& id){if(done.count(id))return true;if(!visiting.insert(id).second){error="cyclic world dependency at "+id;return false;}for(auto& d:m.regions.at(id).dependencies){if(!m.regions.count(d)){error="missing world dependency "+d;return false;}if(!visit(d))return false;}visiting.erase(id);done.insert(id);return true;};
    for(auto& [id,r]:m.regions)if(!visit(id))return false;
    out=std::move(m);return true;
}
bool LoadWorldManifest(const std::string& path,WorldManifest& out,std::string& error){std::string text;return read(path,text,error,1024*1024)&&ParseWorldManifest(text,out,error);}
bool ValidateWorldManifest(const WorldManifest& m,const Project& project,std::string& error){namespace fs=std::filesystem;if(project.Settings().legacyGameplay){error="World manifests require project-owned scripted control (legacy-gameplay false)";return false;}for(auto& [id,r]:m.regions){fs::path path(r.scene);auto real=fs::weakly_canonical(project.Resolve(r.scene));auto root=fs::weakly_canonical(project.ScenesDir());auto relative=real.lexically_relative(root);if(path.is_absolute()||path.extension()!=".judas"||relative.empty()||*relative.begin()==".."||!fs::is_regular_file(real)){error="region "+id+" source is not a registered project scene";return false;}}return true;}
bool PrepareWorldRegion(const WorldRegion& region,const Project& project,const AssetDatabase& assets,PreparedWorldRegion& out,std::string& error,const JobContext* context){
    auto cancelled=[&]{if(context&&context->CancelRequested()){error="cancelled";return true;}return false;};
    std::string text;if(cancelled()||!read(project.Resolve(region.scene),text,error))return false;
    Scene authored,resolved,flat;if(!LoadSceneFromString(text,authored,error)||!ResolvePrefabs(authored,&assets,resolved,error)||!FlattenHierarchy(resolved,flat,error))return false;
    if(flat.Objects().size()>4096){error="region exceeds 4096 registration units; split the authored region";return false;}
    if(!ValidateSceneClassification(flat,project.Settings().classification,error))return false;
    PreparedWorldRegion result;result.source=std::move(resolved);std::string canonical;
    if(!ComputeSceneFingerprint(result.source,canonical,error))return false;
    result.fingerprint=canonical;result.bytes=text.size()+result.source.Objects().size()*sizeof(SceneObject)*3;
    for(auto& o:flat.Objects()){
        if(cancelled())return false;
        if(o.playerStart||o.audioListener||o.ui||o.vehicle||o.door||o.lightSwitch||o.combustible||o.atmosphere||o.fluidVolume||o.celestial||(o.body&&(o.body->managed||o.body->shape==SceneShape::Terrain))){error="region "+region.id+": session-owned/unsupported component on "+std::to_string(o.id);return false;}
        auto validate=o;validate.gravity.reset();if(!RuntimeWorld::ValidateEntityDefinition(validate,error))return false;
        for(auto& [name,value]:ObjectProperties(o)){
            if(value.empty())continue;
            const auto* a=assets.Find(value);
            bool expected=name.find("asset")!=std::string::npos||name=="render.mesh"||name=="render.texture"||name=="liquid.basin.geometry"||name=="liquid.container.geometry";
            if(a){if(a->missing){error="missing region asset "+value;return false;}}else if(expected&&value.size()==32){error="unregistered region asset "+value;return false;}
        }
        for(auto& slot:o.scripts){const auto* a=assets.Find(slot.asset);if(!a||a->missing||a->type!=AssetType::Script){error="missing region script "+slot.asset;return false;}}
        if(o.navigationSurface&&o.navigationSurface->enabled){const auto* a=assets.Find(o.navigationSurface->asset);auto data=std::make_shared<NavigationData>();NavigationGeometry geometry;
            if(!a||a->type!=AssetType::Navigation||!LoadNavigation(a->path,*data,error)||!CollectNavigationGeometry(flat,o,project.Settings().navigation,geometry,error,&assets)||geometry.fingerprint!=data->fingerprint){error="stale/missing region navigation bake: "+error;return false;}
            if(data->layers.size()>128){error="region surface exceeds 128 indivisible native tiles; split it";return false;}
            for(auto& l:data->layers)result.bytes+=l.size()*4;
            result.navigation[o.id]=data;
        }
        if(o.particleEmitter)result.bytes+=size_t(o.particleEmitter->maxParticles)*(sizeof(VisualParticle)+sizeof(ParticleBillboard));
        if(o.ragdoll||o.animation){if(!o.render||o.render->meshAsset.empty()||!assets.Find(o.render->meshAsset)||assets.Find(o.render->meshAsset)->type!=AssetType::Mesh){error="ragdoll region requires an imported skeletal mesh";return false;}result.requiredGeometry.push_back(o.render->meshAsset);}
        if(o.body){if(!o.body->collisionAsset.empty())result.requiredGeometry.push_back(o.body->collisionAsset);for(auto& c:o.body->compoundBoxes)if(!c.assetId.empty())result.requiredGeometry.push_back(c.assetId);}
        if(o.deformable)result.requiredGeometry.push_back(o.deformable->asset);
        if(o.liquidBasin)for(auto id:{o.liquidBasin->geometry,o.liquidBasin->asset})result.requiredGeometry.push_back(id);
        if(o.liquidContainer)result.requiredGeometry.push_back(o.liquidContainer->geometry);
        if(o.liquidBasin&&o.liquidBasin->enabled){const auto* a=assets.Find(o.liquidBasin->asset);std::string bytes;LiquidResource resource;
            if(!a||!read(a->path,bytes,error,128*1024*1024)||!DecodeLiquidResource({bytes.begin(),bytes.end()},resource,error)||!resource.basin||resource.basin->fingerprint!=LiquidSourceFingerprint(flat,o,assets,error)){error="stale/missing region liquid bake: "+error;return false;}
        }
    }
    for(auto& id:result.requiredGeometry){auto* asset=assets.Find(id);if(!asset||asset->missing||(asset->type!=AssetType::Mesh&&asset->type!=AssetType::Deformable&&asset->type!=AssetType::Collision&&asset->type!=AssetType::Liquid)){error="missing/incompatible required region geometry "+id;return false;}}
    std::sort(result.requiredGeometry.begin(),result.requiredGeometry.end());result.requiredGeometry.erase(std::unique(result.requiredGeometry.begin(),result.requiredGeometry.end()),result.requiredGeometry.end());
    out=std::move(result);return true;
}
bool ValidateWorldQualifiedReferences(const WorldManifest& manifest,const std::map<std::string,PreparedWorldRegion>& products,std::string& error){
    for(auto& ref:manifest.references){auto source=products.find(ref.region),target=products.find(ref.target);
        if(source==products.end()||target==products.end()||!source->second.source.Find(ref.local)||!target->second.source.Find(ref.targetLocal)){error="missing qualified reference endpoint: "+ref.region+" -> "+ref.target;return false;}
        auto object=*source->second.source.Find(ref.local);if(!reference(object,ref.field,ref.targetLocal)){error="invalid qualified reference component: "+ref.field;return false;}
    }return true;
}
struct WorldStreaming::Impl {
    RuntimeWorld& world;ResourceManager& resources;Project project;WorldManifest manifest;
    bool restoredRecords=false;std::string baselineError,restoredBaseline;uint64_t previousResourceBudget=0;JobHandle baselineJob;std::shared_ptr<std::string> baseline=std::make_shared<std::string>();
    struct Product {PreparedWorldRegion data;std::string error;double elapsed=0;};
    struct Snapshot {SceneObject definition;EntityPhysicalState state;bool destroyed=false;std::vector<ScriptStateRecord> scripts;std::string deformable;std::string animation;};
    struct Region {
        RegionStatus status;JobHandle job;std::shared_ptr<Product> product;
        std::map<SceneObjectId,EntityId> mapping;std::vector<EntityId> members;Scene local,flat;
        struct Relocation {Scene local,flat;std::vector<SceneObject> externalParents;std::string error;};
        std::shared_ptr<Relocation> relocation;JobHandle relocationJob;size_t allocationNext=0,relocationNext=0,restoreNext=0;unsigned relocationPhase=0;
        std::map<EntityId,SceneObjectId> originals;std::map<EntityId,size_t> localIndices;std::map<EntityId,EntityId> savedReplacements;std::vector<EntityId> restoreKeys;
        std::map<SceneObjectId,Snapshot> saved;std::set<std::string> manualPins;
        std::vector<AssetId> handoffRefs;size_t next=0,navNext=0,resourceNext=0,retainedBytes=0;bool activate=false,interest=false,unload=false,scriptsEnded=false;
        std::string suspensionError;int interestPriority=0;Clock::time_point requested{};
    };
    struct Demand {std::string region;bool preload=false;EntityId owner=0;uint64_t slot=0;};
    struct InterestSource {glm::vec3 position;float load,retain;int priority;};
    std::map<std::string,Region> regions;std::map<uint64_t,Demand> requests;std::map<EntityId,std::string> owners;
    std::map<std::string,std::vector<std::string>> dependents;
    std::map<std::string,InterestSource> interests;std::map<EntityId,std::string> gravityOrder;
    std::map<EntityId,std::string> identities;
    StreamingStats stats;std::function<bool(const JobContext&)> gate;
    Impl(RuntimeWorld& w,ResourceManager& r,Project p,WorldManifest m):world(w),resources(r),project(std::move(p)),manifest(std::move(m)){previousResourceBudget=r.BudgetBytes();r.SetBudgetBytes(manifest.resourceCacheBytes);for(auto& [id,_]:manifest.regions){Region region;region.status.id=id;regions.emplace(id,std::move(region));for(auto& dep:manifest.regions.at(id).dependencies)dependents[dep].push_back(id);}}
    ~Impl(){resources.SetBudgetBytes(previousResourceBudget);if(baselineJob.IsValid())resources.Jobs()->Cancel(baselineJob);for(auto& [_,r]:regions){if(r.job.IsValid())resources.Jobs()->Cancel(r.job);if(r.relocationJob.IsValid())resources.Jobs()->Cancel(r.relocationJob);releaseHandoff(r);}}
    void releaseHandoff(Region& r){for(auto& a:r.handoffRefs)resources.ReleaseRef(a);r.handoffRefs.clear();}
    void clearPrepared(Region& r){if(r.relocationJob.IsValid()){resources.Jobs()->Cancel(r.relocationJob);resources.Jobs()->Forget(r.relocationJob);r.relocationJob={};}r.relocation.reset();r.relocationPhase=0;r.allocationNext=r.relocationNext=r.restoreNext=0;r.originals.clear();r.localIndices.clear();r.savedReplacements.clear();r.restoreKeys.clear();releaseHandoff(r);r.product.reset();r.local=Scene{};r.flat=Scene{};for(auto& [_,id]:r.mapping)if(!owners.count(id))gravityOrder.erase(id);r.mapping.clear();r.members.clear();r.next=r.navNext=r.resourceNext=0;}
    size_t savedBytes(const std::map<SceneObjectId,Snapshot>& saved)const {size_t bytes=0;for(auto& [_,snap]:saved){bytes+=sizeof(Snapshot)+sizeof(SceneObject)*2+snap.deformable.size()+snap.animation.size();for(auto& [key,value]:ObjectProperties(snap.definition))bytes+=key.size()+value.size();for(auto& js:snap.scripts)bytes+=js.json.size();}return bytes;}
    void recount(){stats.resourceResidentBytes=resources.Stats().bytesResident;stats.resourceCacheBudget=resources.BudgetBytes();stats.pendingBytes=stats.liveBytes=stats.retainedBytes=stats.active=stats.pending=0;for(auto& [id,r]:regions){auto& s=r.status;s.demands=0;for(auto& [_,q]:requests)if(q.region==id)++s.demands;s.retained=r.retainedBytes;stats.retainedBytes+=s.retained;
        if(s.state=="active"||s.state=="unloading"){++stats.active;stats.liveBytes+=s.bytes;}else if(s.state=="preparing"||s.state=="prepared"||s.state=="installing"){++stats.pending;stats.pendingBytes+=s.bytes;if(s.state=="installing")stats.liveBytes+=s.bytes;}
    }}
    bool wanted(const std::string& id)const{auto& r=regions.at(id);if(r.interest||!r.manualPins.empty())return true;for(auto& [_,q]:requests)if(q.region==id)return true;if(auto it=dependents.find(id);it!=dependents.end())for(auto& other:it->second)if(regions.at(other).status.state=="active"||wantsDirect(other))return true;return false;}
    bool wantsActivation(const std::string& id)const{auto& r=regions.at(id);if(r.interest)return true;for(auto& [_,q]:requests)if(q.region==id&&!q.preload)return true;for(auto& [other,data]:manifest.regions)if(other!=id&&std::find(data.dependencies.begin(),data.dependencies.end(),id)!=data.dependencies.end()&&wanted(other)&&wantsDirect(other))return true;return false;}
    bool wantsDirect(const std::string& id)const{auto& r=regions.at(id);if(r.interest)return true;for(auto& [_,q]:requests)if(q.region==id&&!q.preload)return true;return false;}
    struct ReferenceCache {std::string asset,properties;std::vector<EntityId> targets;};
    std::map<std::pair<EntityId,uint64_t>,ReferenceCache> referenceCache;
    std::map<std::string,bool> scriptReferencePins;bool referenceMetadataPending=false;
    std::vector<SceneObject> pinObjects;unsigned pinVersion=~0u;
    void refreshScriptReferences(){
        pinObjects=world.ScriptObjects();pinVersion=world.EntityVersion();
        scriptReferencePins.clear();referenceMetadataPending=false;std::set<std::pair<EntityId,uint64_t>> live;
        for(const auto& object:pinObjects)for(const auto& slot:object.scripts)if(slot.enabled){
            auto key=std::make_pair(object.id,slot.id);live.insert(key);
            auto [it,inserted]=referenceCache.try_emplace(key);
            if(inserted||it->second.properties!=slot.properties||it->second.asset!=slot.asset){it->second.properties=slot.properties;it->second.asset=slot.asset;it->second.targets.clear();}
            // Only normal script initialization knows the exported schema. Never
            // guess that an arbitrary JSON object/string is a declared reference.
            const auto declared=world.Scripts()?world.Scripts()->DeclaredReferences(object.id,slot):std::optional<std::vector<EntityId>>{};
            if(!declared){referenceMetadataPending=true;continue;}
            it->second.targets=*declared;
            for(auto target:it->second.targets)if(world.RuntimeDefinition(target)&&Owner(target)!=Owner(object.id))scriptReferencePins[Owner(target)]=true;
        }
        for(auto it=referenceCache.begin();it!=referenceCache.end();)if(!live.count(it->first))it=referenceCache.erase(it);else ++it;
    }
    void pins(const std::string& id){if(pinVersion!=world.EntityVersion())refreshScriptReferences();auto& r=regions.at(id);r.status.pins.assign(r.manualPins.begin(),r.manualPins.end());auto add=[&](std::string reason){if(std::find(r.status.pins.begin(),r.status.pins.end(),reason)==r.status.pins.end())r.status.pins.push_back(std::move(reason));};
        if(!r.suspensionError.empty())add(r.suspensionError);
        if(referenceMetadataPending)add("script reference metadata awaiting normal synchronization");
        if(manifest.regions.at(id).policy=="resident")add("authored resident policy");
        for(auto& o:pinObjects){
            auto own=owners.find(o.id);bool here=own!=owners.end()&&own->second==id;
            if(o.deformable){std::string error;auto* deform=world.RuntimeDeformable(o.id,error);if(here&&deform&&deform->asset->fracture)add("live fracture family: physical pieces retained; remove/adopt family before suspension");if(!here&&deform&&deform->asset->fracture&&deform->asset->fracture->rigid)for(unsigned p=0;p<deform->asset->fracture->parts.size();++p){auto child=world.FracturePartEntity(o.id,p,true);if(child&&Owner(child)==id){add("material piece owned by an external fracture family");break;}}const auto& settings=deform?deform->settings:*o.deformable;for(size_t i=0;i<settings.attachments.size();++i){const auto& a=settings.attachments[i];if(a.enabled&&(!deform||!deform->released[i])&&a.target&&Owner(a.target)==id&&Owner(o.id)!=id)add("external deformable attachment");}}
            if(here&&(o.liquidBasin||o.liquidContainer||o.liquidConnection))add("conserved liquid group: no lossless suspension");
            if(!here&&o.parent&&Owner(o.parent)==id)add("external hierarchy parent");
            if(!here&&o.socket&&Owner(o.socket->target)==id)add("external visual socket");
            if(scriptReferencePins.count(id))add("external script entity property");
            if(here)if(auto* agent=world.Navigation().Agent(o.id);agent&&(agent->hasDestination||agent->onLink||agent->stopped))add("navigation intent in use: adopt or clear before suspension");
            if(here&&(o.animation||o.ragdoll)&&!WorldPersistence::CanSuspendAnimation(world,o.id))add("active articulation/return: suspend after physics authority ends");
            if(here&&std::find_if(r.mapping.begin(),r.mapping.end(),[&](auto& mapping){return mapping.second==o.id;})==r.mapping.end())add("adopted runtime member: transfer to root before suspension");
            if(o.characterMotor)if(auto* motor=world.RuntimeCharacter(o.id)){auto support=world.EntityIdOfBody(motor->result.support);if(support&&Owner(support)==id&&Owner(o.id)!=id)add("active character support");}
            if(o.joint){if(Owner(o.joint->bodyA)==id&&Owner(o.id)!=id)add("external joint body A");if(Owner(o.joint->bodyB)==id&&Owner(o.id)!=id)add("external joint body B");}
            if(auto* agent=world.Navigation().Agent(o.id);agent&&(agent->onLink||agent->hasDestination)&&!here){for(auto& corner:agent->path.corners){if(corner.link&&Owner(corner.link)==id)add("active navigation link/corridor");auto surface=world.Navigation().Sample(corner.position,1);if(surface&&Owner(surface->surface)==id)add("active navigation surface corridor");}}
            if(!here&&((o.body&&o.body->motion==SceneBodyMotion::Dynamic)||o.characterMotor||o.liquidContainer||o.deformable)){
                auto gravityDependency=[&](glm::vec3 position){
                    EntityId winner=0;std::string first;
                    for(auto& g:world.GravityRegions()){auto d=glm::inverse(g.rotation)*(position-g.position);bool inside=g.component.regionShape==SceneRegionShape::Sphere?glm::length(d)<=g.component.regionRadius:glm::all(glm::lessThanEqual(glm::abs(d),g.component.regionHalfExtents));
                        auto key=gravityOrder.find(g.id);if(inside&&key!=gravityOrder.end()&&(winner==0||key->second<first)){winner=g.id;first=key->second;}}
                    return winner&&Owner(winner)==id;
                };
                if(o.deformable){std::string error;if(auto* deform=world.RuntimeDeformable(o.id,error))for(auto position:deform->positions)if(gravityDependency(glm::vec3(position))){add("deformable node gravity dependency in use");break;}}
                else {EntityPhysicalState state;if(world.GetEntityState(o.id,state)&&gravityDependency(state.position))add("gravity dependency in use");}
            }
        }
        for(auto& [other,def]:manifest.regions)if(other!=id&&regions.at(other).status.state=="active"&&std::find(def.dependencies.begin(),def.dependencies.end(),id)!=def.dependencies.end())add("active dependent "+other);
    }
    std::string Owner(EntityId id)const{auto it=owners.find(id);return it==owners.end()?"root":it->second;}
    bool snapshot(Region& r,std::string& error){auto records=world.Scripts()?world.Scripts()->Capture(false,&r.members):std::vector<ScriptStateRecord>{};auto result=r.saved;
        for(auto& [local,id]:r.mapping){Snapshot s;const auto* def=world.RuntimeDefinition(id);s.destroyed=!def;if(def){s.definition=*def;s.definition.tags=world.TagsOf(id);if((def->animation||def->ragdoll)&&!WorldPersistence::CaptureAnimation(world,id,s.animation,error))return false;if(def->deformable){auto* deform=world.RuntimeDeformable(id,error);if(!deform||!world.CaptureDeformable(id,s.deformable,error))return false;s.definition.deformable=deform->settings;}world.GetEntityState(id,s.state);if(s.definition.joint){JointState joint;if(world.Physics().GetJoint(world.RuntimeJoint(id),joint)){s.definition.joint->settings=joint.settings;
                    // Native world anchors are already in the simulation frame; authored
                    // settings are relative to their joint entity, including on revisit.
                    if(!s.definition.joint->bodyB){auto inverse=glm::inverse(s.definition.transform.rotation);s.definition.joint->settings.anchorB=inverse*(joint.settings.anchorB-s.definition.transform.position);s.definition.joint->settings.frameB=glm::normalize(inverse*joint.settings.frameB);}
                    s.definition.joint->settings.bodyA={};s.definition.joint->settings.bodyB={};}}}for(auto& js:records)if(js.entity==id){if(js.json.empty()){error="script state is not serializable";return false;}s.scripts.push_back(js);}result[local]=std::move(s);}
        const auto bytes=savedBytes(result);recount();if(stats.retainedBytes-r.status.retained+bytes>manifest.retainedBytes){error="retained session byte budget";return false;}r.saved=std::move(result);r.retainedBytes=bytes;return true;
    }
};
WorldStreaming::WorldStreaming(RuntimeWorld& w,ResourceManager& r,Project p,WorldManifest m):m(std::make_unique<Impl>(w,r,std::move(p),std::move(m))){
    unsigned rootOrder=0;for(auto& g:w.GravityRegions()){std::ostringstream key;key<<"1:"<<std::setw(10)<<std::setfill('0')<<rootOrder++;this->m->gravityOrder[g.id]=key.str();}
    const auto rootBaseline=w.BaselineFingerprint();
    w.SetCompositionFingerprint("composed baseline preparing; disk saves disabled");
    auto product=this->m->baseline;auto project=this->m->project;auto assets=*r.Assets();auto manifest=this->m->manifest;
    this->m->baselineJob=r.Jobs()->Submit([product,project,assets,manifest,rootBaseline](JobContext& ctx){std::string identity="Judas.ComposedWorld.1:"+rootBaseline+":"+Project::SerializeToString(project.Settings()),error;
        for(auto& [id,region]:manifest.regions){if(ctx.CancelRequested()){ctx.ReportCancelled();return;}std::string bytes;if(!read(project.Resolve(region.scene),bytes,error)){ctx.SetError(error);return;}identity+=id+":"+SceneFingerprintSha256(bytes);}
        for(auto& [id,asset]:assets.Records()){if(ctx.CancelRequested()){ctx.ReportCancelled();return;}std::string digest;if(!SceneFingerprintSha256File(asset.path,digest,error,[&]{return ctx.CancelRequested();})){if(ctx.CancelRequested())ctx.ReportCancelled();else ctx.SetError(error);return;}identity+=id+":"+AssetTypeName(asset.type)+":"+digest;}
        *product=SceneFingerprintSha256(identity);
    },JobPriority::Low,"composed content identity");
}
WorldStreaming::~WorldStreaming()=default;
uint64_t WorldStreaming::Request(const std::string& id,bool preload,std::string& error,SceneObjectId requester,uint64_t slot){if(!m->regions.count(id)){error="unknown world region "+id;return 0;}if(m->requests.size()>=1024){error="stream request limit (release finished demand)";return 0;}static std::atomic<uint64_t> nextToken{1};auto token=nextToken.fetch_add(1);m->requests[token]={id,preload,requester,slot};return token;}
bool WorldStreaming::Activate(uint64_t id){auto it=m->requests.find(id);if(it==m->requests.end())return false;it->second.preload=false;return true;}
bool WorldStreaming::Release(uint64_t id){return m->requests.erase(id)!=0;}
void WorldStreaming::ReleaseRequester(SceneObjectId entity,uint64_t slot){for(auto it=m->requests.begin();it!=m->requests.end();)if(it->second.owner==entity&&it->second.slot==slot)it=m->requests.erase(it);else ++it;}
bool WorldStreaming::Unload(const std::string& id){auto it=m->regions.find(id);if(it==m->regions.end()||m->wanted(id))return false;it->second.unload=true;it->second.activate=false;return true;}
std::optional<RegionStatus> WorldStreaming::Status(uint64_t id)const{auto it=m->requests.find(id);if(it==m->requests.end())return {};return m->regions.at(it->second.region).status;}
std::vector<RegionStatus> WorldStreaming::Regions()const{std::vector<RegionStatus> result;for(auto& [_,r]:m->regions)result.push_back(r.status);return result;}
const StreamingStats& WorldStreaming::Stats()const{return m->stats;}
std::string WorldStreaming::Owner(EntityId id)const{return m->Owner(id);}
SceneObjectId WorldStreaming::Resolve(const std::string& id,SceneObjectId local)const{if(id=="root")return m->world.RuntimeDefinition(local)?local:0;auto it=m->regions.find(id);if(it==m->regions.end()||it->second.status.state!="active")return 0;auto p=it->second.mapping.find(local);return p==it->second.mapping.end()||!m->world.RuntimeDefinition(p->second)?0:p->second;}
bool WorldStreaming::Pin(const std::string& id,const std::string& reason,bool value){auto it=m->regions.find(id);if(it==m->regions.end()||reason.empty()||reason.size()>128)return false;if(value){if(it->second.manualPins.size()>=32)return false;it->second.manualPins.insert(reason);}else it->second.manualPins.erase(reason);return true;}
bool WorldStreaming::Adopt(EntityId root,const std::string& target,std::string& error){if(target!="root"&&(!m->regions.count(target)||m->regions.at(target).status.state!="active")){error="adoption destination is not active";return false;}
    auto objects=m->world.ScriptObjects();std::set<EntityId> ids{root};bool changed=true;while(changed){changed=false;for(auto& o:objects)if(o.parent&&ids.count(o.parent)&&ids.insert(o.id).second)changed=true;}
    if(!m->world.RuntimeDefinition(root)){error="stale adoption root";return false;}
    for(auto& o:objects)if(ids.count(o.id))for(auto& slot:o.scripts)for(auto dependency:ScriptSystem::PropertyEntities(slot.properties))if(!ids.count(dependency)&&Owner(dependency)!=target){error="adoption would leave an external script reference; keep dependency pinned";return false;}
    for(auto& o:objects)if(o.socket&&(ids.count(o.id)||ids.count(o.socket->target))&&(!ids.count(o.id)||!ids.count(o.socket->target))){error="adopt complete socket assembly or keep dependency pinned";return false;}
    for(auto& o:objects)if(o.joint&&(ids.count(o.id)||ids.count(o.joint->bodyA)||ids.count(o.joint->bodyB))){if(!ids.count(o.id)||!ids.count(o.joint->bodyA)||(o.joint->bodyB&&!ids.count(o.joint->bodyB))){error="adopt complete joint assembly or keep dependency pinned";return false;}}
    for(auto id:ids){const auto owner=m->Owner(id);if(owner!="root"&&m->regions.at(owner).status.state!="active"){error="adoption source is departing/not active";return false;}auto* o=m->world.RuntimeDefinition(id);if(o&&(o->gravity||o->liquidBasin||o->liquidContainer||o->ragdoll)){error="group-owned gravity/liquid/articulation cannot be adopted separately";return false;}}
    for(auto id:ids){m->identities.try_emplace(id,PersistentKey(id));auto from=m->Owner(id);if(from!="root"){auto& r=m->regions.at(from);r.members.erase(std::remove(r.members.begin(),r.members.end(),id),r.members.end());for(auto it=r.mapping.begin();it!=r.mapping.end();)if(it->second==id){r.saved[it->first].destroyed=true;it=r.mapping.erase(it);}else ++it;r.retainedBytes=m->savedBytes(r.saved);}m->owners[id]=target;if(target!="root")m->regions.at(target).members.push_back(id);}
    return true;
}
bool WorldStreaming::Interest(const std::string& id,glm::vec3 p,float load,float retain,int priority,std::string& error){if(id.empty()||id.size()>128||!finite(glm::dvec3(p))||!std::isfinite(load+retain)||load<0||retain<load||priority<-100000||priority>100000||(m->interests.size()>=32&&!m->interests.count(id))){error="invalid interest source or 32-source limit";return false;}m->interests[id]={p,load,retain,priority};return true;}
void WorldStreaming::RemoveInterest(const std::string& id){m->interests.erase(id);}
void WorldStreaming::SetPreparationGate(std::function<bool(const JobContext&)> gate){m->gate=std::move(gate);}
void WorldStreaming::Advance(bool paused){
    JUDAS_PROFILE_SCOPE("Streaming outer integration");
    for(auto it=m->requests.begin();it!=m->requests.end();){auto& demand=it->second;const auto* owner=demand.owner?m->world.RuntimeDefinition(demand.owner):nullptr;
        bool gone=demand.owner&&!owner;if(owner&&demand.slot)gone=std::none_of(owner->scripts.begin(),owner->scripts.end(),[&](const auto& slot){return slot.id==demand.slot&&slot.enabled;});
        if(gone)it=m->requests.erase(it);else ++it;
    }
    if(m->baselineJob.IsValid()&&m->resources.Jobs()->IsFinished(m->baselineJob)){
        if(m->resources.Jobs()->StateOf(m->baselineJob)==JobState::Completed)m->world.SetCompositionFingerprint(m->restoredBaseline.empty()?*m->baseline:m->restoredBaseline);
        else m->baselineError="composed identity: "+m->resources.Jobs()->ErrorOf(m->baselineJob);
        m->resources.Jobs()->Forget(m->baselineJob);m->baselineJob={};
    }
    auto start=Clock::now();m->stats.integrationMs=0;unsigned units=0;
    m->refreshScriptReferences();
    for(auto& [id,r]:m->regions){bool wanted=false;r.interestPriority=0;const auto& def=m->manifest.regions.at(id);auto center=glm::vec3(def.origin-m->world.Settings().worldOrigin);for(auto& [_,interest]:m->interests){auto local=glm::inverse(def.rotation)*(interest.position-center);float distance=glm::length(glm::max(glm::abs(local)-def.halfExtents,glm::vec3(0)));if(distance<=(r.interest?interest.retain:interest.load)){wanted=true;r.interestPriority=std::max(r.interestPriority,interest.priority);}}r.interest=wanted;}
    // Hard dependency demand is propagated transitively before scheduling.
    std::set<std::string> needed;std::function<void(const std::string&)> need=[&](const std::string& id){if(!needed.insert(id).second)return;for(auto& d:m->manifest.regions.at(id).dependencies)need(d);};for(auto& [id,r]:m->regions)if(m->wanted(id))need(id);
    std::vector<std::string> order;for(auto& [id,_]:m->regions)order.push_back(id);std::stable_sort(order.begin(),order.end(),[&](auto& a,auto& b){auto p=std::max(m->manifest.regions.at(a).priority,m->regions.at(a).interestPriority),q=std::max(m->manifest.regions.at(b).priority,m->regions.at(b).interestPriority);return p==q?a<b:p>q;});
    std::set<std::string> activationNeeded;std::function<void(const std::string&)> activateNeed=[&](const std::string& id){if(!activationNeeded.insert(id).second)return;for(auto& d:m->manifest.regions.at(id).dependencies)activateNeed(d);};
    for(auto& [id,r]:m->regions)if(m->wantsDirect(id))activateNeed(id);
    m->recount();size_t preparing=0;for(auto& [_,r]:m->regions)if(r.status.state=="preparing")++preparing;
    auto unit=[&](const std::function<void()>& fn,Impl::Region& r,const char* name="Streaming dispatch"){auto t=Clock::now();fn();auto cost=ms(t);r.status.integrationMs+=cost;r.status.largestUnitMs=std::max(r.status.largestUnitMs,cost);m->stats.largestUnitMs=std::max(m->stats.largestUnitMs,cost);++units;if(std::getenv("JUDAS_STREAM_UNIT_TRACE"))std::fprintf(stderr,"STREAM unit region=%s name=%s ms=%.6f budget=%.3f\n",r.status.id.c_str(),name,cost,m->manifest.installMilliseconds);};
    auto budget=[&]{return units<m->manifest.unitsPerFrame&&ms(start)<m->manifest.installMilliseconds;};
    for(auto& id:order){auto& r=m->regions.at(id);const auto& def=m->manifest.regions.at(id);auto& s=r.status;
        if((s.state=="unloaded"||s.state=="cancelled"||s.state=="blocked")&&needed.count(id)&&!r.product&&r.members.empty()){
            if(preparing>=m->manifest.maxPreparing||m->stats.pendingBytes+def.estimatedBytes>m->manifest.pendingBytes){s.state="blocked";s.error="pending bytes/concurrency budget";continue;}
            s.state="preparing";s.error.clear();s.bytes=def.estimatedBytes;r.requested=Clock::now();r.product=std::make_shared<Impl::Product>();auto product=r.product;auto project=m->project;auto assets=*m->resources.Assets();auto gate=m->gate;
            r.job=m->resources.Jobs()->Submit([product,def,project,assets,gate](JobContext& ctx){auto t=Clock::now();if(gate&&!gate(ctx)){ctx.ReportCancelled();return;}if(!PrepareWorldRegion(def,project,assets,product->data,product->error,&ctx)){if(ctx.CancelRequested())ctx.ReportCancelled();else ctx.SetError(product->error);}product->elapsed=ms(t);},std::max(def.priority,r.interestPriority)>0?JobPriority::High:JobPriority::Normal,"region "+id);++preparing;m->stats.pendingBytes+=s.bytes;
        }
        if(s.state=="preparing"){
            if(!needed.count(id))m->resources.Jobs()->Cancel(r.job);
            if(!m->resources.Jobs()->IsFinished(r.job))continue;
            auto state=m->resources.Jobs()->StateOf(r.job);m->resources.Jobs()->Forget(r.job);r.job={};
            if(state!=JobState::Completed){s.state=state==JobState::Cancelled?"cancelled":"failed";s.error=r.product->error;m->clearPrepared(r);s.bytes=0;continue;}
            s.preparationMs=r.product->elapsed;s.bytes=r.product->data.bytes;
            m->recount();if(m->stats.pendingBytes>m->manifest.pendingBytes){s.state="failed";s.error="prepared region exceeds pending byte cap";m->clearPrepared(r);s.bytes=0;continue;}
            s.state="prepared";s.entities=r.product->data.source.Objects().size();
        }
        if((s.state=="prepared"||s.state=="installing")&&!needed.count(id)){s.state="unloading";r.scriptsEnded=true;r.unload=true;}
        if(s.state=="prepared"&&needed.count(id)){
            if(m->baselineJob.IsValid()){s.error="waiting for authored baseline";continue;}if(!m->baselineError.empty()){s.state="failed";s.error=m->baselineError;m->clearPrepared(r);s.bytes=0;continue;}
            auto& required=r.product->data.requiredGeometry;
            while(budget()&&r.resourceNext<required.size()){auto id=required[r.resourceNext++];unit([&]{m->resources.AddRef(id);r.handoffRefs.push_back(id);if(m->resources.Assets()->Find(id)->type==AssetType::Mesh)m->resources.RequestMesh(id);else if(m->resources.Assets()->Find(id)->type==AssetType::Collision)m->resources.RequestCollision(id);else if(m->resources.Assets()->Find(id)->type==AssetType::Deformable)m->resources.RequestDeformable(id);else m->resources.RequestLiquid(id);},r,"Streaming required resource request");}
            bool loading=r.resourceNext<required.size();for(auto& asset:required){auto state=m->resources.StateOf(asset);if(state==ResourceState::Failed){s.state="failed";s.error="required geometry resource: "+m->resources.ErrorOf(asset);break;}if(state!=ResourceState::Ready)loading=true;else if(m->resources.Assets()->Find(asset)->type==AssetType::Mesh&&!m->resources.TryGetSkeletal(asset)){s.state="failed";s.error="required ragdoll mesh has no skeleton";break;}}
            if(s.state=="failed"){m->clearPrepared(r);s.bytes=0;continue;}if(loading){s.error="waiting for required geometry";continue;}
        }
        if(s.state=="prepared"&&needed.count(id)&&!paused&&activationNeeded.count(id)&&budget()){
            bool ready=true;for(auto& dep:def.dependencies)if(m->regions.at(dep).status.state!="active")ready=false;if(!ready){s.error="waiting for dependency";continue;}
            m->recount();if(m->stats.liveBytes+s.bytes>m->manifest.liveBytes){s.error="live byte budget";continue;}
            // No scene copy, remap or hierarchy flattening is repeated on a yield.
            unit([&]{JUDAS_PROFILE_SCOPE("Streaming relocation begin");r.local=std::move(r.product->data.source);r.relocationPhase=1;s.state="installing";s.error.clear();},r,"Streaming relocation begin");
        }
        if(s.state=="installing"&&!paused&&r.relocationPhase==1){
            while(budget()&&r.allocationNext<r.local.Objects().size())unit([&]{JUDAS_PROFILE_SCOPE("Streaming identity allocation");auto source=r.local.Objects()[r.allocationNext++].id;auto runtime=m->world.AllocateRuntimeEntityId();r.mapping[source]=runtime;r.originals[runtime]=source;if(auto snap=r.saved.find(source);snap!=r.saved.end()){r.savedReplacements[snap->second.definition.id]=runtime;r.restoreKeys.push_back(source);}},r,"Streaming identity allocation");
            if(r.allocationNext==r.local.Objects().size())r.relocationPhase=2;
        }
        if(s.state=="installing"&&!paused&&r.relocationPhase==2){
            while(budget()&&r.relocationNext<r.local.Objects().size()){
                bool ok=true;unit([&]{JUDAS_PROFILE_SCOPE("Streaming object relocation");auto& o=r.local.Objects()[r.relocationNext];auto source=o.id;remap(o,r.mapping);r.localIndices[o.id]=r.relocationNext;
                    if(!o.parent){o.transform.position=glm::vec3(def.origin-m->world.Settings().worldOrigin)+def.rotation*o.transform.position;o.transform.rotation=glm::normalize(def.rotation*o.transform.rotation);}
                    if(o.body)o.body->initialLinearVelocity=def.rotation*o.body->initialLinearVelocity;
                    for(auto& ref:m->manifest.references)if(ref.region==id&&ref.local==source){auto target=Resolve(ref.target,ref.targetLocal);if((!target&&ref.hard)||!reference(o,ref.field,target)){ok=false;s.error="unresolved/invalid qualified reference";}}
                    if(auto snap=r.saved.find(source);snap!=r.saved.end()&&snap->second.destroyed)o.scripts.clear();
                    m->gravityOrder[o.id]=orderKey(def,source);++r.relocationNext;
                },r,"Streaming object relocation");
                if(!ok){s.state="failed";m->clearPrepared(r);break;}
            }
            if(s.state=="installing"&&r.relocationNext==r.local.Objects().size()&&budget())unit([&]{JUDAS_PROFILE_SCOPE("Streaming hierarchy handoff");
                r.relocation=std::make_shared<Impl::Region::Relocation>();auto product=r.relocation;
                SceneObjectId next=1;for(auto& [_,runtime]:r.mapping)next=std::max(next,runtime+1);r.local.SetNextId(next);
                for(auto& o:r.local.Objects())if(o.parent&&!r.localIndices.count(o.parent)){auto* parent=m->world.RuntimeDefinition(o.parent);if(!parent){product->error="unavailable qualified parent";break;}SceneObject frame;frame.id=parent->id;frame.transform=parent->transform;EntityPhysicalState state;if(m->world.GetEntityState(frame.id,state)){frame.transform.position=state.position;frame.transform.rotation=state.rotation;}product->externalParents.push_back(frame);}
                product->local=std::move(r.local);
                r.relocationJob=m->resources.Jobs()->Submit([product](JobContext& ctx){
                    if(!product->error.empty()){ctx.SetError(product->error);return;}Scene input=product->local;std::set<EntityId> local;
                    for(auto& o:input.Objects())local.insert(o.id);
                    for(auto& frame:product->externalParents)if(!local.count(frame.id))input.Objects().push_back(frame);
                    if(ctx.CancelRequested()){ctx.ReportCancelled();return;}
                    if(!FlattenHierarchy(input,product->flat,product->error)){ctx.SetError(product->error);return;}
                    for(auto& o:product->flat.Objects()){if(ctx.CancelRequested()){ctx.ReportCancelled();return;}auto check=o;check.gravity.reset();if(!RuntimeWorld::ValidateEntityDefinition(check,product->error)){ctx.SetError(product->error);return;}}
                    auto& objects=product->flat.Objects();objects.erase(std::remove_if(objects.begin(),objects.end(),[&](const auto& o){return !local.count(o.id);}),objects.end());
                },JobPriority::High,"region hierarchy "+id);r.relocationPhase=3;
            },r,"Streaming hierarchy handoff");
        }
        if(s.state=="installing"&&r.relocationPhase==3&&m->resources.Jobs()->IsFinished(r.relocationJob)){
            auto state=m->resources.Jobs()->StateOf(r.relocationJob);m->resources.Jobs()->Forget(r.relocationJob);r.relocationJob={};
            if(state!=JobState::Completed){s.error=r.relocation->error;s.state="failed";m->clearPrepared(r);continue;}
            r.local=std::move(r.relocation->local);r.flat=std::move(r.relocation->flat);r.relocation.reset();r.relocationPhase=4;r.next=0;
        }
        if(s.state=="installing"&&!paused&&r.relocationPhase==4){
            while(budget()&&r.next<r.flat.Objects().size()){
                std::string error;bool ok=true;
                unit([&]{JUDAS_PROFILE_SCOPE("Streaming component registration");
                    auto o=r.flat.Objects()[r.next];auto local=r.local.Objects()[r.localIndices.at(o.id)];auto original=r.originals.at(o.id);
                    if(auto snap=r.saved.find(original);snap!=r.saved.end()&&snap->second.destroyed)return;
                    if(auto snap=r.saved.find(original);snap!=r.saved.end()){
                        auto saved=snap->second.definition;remap(saved,r.savedReplacements);saved.id=o.id;saved.transform.position=snap->second.state.position;saved.transform.rotation=snap->second.state.rotation;o=std::move(saved);
                    }
                    for(auto& ref:m->manifest.references)if(ref.region==id&&ref.local==original)reference(o,ref.field,Resolve(ref.target,ref.targetLocal));
                    ok=m->world.StageRegionObject(o,local,error,id+":"+std::to_string(original));r.members.push_back(o.id);m->owners[o.id]=id;m->identities[o.id]="region:"+id+":"+std::to_string(original);
                },r,"Streaming component registration");
                if(!ok){s.state="unloading";s.error=error;r.scriptsEnded=true;r.unload=true;break;}
                ++r.next;s.installed=r.next;
            }
            if(s.state=="installing"&&r.next==r.flat.Objects().size()){
                auto& nav=r.product->data.navigation;
                while(budget()&&r.navNext<nav.size()){auto it=nav.begin();std::advance(it,r.navNext);auto runtime=r.mapping.at(it->first);auto* o=&r.flat.Objects().at(r.localIndices.at(runtime));std::string error;bool ok=false;unit([&]{JUDAS_PROFILE_SCOPE("Streaming native navigation registration");ok=m->world.Navigation().LoadSurface(runtime,o->transform,it->second,error,false);},r,"Streaming native navigation registration");if(!ok){s.state="unloading";s.error=error;r.unload=true;r.scriptsEnded=true;break;}++r.navNext;}
            }
            if(s.state=="installing"&&r.next==r.flat.Objects().size()&&r.navNext==r.product->data.navigation.size()&&budget()){
                bool ok=true;std::string error;
                while(budget()&&r.restoreNext<r.restoreKeys.size()&&ok)unit([&]{JUDAS_PROFILE_SCOPE("Streaming retained entity restoration");
                    auto source=r.restoreKeys[r.restoreNext++];const auto& snap=r.saved.at(source);if(snap.destroyed)return;auto runtime=r.mapping.at(source);
                    if(!snap.deformable.empty()){
                        if(!m->world.RestoreDeformable(runtime,snap.deformable,error,true)){ok=false;return;}
                        auto* deform=m->world.RuntimeDeformable(runtime,error,true);
                        for(auto& attachment:deform->settings.attachments)if(auto it=r.savedReplacements.find(attachment.target);it!=r.savedReplacements.end())attachment.target=it->second;
                        for(auto& ref:m->manifest.references)if(ref.region==id&&ref.local==source&&ref.field.rfind("deformable:",0)==0)for(auto& attachment:deform->settings.attachments)if(attachment.group==ref.field.substr(11))attachment.target=Resolve(ref.target,ref.targetLocal);
                    }
                    auto t=snap.definition.transform;t.position=snap.state.position;t.rotation=snap.state.rotation;
                    auto records=snap.scripts;for(auto& js:records)js.entity=runtime;
                    ok=m->world.RestoreRegionObject(runtime,t,snap.state,records,error,m->restoredRecords);
                    if(ok&&!snap.animation.empty())ok=WorldPersistence::RestoreAnimation(m->world,runtime,snap.animation,error);
                },r,"Streaming retained entity restoration");
                if(!ok){s.state="unloading";s.error=error;r.unload=true;r.scriptsEnded=true;}
                else if(r.restoreNext==r.restoreKeys.size()&&budget()){
                    // Only the enable/visibility commit remains atomic. Expensive
                    // serialization and reconstruction have completed while private.
                    unit([&]{JUDAS_PROFILE_SCOPE("Streaming atomic publication");m->world.Navigation().PublishSurfaces(r.members);m->world.PublishRegion(r.members);m->world.RebuildRegionGravity(m->gravityOrder);
                        s.state="active";s.loadMs=ms(r.requested);m->releaseHandoff(r);r.product.reset();r.local=Scene{};r.flat=Scene{};r.activate=false;
                    },r,"Streaming atomic publication");
                }
            }
        }
        if(s.state=="active"&&!needed.count(id)&&!paused&&budget()){r.suspensionError.clear();m->pins(id);if(s.pins.empty()){unit([&]{JUDAS_PROFILE_SCOPE("Streaming snapshot and script teardown");std::string error;if(!m->snapshot(r,error)){r.suspensionError=error;s.pins.push_back(error);}else {s.state="unloading";m->world.EndRegionScripts(r.members);m->world.HideRegion(r.members);r.scriptsEnded=true;r.unload=true;}},r,"Streaming snapshot and script teardown");}}
        if(s.state=="unloading"&&!paused){
            while(budget()&&!r.members.empty()){auto entity=r.members.back();unit([&]{JUDAS_PROFILE_SCOPE("Streaming unregister and resource release");m->world.RemoveRegionObject(entity);},r,"Streaming unregister and resource release");r.members.pop_back();m->owners.erase(entity);m->gravityOrder.erase(entity);}
            if(r.members.empty()&&budget()){unit([&]{m->world.Navigation().Update(m->world,0);m->world.RebuildRegionGravity(m->gravityOrder);},r,"Streaming retirement commit");bool failed=!s.error.empty();m->clearPrepared(r);r.activate=false;r.unload=false;s.bytes=0;s.installed=0;s.state=failed?"failed":"unloaded";}
        }
    }
    // Soft visual references are rebound only between published regions.
    for(auto& ref:m->manifest.references)if(!ref.hard){auto source=Resolve(ref.region,ref.local);auto target=Resolve(ref.target,ref.targetLocal);if(source)m->world.BindRegionCameraReference(source,target);}
    m->recount();for(auto& [id,r]:m->regions)if(r.status.state=="active"){m->pins(id);r.status.visualReady=m->world.RegionVisualReady(r.members);}else r.status.visualReady=false;
    m->stats.integrationMs=ms(start);JUDAS_PROFILE_COUNTER("Streaming pending bytes",double(m->stats.pendingBytes),ProfileCounterMode::Latest);JUDAS_PROFILE_COUNTER("Streaming live estimated bytes",double(m->stats.liveBytes),ProfileCounterMode::Latest);JUDAS_PROFILE_COUNTER("Streaming retained bytes",double(m->stats.retainedBytes),ProfileCounterMode::Latest);
}

bool WorldStreaming::SaveReady()const{for(auto& [_,r]:m->regions)if(r.status.state=="installing"||r.status.state=="unloading")return false;return true;}
std::string WorldStreaming::PersistentKey(SceneObjectId id)const{
 if(!m->world.RuntimeDefinition(id))return {};
 if(auto it=m->identities.find(id);it!=m->identities.end())return it->second;
 auto owner=m->Owner(id);if(owner!="root")for(auto& [local,runtime]:m->regions.at(owner).mapping)if(runtime==id)return "region:"+owner+":"+std::to_string(local);
 return "entity:"+std::to_string(id);
}
SceneObjectId WorldStreaming::ResolvePersistentKey(const std::string& key)const{
 for(auto& [id,identity]:m->identities)if(identity==key&&m->world.RuntimeDefinition(id))return id;
 try{if(key.rfind("entity:",0)==0){auto tail=key.substr(7);size_t end=0;auto id=std::stoull(tail,&end);return end==tail.size()&&m->world.RuntimeDefinition(id)?id:0;}
 if(key.rfind("region:",0)==0){auto split=key.rfind(':');if(split<=7)return 0;auto tail=key.substr(split+1);size_t end=0;auto local=std::stoull(tail,&end);return end==tail.size()?Resolve(key.substr(7,split-7),local):0;}}catch(...){}return 0;
}
void WorldStreaming::ResumeOwnership(){m->previousResourceBudget=m->resources.BudgetBytes();m->resources.SetBudgetBytes(m->manifest.resourceCacheBytes);}
unsigned WorldStreaming::ArchiveVersion()const{for(auto& [_,r]:m->regions)for(auto& [__,s]:r.saved)if(!s.animation.empty())return 3;for(auto& [_,r]:m->regions)for(auto& [__,s]:r.saved)if(!s.deformable.empty())return 2;return 1;}
void WorldStreaming::Persist(SaveArchive& a,unsigned version){
 a.Require(version>=1&&version<=3,"unsupported streaming archive version");
 std::string baseline=m->world.BaselineFingerprint();a(baseline);a.Require(baseline.size()==64,"composed authored identity not ready");if(a.reading){m->restoredRecords=true;m->restoredBaseline=baseline;m->world.SetCompositionFingerprint(baseline);}
 a.Require(a.reading||SaveReady(),"streaming residency transaction pending");
 uint32_t count=uint32_t(m->regions.size());a(count);a.Require(count==m->regions.size(),"saved manifest region mismatch");std::set<std::string> seen;
 for(uint32_t i=0;i<count;++i){std::string id;if(!a.reading){auto it=m->regions.begin();std::advance(it,i);id=it->first;}a(id);a.Require(m->regions.count(id)&&seen.insert(id).second,"unknown/duplicate saved region");auto& r=m->regions.at(id);
  bool active=r.status.state=="active";a(active,r.mapping,r.members,r.manualPins);uint64_t bytes=r.status.bytes;a(bytes);a.Require(bytes<=128*1024*1024,"region saved byte bound");
  uint32_t records=uint32_t(r.saved.size());a(records);a.Require(records<=4096,"region retained record bound");if(a.reading)r.saved.clear();
  for(uint32_t j=0;j<records;++j){SceneObjectId local=0;Impl::Snapshot snapshot;std::string text;
   if(!a.reading){auto it=r.saved.begin();std::advance(it,j);local=it->first;snapshot=it->second;WriteSceneObjectBlock(snapshot.definition,text);}
   a(local,snapshot.destroyed,text,snapshot.state.position,snapshot.state.rotation,snapshot.state.linearVelocity,snapshot.state.angularVelocity);if(version>=2)a(snapshot.deformable);if(version>=3){a(snapshot.animation);a.Require(snapshot.animation.size()<=1024*1024,"retained animation snapshot bound");}
   uint32_t scripts=uint32_t(snapshot.scripts.size());a(scripts);a.Require(scripts<=16,"region script count bound");if(a.reading)snapshot.scripts.resize(scripts);for(auto& s:snapshot.scripts)a(s.entity,s.slot,s.json);
   if(a.reading){if(!snapshot.destroyed){std::vector<std::string> lines;std::istringstream input(text);std::string line;while(std::getline(input,line))lines.push_back(line);size_t cursor=0;std::string error;a.Require(ParseSceneObjectBlock(lines,cursor,snapshot.definition,error)&&cursor==lines.size(),"invalid retained definition");}a.Require(r.saved.emplace(local,std::move(snapshot)).second,"duplicate retained identity");}
  }
  if(a.reading){r.status.state=active?"active":"unloaded";r.status.bytes=active?size_t(bytes):0;r.status.visualReady=active;r.status.entities=r.members.size();r.status.installed=r.members.size();r.retainedBytes=m->savedBytes(r.saved);}
 }
 a(m->owners,m->gravityOrder,m->identities);uint32_t interests=uint32_t(m->interests.size());a(interests);a.Require(interests<=32,"saved interest limit");if(a.reading)m->interests.clear();for(uint32_t i=0;i<interests;++i){std::string key;Impl::InterestSource source;if(!a.reading){auto it=m->interests.begin();std::advance(it,i);key=it->first;source=it->second;}a(key,source.position,source.load,source.retain,source.priority);a.Require(source.load>=0&&source.retain>=source.load,"invalid saved streaming interest");if(a.reading)a.Require(m->interests.emplace(key,source).second,"duplicate streaming interest");}
 if(a.reading){for(auto& [id,owner]:m->owners)a.Require(owner=="root"||m->regions.count(owner),"invalid saved ownership");for(auto& [_,r]:m->regions)for(auto& [local,id]:r.mapping){(void)local;a.Require(id!=0,"invalid qualified mapping");}m->world.RebuildRegionGravity(m->gravityOrder);m->recount();a.Require(m->stats.retainedBytes<=m->manifest.retainedBytes,"saved retained budget exceeded");}
}
