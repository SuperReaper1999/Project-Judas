#include "Deformable.h"
#include "RuntimeWorld.h"
#include "ResourceManager.h"
#include "Simulation.h"
#include <chrono>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

void RunDeformableV2CouplingChecks(void(*check)(bool,const char*)){
    struct Scratch {std::filesystem::path root=std::filesystem::temp_directory_path()/("judas-material-coupling-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));~Scratch(){std::error_code error;std::filesystem::remove_all(root,error);}} scratch;
    auto assets=scratch.root/"Assets";std::filesystem::create_directories(assets);
    auto sheet=MakeDeformableSheet(3,3,.4,.4);sheet.groups.clear();
    for(unsigned i=0;i<sheet.nodes.size();++i){sheet.nodes[i].y-=.2;sheet.groups["physical-mount"].push_back(i);}
    std::string error;if(!PrepareDeformableAsset(sheet,error))throw std::runtime_error(error);
    {std::ofstream file(assets/"sheet.judasdeform",std::ios::binary);file<<EncodeDeformableAsset(sheet);}
    AssetDatabase db;db.Scan(scratch.root.string(),assets.string());AssetRecord asset;
    if(!db.Track((assets/"sheet.judasdeform").string(),asset,error))throw std::runtime_error(error);
    ResourceManager resources(nullptr,&db);resources.RequestDeformable(asset.id);resources.WaitForAll();
    {
        Scene scene;auto& owner=scene.CreateObject("Unattached physical material without rigid actors");auto id=owner.id;
        owner.deformable=DeformableSettings{};owner.deformable->asset=asset.id;owner.deformable->material.density=12.5;owner.deformable->material.damping=0;owner.deformable->selfContact=false;
        RuntimeWorld world;if(!world.Build(scene,&resources,error))throw std::runtime_error(error);
        auto* material=world.RuntimeDeformable(id,error);if(!material)throw std::runtime_error(error);
        material->Force("",{12,0,0});InputSystem input;StepAuthoritativeWorld(world,input,1.f/60);
        glm::dvec3 momentum(0);for(unsigned n=0;n<material->positions.size();++n)momentum+=material->NodeMass(n)*material->velocities[n];
        check(material->error.empty()&&std::abs(momentum.x-.2)<1e-6&&material->stats.substeps==4,"unattached material advances and receives its full force even when MK2 has no rigid actors");
    }
    for(unsigned quality:{4u,16u}){
        Scene scene;auto& mount=scene.CreateObject("Free finite-mass rigid mount");auto bodyId=mount.id;
        mount.body=SceneBodyComponent{};mount.body->motion=SceneBodyMotion::Dynamic;mount.body->mass=2;mount.body->halfExtents={.2,.2,.05};mount.body->collisionMask=0;
        auto& panel=scene.CreateObject("Finite-mass attached material");auto materialId=panel.id;
        panel.deformable=DeformableSettings{};auto& config=*panel.deformable;config.asset=asset.id;config.substeps=quality;config.iterations=12;config.selfContact=false;config.collisionMask=0;config.material.density=12.5;config.material.damping=0;
        DeformableAttachment attachment;attachment.group="physical-mount";attachment.kind=DeformableAttachment::Kind::Body;attachment.target=bodyId;config.attachments={attachment};
        RuntimeWorld world;if(!world.Build(scene,&resources,error))throw std::runtime_error(error);
        auto* material=world.RuntimeDeformable(materialId,error);if(!material)throw std::runtime_error(error);
        auto body=world.RuntimeBody(bodyId);world.Physics().SetLinearVelocity(body,{1,0,0});
        InputSystem input;StepAuthoritativeWorld(world,input,1.f/60);
        glm::dvec3 momentum=2.*glm::dvec3(world.Physics().GetLinearVelocity(body));
        for(unsigned i=0;i<material->velocities.size();++i)momentum+=material->NodeMass(i)*material->velocities[i];
        auto pose=world.Physics().GetTransform(body);
        std::printf("NATIVE_MATERIAL_COUPLING quality=%u x=%.9g velocity=%.9g momentum_error=%.9g material_steps=%u\n",quality,pose.position.x,world.Physics().GetLinearVelocity(body).x,glm::length(momentum-glm::dvec3(2,0,0)),material->stats.substeps);
        check(material->error.empty()&&!material->PhysicalStepActive(),"authoritative world completes coupled material state at its fixed boundary");
        check(glm::length(momentum-glm::dvec3(2,0,0))<1e-4,"native soft/rigid coupling preserves equal and opposite linear momentum");
        check(std::abs(pose.position.x-1./120)<.001,"material reaction acts before MK2 pose advancement rather than one frame later");
        check(material->stats.substeps==quality,"local material accuracy does not add whole-world chronological subdivisions");
        for(unsigned step=0;step<120;++step)StepAuthoritativeWorld(world,input,1.f/60);
        auto linear=glm::dvec3(world.Physics().GetLinearVelocity(body)),angular=glm::dvec3(world.Physics().GetAngularVelocity(body));
        double energy=glm::dot(linear,linear)+.5*glm::dot(angular,glm::dmat3(world.Physics().GetInertiaWorld(body))*angular);
        glm::dvec3 center(0);momentum=2.*linear;
        for(unsigned n=0;n<material->positions.size();++n){auto mass=material->NodeMass(n);center+=mass*material->positions[n];momentum+=mass*material->velocities[n];energy+=.5*mass*glm::dot(material->velocities[n],material->velocities[n]);}
        center/=material->Mass();
        std::printf("SUSTAINED_MATERIAL_COUPLING quality=%u energy=%.9g momentum_error=%.9g attachment_drift=%.9g\n",quality,energy,glm::length(momentum-glm::dvec3(2,0,0)),glm::length(center-glm::dvec3(world.Physics().GetCenterOfMass(body))));
        check(material->error.empty()&&glm::length(momentum-glm::dvec3(2,0,0))<.002&&energy<=1.0002,"sustained physical coupling remains momentum-consistent and does not generate energy");
        check(glm::length(center-glm::dvec3(world.Physics().GetCenterOfMass(body)))<.005,"sustained shared motion does not accumulate a separated attachment pose");
        RuntimeWorld resting;if(!resting.Build(scene,&resources,error))throw std::runtime_error(error);
        for(unsigned step=0;step<100;++step)StepAuthoritativeWorld(resting,input,1.f/60);
        auto* quiet=resting.RuntimeDeformable(materialId,error);
        check(quiet&&quiet->sleeping&&resting.Physics().IsSleeping(resting.RuntimeBody(bodyId)),"a naturally stationary coupled material and its dynamic mount both reach sleep");

    }
    // A remote structural asset's local quality must not alter the clock or
    // trajectory of unrelated MK2 bodies anywhere else in the world.
    auto structure=MakeFractureBlock({2,1,1},{1,.2,.4},true);
    for(auto& connection:structure.fracture->bonds)connection.material.enabled=true;
    if(!PrepareDeformableAsset(structure,error))throw std::runtime_error(error);
    {std::ofstream file(assets/"structure.judasdeform",std::ios::binary);file<<EncodeDeformableAsset(structure);}
    AssetRecord structuralAsset;if(!db.Track((assets/"structure.judasdeform").string(),structuralAsset,error))throw std::runtime_error(error);
    resources.RequestDeformable(structuralAsset.id);resources.WaitForAll();
    std::vector<PhysicsWorld::BodyMotionSegment> reference;BodyTransform referencePose;
    for(unsigned quality:{2u,16u}){
        Scene scene;auto& frame=scene.CreateObject("Remote physical structure");frame.transform.position={100,0,0};
        frame.deformable=DeformableSettings{};frame.deformable->asset=structuralAsset.id;frame.deformable->substeps=quality;frame.deformable->iterations=quality;
        auto& independent=scene.CreateObject("Unrelated freely spinning body");auto id=independent.id;
        independent.body=SceneBodyComponent{};independent.body->motion=SceneBodyMotion::Dynamic;independent.body->halfExtents={.2,.3,.4};
        RuntimeWorld world;if(!world.Build(scene,&resources,error))throw std::runtime_error(error);
        auto body=world.RuntimeBody(id);world.Physics().SetLinearVelocity(body,{1,2,3});world.Physics().SetAngularVelocity(body,{4,5,6});
        InputSystem input;StepAuthoritativeWorld(world,input,1.f/60);
        auto segments=world.Physics().GetBodyMotionSegments(body);auto pose=world.Physics().GetTransform(body);
        if(quality==2){reference=segments;referencePose=pose;}
        bool same=segments.size()==reference.size()&&pose.position==referencePose.position&&pose.rotation==referencePose.rotation;
        for(unsigned n=0;same&&n<segments.size();++n)same=segments[n].begin==reference[n].begin&&segments[n].end==reference[n].end;
        check(!segments.empty()&&same,"remote deformable quality cannot change native MK2 motion intervals or unrelated rigid trajectories");
    }

}
