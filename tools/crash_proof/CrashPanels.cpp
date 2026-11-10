#include "CrashPanels.h"
#include "deformables/PanelCook.h"
#include "deformables/PanelIslands.h"
#include "RuntimeWorld.h"
#include "SaveArchive.h"
#include "ResourceManager.h"
#include "AssetDatabase.h"
#include "StructuralMass.h"
#include "SceneFingerprint.h"
#include <filesystem>
#include <fstream>
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
    const auto islands=MeshPanelIslands(body);
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
void CrashPanel::Author(Scene& scene,const DeformableAsset& frame,const CrashRig& rig,AssetDatabase& database,ResourceManager& resources,const MeshData& hood){
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
    if(!frame.fracture||!frame.fracture->rigid)throw std::runtime_error("panel authoring requires a physical structural carrier");
    DeformableSettings settings;settings.substeps=4;settings.iterations=6;settings.panel.enabled=true;
    double area=0;for(const auto& triangle:a.cloth)area+=triangle.area;
    settings.material.density=25/area;settings.material.stretchCompliance=settings.material.shearCompliance=1e-9;
    settings.material.damping=.4;settings.material.thickness=.012;
    for(const auto& [group,nodes]:a.groups){
        V centroid(0);for(auto n:nodes)centroid+=a.nodes[n]/double(nodes.size());
        unsigned part=0;double distance=1e100;
        for(unsigned i=0;i<frame.fracture->parts.size();++i){auto delta=frame.fracture->parts[i].proxyCenter-centroid;double next=glm::dot(delta,delta);if(next<distance){distance=next;part=i;}}
        DeformableAttachment mount;mount.group=group;mount.kind=DeformableAttachment::Kind::Body;
        mount.target=rig.owner;mount.part=frame.fracture->parts[part].key;
        mount.offset=-IntegrateStructuralMass(frame,frame.fracture->parts[part],1).center;
        mount.tearWork=MountTearWork;mount.forceLimit=group=="front-latch"?8000:20000;mount.torqueLimit=8000;
        settings.attachments.push_back(mount);
    }
    auto bytes=EncodeDeformableAsset(a);auto digest=SceneFingerprintSha256(bytes);
    auto path=std::filesystem::path(database.AssetsDir())/("physical-panel-"+digest+".judasdeform");
    if(!std::filesystem::exists(path)){std::ofstream file(path,std::ios::binary);file.write(bytes.data(),std::streamsize(bytes.size()));file.close();if(!file)throw std::runtime_error("cannot preserve private panel derivative");}
    AssetRecord record;
    const auto relative="Assets/"+path.filename().generic_string();
    if(auto* existing=database.FindByRelativePath(relative))record=*existing;
    else if(!database.Track(path.string(),record,error))throw std::runtime_error(error);
    resources.RequestDeformable(record.id);resources.WaitForAll();settings.asset=record.id;
    auto transform=scene.Find(rig.owner)->transform;
    auto& object=scene.CreateObject("Physically deformable bonnet");owner=object.id;
    object.transform=transform;object.deformable=std::move(settings);
    object.render=SceneRenderComponent{};object.render->color={1,1,1};
}
void CrashPanel::Bind(RuntimeWorld& runtime,SceneObjectId existing){
    world=&runtime;if(existing)owner=existing;
    auto& material=Material();world->UpdatePanelCollisionExclusions(material);observedRelease=material.released;
    peakForces.assign(material.settings.attachments.size(),0);peakMoments.assign(peakForces.size(),0);
    maximumSpeed=maximumKineticEnergy=0;Map();
    std::cout<<"PANEL_INITIALIZED owner="<<owner<<" nodes="<<material.positions.size()<<" mass="<<material.Mass()<<" triangles="<<material.asset->render.indices.size()/3<<'\n';
}
DeformableInstance& CrashPanel::Material()const{
    if(!world||!owner)throw std::logic_error("panel fixture is not bound to its native owner");
    std::string error;auto* material=world->RuntimeDeformable(owner,error);
    if(!material)throw std::runtime_error("native panel: "+error);
    return *material;
}
void CrashPanel::Observe(){
    auto& material=Material();if(!material.error.empty())throw std::runtime_error("native panel: "+material.error);
    for(unsigned i=0;i<material.released.size();++i)if(material.released[i]&&!observedRelease[i]){
        const auto event=material.attachmentEvents.find(material.settings.attachments[i].group);
        std::cout<<"MOUNT_FAILURE group="<<material.settings.attachments[i].group<<" reason="<<(event==material.attachmentEvents.end()?"completed-release":event->second)<<" force="<<material.attachmentState[i].peakForce<<" moment="<<material.attachmentState[i].peakTorque<<'\n';
    }
    observedRelease=material.released;
    double kinetic=0;for(unsigned n=0;n<material.velocities.size();++n){double speed2=glm::dot(material.velocities[n],material.velocities[n]);maximumSpeed=std::max(maximumSpeed,std::sqrt(speed2));kinetic+=.5*material.NodeMass(n)*speed2;}
    maximumKineticEnergy=std::max(maximumKineticEnergy,kinetic);
    for(unsigned i=0;i<peakForces.size();++i){peakForces[i]=std::max(peakForces[i],material.attachmentState[i].peakForce);peakMoments[i]=std::max(peakMoments[i],material.attachmentState[i].peakTorque);}
}
bool CrashPanel::CommitMountFailures(){auto& material=Material();bool changed=material.CommitPanelFailures();world->UpdatePanelCollisionExclusions(material);return changed;}
void CrashPanel::Persist(SaveArchive& archive){
    if(archive.reading)throw std::logic_error("native panels restore through WorldPersistence");
    Material().Persist(archive);
}
double CrashPanel::PlasticExtent()const{double extent=0;for(const auto& hinge:Material().panelBending->hinges)extent=std::max(extent,hinge.plastic);return extent;}
void CrashPanel::Map(){Material().MapRender(1,mapped);}
double CrashPanel::MountWork(unsigned index)const{return Material().AttachmentWork(index);}
bool CrashPanel::AttachedTo(const DeformableInstance& frame,unsigned referencePart)const{
    if(referencePart>=frame.fracture.component.size()||frame.fracture.removed[referencePart])return false;
    const auto& material=Material();const auto& parts=frame.asset->fracture->parts;
    for(unsigned i=0;i<material.settings.attachments.size();++i)if(!material.released[i]){
        const auto& key=material.settings.attachments[i].part;
        auto part=std::find_if(parts.begin(),parts.end(),[&](const auto& p){return p.key==key;});
        if(part!=parts.end()){auto index=unsigned(part-parts.begin());if(!frame.fracture.removed[index]&&frame.fracture.component[index]==frame.fracture.component[referencePart])return true;}
    }
    return false;
}
glm::dvec3 CrashPanel::Center()const{const auto& material=Material();glm::dvec3 value(0);for(unsigned n=0;n<material.positions.size();++n)value+=material.NodeMass(n)*material.positions[n];return value/material.Mass();}
glm::dvec3 CrashPanel::Momentum()const{const auto& material=Material();glm::dvec3 value(0);for(unsigned n=0;n<material.velocities.size();++n)value+=material.NodeMass(n)*material.velocities[n];return value;}
