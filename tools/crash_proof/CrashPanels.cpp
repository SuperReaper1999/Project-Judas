#include "CrashPanels.h"
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
    auto a=MakeDeformableSheet(6,6,hi.x-lo.x,hi.z-lo.z,1);
    for(auto& n:a.nodes){
        double x=n.x+(hi.x+lo.x)*.5,z=n.y+lo.z;
        double nearest=1e100,y=0;
        for(const auto& v:hood.vertices){double d=(v.position.x-x)*(v.position.x-x)+(v.position.z-z)*(v.position.z-z);if(d<nearest){nearest=d;y=v.position.y;}}
        bool found=false;double surface=-1e20;
        for(unsigned t=0;t<hood.indices.size();t+=3){
            V p=hood.vertices[hood.indices[t]].position,q=hood.vertices[hood.indices[t+1]].position,r=hood.vertices[hood.indices[t+2]].position;
            glm::dmat2 e(glm::dvec2(q.x-p.x,q.z-p.z),glm::dvec2(r.x-p.x,r.z-p.z));
            if(std::abs(glm::determinant(e))<1e-12)continue;
            auto w=glm::inverse(e)*glm::dvec2(x-p.x,z-p.z);
            if(w.x>=0&&w.y>=0&&w.x+w.y<=1){surface=std::max(surface,p.y*(1-w.x-w.y)+q.y*w.x+r.y*w.y);found=true;}
        }
        n={x,found?surface:y,z};
    }
    a.groups.clear();
    for(unsigned n=0;n<a.nodes.size();++n){const auto p=a.nodes[n];
        if(p.z>hi.z-.01 && p.x<-.4)a.groups["hinge-left"].push_back(n);
        if(p.z>hi.z-.01 && p.x>.4)a.groups["hinge-right"].push_back(n);
        if(p.z<lo.z+.01 && std::abs(p.x)<.4)a.groups["front-latch"].push_back(n);
    }
    a.render={};a.binding.clear();
    // Rebuild the solver's original grid render after curving the rest surface.
    for(unsigned n=0;n<a.nodes.size();++n){MeshVertex v;v.position=glm::vec3(a.nodes[n]);a.render.vertices.push_back(v);a.binding.push_back({glm::uvec4(n),{1,0,0,0}});}
    for(auto t:a.triangles)for(int k=0;k<3;++k)a.render.indices.push_back(t[k]);
    exterior=hood;exteriorBinding.clear();normalOffset.clear();
    for(const auto& vertex:hood.vertices){
        bool bound=false;
        for(const auto& t:a.triangles){
            auto p=a.nodes[t.x],q=a.nodes[t.y],r=a.nodes[t.z];
            glm::dmat2 edges(glm::dvec2(q.x-p.x,q.z-p.z),glm::dvec2(r.x-p.x,r.z-p.z));
            auto w=glm::inverse(edges)*glm::dvec2(vertex.position.x-p.x,vertex.position.z-p.z);
            glm::dvec3 bary(1-w.x-w.y,w.x,w.y);
            if(glm::all(glm::greaterThanEqual(bary,glm::dvec3(-1e-8)))){
                auto normal=glm::normalize(glm::cross(q-p,r-p));
                auto projected=p*bary.x+q*bary.y+r*bary.z;
                exteriorBinding.push_back({{t.x,t.y,t.z,t.z},{bary,0}});
                // Store the FULL rest offset in the local triangle frame below.
                normalOffset.push_back(glm::dot(V(vertex.position)-projected,normal));
                bound=true;break;
            }
        }
        if(!bound)throw std::runtime_error("hood vertex lies outside physical shell");
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
    carrierParts.clear();targets.clear();
    for(const auto& [group,nodes]:a.groups){
        V centroid(0);for(auto n:nodes)centroid+=a.nodes[n]/double(nodes.size());
        unsigned part=0;double best=1e100;
        for(unsigned p=0;p<frame->asset->fracture->parts.size();++p){auto d=frame->asset->fracture->parts[p].proxyCenter-centroid;if(glm::dot(d,d)<best){best=glm::dot(d,d);part=p;}}
        carrierParts.push_back(part);
        auto entity=world.FracturePartEntity(rig.owner,part);auto body=world.RuntimeBody(entity);auto pose=world.Physics().GetTransform(body);
        DeformableAttachment mount;mount.group=group;mount.kind=DeformableAttachment::Kind::Body;
        mount.target=entity;mount.offset=-frame->asset->fracture->parts[part].proxyCenter;
        mount.forceLimit=group=="front-latch"?8000:20000;mount.torqueLimit=8000;s.attachments.push_back(mount);
        DeformableTarget target;target.valid=true;target.body=body;
        target.current=glm::translate(glm::dmat4(1),V(pose.position))*glm::mat4_cast(glm::dquat(pose.rotation));target.previous=target.current;targets.push_back(target);
    }
    auto parent=world.RuntimeDefinition(rig.owner)->transform;
    auto transform=glm::translate(glm::dmat4(1),V(parent.position))*glm::mat4_cast(glm::dquat(parent.rotation));
    shell=std::make_shared<DeformableAsset>(std::move(a));
    material.Initialize(shell,s,transform,1);
    material.panelBending.emplace();
    material.panelBending->stiffness=2000;material.panelBending->yieldMoment=80;material.panelBending->hardening=80;
    material.panelBending->Initialize(shell->nodes,shell->triangles);
    Map();
    std::cout<<"PANEL_INITIALIZED nodes="<<material.positions.size()<<" tets="<<material.asset->solids.size()<<" mass="<<material.Mass()<<" triangles="<<hood.indices.size()/3<<'\n';
}
void CrashPanel::Step(RuntimeWorld& world,const CrashRig& rig,double dt){
    for(unsigned i=0;i<targets.size();++i){
        auto body=world.RuntimeBody(world.FracturePartEntity(rig.owner,carrierParts[i]));
        auto& t=targets[i];t.previous=t.current;t.body=body;t.valid=world.Physics().IsBodyEnabled(body);
        if(t.valid){auto pose=world.Physics().GetTransform(body);t.current=glm::translate(glm::dmat4(1),glm::dvec3(pose.position))*glm::mat4_cast(glm::dquat(pose.rotation));}
        else material.released[i]=1;
    }
    auto gravity=world.GravityForEntity(rig.owner);
    material.Step(dt,world.Physics(),gravity,targets);
    if(!material.error.empty())throw std::runtime_error("physical hood: "+material.error);
}
void CrashPanel::Persist(SaveArchive& a){material.Persist(a);material.panelBending->Persist(a);for(auto& t:targets)a(t.previous,t.current,t.valid);}
double CrashPanel::PlasticExtent()const{double peak=0;for(const auto& h:material.panelBending->hinges)peak=std::max(peak,h.plastic);return peak;}
void CrashPanel::Map(){
    if(mapped.vertices.size()!=exterior.vertices.size())mapped=exterior;
    for(unsigned i=0;i<mapped.vertices.size();++i){const auto& b=exteriorBinding[i];
        auto p=material.positions[b.nodes.x],q=material.positions[b.nodes.y],r=material.positions[b.nodes.z];
        auto normal=glm::normalize(glm::cross(q-p,r-p));
        auto position=p*b.weights.x+q*b.weights.y+r*b.weights.z+normal*normalOffset[i];
        // Preserve the exact imported rest position using its barycentric
        // in-plane residual rotated by the current physical triangle frame.
        auto rp=shell->nodes[b.nodes.x],rq=shell->nodes[b.nodes.y],rr=shell->nodes[b.nodes.z];
        auto rn=glm::normalize(glm::cross(rq-rp,rr-rp));
        auto restPoint=rp*b.weights.x+rq*b.weights.y+rr*b.weights.z+rn*normalOffset[i];
        auto restX=glm::normalize(rq-rp),nowX=glm::normalize(q-p);
        auto restY=glm::cross(rn,restX),nowY=glm::cross(normal,nowX);
        auto offset=glm::dvec3(exterior.vertices[i].position)-restPoint;
        position+=nowX*glm::dot(offset,restX)+nowY*glm::dot(offset,restY);
        mapped.vertices[i].position=glm::vec3(position);
        auto tangent=glm::dvec3(glm::vec3(exterior.vertices[i].tangent));
        mapped.vertices[i].tangent=glm::vec4(glm::vec3(nowX*glm::dot(tangent,restX)+nowY*glm::dot(tangent,restY)+normal*glm::dot(tangent,rn)),exterior.vertices[i].tangent.w);
        auto n=glm::dvec3(exterior.vertices[i].normal);
        mapped.vertices[i].normal=glm::vec3(nowX*glm::dot(n,restX)+nowY*glm::dot(n,restY)+normal*glm::dot(n,rn));
    }
}

bool CrashPanel::AttachedTo(const DeformableInstance& frame,unsigned referencePart)const{
    if(referencePart>=frame.fracture.component.size()||frame.fracture.removed[referencePart])return false;
    for(unsigned i=0;i<carrierParts.size();++i)if(!material.released[i]&&!frame.fracture.removed[carrierParts[i]]&&frame.fracture.component[carrierParts[i]]==frame.fracture.component[referencePart])return true;
    return false;
}
