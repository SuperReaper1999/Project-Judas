#include "Deformable.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "SceneSerialization.h"
#include <cstdlib>
#include "deformables/PanelCook.h"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <stdexcept>

void RunDeformableV2WorldChecks(void(*check)(bool,const char*)){
    struct Scratch {std::filesystem::path path=std::filesystem::temp_directory_path()/("judas-v2-panels-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));~Scratch(){std::error_code error;std::filesystem::remove_all(path,error);}} scratch;
    auto root=scratch.path/"Assets";std::filesystem::create_directories(root);
    auto frame=MakeFractureBlock({2,1,1},{2,.2,1},true);frame.fracture->structural=true;
    for(auto& bond:frame.fracture->bonds){bond.material.enabled=true;bond.material.yieldForce=50000;bond.material.yieldTorque=10000;bond.material.hardeningForce=15000;bond.material.hardeningTorque=2000;bond.material.maximumTranslation=.3f;bond.material.maximumAngle=.8f;}
    std::string error;if(!PrepareDeformableAsset(frame,error))throw std::runtime_error(error);
    {std::ofstream out(root/"frame.judasdeform",std::ios::binary);out<<EncodeDeformableAsset(frame);}
    auto sheet=MakeDeformableSheet(4,4,1,.5).render;
    for(auto& v:sheet.vertices)v.position={v.position.x,.3f,v.position.y-.25f};
    DeformableAsset panel;if(!CookDeformablePanel(sheet,{4,4,1},panel,error))throw std::runtime_error(error);
    {std::ofstream out(root/"panel.judasdeform",std::ios::binary);out<<EncodeDeformableAsset(panel);}
    AssetDatabase database;database.Scan(scratch.path.string(),root.string());AssetRecord frameRecord,panelRecord;
    if(!database.Track((root/"frame.judasdeform").string(),frameRecord,error)||!database.Track((root/"panel.judasdeform").string(),panelRecord,error))throw std::runtime_error(error);
    ResourceManager resources(nullptr,&database);resources.RequestDeformable(frameRecord.id);resources.RequestDeformable(panelRecord.id);resources.WaitForAll();
    Scene scene;auto& assembly=scene.CreateObject("Physical structural carrier");const auto carrier=assembly.id;
    assembly.deformable=DeformableSettings{};assembly.deformable->asset=frameRecord.id;assembly.deformable->material.density=1000;
    std::vector<uint64_t> owners;
    for(unsigned i=0;i<2;++i){auto& object=scene.CreateObject("Independent physical panel "+std::to_string(i));owners.push_back(object.id);
        object.transform.position.x=i?.6f:-.6f;
        object.deformable=DeformableSettings{};auto& s=*object.deformable;s.asset=panelRecord.id;s.panel.enabled=true;s.material.stretchCompliance=s.material.shearCompliance=1e-9;
        DeformableAttachment attachment;attachment.group=i?"right":"left";attachment.kind=DeformableAttachment::Kind::Body;attachment.target=carrier;attachment.part=frame.fracture->parts[i].key;attachment.offset=glm::dvec3(object.transform.position)-frame.fracture->parts[i].proxyCenter;attachment.tearWork=20;
        s.attachments.push_back(attachment);
    }
    if(const char* destination=std::getenv("JUDAS_PANEL_EDITOR_FIXTURE")){
        Project project;if(!Project::CreateNew(destination,"Native Panel Workflow",project,error))throw std::runtime_error(error);
        std::filesystem::copy(root,std::filesystem::path(destination)/"Assets",std::filesystem::copy_options::recursive|std::filesystem::copy_options::overwrite_existing);
        project.Settings().startupScene="Scenes/panels.judas";
        if(!SaveSceneToFile(scene,project.StartupScenePath(),error)||!project.Save(error))throw std::runtime_error(error);
    }
    RuntimeWorld world;
    if(!world.Build(scene,&resources,error))throw std::runtime_error(error);
    world.UpdateDeformables(1./60);
    auto* first=world.RuntimeDeformable(owners[0],error);auto* second=world.RuntimeDeformable(owners[1],error);
    check(first&&second&&first->error.empty()&&second->error.empty()&&!first->released[0]&&!second->released[0],"multiple ordinary scene panels resolve stable mounts on the same structural assembly");
    check(first->carrierCollisionExclusions.size()==2&&second->carrierCollisionExclusions.size()==2,"attached panel excludes only its physically connected coarse carrier, without per-car collision layers");
    const auto& attachment=first->settings.attachments[0];auto body=world.DeformableAttachmentBody(attachment,error);
    check(body.IsValid()&&body.id==world.RuntimeBody(world.FracturePartEntity(carrier,0)).id,"stable section mount resolves the actual MK2 body");
    first->Release("left");world.UpdateDeformables(1./60);
    check(first->carrierCollisionExclusions.empty()&&!second->carrierCollisionExclusions.empty(),"complete detachment restores own-car collision without changing another panel's ownership");
    auto shape=first->positions,velocities=first->velocities;
    std::map<std::string,SaveChunk> chunks;
    if(!WorldPersistence::Capture(world,chunks,error))throw std::runtime_error("capture panels: "+error);
    check(chunks.at("deformables").version==7,"current V2 save participant carries panel material and physical mounts");
    Scene restoredScene;RuntimeWorld restored;
    if(!WorldPersistence::SceneFrom(chunks,restoredScene,error)||!restored.Build(restoredScene,&resources,error,nullptr,nullptr,true)||!WorldPersistence::Prepare(restored,error)||!WorldPersistence::Restore(restored,chunks,error))throw std::runtime_error("restore panels: "+error);
    WorldPersistence::Published(restored);
    auto* copy=restored.RuntimeDeformable(owners[0],error);
    check(copy&&copy->positions==shape&&copy->velocities==velocities&&copy->released[0]&&copy->settings.panel.enabled&&copy->settings.attachments[0].part==attachment.part,"fresh RuntimeWorld restores independent panel geometry, momentum, material, stable mount and release");
    restored.UpdateDeformables(1./60);
    check(copy->carrierCollisionExclusions.empty()&&restored.RuntimeDeformable(owners[1],error)->carrierCollisionExclusions.size()==2,"restored panels rebuild collision exclusions from actual connection state");
    auto retired=chunks;retired["deformables"].version=6;
    RuntimeWorld rejected;
    if(!rejected.Build(restoredScene,&resources,error,nullptr,nullptr,true)||!WorldPersistence::Prepare(rejected,error))throw std::runtime_error(error);
    check(!WorldPersistence::Restore(rejected,retired,error),"retired incomplete material snapshot is explicitly rejected rather than inventing panel history");
}
