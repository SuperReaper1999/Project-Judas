#include "../tools/crash_proof/CrashRig.h"
#include "../tools/crash_proof/CrashPanels.h"
#include "SaveArchive.h"
#include "ResourceManager.h"
#include "RuntimeWorld.h"
#include "Simulation.h"
#include "WorldPersistence.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>
#include <stdexcept>

unsigned RunStructuralCrashChecks() {
    unsigned checks = 0;
    auto check = [&](bool value, const std::string &label) {
        ++checks;
        if (!value)
            throw std::runtime_error(label);
        std::cout << "PASS " << label << '\n';
    };
    struct Scratch {
        std::filesystem::path path =
            std::filesystem::temp_directory_path() /
            ("judas-structural-crash-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ~Scratch() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } scratch;
    std::filesystem::create_directories(scratch.path / "Assets");
    auto asset = MakeCrashStructure();
    {
        std::ofstream out(scratch.path / "Assets/structure.judasdeform", std::ios::binary);
        out << EncodeDeformableAsset(asset);
    }
    AssetDatabase database;
    database.Scan(scratch.path.string(), (scratch.path / "Assets").string());
    AssetRecord record;
    std::string error;
    check(database.Track((scratch.path / "Assets/structure.judasdeform").string(), record, error),
          "original procedural crash asset tracks");
    ResourceManager resources(nullptr, &database);
    resources.RequestDeformable(record.id);
    resources.WaitForAll();
    // Engine-owned procedural exterior; downloaded meshes are never test inputs.
    MeshData exterior = asset.render;
    InputSystem input;
    auto work = [](const DeformableInstance &s) {
        return std::accumulate(s.fracture.plasticWork.begin(), s.fracture.plasticWork.end(), 0.);
    };
    for (const std::string kind : {"frontal", "offset", "side", "rear", "cars"}) {
        Scene scene;
        auto &ground = scene.CreateObject("Ground");
        ground.body = SceneBodyComponent{};
        ground.body->halfExtents = {100, .5f, 100};
        ground.transform.position = {0, -.5f, 0};
        auto &gravity = scene.CreateObject("Gravity");
        gravity.gravity = SceneGravityComponent{};
        gravity.gravity->kind = SceneGravityKind::Uniform;
        gravity.gravity->regionRadius = 10000;
        std::vector<CrashRig> rigs;
        rigs.push_back(AddCrashRig(scene, asset, record.id, {0, 1.02f, 0}, exterior));
        SceneObjectId obstacle = 0;
        if (kind == "cars") {
            rigs.push_back(AddCrashRig(scene, asset, record.id, {0, 1.02f, -12}, exterior));
            scene.Find(rigs[1].owner)->transform.rotation =
                glm::angleAxis(glm::pi<float>(), glm::vec3(0, 1, 0));
        } else {
            auto &wall = scene.CreateObject("Obstacle");
            obstacle = wall.id;
            wall.body = SceneBodyComponent{};
            wall.body->halfExtents =
                kind == "side" ? glm::vec3(.18f, 1, .18f) : glm::vec3(kind == "offset" ? .7f : 3.f, 1, .4f);
            wall.transform.position =
                kind == "side" ? glm::vec3(4, 1, 0)
                               : glm::vec3(kind == "offset" ? 1.2f : 0, 1, kind == "rear" ? 7.f : -7.f);
        }
        RuntimeWorld world;
        check(world.Build(scene, &resources, error), kind + " real MK2 structure builds: " + error);
        world.UpdateDeformables(1. / 60);
        for (unsigned step = 0; step < 120; ++step)
            StepAuthoritativeWorld(world, input, 1.f / 60);
        for (auto &rig : rigs) {
            auto *s = world.RuntimeDeformable(rig.owner, error);
            check(s && s->error.empty() && work(*s) == 0,
                  kind + " quiet support does not create permanent damage");
        }
        double mass = 0;
        for (unsigned i = 0; i < rigs.size(); ++i)
            for (unsigned p = 0; p < asset.fracture->parts.size(); ++p) {
                auto body = world.RuntimeBody(world.FracturePartEntity(rigs[i].owner, p));
                mass += world.Physics().GetMass(body);
                world.Physics().SetLinearVelocity(
                    body, kind == "side"
                              ? glm::vec3(12, 0, 0)
                              : glm::vec3(0, 0, kind == "rear" || (kind == "cars" && i == 1) ? 12 : -12));
            }
        check(std::abs(mass - 1500 * rigs.size()) < .01,
              kind + " physical graph owns the full vehicle mass exactly once");
        std::set<unsigned> obstacleBodies;
        if (obstacle)
            obstacleBodies.insert(world.RuntimeBody(obstacle).id);
        else
            for (unsigned p = 0; p < asset.fracture->parts.size(); ++p)
                obstacleBodies.insert(world.RuntimeBody(world.FracturePartEntity(rigs[1].owner, p)).id);
        bool collision = false;
        double peak = 0;
        for (unsigned step = 0; step < 240; ++step) {
            StepAuthoritativeWorld(world, input, 1.f / 60);
            for (auto &rig : rigs) {
                auto *s = world.RuntimeDeformable(rig.owner, error);
                if (!s || !s->error.empty())
                    throw std::runtime_error(kind + " structural step failed: " + error);
                peak = std::max(peak, work(*s));
            }
            if (!collision) {
                for (unsigned p = 0; p < asset.fracture->parts.size(); ++p) {
                    auto body = world.RuntimeBody(world.FracturePartEntity(rigs[0].owner, p));
                    std::vector<PhysicsWorld::BodyContactSample> contacts;
                    if (world.Physics().ReadBodyContactSample(body, contacts))
                        for (auto &contact : contacts)
                            if (obstacleBodies.count(contact.other.id))
                                collision = true;
                }
            }
        }
        check(collision && peak > 100, kind + " actual collision produces finite irreversible plastic work");
        auto *original = world.RuntimeDeformable(rigs[0].owner, error);
        auto plastic = original->fracture.plasticTranslation;
        auto rotations = original->fracture.plasticRotation;
        auto broken = original->fracture.broken;
        auto reference = original->positions;
        std::map<std::string, SaveChunk> chunks;
        check(WorldPersistence::Capture(world, chunks, error),
              kind + " current snapshot captures damaged vehicle: " + error);
        Scene restoredScene;
        check(WorldPersistence::SceneFrom(chunks, restoredScene, error),
              kind + " snapshot reconstructs its scene");
        RuntimeWorld restored;
        check(restored.Build(restoredScene, &resources, error, nullptr, nullptr, true) &&
                  WorldPersistence::Prepare(restored, error) &&
                  WorldPersistence::Restore(restored, chunks, error),
              kind + " damaged physical family restores: " + error);
        WorldPersistence::Published(restored);
        bool wheelsPublished=true;
        for(auto id:rigs[0].wheels){auto* wheel=restored.RuntimeWheel(id);wheelsPublished&=wheel&&wheel->state.available;}
        check(wheelsPublished,kind+" restored structural wheel visuals publish before the first physics step");
        auto *copy = restored.RuntimeDeformable(rigs[0].owner, error);
        double maximum = 0;
        for (unsigned n = 0; n < reference.size(); ++n)
            maximum = std::max(maximum, glm::length(reference[n] - copy->positions[n]));
        check(maximum < 1e-6 && plastic == copy->fracture.plasticTranslation &&
                  rotations == copy->fracture.plasticRotation && broken == copy->fracture.broken,
              kind + " restoration preserves shape and permanent damage");
        for (unsigned step = 0; step < 30; ++step)
            StepAuthoritativeWorld(restored, input, 1.f / 60);
        check(restored.RuntimeDeformable(rigs[0].owner, error)->error.empty(),
              kind + " restored graph continues through MK2");
        std::cout << "CRASH_CONTROL " << kind << " plastic_work_j=" << peak
                  << " restored_shape_error_m=" << maximum << '\n';
    }
    {
        Scene scene;
        auto& ground=scene.CreateObject("Panel fixture ground");ground.body=SceneBodyComponent{};
        ground.body->halfExtents={30,.5f,30};ground.transform.position={0,-.5f,0};
        auto& gravity=scene.CreateObject("Gravity");gravity.gravity=SceneGravityComponent{};
        gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionRadius=1000;
        auto rig=AddCrashRig(scene,asset,record.id,{0,.665f,0},exterior);
        auto& config=*scene.Find(rig.owner)->deformable;config.material.density*=1475./1500.;config.collisionLayer=8;
        auto& obstacle=scene.CreateObject("Panel fixture wall");obstacle.body=SceneBodyComponent{};
        obstacle.body->halfExtents={3,1,.4f};obstacle.transform.position={0,1,-5};
        ProjectClassification categories;categories.collision.names.emplace(8,"Frame");categories.collision.nextId=9;
        RuntimeWorld world;
        MeshData hood;
        for(auto p:{glm::vec3(-.7f,.1f,-2.35f),glm::vec3(.7f,.1f,-2.35f),glm::vec3(.7f,.1f,-.95f),glm::vec3(-.7f,.1f,-.95f)}){MeshVertex v;v.position=p;v.normal={0,1,0};hood.vertices.push_back(v);}
        hood.indices={0,2,1,0,3,2};
        CrashPanel panel;panel.Author(scene,asset,rig,database,resources,hood);
        check(world.Build(scene,&resources,error,&categories),"ordinary scene panel and physical frame build");world.PrepareDeformableMechanics();panel.Bind(world);
        double mass=panel.Material().Mass();for(unsigned p=0;p<asset.fracture->parts.size();++p)mass+=world.Physics().GetMass(world.RuntimeBody(world.FracturePartEntity(rig.owner,p)));
        check(std::abs(mass-1500)<.01,"physical panel and structural frame count total mass once");
        check(panel.Material().settings.attachments.size()==3,"physical panel has two real hinges and a finite front latch");
        for(unsigned step=0;step<180;++step){StepAuthoritativeWorld(world,input,1.f/60);panel.Observe();}
        check(panel.PlasticExtent()<1e-6,"supported procedural bonnet does not plastically sag under ordinary gravity");
        for(unsigned p=0;p<asset.fracture->parts.size();++p)world.Physics().SetLinearVelocity(world.RuntimeBody(world.FracturePartEntity(rig.owner,p)),{0,0,-12});
        for(auto& v:panel.Material().velocities)v={0,0,-12};
        unsigned contacts=0;double peakPlastic=0;
        for(unsigned step=0;step<240;++step){StepAuthoritativeWorld(world,input,1.f/60);panel.Observe();contacts+=unsigned(panel.Material().stats.contacts);peakPlastic=std::max(peakPlastic,panel.PlasticExtent());}
        check(contacts>0,"physical panel receives real collision samples during barrier impact");
        check(peakPlastic>1e-5,"barrier impact produces internal permanent panel bending");
        SaveArchive stored;panel.Persist(stored);
        std::map<std::string,SaveChunk> chunks;check(WorldPersistence::Capture(world,chunks,error),"damaged frame and panel capture through current persistence");
        Scene restoredScene;check(WorldPersistence::SceneFrom(chunks,restoredScene,error),"panel fixture snapshot reconstructs native scene");
        RuntimeWorld restored;check(restored.Build(restoredScene,&resources,error,&categories,nullptr,true),"panel fixture restores with current categories");
        check(WorldPersistence::Prepare(restored,error)&&WorldPersistence::Restore(restored,chunks,error),"panel fixture restores actual MK2 structural state");WorldPersistence::Published(restored);
        CrashPanel restoredPanel;restoredPanel.Bind(restored,panel.owner);
        SaveArchive again;restoredPanel.Persist(again);check(again.bytes==stored.bytes,"panel positions velocities plastic volume rest and physical mounts restore exactly");
        for(unsigned step=0;step<10;++step){StepAuthoritativeWorld(restored,input,1.f/60);restoredPanel.Observe();}
        check(restoredPanel.Material().error.empty(),"restored panel continues physical coupling");
    }
    {
        // Isolate publication of physical material history from its already
        // independently tested return mapping and real crash generation.
        Scene scene;auto& ground=scene.CreateObject("Detached panel ground");ground.body=SceneBodyComponent{};
        ground.body->halfExtents={150,.5f,150};ground.transform.position={0,-.5f,0};
        auto& gravity=scene.CreateObject("Gravity");gravity.gravity=SceneGravityComponent{};
        gravity.gravity->kind=SceneGravityKind::Uniform;gravity.gravity->regionRadius=1000;
        auto rig=AddCrashRig(scene,asset,record.id,{0,.665f,0},exterior);
        scene.Find(rig.owner)->deformable->collisionLayer=8;
        ProjectClassification categories;categories.collision.names.emplace(8,"Frame");categories.collision.nextId=9;
        RuntimeWorld world;
        MeshData hood;for(auto p:{glm::vec3(-.7f,.1f,-2.35f),glm::vec3(.7f,.1f,-2.35f),glm::vec3(.7f,.1f,-.95f),glm::vec3(-.7f,.1f,-.95f)}){MeshVertex v;v.position=p;hood.vertices.push_back(v);}hood.indices={0,2,1,0,3,2};
        CrashPanel panel;panel.Author(scene,asset,rig,database,resources,hood);
        check(world.Build(scene,&resources,error,&categories),"ordinary scene panel and physical frame build");world.PrepareDeformableMechanics();panel.Bind(world);
        auto& hinges=panel.Material().panelBending->hinges;
        auto localHinge=[&](unsigned mount){
            const auto& group=panel.Material().asset->groups.at(panel.Material().settings.attachments[mount].group);
            for(unsigned h=0;h<hinges.size();++h){unsigned own=0,other=0;for(unsigned k=0;k<4;++k){auto n=hinges[h].nodes[k];own+=std::find(group.begin(),group.end(),n)!=group.end();for(unsigned a=0;a<3;++a)if(a!=mount){const auto& g=panel.Material().asset->groups.at(panel.Material().settings.attachments[a].group);other+=std::find(g.begin(),g.end(),n)!=g.end();}}if(own&&!other)return std::pair<unsigned,unsigned>{h,own};}
            throw std::runtime_error("no isolated attachment ligament");
        };
        auto history=[&](unsigned index,double work){
            auto& hinge=hinges[index];const auto& law=*panel.Material().panelBending;
            hinge.work=work;
            // Admissible accumulated plastic history: W = Y p + H p²/2.
            hinge.plastic=2*work/(law.yieldMoment+std::sqrt(law.yieldMoment*law.yieldMoment+2*law.hardening*work));
        };
        auto left=localHinge(1);history(left.first,(CrashPanel::MountTearWork-.1)*4/left.second);
        check(!panel.CommitMountFailures()&&!panel.Material().released[1],"subcritical local plastic work does not tear the attachment");
        const auto positions=panel.Material().positions,velocities=panel.Material().velocities;const double mass=panel.Material().Mass();
        history(left.first,(CrashPanel::MountTearWork+.1)*4/left.second);
        check(panel.CommitMountFailures()&&panel.Material().released==std::vector<uint8_t>({0,1,0}),"local left mounting-sheet work tears only that physical connection");
        check(panel.Material().positions==positions&&panel.Material().velocities==velocities&&panel.Material().Mass()==mass,"publishing a tear preserves positions momentum and finite mass");
        check(!panel.CommitMountFailures(),"already published damage cannot release a connection twice");
        check(!panel.Material().carrierCollisionExclusions.empty(),"a remaining attachment retains its exact connected-carrier collision exclusions");
        SaveArchive partial;panel.Persist(partial);
        auto restoreNative=[&](RuntimeWorld& target){
            std::map<std::string,SaveChunk> chunks;if(!WorldPersistence::Capture(world,chunks,error))throw std::runtime_error(error);
            Scene state;if(!WorldPersistence::SceneFrom(chunks,state,error)||!target.Build(state,&resources,error,&categories,nullptr,true)||!WorldPersistence::Prepare(target,error)||!WorldPersistence::Restore(target,chunks,error))throw std::runtime_error(error);
            WorldPersistence::Published(target);
        };
        RuntimeWorld partialWorld;restoreNative(partialWorld);CrashPanel restored;restored.Bind(partialWorld,panel.owner);
        SaveArchive againPartial;restored.Persist(againPartial);
        check(partial.bytes==againPartial.bytes&&restored.Material().released==panel.Material().released,"partially failed mounts and local work restore exactly");
        for(unsigned i:{0u,2u}){auto h=localHinge(i);history(h.first,(CrashPanel::MountTearWork+.1)*4/h.second);}
        check(panel.CommitMountFailures()&&panel.Material().released==std::vector<uint8_t>({1,1,1}),"each remaining mounting sheet must reach its own failure work");
        auto* frame=world.RuntimeDeformable(rig.owner,error);
        check(!panel.AttachedTo(*frame,rig.referencePart)&&panel.Material().carrierCollisionExclusions.empty(),"fully detached bonnet has no chassis attachment and collides with its former vehicle");
        auto centre=panel.Center();
        for(unsigned part=0;part<asset.fracture->parts.size();++part){auto body=world.RuntimeBody(world.FracturePartEntity(rig.owner,part));auto pose=world.Physics().GetTransform(body);world.Physics().ResetBody(body,pose.position+glm::vec3(100,0,0),pose.rotation);}
        StepAuthoritativeWorld(world,input,1.f/60);panel.Observe();
        check(std::abs(panel.Center().x-centre.x)<1e-6&&panel.Material().Mass()==mass,"moving former carrier frames cannot move or duplicate detached material");
        unsigned contacts=0;for(unsigned step=0;step<120;++step){StepAuthoritativeWorld(world,input,1.f/60);panel.Observe();contacts+=unsigned(panel.Material().stats.contacts);}
        check(panel.Material().error.empty()&&contacts>0&&panel.Center().y>-.1,"independent finite-mass bonnet falls onto and collides with real ground");
        SaveArchive full;panel.Persist(full);RuntimeWorld fullWorld;restoreNative(fullWorld);CrashPanel fullCopy;fullCopy.Bind(fullWorld,panel.owner);SaveArchive againFull;fullCopy.Persist(againFull);
        check(full.bytes==againFull.bytes&&fullCopy.Material().carrierCollisionExclusions.empty(),"fully detached material restores its physical state and former-car collision policy");
    }
    return checks;
}
