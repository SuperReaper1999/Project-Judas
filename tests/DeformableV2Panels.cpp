#include "Deformable.h"
#include "SaveArchive.h"
#include "Scene.h"
#include "UniformGravity.h"
#include "deformables/PanelCook.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>

void RunDeformableV2PanelChecks(void(*check)(bool,const char*)){
    {
        auto fine=MakeDeformableSheet(8,8,2,1,2).render;
        for(auto& v:fine.vertices){v.position.z=.15f*v.position.x*v.position.x;v.normal=glm::normalize(glm::vec3(-.3f*v.position.x,0,1));}
        fine.materials.resize(1);fine.materialKeys={"original-panel"};fine.primitives={{0,unsigned(fine.indices.size()),0,"panel",-1}};
        fine.materials[0].baseColor={.2f,.4f,.7f,1};
        DeformableAsset cooked;std::string error;
        check(CookDeformablePanel(fine,{4,4,2},cooked,error)&&cooked.nodes.size()==16&&cooked.render.vertices.size()==fine.vertices.size(),"detailed curved panel cooking bounds physical resolution independently of render detail");
        auto encoded=EncodeDeformableAsset(cooked);DeformableAsset decoded;
        check(DecodeDeformableAsset({encoded.begin(),encoded.end()},decoded,error)&&decoded.panelBinding&&decoded.render.primitives[0].part=="panel"&&decoded.render.materialKeys==fine.materialKeys&&decoded.render.materials[0].baseColor==fine.materials[0].baseColor,"panel asset preserves its detailed exterior, part identity and source material through the normal model codec");
        auto data=std::make_shared<DeformableAsset>(decoded);DeformableSettings config;config.panel.enabled=true;
        DeformableInstance instance;
        auto rotation=glm::rotate(glm::dmat4(1),.7,glm::normalize(glm::dvec3(1,2,3)));
        auto transform=glm::translate(glm::dmat4(1),glm::dvec3(3,4,5))*rotation;
        instance.Initialize(data,config,transform,99);MeshData mapped;instance.MapRender(1,mapped);
        double maximum=0,normalError=0;
        for(unsigned i=0;i<fine.vertices.size();++i){maximum=std::max(maximum,glm::length(glm::dvec3(mapped.vertices[i].position)-glm::dvec3(transform*glm::dvec4(fine.vertices[i].position,1))));normalError=std::max(normalError,glm::length(glm::dvec3(mapped.vertices[i].normal)-glm::dmat3(rotation)*glm::dvec3(fine.vertices[i].normal)));}
        check(maximum<1e-6&&normalError<1e-6,"detailed panel rest residuals and smooth normals follow the actual rotated physical frame without snapping to grid");
        auto invalid=decoded;invalid.binding[0].nodes.y=invalid.binding[0].nodes.x;
        check(!PrepareDeformableAsset(invalid,error),"degenerate detailed binding is rejected before rendering");
    }
    auto asset=std::make_shared<DeformableAsset>(MakeDeformableSheet(4,4,1,1));
    asset->groups["latch"]={5};asset->groups["hinge"]={0};
    DeformableSettings config;config.panel.enabled=true;config.material.damping=0;
    DeformableAttachment latch;latch.group="latch";latch.tearWork=1;
    DeformableAttachment hinge;hinge.group="hinge";hinge.tearWork=100;
    config.attachments={latch,hinge};
    DeformableInstance panel;panel.Initialize(asset,config,glm::dmat4(1),1);
    check(panel.panelBending&&panel.panelBending->stiffness==2000,"normal engine initialization owns the authored panel material");
    check(!panel.CommitPanelFailures(),"undamaged sheet does not release its physical mounts");
    unsigned damaged=~0u;
    for(unsigned i=0;i<panel.panelBending->hinges.size();++i){auto& h=panel.panelBending->hinges[i];for(unsigned k=0;k<4;++k)if(h.nodes[k]==5)damaged=i;}
    if(damaged==~0u)throw std::runtime_error("procedural test patch has no local hinge");
    // Generate actual constitutive plastic work, not a damage counter or speed trigger.
    auto& law=*panel.panelBending;auto& h=law.hinges[damaged];
    glm::dvec3 points[4],gradient[4];double angle=0;
    for(unsigned k=0;k<4;++k)points[k]=panel.positions[h.nodes[k]];
    PanelBending::AngleGradient(points,angle,gradient);h.rest=angle-.7;
    law.Plastic(panel.positions,1./60);
    auto positions=panel.positions,velocities=panel.velocities;
    check(panel.AttachmentWork(0)>1&&panel.AttachmentWork(1)<100,"mount damage is local accumulated irreversible bending work");
    check(panel.CommitPanelFailures()&&panel.released[0]&&!panel.released[1],"a weak local mounting sheet can fail while its remaining hinge stays attached");
    check(panel.positions==positions&&panel.velocities==velocities,"mount failure adds no ejection impulse or transform correction");
    config.attachments[0].part="front";config.attachments[0].kind=DeformableAttachment::Kind::Body;config.attachments[0].target=12;
    SceneObject authored;authored.deformable=config;auto properties=DeformableProperties(authored);SceneObject restored;std::string error;
    check(ApplyDeformableProperties(properties,restored,error)&&restored.deformable->panel.enabled&&restored.deformable->attachments[0].part=="front"&&restored.deformable->attachments[0].tearWork==1,"scene panel material and stable section mount round trip without solver arrays");
    properties["deformable.attachmentMounts"]="1 \"missing\" \"front\" 1";
    check(!ApplyDeformableProperties(properties,restored,error),"scene mount rejects an unknown physical group");
    auto invalid=config;invalid.panel.stiffness=std::numeric_limits<double>::infinity();
    check(!ValidDeformableSettings(invalid,error),"nonfinite panel material is rejected before construction");
    SaveArchive snapshot;panel.Persist(snapshot);
    DeformableInstance cold;cold.Initialize(asset,config,glm::dmat4(1),2);
    SaveArchive read(snapshot.bytes);cold.Persist(read);read.Finish();
    check(cold.positions==panel.positions&&cold.velocities==panel.velocities&&cold.released==panel.released&&cold.panelBending->hinges[damaged].work==h.work&&cold.panelBending->hinges[damaged].rest==h.rest,"fresh engine instance restores plastic shape, local work and irreversible mount failure");
    check(!cold.CommitPanelFailures(),"restored release is not emitted a second time");
    PhysicsWorld physics;physics.Init();UniformGravity zero({0,0,0});
    panel.Step(1./60,physics,zero,{});cold.Step(1./60,physics,zero,{});
    check(panel.error.empty()&&cold.error.empty()&&cold.positions==panel.positions&&cold.velocities==panel.velocities,"restored panel continues the same physical solve");
}
