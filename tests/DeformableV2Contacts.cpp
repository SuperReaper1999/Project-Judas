#include "Deformable.h"
#include "UniformGravity.h"
#include "RadialTerrain.h"
#include "CollisionAsset.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

void RunDeformableV2ContactChecks(void(*check)(bool,const char*)){
    using V=glm::dvec3;
    auto sheet=std::make_shared<DeformableAsset>(MakeDeformableSheet(3,3,.3,.3));
    DeformableSettings config;config.selfContact=false;config.material.damping=0;
    UniformGravity zero({0,0,0});
    {
        // More reacting bodies than the bounded interval wake publication can
        // retain. Opposing thin plates exercise real projection and reactions;
        // overflow must keep waking, including a later attachment recipient.
        PhysicsWorld world;world.Init();std::vector<BodyHandle> plates;
        for(unsigned i=0;i<70;++i){auto body=world.CreateDynamicBox({0,0,i%2?-.11f:.11f},{2,2,.1f},10000,0,0);
            world.SetCollisionFilter(body,0,CategoryBit(1));plates.push_back(body);}
        auto mount=world.CreateDynamicBox({0,0,10},{.1f,.1f,.1f},10000,0,0);
        world.SetCollisionFilter(mount,0,CategoryBit(1));
        for(unsigned step=0;step<120;++step)world.Step(1.f/60);
        check(std::all_of(plates.begin(),plates.end(),[&](auto b){return world.IsSleeping(b);})&&world.IsSleeping(mount),"overflow fixture starts with sleeping independent rigid recipients");
        auto settings=config;settings.collisionLayer=1;settings.collisionMask=CategoryBit(0);settings.substeps=1;settings.iterations=2;
        DeformableAttachment pin;pin.group="top";pin.kind=DeformableAttachment::Kind::Body;pin.target=1;settings.attachments={pin};
        DeformableInstance material;material.Initialize(sheet,settings,glm::dmat4(1),701);
        DeformableTarget target;target.valid=true;target.body=mount;
        material.Step(1./60,world,zero,{target});
        check(material.error.empty()&&material.stats.contacts>=70,"dense material contacts survive bounded wake-publication overflow");
        check(std::all_of(plates.begin(),plates.end(),[&](auto b){return !world.IsSleeping(b);})&&!world.IsSleeping(mount),"every reacting body and the later attachment wake despite full publication storage");
        check(glm::length(world.GetLinearVelocity(plates.back()))>0&&glm::length(world.GetLinearVelocity(mount))>0,"overflow fallback preserves collision and attachment impulses");
    }
    {
        PhysicsWorld world;world.Init();std::vector<CompoundBox> boxes;
        for(unsigned i=0;i<37;++i)boxes.push_back({{0,0,.1f},{1,1,.1f}});
        auto body=world.CreateShape(Shape::Compound(boxes),{{0,0,0},{1,0,0,0}},false,0,0,0);
        DeformableInstance material;material.Initialize(sheet,config,glm::dmat4(1),702);
        material.Step(1./60,world,zero,{});
        check(material.error.empty()&&material.stats.contacts>0,"large compound retains full material contacts beyond preparation-cache bounds");
        world.ResetBody(body,{0,0,10},{1,0,0,0});
        for(auto& p:material.positions)p.z=0;for(auto& v:material.velocities)v=V(0);
        material.Step(1./60,world,zero,{});
        check(material.error.empty()&&material.stats.contacts==0,"next material step cannot reuse stale large-compound geometry");
    }
    for(bool rotated:{false,true}){
        PhysicsWorld world;check(world.Init(),"V2 capsule contact world initializes");
        auto rotation=rotated?glm::angleAxis(.7f,glm::vec3(0,0,1)):glm::quat(1,0,0,0);
        auto body=world.CreateShape(Shape::Capsule(.5f,.5f),{{.25f,.15f,0},rotation},false,0,0,0);
        if(rotated)check(!world.SetMotionType(body,BodyMotionType::Kinematic),"capsule mass/motion limitation is explicit rather than silently claiming kinematic support");
        DeformableInstance material;material.Initialize(sheet,config,glm::dmat4(1),1);
        material.Step(1./60,world,zero,{});
        check(material.error.empty()&&material.stats.contacts>0,"soft surface samples collide with the actual capsule surface");

    }
    {
        PhysicsWorld world;world.Init();
        auto terrain=std::make_shared<RadialTerrain>(1,[](const glm::vec3&){return 0.f;},0);
        world.CreateStaticTerrain({0,0,0},{1,0,0,0},terrain,0,0);
        DeformableInstance material;material.Initialize(sheet,config,glm::translate(glm::dmat4(1),V(.7,0,0)),2);
        material.Step(1./60,world,zero,{});
        check(material.error.empty()&&material.stats.contacts>0,"radial terrain uses the authoritative analytic surface for soft contacts");
    }
    {
        PhysicsWorld world;world.Init();DeformableInstance material;material.Initialize(sheet,config,glm::dmat4(1),3);
        for(unsigned i=0;i<90;++i)material.Step(1./60,world,zero,{});
        check(material.sleeping,"quiet soft material enters sleep");
        auto remote=world.CreateStaticSphere({100,.15f,0},.5f,0,0);
        material.Step(1./60,world,zero,{});
        check(material.sleeping,"unrelated static geometry does not wake a remote soft body");
        world.ResetBody(remote,{.25f,.15f,0},{1,0,0,0});
        material.Step(1./60,world,zero,{});
        check(!material.sleeping&&material.stats.contacts>0,"moving static geometry into a sleeping patch wakes and resolves real contact");
    }
    {
        PhysicsWorld world;world.Init();DeformableInstance material;material.Initialize(sheet,config,glm::dmat4(1),4);
        for(unsigned i=0;i<90;++i)material.Step(1./60,world,zero,{});
        world.CreateStaticSphere({.25f,.15f,0},.5f,0,0);
        material.Step(1./60,world,zero,{});
        check(!material.sleeping&&material.stats.contacts>0,"new overlapping static geometry wakes a sleeping soft body");
    }
    {
        PhysicsWorld world;world.Init();auto body=world.CreateDynamicBox({0,0,0},{.1f,.1f,.1f},2,0,0);world.SetCollisionFilter(body,0,0);
        for(unsigned i=0;i<120;++i)world.Step(1.f/60);
        check(world.IsSleeping(body),"stationary dynamic attachment target is genuinely asleep");
        auto settings=config;settings.collisionMask=0;DeformableAttachment pin;pin.group="top";pin.kind=DeformableAttachment::Kind::Body;pin.target=1;settings.attachments={pin};
        DeformableInstance material;material.Initialize(sheet,settings,glm::dmat4(1),5);
        DeformableTarget target;target.valid=true;target.body=body;
        for(unsigned i=0;i<90;++i)material.Step(1./60,world,zero,{target});
        check(material.sleeping&&world.IsSleeping(body),"resting dynamic attachment does not prohibit soft sleep");
        world.ApplyLinearImpulse(body,{1,0,0});material.Step(1./60,world,zero,{target});
        check(!material.sleeping,"an actual dynamic attachment impulse wakes its soft material");
    }
    {
        PhysicsWorld world;world.Init();
        auto cook=[&](float z){MeshData mesh;for(auto p:{glm::vec3(-1,-1,z),glm::vec3(1,-1,z),glm::vec3(1,1,z),glm::vec3(-1,1,z)}){MeshVertex v;v.position=p;mesh.vertices.push_back(v);}mesh.indices={0,1,2,0,2,3};CollisionCookSettings settings;settings.twoSided=true;auto asset=std::make_shared<CollisionAsset>();std::string error;check(CookCollision(mesh,settings,*asset,error),"edited collision fixture cooks through the real mesh pipeline");return asset;};
        auto body=world.CreateShape(Shape::Cooked(cook(-.1f),""),{{0,0,0},{1,0,0,0}},false,0,0,0);
        DeformableInstance material;material.Initialize(sheet,config,glm::dmat4(1),6);
        for(unsigned i=0;i<90;++i)material.Step(1./60,world,zero,{});
        check(material.sleeping,"nearby but separated cooked geometry permits material sleep");
        check(world.ReplaceStaticCollision(body,cook(0)),"edited mesh publishes on the same body identity");
        material.Step(1./60,world,zero,{});
        check(!material.sleeping&&material.stats.contacts>0,"same-body cooked terrain replacement wakes and contacts sleeping material");
    }

}
