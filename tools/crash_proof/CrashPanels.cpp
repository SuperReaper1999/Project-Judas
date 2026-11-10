#include "CrashPanels.h"
#include "deformables/PanelCook.h"
#include "PanelIslands.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "GravityField.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <map>
#include <array>
#include <numeric>
#include <set>
#include <stdexcept>

MeshData ExtractCrashHood(MeshData& body,const std::vector<std::string>& additionalParts){
    std::set<std::string> requested(additionalParts.begin(),additionalParts.end()),found;
    MeshData hood=body, remaining=body;
    hood.vertices.clear();hood.indices.clear();hood.primitives.clear();
    remaining.vertices.clear();remaining.indices.clear();remaining.primitives.clear();
    const auto islands=CrashPanelIslands(body);
    struct Bounds{glm::vec3 lo{1e20},hi{-1e20};};std::map<unsigned,Bounds> bounds;
    for(unsigned t=0;t<body.indices.size();t+=3){auto& b=bounds[islands[t/3]];for(unsigned k=0;k<3;++k){auto v=body.vertices[body.indices[t+k]].position;b.lo=glm::min(b.lo,v);b.hi=glm::max(b.hi,v);}}
    for(const auto& p:body.primitives){
        unsigned h=unsigned(hood.indices.size()),r=unsigned(remaining.indices.size());
        for(unsigned t=0;t<p.count/3;++t){
            const auto& b=bounds.at(islands[p.first/3+t]);auto c=(b.lo+b.hi)*.5f;auto size=b.hi-b.lo;
            const bool paint=p.part.find("Carpaint")!=std::string::npos||p.part.find("CarPaint")!=std::string::npos;
            const bool explicitMember=requested.count(p.part)!=0;
            if(explicitMember)found.insert(p.part);
            const bool selected=explicitMember||(paint && std::abs(c.x)<.1f && c.z<-.9f && c.y>-.2f && c.y<.3f && size.x>1.f && size.z>.3f && size.y<.35f);
            auto& out=selected?hood:remaining;
            for(int k=0;k<3;++k){out.indices.push_back(unsigned(out.vertices.size()));out.vertices.push_back(body.vertices[body.indices[p.first+t*3+k]]);}
        }
        if(hood.indices.size()>h)hood.primitives.push_back({h,unsigned(hood.indices.size())-h,p.material,p.part,p.node});
        if(remaining.indices.size()>r)remaining.primitives.push_back({r,unsigned(remaining.indices.size())-r,p.material,p.part,p.node});
    }
    for(const auto& part:requested)if(!found.count(part))throw std::runtime_error("panel membership names an absent imported part: "+part);
    if(hood.indices.empty())throw std::runtime_error("authored hood region selected no geometry");
    auto compact=[](MeshData& mesh){
        std::map<int,int> slots;std::vector<MaterialDefinition> materials;std::vector<std::string> keys;
        for(auto& p:mesh.primitives)if(p.material>=0){
            auto [it,added]=slots.emplace(p.material,int(materials.size()));
            if(added){materials.push_back(std::move(mesh.materials.at(p.material)));keys.push_back(mesh.materialKeys.at(p.material));}
            p.material=it->second;
        }
        mesh.materials=std::move(materials);mesh.materialKeys=std::move(keys);
    };
    // The bonnet must not deep-copy every unrelated interior/trim image for
    // each car. Preserve every referenced slot and its exact material data.
    compact(hood);compact(remaining);
    body=std::move(remaining);return hood;
}
void CrashPanel::Initialize(RuntimeWorld& world,const CrashRig& rig,const MeshData& hood){
    using V=glm::dvec3;
    V lo(1e20),hi(-1e20);for(const auto& v:hood.vertices){lo=glm::min(lo,V(v.position));hi=glm::max(hi,V(v.position));}
    lo-=V(.001,0,.001);hi+=V(.001,0,.001);
    DeformableAsset a;std::string cookError;
    if(!CookDeformablePanel(hood,{6,6,1,.001},a,cookError))throw std::runtime_error("physical panel cook: "+cookError);
    a.groups.clear();
    for(unsigned n=0;n<a.nodes.size();++n){const auto p=a.nodes[n];
        if(p.z>hi.z-.01 && p.x<-.4)a.groups["hinge-left"].push_back(n);
        if(p.z>hi.z-.01 && p.x>.4)a.groups["hinge-right"].push_back(n);
        if(p.z<lo.z+.01 && std::abs(p.x)<.4)a.groups["front-latch"].push_back(n);
    }
    std::string error;if(!PrepareDeformableAsset(a,error))throw std::runtime_error(error);
    auto* frame=world.RuntimeDeformable(rig.owner,error);
    if(!frame||!frame->error.empty())throw std::runtime_error("frame is unavailable for panel mount authoring");
    DeformableSettings s;s.substeps=4;s.iterations=6;s.selfContact=true;
    double area=0;for(auto t:a.cloth)area+=t.area;
    s.material.density=25/area;s.material.stretchCompliance=1e-9;s.material.shearCompliance=1e-9;
    s.material.damping=.4;s.material.thickness=.012;
    // Stage-1 test deliberately excludes the coarse inner frame, whose volume
    // overlaps the shell. Each proof vehicle has a distinct registered layer, so other vehicles remain collidable.
    s.collisionMask=kAllCategories&~CategoryBit(frame->settings.collisionLayer);
    attachedCollisionMask=s.collisionMask;
    carrierParts.clear();targets.clear();
    for(const auto& [group,nodes]:a.groups){
        V centroid(0);for(auto n:nodes)centroid+=a.nodes[n]/double(nodes.size());
        unsigned part=0;double best=1e100;
        for(unsigned p=0;p<frame->asset->fracture->parts.size();++p){auto d=frame->asset->fracture->parts[p].proxyCenter-centroid;if(glm::dot(d,d)<best){best=glm::dot(d,d);part=p;}}
        carrierParts.push_back(part);
        auto entity=world.FracturePartEntity(rig.owner,part);auto body=world.RuntimeBody(entity);auto pose=world.Physics().GetTransform(body);
        DeformableAttachment mount;mount.group=group;mount.kind=DeformableAttachment::Kind::Body;
        mount.target=rig.owner;mount.part=frame->asset->fracture->parts[part].key;mount.offset=-frame->asset->fracture->parts[part].proxyCenter;
        mount.tearWork=MountTearWork;mount.forceLimit=group=="front-latch"?8000:20000;mount.torqueLimit=8000;s.attachments.push_back(mount);
        DeformableTarget target;target.valid=true;target.body=body;
        target.current=glm::translate(glm::dmat4(1),V(pose.position))*glm::mat4_cast(glm::dquat(pose.rotation));target.previous=target.current;targets.push_back(target);
    }
    auto parent=world.RuntimeDefinition(rig.owner)->transform;
    auto transform=glm::translate(glm::dmat4(1),V(parent.position))*glm::mat4_cast(glm::dquat(parent.rotation));
    shell=std::make_shared<DeformableAsset>(std::move(a));
    s.panel.enabled=true;
    material.Initialize(shell,s,transform,1);
    maximumSpeed=maximumKineticEnergy=0;
    peakForces.assign(targets.size(),0);peakMoments.assign(targets.size(),0);
    Map();
    std::cout<<"PANEL_INITIALIZED nodes="<<material.positions.size()<<" tets="<<material.asset->solids.size()<<" mass="<<material.Mass()<<" triangles="<<hood.indices.size()/3<<'\n';
}
void CrashPanel::Step(RuntimeWorld& world,const CrashRig& rig,double dt){
    std::array<uint8_t,64> previouslyReleased{};
    std::copy(material.released.begin(),material.released.end(),previouslyReleased.begin());
    for(unsigned i=0;i<targets.size();++i){
        if(material.released[i]){targets[i].valid=false;targets[i].body={};continue;}
        auto body=world.RuntimeBody(world.FracturePartEntity(rig.owner,carrierParts[i]));
        auto& t=targets[i];t.previous=t.current;t.body=body;t.valid=world.Physics().IsBodyEnabled(body);
        if(t.valid){auto pose=world.Physics().GetTransform(body);t.current=glm::translate(glm::dmat4(1),glm::dvec3(pose.position))*glm::mat4_cast(glm::dquat(pose.rotation));}
        else material.released[i]=1;
    }
    auto gravity=world.GravityForEntity(rig.owner);
    material.Step(dt,world.Physics(),gravity,targets);
    if(!material.error.empty())throw std::runtime_error("physical hood: "+material.error);
    for(unsigned i=0;i<material.released.size();++i)if(material.released[i]&&!previouslyReleased[i])std::cout<<"MOUNT_FAILURE group="<<material.settings.attachments[i].group<<(material.settings.attachments[i].tearWork>0&&material.AttachmentWork(i)>=material.settings.attachments[i].tearWork?" reason=mounting-sheet-tear force=":" reason=hardware-capacity force=")<<material.attachmentState[i].peakForce<<" moment="<<material.attachmentState[i].peakTorque<<'\n';
    double kinetic=0;for(unsigned n=0;n<material.velocities.size();++n){double speed2=glm::dot(material.velocities[n],material.velocities[n]);maximumSpeed=std::max(maximumSpeed,std::sqrt(speed2));kinetic+=.5*material.NodeMass(n)*speed2;}maximumKineticEnergy=std::max(maximumKineticEnergy,kinetic);
    for(unsigned i=0;i<material.attachmentState.size();++i){peakForces[i]=std::max(peakForces[i],material.attachmentState[i].peakForce);peakMoments[i]=std::max(peakMoments[i],material.attachmentState[i].peakTorque);}
    CommitMountFailures();
}
bool CrashPanel::CommitMountFailures(){
    bool changed=false;
    // The fasteners and the sheet they pass through are different failure paths.
    // Existing force/moment limits protect the hardware. Local irreversible
    // bending work tears the surrounding sheet, including under repeated loads.
    // Publish release at the completed material step, without changing momentum.
    for(unsigned i=0;i<targets.size();++i)if(!material.released[i]&&MountWork(i)>=MountTearWork){
        std::cout<<"MOUNT_FAILURE group="<<material.settings.attachments[i].group<<" reason=mounting-sheet-tear work_j="<<MountWork(i)<<'\n';
        material.Release(material.settings.attachments[i].group);changed=true;
    }
    RefreshCollisionMask();
    return changed;
}
void CrashPanel::RefreshCollisionMask(){
    const bool detached=std::all_of(material.released.begin(),material.released.end(),[](auto r){return r!=0;});
    // Independent debris collides with its former car as well as the world.
    material.settings.collisionMask=detached?kAllCategories:attachedCollisionMask;
}
void CrashPanel::Persist(SaveArchive& a){material.Persist(a);for(auto& t:targets)a(t.previous,t.current,t.valid);if(a.reading)RefreshCollisionMask();}
double CrashPanel::PlasticExtent()const{double peak=0;for(const auto& h:material.panelBending->hinges)peak=std::max(peak,h.plastic);return peak;}
void CrashPanel::Map(){material.MapRender(1,mapped);}

bool CrashPanel::AttachedTo(const DeformableInstance& frame,unsigned referencePart)const{
    if(referencePart>=frame.fracture.component.size()||frame.fracture.removed[referencePart])return false;
    for(unsigned i=0;i<carrierParts.size();++i)if(!material.released[i]&&!frame.fracture.removed[carrierParts[i]]&&frame.fracture.component[carrierParts[i]]==frame.fracture.component[referencePart])return true;
    return false;
}

// Partition the existing physical bending work by the material nodes belonging
// to the attachment patch. No remote panel work, elapsed time or impact count.
double CrashPanel::MountWork(unsigned index) const {
    return material.AttachmentWork(index);
}

glm::dvec3 CrashPanel::Center() const {
    glm::dvec3 value(0);for(unsigned n=0;n<material.positions.size();++n)value+=material.NodeMass(n)*material.positions[n];return value/material.Mass();
}
glm::dvec3 CrashPanel::Momentum() const {
    glm::dvec3 value(0);for(unsigned n=0;n<material.velocities.size();++n)value+=material.NodeMass(n)*material.velocities[n];return value;
}
