#include "Deformable.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "WorldPersistence.h"
#include "Project.h"
#include "SceneSerialization.h"
#include <cstdlib>
#include "deformables/PanelCook.h"
#include "deformables/StructuralSkin.h"
#include "ModelArchive.h"
#include "SceneFingerprint.h"
#include "Simulation.h"
#include "terrain/TerrainBrush.h"
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
    // Original engine-owned detail, loaded through the ordinary resource path.
    std::vector<uint8_t> sourceBytes;if(!EncodeModelArchive(frame.render,sourceBytes,error))throw std::runtime_error(error);
    {std::ofstream out(root/"body.judasmodel",std::ios::binary);out.write(reinterpret_cast<const char*>(sourceBytes.data()),std::streamsize(sourceBytes.size()));}
    AssetRecord bodyRecord;if(!database.Track((root/"body.judasmodel").string(),bodyRecord,error))throw std::runtime_error(error);
    frame.sourceAsset=bodyRecord.id;frame.sourceFingerprint=SceneFingerprintSha256(std::string_view(reinterpret_cast<const char*>(sourceBytes.data()),sourceBytes.size()));
    {std::ofstream out(root/"frame.judasdeform",std::ios::binary);out<<EncodeDeformableAsset(frame);}
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
    auto* chassis=world.RuntimeDeformable(carrier,error);
    check(chassis&&chassis->asset->structuralSkin,"ordinary structural asset loads its shared detailed skin through ResourceManager");
    if(chassis&&chassis->asset->structuralSkin){
        MeshData mapped;chassis->asset->structuralSkin->BuildGpuMesh(*chassis,mapped);
        std::vector<glm::mat4> palette;chassis->asset->structuralSkin->Palette(*chassis,1,palette);
        check(!mapped.indices.empty()&&mapped.skinVertices.size()==mapped.vertices.size()&&palette.size()==frame.fracture->parts.size(),"native structural presentation uses the physical section palette and detailed source topology");
        check(resources.BytesOf(frameRecord.id)>=chassis->asset->structuralSkin->Bytes(),"shared detailed structural payload is included in resource residency accounting");
    }
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
    {
        JobSystem jobs(2);ResourceManager terrainResources(nullptr,&database,&jobs);
        terrainResources.RequestDeformable(panelRecord.id);terrainResources.WaitForAll();
        Scene terrainScene;auto& ground=terrainScene.CreateObject("Editable Terrain V2 support");ground.body=SceneBodyComponent{};auto terrainId=ground.id;
        auto& falling=terrainScene.CreateObject("Native physical panel");auto materialId=falling.id;
        falling.transform.position={0,1,0};falling.deformable=DeformableSettings{};falling.deformable->asset=panelRecord.id;falling.deformable->panel.enabled=true;falling.deformable->selfContact=false;
        falling.deformable->material.stretchCompliance=falling.deformable->material.shearCompliance=1e-9;
        auto& gravity=terrainScene.CreateObject("Gravity");gravity.gravity=SceneGravityComponent{};gravity.gravity->kind=SceneGravityKind::Uniform;
        RuntimeWorld terrainWorld;if(!terrainWorld.Build(terrainScene,&terrainResources,error))throw std::runtime_error(error);
        if(!terrainWorld.LiveTerrain().CreateLandscape(terrainId,TerrainV2::Create(17,17,.5f),error)||!terrainWorld.LiveTerrain().Wait(terrainId,error))throw std::runtime_error(error);
        InputSystem input;for(unsigned step=0;step<240;++step)StepAuthoritativeWorld(terrainWorld,input,1.f/60);
        auto* patch=terrainWorld.RuntimeDeformable(materialId,error);
        double restSpeed=0;for(auto v:patch->velocities)restSpeed=std::max(restSpeed,glm::length(v));
        std::printf("TERRAIN_PANEL_REST min=%.9g max=%.9g speed=%.9g sleeping=%d contacts=%zu\n",patch->Minimum().y,patch->Maximum().y,restSpeed,int(patch->sleeping),patch->stats.contacts);
        check(patch&&patch->error.empty()&&patch->Minimum().y>0&&patch->Maximum().y<.1,"native physical panel lands on actual Terrain V2 collision instead of falling through");
        check(patch&&patch->sleeping,"material supported by quiet Terrain V2 can naturally sleep");
        TerrainV2::SurfaceBrush brush;brush.radius=2;brush.strength=.35f;
        if(!terrainWorld.LiveTerrain().Surface(terrainId,{0,0,0},brush,error)||!terrainWorld.LiveTerrain().Wait(terrainId,error))throw std::runtime_error(error);
        StepAuthoritativeWorld(terrainWorld,input,1.f/60);
        std::printf("TERRAIN_PANEL_EDIT min=%.9g max=%.9g sleeping=%d contacts=%zu error=%s\n",patch->Minimum().y,patch->Maximum().y,int(patch->sleeping),patch->stats.contacts,patch->error.c_str());
        check(!patch->sleeping&&patch->stats.contacts>0&&patch->Maximum().y>.2,"edited Terrain V2 wakes sleeping material and publishes real changed contacts");
        auto before=patch->positions;
        if(!terrainWorld.RequestOrigin({512,0,0},error)||!terrainWorld.CommitOrigin())throw std::runtime_error(error);
        bool shifted=patch->positions.size()==before.size();for(unsigned n=0;n<before.size();++n)shifted&=glm::length(patch->positions[n]-(before[n]-glm::dvec3(512,0,0)))<1e-9;
        check(shifted,"terrain and physical panel origin shift preserves every deformed node in the new local frame");
        StepAuthoritativeWorld(terrainWorld,input,1.f/60);
        check(patch->error.empty()&&patch->Minimum().y>0,"shifted Terrain V2 continues supporting the physical material");
    }
    {std::ofstream out(root/"body.judasmodel",std::ios::app|std::ios::binary);out<<"changed";}
    ResourceManager stale(nullptr,&database);stale.RequestDeformable(frameRecord.id);stale.WaitForAll();
    check(!stale.GetDeformable(frameRecord.id,error)&&error.find("stale deformable")!=std::string::npos,"source changes reject a structural binding before publication instead of rendering stale damage geometry");
}
