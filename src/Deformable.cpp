#include "Deformable.h"
#include "CollisionGeometry.h"
#include "Narrowphase.h"
#include "SaveArchive.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace {
using V=glm::dvec3;
    using M=glm::dmat3;
bool finite(V v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
        }
bool validMasses(const DeformableAsset& a,double density){
    double total=0;
    for(size_t i=0;i<a.measures.size();++i){
        double mass=a.measures[i]*a.massWeights[i]*density;
        if(!std::isfinite(mass)||mass<=0||!std::isfinite(1/mass))return false;
        total+=mass;
    }
    return std::isfinite(total)&&total>0;
}
M columns(const std::vector<V>& x,glm::uvec4 n){return M(x[n.y]-x[n.x],x[n.z]-x[n.x],x[n.w]-x[n.x]);
        }
M cofactor(M f){return M(glm::cross(f[1],f[2]),glm::cross(f[2],f[0]),glm::cross(f[0],f[1]));
        }
V target(const glm::dmat4& m,V p){return V(m*glm::dvec4(p,1));
        }
// Closest triangle coordinates, valid for edges/vertices as well as its face.
V closest(V p,V a,V b,V c){V ab=b-a,ac=c-a,ap=p-a;
        double d1=glm::dot(ab,ap),d2=glm::dot(ac,ap);
        if(d1<=0&&d2<=0)return {1,0,0};
        V bp=p-b;
        double d3=glm::dot(ab,bp),d4=glm::dot(ac,bp);
        if(d3>=0&&d4<=d3)return {0,1,0};
        double vc=d1*d4-d3*d2;
        if(vc<=0&&d1>=0&&d3<=0){double v=d1/(d1-d3);
            return {1-v,v,0};
            }V cp=p-c;
        double d5=glm::dot(ab,cp),d6=glm::dot(ac,cp);
        if(d6>=0&&d5<=d6)return {0,0,1};
        double vb=d5*d2-d1*d6;
        if(vb<=0&&d2>=0&&d6<=0){double w=d2/(d2-d6);
            return {1-w,0,w};
            }double va=d3*d6-d5*d4;
        if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){double w=(d4-d3)/((d4-d3)+(d5-d6));
            return {0,1-w,w};
            }double inv=1/(va+vb+vc);
        return {1-(vb+vc)*inv,vb*inv,vc*inv};
        }
std::pair<double,double> segments(V a,V b,V c,V d){V u=b-a,v=d-c,r=a-c;
        double A=glm::dot(u,u),B=glm::dot(u,v),C=glm::dot(v,v),D=glm::dot(u,r),E=glm::dot(v,r),den=A*C-B*B;
        if(A<1e-15||C<1e-15)return {0,0};
        double s=den>1e-15?std::clamp((B*E-C*D)/den,0.,1.):0;
        double t=std::clamp((B*s+E)/C,0.,1.);
        s=std::clamp((B*t-D)/A,0.,1.);
        return {s,t};
        }
void scalar(std::vector<V>& x,const std::vector<double>& w,const unsigned* ids,const V* gradients,unsigned n,double C,double compliance,double h,double& lambda){
    double alpha=compliance/(h*h),den=alpha;
        for(unsigned k=0;k<n;++k)den+=w[ids[k]]*glm::dot(gradients[k],gradients[k]);
        if(den<1e-20)return;
        double dl=(-C-alpha*lambda)/den;
        lambda+=dl;
        for(unsigned k=0;k<n;++k)x[ids[k]]+=w[ids[k]]*dl*gradients[k];
}
double effective(PhysicsWorld& p,BodyHandle b,V point,V normal){
    if(!p.IsDynamicBody(b))return 0;
        double m=p.GetMass(b);
        if(m<=0)return 0;
        V r=point-V(p.GetCenterOfMass(b)),rn=glm::cross(r,normal);
        return 1/m+glm::dot(rn,M(glm::inverse(p.GetInertiaWorld(b)))*rn);
}
V pointVelocity(PhysicsWorld& p,BodyHandle b,V point){return V(p.GetLinearVelocity(b))+glm::cross(V(p.GetAngularVelocity(b)),point-V(p.GetCenterOfMass(b)));
        }
}
void DeformableInstance::Initialize(std::shared_ptr<const DeformableAsset> data,const DeformableSettings& config,const glm::dmat4& transform,uint64_t epoch){
    std::string e;
    if(!data||!ValidDeformableSettings(config,e))throw std::invalid_argument(e.empty()?"missing deformable asset":e);
    if(data->nodes.size()!=data->measures.size()||data->nodes.size()!=data->massWeights.size()||data->neighbors.size()!=data->nodes.size())throw std::invalid_argument("deformable topology is not prepared");
    if(!validMasses(*data,config.material.density))throw std::invalid_argument("deformable derived mass is not finite and positive");
    if(data->fracture&&data->fracture->rigid&&std::any_of(config.attachments.begin(),config.attachments.end(),[](const auto& a){return a.forceLimit>0||a.torqueLimit>0;}))throw std::invalid_argument("finite material attachment strength requires deformable material; rigid fracture uses section bonds");
    asset=std::move(data);
    settings=config;
    generation=epoch;
    initial=transform;
    size_t n=asset->nodes.size();
    positions.resize(n);
    previous.resize(n);
    start.resize(n);
    velocities.resize(n);
    forces.resize(n);
    lastGravity.resize(n);
    mass.resize(n);
    inverseMass.resize(n);
    lambda.resize(asset->cloth.size()*3+asset->solids.size()*6+asset->bends.size());
    volumeLambda.resize(asset->solids.size());
    plasticRest.resize(asset->solids.size());
    released.resize(config.attachments.size());
    attachmentState.resize(config.attachments.size());
    candidates.reserve(std::max(asset->triangles.size(),asset->edges.size()));
    InitializeSurfaceTree(faceTree,false);InitializeSurfaceTree(edgeTree,true);
    faceExcluded.resize(asset->triangles.size());edgeExcluded.resize(asset->edges.size());
    contactReference.resize(n);contactStartReference.resize(n);
    selfFacePairs.reserve(std::min<size_t>(524288,asset->nodes.size()*64));selfEdgePairs.reserve(std::min<size_t>(524288,asset->edges.size()*64));
    rigidCandidates.reserve(256);
    if(asset->fracture)fracture.Initialize(*asset->fracture);
    Reset(transform);
}
void DeformableInstance::Reset(const glm::dmat4& transform){
    initial=transform;
    selfCandidatesValid=false;
    error.clear();
    for(size_t i=0;i<positions.size();++i){positions[i]=previous[i]=start[i]=target(transform,asset->nodes[i]);
        velocities[i]=forces[i]=lastGravity[i]=V(0);
        mass[i]=asset->measures[i]*asset->massWeights[i]*settings.material.density;
        inverseMass[i]=1/mass[i];
        }
    for(size_t i=0;i<plasticRest.size();++i)plasticRest[i]=M(transform)*asset->solids[i].rest;
    std::fill(released.begin(),released.end(),false);
    attachmentState.assign(settings.attachments.size(),{});
    Wake();
}
void DeformableInstance::ShiftOrigin(glm::dvec3 delta){
    for(auto* points:{&positions,&previous,&start,&contactReference,&contactStartReference})for(auto& p:*points)p-=delta;
    initial[3]-=glm::dvec4(delta,0);
    for(auto* tree:{&faceTree,&edgeTree})for(auto& node:*tree){node.low-=delta;node.high-=delta;}
    // Rest matrices, plastic strain, multipliers, velocities and forces are
    // translation-invariant. Candidate identities and sleep clocks stay intact.
}
bool DeformableInstance::SetMaterial(const DeformableMaterial& m,std::string& e){if(asset->fracture&&asset->fracture->rigid){e="rigid fracture material is authored; reload to change mass/contact properties";return false;}auto s=settings;
    s.material=m;
    if(!ValidDeformableSettings(s,e))return false;
    // Preflight before changing any runtime material/mass state.
    if(!validMasses(*asset,m.density)){e="deformable derived mass is not finite and positive";return false;}
    settings=s;selfCandidatesValid=false;
    for(size_t i=0;i<mass.size();++i){mass[i]=asset->measures[i]*asset->massWeights[i]*m.density;
        inverseMass[i]=1/mass[i];
        }Wake();
    return true;
    }
double DeformableInstance::Mass()const{double total=0;
    for(unsigned n=0;n<mass.size();++n)if(!asset->fracture||!fracture.removed[asset->fracture->nodePart[n]])total+=mass[n];
    return total;
    }
bool DeformableInstance::PartLoad(unsigned part,V value,bool impulse){if(!asset->fracture||part>=asset->fracture->parts.size()||fracture.removed[part]||!finite(value)||!settings.enabled)return false;const auto& nodes=asset->fracture->parts[part].nodes;double total=0;for(auto n:nodes)total+=mass[n];for(auto n:nodes)if(impulse)velocities[n]+=value/total;else forces[n]+=value*mass[n]/total;Wake();return true;}
bool DeformableInstance::Impulse(const std::string& group,V impulse){if(!finite(impulse))return false;
    auto it=asset->groups.find(group);
    if(!group.empty()&&it==asset->groups.end())return false;
    double total=0;
    for(unsigned i=0;i<mass.size();++i)if((!asset->fracture||!fracture.removed[asset->fracture->nodePart[i]])&&(group.empty()||std::find(it->second.begin(),it->second.end(),i)!=it->second.end()))total+=mass[i];
    for(unsigned i=0;i<mass.size();++i)if((!asset->fracture||!fracture.removed[asset->fracture->nodePart[i]])&&(group.empty()||std::find(it->second.begin(),it->second.end(),i)!=it->second.end()))velocities[i]+=impulse/total;
    Wake();
    return true;
    }
bool DeformableInstance::Force(const std::string& group,V force){if(!settings.enabled||!error.empty()||!finite(force))return false;
    auto it=asset->groups.find(group);
    if(!group.empty()&&it==asset->groups.end())return false;
    double total=0;
    for(unsigned i=0;i<mass.size();++i)if((!asset->fracture||!fracture.removed[asset->fracture->nodePart[i]])&&(group.empty()||std::find(it->second.begin(),it->second.end(),i)!=it->second.end()))total+=mass[i];
    for(unsigned i=0;i<mass.size();++i)if((!asset->fracture||!fracture.removed[asset->fracture->nodePart[i]])&&(group.empty()||std::find(it->second.begin(),it->second.end(),i)!=it->second.end()))forces[i]+=force*mass[i]/total;
    Wake();
    return true;
    }
bool DeformableInstance::Impulse(const DeformableLocation& hit,V impulse){if(hit.generation!=generation||(asset->fracture&&hit.topologyRevision!=fracture.revision)||hit.triangle>=asset->triangles.size()||!FaceVisible(hit.triangle)||!finite(hit.weights)||!finite(impulse)||glm::any(glm::lessThan(hit.weights,V(0)))||std::abs(hit.weights.x+hit.weights.y+hit.weights.z-1)>1e-7)return false;
    auto f=asset->triangles[hit.triangle];
    for(int k=0;k<3;++k)velocities[f[k]]+=impulse*hit.weights[k]/mass[f[k]];
    Wake();
    return true;
    }
bool DeformableInstance::Release(const std::string& group){bool found=false;
    for(size_t i=0;i<settings.attachments.size();++i)if(settings.attachments[i].group==group){released[i]=true;
        found=true;
        }if(found)Wake();
    return found;
    }
V DeformableInstance::Minimum()const{V b(std::numeric_limits<double>::max());
    for(auto p:positions)b=glm::min(b,p);
    return b;
    }
V DeformableInstance::Maximum()const{V b(-std::numeric_limits<double>::max());
    for(auto p:positions)b=glm::max(b,p);
    return b;
    }

void DeformableInstance::Cohesive(double h){
    if(!asset->fracture||asset->fracture->rigid)return;
    JUDAS_PROFILE_SCOPE("Fracture cohesive constraints");
    const auto& c=*asset->fracture;
    for(unsigned b=0;b<c.bonds.size();++b){const auto& bond=c.bonds[b];if(fracture.broken[b]||fracture.removed[bond.a]||fracture.removed[bond.b])continue;
        for(unsigned pair=0;pair<bond.pairs.size();++pair){auto ids=bond.pairs[pair];double alpha=c.compliance*bond.pairs.size()/(bond.area*h*h),den=inverseMass[ids.x]+inverseMass[ids.y]+alpha;if(den<=0)continue;
            auto& lambda=fracture.multipliers[b*16+pair];V delta=positions[ids.y]-positions[ids.x],dl=(-delta-alpha*lambda)/den;lambda+=dl;positions[ids.x]-=dl*inverseMass[ids.x];positions[ids.y]+=dl*inverseMass[ids.y];
        }
    }
}
void DeformableInstance::Internal(double h){
    JUDAS_PROFILE_SCOPE("Deformable internal constraints");
    const auto& m=settings.material;
    size_t row=0;
    for(auto& t:asset->cloth){unsigned ids[3]={t.nodes.x,t.nodes.y,t.nodes.z};
        V e1=positions[ids[1]]-positions[ids[0]],e2=positions[ids[2]]-positions[ids[0]];
        V f[2]={e1*t.inverseRest[0][0]+e2*t.inverseRest[0][1],e1*t.inverseRest[1][0]+e2*t.inverseRest[1][1]};
        for(unsigned c=0;c<3;++c){unsigned a=c==1?1:0,b=c==0?0:1;
            V gradients[3];
            double C=a==b?.5*(glm::dot(f[a],f[b])-1):glm::dot(f[a],f[b]);
            for(unsigned k=1;k<3;++k)gradients[k]=a==b?f[a]*t.inverseRest[a][k-1]:f[b]*t.inverseRest[a][k-1]+f[a]*t.inverseRest[b][k-1];
            gradients[0]=-gradients[1]-gradients[2];
            scalar(positions,inverseMass,ids,gradients,3,C,(a==b?m.stretchCompliance:m.shearCompliance)/t.area,h,lambda[row++]);
            e1=positions[ids[1]]-positions[ids[0]];
            e2=positions[ids[2]]-positions[ids[0]];
            f[0]=e1*t.inverseRest[0][0]+e2*t.inverseRest[0][1];
            f[1]=e1*t.inverseRest[1][0]+e2*t.inverseRest[1][1];
        }
    }
    for(size_t index=0;index<asset->solids.size();++index){auto& t=asset->solids[index];
        if(asset->fracture&&fracture.removed[asset->fracture->nodePart[t.nodes.x]]){row+=6;continue;}
        unsigned ids[4]={t.nodes.x,t.nodes.y,t.nodes.z,t.nodes.w};
        // Rest matrices remain in their saved authored world frame. Express
        // elastic columns in the material frame so global rotation cannot
        // rotate strain components into a different compliance response.
        M inv=glm::inverse(plasticRest[index])*M(initial);
        for(unsigned a=0;a<3;++a)for(unsigned b=a;b<3;++b){M f=columns(positions,t.nodes)*inv;
            V gradients[4];
            double C=a==b?.5*(glm::dot(f[a],f[b])-1):glm::dot(f[a],f[b]);
            for(unsigned k=1;k<4;++k)gradients[k]=a==b?f[a]*inv[a][k-1]:f[b]*inv[a][k-1]+f[a]*inv[b][k-1];
            gradients[0]=-gradients[1]-gradients[2]-gradients[3];
            scalar(positions,inverseMass,ids,gradients,4,C,m.shearCompliance/t.volume,h,lambda[row++]);
            }
        M f=columns(positions,t.nodes)*inv,d=cofactor(f)*glm::transpose(inv);
        V gradients[4]={-d[0]-d[1]-d[2],d[0],d[1],d[2]};
        double J=glm::determinant(f);
        stats.minimumJacobian=std::min(stats.minimumJacobian,J);
        scalar(positions,inverseMass,ids,gradients,4,J-1,m.volumeCompliance/t.volume,h,volumeLambda[index]);
    }
    for(auto b:asset->bends){unsigned ids[2]={b.a,b.b};
        V delta=positions[b.a]-positions[b.b];
        double len=glm::length(delta);
        if(len>1e-12){V gradients[2]={delta/len,-delta/len};
            scalar(positions,inverseMass,ids,gradients,2,len-b.rest,m.bendCompliance,h,lambda[row]);
            }++row;
        }
}
void DeformableInstance::Plastic(double h){
    JUDAS_PROFILE_SCOPE("Deformable plastic flow");
    auto& m=settings.material;
    for(size_t i=0;i<plasticRest.size();++i){if(asset->fracture&&fracture.removed[asset->fracture->nodePart[asset->solids[i].nodes.x]])continue;M ds=columns(positions,asset->solids[i].nodes),f=ds*glm::inverse(plasticRest[i]);
        M strain=glm::transpose(f)*f-M(1);
        double norm=std::sqrt(glm::dot(strain[0],strain[0])+glm::dot(strain[1],strain[1])+glm::dot(strain[2],strain[2]))*.5;
        stats.maximumStrain=std::max(stats.maximumStrain,norm);
        if(m.yieldStrain<=0||m.plasticRate<=0||norm<=m.yieldStrain||glm::determinant(f)<.2)continue;
        M rotation=f;
        for(unsigned k=0;k<8;++k)rotation=.5*(rotation+glm::transpose(glm::inverse(rotation)));
        double flow=std::min(m.plasticRate*h,(norm-m.yieldStrain)/std::max(norm,1e-12));
        M next=plasticRest[i]*(1-flow)+glm::transpose(rotation)*ds*flow;
        // Isochoric plastic flow: rest mass/volume stay unchanged. Stop flow
        // at the authored rest-strain envelope instead of repairing geometry.
        double det=glm::determinant(next),originalDet=glm::determinant(asset->solids[i].rest);
        if(det<=1e-12)continue;
        next*=std::cbrt(originalDet/det);
        M deformation=next*glm::inverse(M(initial)*asset->solids[i].rest),plasticStrain=glm::transpose(deformation)*deformation-M(1);
        double extent=std::sqrt(glm::dot(plasticStrain[0],plasticStrain[0])+glm::dot(plasticStrain[1],plasticStrain[1])+glm::dot(plasticStrain[2],plasticStrain[2]))*.5;
        if(extent<=m.maximumPlasticStrain)plasticRest[i]=next;
    }
}
void DeformableInstance::Attach(double h,double fraction,PhysicsWorld& physics,const std::vector<DeformableTarget>& targets){
    JUDAS_PROFILE_SCOPE("Deformable attachments");
    for(size_t a=0;a<settings.attachments.size();++a){const auto& attachment=settings.attachments[a];
        if(released[a]||!attachment.enabled||a>=targets.size()||!targets[a].valid)continue;
        auto group=asset->groups.find(attachment.group);
        if(group==asset->groups.end())continue;
        const auto& t=targets[a];
        const bool dynamicTarget=t.body.IsValid()&&physics.IsDynamicBody(t.body);
        // Keep the moment origin inside the patch, independently of its world
        // placement. A pure couple must remain a torque when its net force is
        // zero; summing per-node impulse magnitudes would invent a force.
        V centre(0);
        if(dynamicTarget){V localCentre(0);unsigned activeNodes=0;
            for(unsigned n:group->second)if(!asset->fracture||!fracture.removed[asset->fracture->nodePart[n]]){localCentre+=asset->nodes[n]+attachment.offset;++activeNodes;}
            if(!activeNodes)continue;
            localCentre/=activeNodes;
            centre=target(t.previous,localCentre)*(1-fraction)+target(t.current,localCentre)*fraction;
        }
        for(unsigned n:group->second){if(asset->fracture&&fracture.removed[asset->fracture->nodePart[n]])continue;V local=asset->nodes[n]+attachment.offset,destination=target(t.previous,local)*(1-fraction)+target(t.current,local)*fraction;
            if(!dynamicTarget){positions[n]=destination;
                continue;
                }
            // Velocity-level bilateral constraint. Body point velocity is read
            // again after each impulse; no repeated reaction against a frozen
            // infinite-mass target. A bounded positional drift bias follows the
            // prior sampled anchor, independently of prescribed target motion.
            double priorFraction=fraction-1./settings.substeps;
            V priorDestination=target(t.previous,local)*(1-priorFraction)+target(t.current,local)*priorFraction;
            for(int axis=0;axis<3;++axis){V normal(0);normal[axis]=1;
                V relative=(positions[n]-start[n])/h-pointVelocity(physics,t.body,destination);
                double den=inverseMass[n]+effective(physics,t.body,destination,normal);
                if(den<=0)continue;
                double impulse=-(relative[axis]+.1*(start[n]-priorDestination)[axis]/h)/den;
                positions[n]+=normal*inverseMass[n]*impulse*h;
                const V reaction=-normal*impulse;
                // The constraint denominator and point velocity use a double
                // lever arm. Preserve that same arm for the actual reaction;
                // rounding an absolute world point to float before subtracting
                // the COM would alter a small patch's torque at large origins.
                const V arm=destination-V(physics.GetCenterOfMass(t.body));
                physics.ApplyLinearImpulse(t.body,glm::vec3(reaction));
                physics.ApplyAngularImpulse(t.body,glm::vec3(glm::cross(arm,reaction)));
                attachmentImpulse[a]+=reaction;
                attachmentMoment[a]+=glm::cross(destination-centre,reaction);
            }
        }
    }
}
void DeformableInstance::RigidContacts(double h,double fraction,PhysicsWorld& physics){
    JUDAS_PROFILE_SCOPE("Deformable rigid contacts");
    double thickness=settings.material.thickness;
    // One tree query per patch, then actual authoritative primitive shapes. Fat
    // bounds are only candidates. Samples include vertices, edge midpoints and
    // face centroids, bounding the supported thin-surface sampling envelope.
    stats.candidates+=rigidCandidates.size();
    auto sample=[&](glm::uvec3 nodes,V weights,BodyHandle body,const Shape& shape,const BodyTransform& pose){
        if(asset->fracture&&fracture.removed[asset->fracture->nodePart[nodes.x]])return;
        V p(0),old(0);
        for(int k=0;k<3;++k){p+=positions[nodes[k]]*weights[k];
            old+=start[nodes[k]]*weights[k];
            }
        auto contact=[&](V center,glm::dquat q,V half,double radius,const PrimitivePose* geometry=nullptr){V normal,point;
            double depth=0;
            V local(0);
            if(!geometry)local=glm::inverse(q)*(p-center);
            if(geometry){auto result=SegmentGeometry(p,p,thickness,*geometry,thickness);if(!result.valid||result.gap>=0)return;normal=result.normal;point=result.point;depth=-result.gap;}
            else if(radius>0){double len=glm::length(local);
                if(len>=radius+thickness)return;
                normal=len>1e-10?q*(local/len):q*V(1,0,0);
                point=center+normal*radius;
                depth=radius+thickness-len;
                }
            else{V nearest=glm::clamp(local,-half,half),d=local-nearest;
                double len=glm::length(d);
                if(len>=thickness)return;
                if(len>1e-10){normal=q*(d/len);
                    point=center+q*nearest;
                    depth=thickness-len;
                    }else{V gap=half-glm::abs(local);
                    unsigned axis=gap.x<gap.y?(gap.x<gap.z?0:2):(gap.y<gap.z?1:2);
                    V n(0);
                    n[axis]=local[axis]>=0?1:-1;
                    normal=q*n;
                    nearest=local;
                    nearest[axis]=n[axis]*half[axis];
                    point=center+q*nearest;
                    depth=gap[axis]+thickness;
                    }}
            double w=0;
            for(int k=0;k<3;++k)w+=inverseMass[nodes[k]]*weights[k]*weights[k];
            double rigid=effective(physics,body,point,normal),den=w+rigid;
            if(den<1e-20)return;
            double dl=depth/den;
            for(int k=0;k<3;++k)positions[nodes[k]]+=normal*dl*inverseMass[nodes[k]]*weights[k];
            // This projection's velocity change IS the particle impulse. The
            // equal opposite impulse is submitted once, including lever arm.
            if(rigid>0)physics.ApplyImpulseAtPoint(body,glm::vec3(-normal*dl/h),glm::vec3(point));
            V velocity=(p-old)/h-pointVelocity(physics,body,point);
            V tangent=velocity-normal*glm::dot(velocity,normal);
            double speed=glm::length(tangent);
            if(speed>1e-10){V dir=tangent/speed;
                double tangentialMass=w+effective(physics,body,point,dir);
                double frictionImpulse=std::min(speed/tangentialMass,settings.material.friction*dl/h);
                for(int k=0;k<3;++k)positions[nodes[k]]-=dir*frictionImpulse*h*inverseMass[nodes[k]]*weights[k];
                if(rigid>0)physics.ApplyImpulseAtPoint(body,glm::vec3(dir*frictionImpulse),glm::vec3(point));
                }++stats.contacts;
        };
        if(shape.type==ShapeType::Sphere)contact(V(pose.position),glm::dquat(pose.rotation),V(0),shape.radius);
        else if(shape.type==ShapeType::Box)contact(V(pose.position),glm::dquat(pose.rotation),V(shape.halfExtents),0);
        else if(shape.type==ShapeType::CompoundBoxes||shape.asset){RigidBody parent;parent.position=pose.position;parent.orientation=pose.rotation;
            for(int i=0;i<PrimitiveCount(shape);++i){auto primitive=PrimitiveAt(shape,parent,i);if(primitive.shape.asset)contact({}, {}, {},0,&primitive);else contact(PrimitiveCenter(primitive),glm::dquat(primitive.body.orientation),V(primitive.shape.halfExtents),primitive.shape.type==ShapeType::Sphere?primitive.shape.radius:0);}
        }
    };
    for(auto body:rigidCandidates){unsigned layer;
        CategoryMask mask;
        if(!physics.GetCollisionFilter(body,layer,mask)||(mask&CategoryBit(settings.collisionLayer))==0||physics.IsBodySensor(body))continue;
        Shape shape;
        BodyTransform pose;
        if(!physics.GetBodyShape(body,shape,pose))continue;
        auto prev=physics.GetPreviousTransform(body);
        prev.position+=prev.rotation*shape.pivotOffset;
        pose.position=glm::mix(prev.position,pose.position,float(fraction));
        pose.rotation=glm::slerp(prev.rotation,pose.rotation,float(fraction));
        for(unsigned n=0;n<positions.size();++n)sample(glm::uvec3(n),V(1,0,0),body,shape,pose);
        for(auto edge:asset->edges)sample({edge.x,edge.y,edge.y},V(.5,.5,0),body,shape,pose);
        for(unsigned f=0;f<asset->triangles.size();++f)if(FaceVisible(f))sample(asset->triangles[f],V(1./3),body,shape,pose);
    }
}
// Balanced trees use rest-space primitive centres only to choose topology. Refit
// includes current and prior substep bounds, so folded/deformed surfaces retain
// exact conservative candidate coverage without cell replication or allocations.
void DeformableInstance::InitializeSurfaceTree(std::vector<SurfaceNode>& tree,bool edge){
    size_t count=edge?asset->edges.size():asset->triangles.size();
    tree.clear();if(!count)return;tree.reserve(count*2-1);
    std::vector<unsigned> indices(count);for(unsigned i=0;i<count;++i)indices[i]=i;
    auto centre=[&](unsigned i){if(edge){auto e=asset->edges[i];return (asset->nodes[e.x]+asset->nodes[e.y])*.5;}auto f=asset->triangles[i];return (asset->nodes[f.x]+asset->nodes[f.y]+asset->nodes[f.z])/3.;};
    auto build=[&](auto&& recurse,size_t begin,size_t end)->unsigned {
        unsigned node=unsigned(tree.size());tree.push_back({});
        if(end-begin==1){tree[node].primitive=indices[begin];return node;}
        V lo(1e300),hi(-1e300);for(size_t i=begin;i<end;++i){lo=glm::min(lo,centre(indices[i]));hi=glm::max(hi,centre(indices[i]));}
        V extent=hi-lo;unsigned axis=extent.y>extent.x?1:0;if(extent.z>extent[axis])axis=2;
        size_t middle=(begin+end)/2;std::nth_element(indices.begin()+begin,indices.begin()+middle,indices.begin()+end,[&](unsigned a,unsigned b){double x=centre(a)[axis],y=centre(b)[axis];return x==y?a<b:x<y;});
        auto left=recurse(recurse,begin,middle),right=recurse(recurse,middle,end);tree[node].left=left;tree[node].right=right;return node;
    };build(build,0,count);
}
void DeformableInstance::BuildSurfaceTree(){
    JUDAS_PROFILE_SCOPE("Deformable surface candidates");
    auto refit=[&](std::vector<SurfaceNode>& tree,bool edge){for(size_t i=tree.size();i-->0;){auto& n=tree[i];
        if(n.primitive!=~0u){n.low=V(1e300);n.high=V(-1e300);auto include=[&](unsigned v){n.low=glm::min(n.low,glm::min(positions[v],start[v]));n.high=glm::max(n.high,glm::max(positions[v],start[v]));};
            if(edge){auto e=asset->edges[n.primitive];include(e.x);include(e.y);}else{auto f=asset->triangles[n.primitive];include(f.x);include(f.y);include(f.z);}
        }else{n.low=glm::min(tree[n.left].low,tree[n.right].low);n.high=glm::max(tree[n.left].high,tree[n.right].high);}
    }};refit(faceTree,false);refit(edgeTree,true);
}
void DeformableInstance::SurfaceCandidates(V low,V high,bool edge,const std::vector<unsigned>* exclusions,unsigned minimum){
    candidates.clear();const auto& tree=edge?edgeTree:faceTree;if(tree.empty())return;
    auto& excluded=edge?edgeExcluded:faceExcluded;
    if(++queryEpoch==0){std::fill(faceExcluded.begin(),faceExcluded.end(),0);std::fill(edgeExcluded.begin(),edgeExcluded.end(),0);++queryEpoch;}
    if(exclusions)for(auto primitive:*exclusions)excluded[primitive]=queryEpoch;
    unsigned stack[64],size=1;stack[0]=0;
    while(size){const auto& n=tree[stack[--size]];if(glm::any(glm::lessThan(high,n.low))||glm::any(glm::greaterThan(low,n.high)))continue;
        if(n.primitive!=~0u){if(n.primitive>=minimum&&excluded[n.primitive]!=queryEpoch)candidates.push_back(n.primitive);}
        else{if(size+2>64){stats.capacityExceeded=true;return;}stack[size++]=n.right;stack[size++]=n.left;}
    }
    std::sort(candidates.begin(),candidates.end());
}
// A Verlet-style conservative pair cache. Both participants may move at most
// skin since construction, so candidate padding is contact thickness + 2*skin.
// Only broadphase work is reused; every material iteration tests current geometry.
void DeformableInstance::PrepareSelfCandidates(){
    JUDAS_PROFILE_SCOPE("Deformable self pair cache");
    double skin=settings.material.thickness*.5;
    bool rebuild=!selfCandidatesValid;
    if(!rebuild)for(size_t i=0;i<positions.size();++i)if(glm::length(positions[i]-contactReference[i])>skin||glm::length(start[i]-contactStartReference[i])>skin){rebuild=true;break;}
    if(!rebuild)return;
    selfCandidatesValid=true;contactReference=positions;contactStartReference=start;
    selfFacePairs.clear();selfEdgePairs.clear();BuildSurfaceTree();
    double padding=settings.material.thickness*2+skin*2;
    auto append=[&](auto& pairs,unsigned x,unsigned y){if(pairs.size()==pairs.capacity()){stats.capacityExceeded=true;selfCandidatesValid=false;return;}pairs.push_back({x,y});};
    for(unsigned n=0;n<positions.size();++n){SurfaceCandidates(glm::min(positions[n],start[n])-V(padding),glm::max(positions[n],start[n])+V(padding),false,&asset->excludedFaces[n]);for(auto f:candidates)append(selfFacePairs,n,f);}
    auto visit=[&](auto&& recurse,unsigned x,unsigned y)->void {
        const auto& a=edgeTree[x];const auto& b=edgeTree[y];
        if(glm::any(glm::lessThan(a.high+V(padding),b.low))||glm::any(glm::greaterThan(a.low-V(padding),b.high)))return;
        bool leafA=a.primitive!=~0u,leafB=b.primitive!=~0u;
        if(leafA&&leafB){unsigned e=a.primitive,f=b.primitive;if(e==f)return;if(e>f)std::swap(e,f);const auto& excluded=asset->excludedEdges[e];if(!std::binary_search(excluded.begin(),excluded.end(),f))append(selfEdgePairs,e,f);return;}
        if(x==y){recurse(recurse,a.left,a.left);recurse(recurse,a.left,a.right);recurse(recurse,a.right,a.right);return;}
        if(!leafA&&(leafB||glm::dot(a.high-a.low,a.high-a.low)>=glm::dot(b.high-b.low,b.high-b.low))){recurse(recurse,a.left,y);recurse(recurse,a.right,y);}else{recurse(recurse,x,b.left);recurse(recurse,x,b.right);}
    };if(!edgeTree.empty())visit(visit,0,0);
}
void DeformableInstance::SurfaceContacts(DeformableInstance& a,DeformableInstance& b,double h){
    JUDAS_PROFILE_SCOPE("Deformable surface contacts");
    bool self=&a==&b;
    if(self&&!a.settings.selfContact)return;
    if((a.settings.collisionMask&CategoryBit(b.settings.collisionLayer))==0||(b.settings.collisionMask&CategoryBit(a.settings.collisionLayer))==0)return;
    if(self)a.PrepareSelfCandidates();else b.BuildSurfaceTree();
    double thickness=a.settings.material.thickness+b.settings.material.thickness;
    {JUDAS_PROFILE_SCOPE("Deformable vertex face contacts");
    auto contact=[&](unsigned n,unsigned t){if(!b.FaceVisible(t))return;auto f=b.asset->triangles[t];
            if(a.asset->fracture&&a.fracture.removed[a.asset->fracture->nodePart[n]])return;
            if(self&&a.asset->fracture){const auto& c=*a.asset->fracture;if(a.fracture.component[c.nodePart[n]]==a.fracture.component[c.nodePart[f.x]])return;}
            ++a.stats.candidates;
            V p=a.positions[n],x=b.positions[f.x],y=b.positions[f.y],z=b.positions[f.z],normal=glm::cross(y-x,z-x);
            double len=glm::length(normal);
            if(len<1e-10)return;
            normal/=len;
            V weights=closest(p,x,y,z),q=x*weights.x+y*weights.y+z*weights.z,delta=p-q;
            double distance=glm::length(delta);
            V oldNormal=glm::cross(b.start[f.y]-b.start[f.x],b.start[f.z]-b.start[f.x]);
            double oldLength=glm::length(oldNormal);
            double oldSide=oldLength>1e-10?glm::dot(a.start[n]-b.start[f.x],oldNormal/oldLength):glm::dot(delta,normal);
            double side=glm::dot(delta,normal);
            bool crossed=oldSide*side<0&&weights.x>1e-8&&weights.y>1e-8&&weights.z>1e-8;
            if(distance>=thickness&&!crossed)return;
            if(crossed)normal*=oldSide>=0?1.:-1.;
            else normal=distance>1e-10?delta/distance:normal*(oldSide>=0?1.:-1.);
            double gap=glm::dot(delta,normal),den=a.inverseMass[n];
            for(int k=0;k<3;++k)den+=b.inverseMass[f[k]]*weights[k]*weights[k];
            if(den<1e-20)return;
            double impulse=(thickness-gap)/den;
            V correction=normal*impulse*a.inverseMass[n];
            a.positions[n]+=correction;
            if(!self){a.velocities[n]+=correction/h;
                a.Wake();
                b.Wake();
                }for(int k=0;k<3;++k){V other=-normal*impulse*b.inverseMass[f[k]]*weights[k];
                b.positions[f[k]]+=other;
                if(!self)b.velocities[f[k]]+=other/h;
                }++a.stats.contacts;

    };
    if(self){for(auto pair:a.selfFacePairs)contact(pair.x,pair.y);}
    else for(unsigned n=0;n<a.positions.size();++n){b.SurfaceCandidates(glm::min(a.positions[n],a.start[n])-V(thickness),glm::max(a.positions[n],a.start[n])+V(thickness),false);for(unsigned f:b.candidates)contact(n,f);}
    }
    {JUDAS_PROFILE_SCOPE("Deformable edge edge contacts");
    // Traverse tree pairs once, rather than issuing one complete BVH query for
    // every edge. Identical subtree pairs visit only their upper triangle.
    if(!self){b.BuildSurfaceTree();a.BuildSurfaceTree();}
    auto contact=[&](unsigned e,unsigned j){if(self){if(e==j)return;if(e>j)std::swap(e,j);const auto& exclusions=a.asset->excludedEdges[e];if(std::binary_search(exclusions.begin(),exclusions.end(),j))return;}
        auto v=a.asset->edges[e],w=b.asset->edges[j];if(a.asset->fracture&&a.fracture.removed[a.asset->fracture->nodePart[v.x]])return;if(b.asset->fracture&&b.fracture.removed[b.asset->fracture->nodePart[w.x]])return;if(self&&a.asset->fracture){const auto& c=*a.asset->fracture;auto p=c.nodePart[v.x],q=c.nodePart[w.x];if(a.fracture.removed[p]||a.fracture.removed[q]||a.fracture.component[p]==a.fracture.component[q])return;}++a.stats.candidates;
            auto [s,t]=segments(a.positions[v.x],a.positions[v.y],b.positions[w.x],b.positions[w.y]);
            V delta=glm::mix(a.positions[v.x],a.positions[v.y],s)-glm::mix(b.positions[w.x],b.positions[w.y],t);
            double d=glm::length(delta);
            if(d>=thickness||d<1e-12)return;
            V normal=delta/d;
            double den=a.inverseMass[v.x]*(1-s)*(1-s)+a.inverseMass[v.y]*s*s+b.inverseMass[w.x]*(1-t)*(1-t)+b.inverseMass[w.y]*t*t;
            if(den<1e-20)return;
            double dl=(thickness-d)/den;
            V ca=normal*dl*a.inverseMass[v.x]*(1-s),cb=normal*dl*a.inverseMass[v.y]*s,cc=-normal*dl*b.inverseMass[w.x]*(1-t),cd=-normal*dl*b.inverseMass[w.y]*t;
            a.positions[v.x]+=ca;
            a.positions[v.y]+=cb;
            b.positions[w.x]+=cc;
            b.positions[w.y]+=cd;
            if(!self){a.velocities[v.x]+=ca/h;
                a.velocities[v.y]+=cb/h;
                b.velocities[w.x]+=cc/h;
                b.velocities[w.y]+=cd/h;
                a.Wake();
                b.Wake();
                }++a.stats.contacts;

    };
    auto visit=[&](auto&& recurse,unsigned x,unsigned y)->void {
        const auto& left=a.edgeTree[x];const auto& right=b.edgeTree[y];
        if(glm::any(glm::lessThan(left.high+V(thickness),right.low))||glm::any(glm::greaterThan(left.low-V(thickness),right.high)))return;
        bool leafA=left.primitive!=~0u,leafB=right.primitive!=~0u;
        if(leafA&&leafB){contact(left.primitive,right.primitive);return;}
        if(self&&x==y){recurse(recurse,left.left,left.left);recurse(recurse,left.left,left.right);recurse(recurse,left.right,left.right);return;}
        if(!leafA&&(leafB||glm::dot(left.high-left.low,left.high-left.low)>=glm::dot(right.high-right.low,right.high-right.low))){recurse(recurse,left.left,y);recurse(recurse,left.right,y);}
        else{recurse(recurse,x,right.left);recurse(recurse,x,right.right);}
    };if(self){for(auto pair:a.selfEdgePairs)contact(pair.x,pair.y);}else if(!a.edgeTree.empty()&&!b.edgeTree.empty())visit(visit,0,0);
    }
    // External pairs resolve once at the fixed-step coupling boundary; self
    // contact is substepped. Apply the same positional delta to velocity once.
    (void)h;
}
void DeformableInstance::Step(double dt,PhysicsWorld& physics,const GravityField& gravity,const std::vector<DeformableTarget>& targets){
    if(!asset||!settings.enabled||!error.empty()||dt<=0){std::fill(forces.begin(),forces.end(),V(0));return;}
    // A prescribed world/bone/static frame has no finite dynamic reaction in
    // this solver. Refuse a finite-strength claim rather than silently treating
    // such a target as an infinite-strength connection. Unlimited legacy roots
    // continue through exactly the original prescribed-target path.
    for(size_t i=0;i<settings.attachments.size();++i){const auto& a=settings.attachments[i];
        if(!released[i]&&a.enabled&&(a.forceLimit>0||a.torqueLimit>0)&&i<targets.size()&&targets[i].valid&&!physics.IsDynamicBody(targets[i].body)){
            error="finite attachment strength requires a dynamic body target";std::fill(forces.begin(),forces.end(),V(0));return;
        }
    }
    JUDAS_PROFILE_SCOPE("Deformable fixed step");
    attachmentState.assign(settings.attachments.size(),{});
    attachmentImpulse.resize(settings.attachments.size());attachmentMoment.resize(settings.attachments.size());attachmentPending.assign(settings.attachments.size(),0);
    stats={};
    stats.nodes=positions.size();
    stats.triangles=asset->triangles.size();
    stats.tets=asset->solids.size();
    stats.substeps=settings.substeps;
    stats.iterations=settings.iterations;
    for(size_t n=0;n<inverseMass.size();++n)inverseMass[n]=1/mass[n];
    for(size_t i=0;i<settings.attachments.size();++i)if(!released[i]&&settings.attachments[i].enabled&&i<targets.size()&&targets[i].valid&&!targets[i].body.IsValid()){auto group=asset->groups.find(settings.attachments[i].group);
        if(group!=asset->groups.end())for(auto n:group->second)inverseMass[n]=0;
        }
    bool targetMoving=false;
    for(auto& t:targets)if(t.valid){double d=0;
        for(int k=0;k<4;++k)d+=glm::length(t.current[k]-t.previous[k]);
        targetMoving|=d>1e-8||(t.body.IsValid()&&physics.IsDynamicBody(t.body));
        }
    bool gravityChanged=false;
    for(size_t i=0;i<positions.size();++i){V g=gravity.Sample(glm::vec3(positions[i]));
        gravityChanged|=glm::length(g-lastGravity[i])>1e-5;
        lastGravity[i]=g;
        }
    if(targetMoving||gravityChanged||settings.material.airDrag>0)Wake();
    if(sleeping){PhysicsQueryFilter f;
        f.includeLayers=settings.collisionMask;
        physics.QueryBodiesInAabbInto(glm::vec3(Minimum()-V(settings.material.thickness)),glm::vec3(Maximum()+V(settings.material.thickness)),rigidCandidates,f);
        for(auto b:rigidCandidates)if(glm::length(physics.GetLinearVelocity(b))+glm::length(physics.GetAngularVelocity(b))>1e-5)Wake();
        if(sleeping)return;
        }
    previous=positions;
    double h=dt/settings.substeps;
    for(unsigned sub=0;sub<settings.substeps;++sub){
        std::fill(attachmentImpulse.begin(),attachmentImpulse.end(),V(0));
        std::fill(attachmentMoment.begin(),attachmentMoment.end(),V(0));
        {JUDAS_PROFILE_SCOPE("Deformable force prediction");
            start=positions;
            for(size_t n=0;n<positions.size();++n){if(asset->fracture&&fracture.removed[asset->fracture->nodePart[n]]){velocities[n]=V(0);continue;}V g=gravity.Sample(glm::vec3(positions[n]));
                V acceleration=g+forces[n]/mass[n];
                velocities[n]+=acceleration*h;
                double air=1-std::exp(-settings.material.airDrag*h);
                velocities[n]+=air*(settings.material.airVelocity-velocities[n]);
                velocities[n]*=std::exp(-settings.material.damping*h);
                positions[n]+=velocities[n]*h;
                }}
        if(asset->fracture)fracture.BeginSubstep();
        std::fill(lambda.begin(),lambda.end(),0);
        std::fill(volumeLambda.begin(),volumeLambda.end(),0);
        double fraction=double(sub+1)/settings.substeps;
        {PhysicsQueryFilter filter;
            filter.includeLayers=settings.collisionMask;
            physics.QueryBodiesInAabbInto(glm::vec3(Minimum()-V(settings.material.thickness+asset->cellSize)),glm::vec3(Maximum()+V(settings.material.thickness+asset->cellSize)),rigidCandidates,filter);
            }
        for(unsigned iteration=0;iteration<settings.iterations;++iteration){Internal(h);Cohesive(h);
            RigidContacts(h,fraction,physics);
            SurfaceContacts(*this,*this,h);
            Attach(h,fraction,physics,targets);
            }
        for(size_t a=0;a<settings.attachments.size();++a){auto& state=attachmentState[a];
            state.force=attachmentImpulse[a]/h;state.moment=attachmentMoment[a]/h;
            const double force=glm::length(state.force),torque=glm::length(state.moment);
            if(!std::isfinite(force)||!std::isfinite(torque)){error="nonfinite material attachment reaction; instance stopped";return;}
            state.peakForce=std::max(state.peakForce,force);state.peakTorque=std::max(state.peakTorque,torque);
            const auto& setting=settings.attachments[a];
            if((setting.forceLimit>0&&force>setting.forceLimit)||(setting.torqueLimit>0&&torque>setting.torqueLimit))attachmentPending[a]=1;
        }
        for(size_t i=0;i<asset->solids.size();++i){if(asset->fracture&&fracture.removed[asset->fracture->nodePart[asset->solids[i].nodes.x]])continue;double J=glm::determinant(columns(positions,asset->solids[i].nodes)*glm::inverse(plasticRest[i]));if(!std::isfinite(J)||J<=0){error="deformable element inversion; outside supported deformation envelope; instance stopped";return;}}
        if(asset->fracture){const auto& c=*asset->fracture;
            for(unsigned b=0;b<c.bonds.size();++b){const auto& bond=c.bonds[b];
                if(fracture.broken[b]||fracture.removed[bond.a]||fracture.removed[bond.b])continue;
                const auto& part=c.parts[bond.a];M rotation(0);double volume=0;
                // A volume-weighted material frame avoids attributing an
                // entire bent section to one arbitrarily chosen tetrahedron.
                if(c.structural){for(auto index:part.tets){const auto& tet=asset->solids[index];rotation+=columns(positions,tet.nodes)*tet.inverseRest*tet.volume;volume+=tet.volume;}rotation/=volume;}
                else{const auto& tet=asset->solids[part.tets.front()];rotation=columns(positions,tet.nodes)*tet.inverseRest;}
                for(int k=0;k<(c.structural?8:5);++k)rotation=.5*(rotation+glm::transpose(glm::inverse(rotation)));
                bool valid=true;for(int k=0;k<3;++k)valid&=finite(rotation[k]);
                if(!valid||glm::determinant(rotation)<=0){error="invalid cohesive material frame; instance stopped";return;}
                V normal=glm::normalize(rotation*bond.normal),force(0),moment(0),centroid(0);
                if(c.structural){auto first=bond.pairs.front();V origin=(positions[first.x]+positions[first.y])*.5;centroid=origin;
                    for(unsigned pair=0;pair<bond.pairs.size();++pair){auto nodes=bond.pairs[pair];centroid+=((positions[nodes.x]+positions[nodes.y])*.5-origin)*bond.sectionWeights[pair];}}
                for(unsigned pair=0;pair<bond.pairs.size();++pair){V demand=-fracture.multipliers[b*16+pair]/(h*h);force+=demand;
                    if(c.structural){auto nodes=bond.pairs[pair];moment+=glm::cross((positions[nodes.x]+positions[nodes.y])*.5-centroid,demand);}
                    else fracture.Observe(c,b,demand*double(bond.pairs.size()),normal);
                }
                if(c.structural){M inverse=glm::transpose(rotation);fracture.ObserveSection(c,b,inverse*force,inverse*moment);}
                else fracture.Observe(c,b,force,normal);
            }
        }
        Plastic(h);
        for(size_t n=0;n<positions.size();++n){if(!finite(positions[n])){error="nonfinite deformable solve; instance stopped";
                return;
                }velocities[n]=(positions[n]-start[n])/h;
            }
        if(stats.capacityExceeded){error="deformable surface candidate capacity exceeded; instance stopped";
            return;
            }
    }
    // Failure is an irreversible change of connection ownership at the fixed
    // step boundary. Both material and body retain their already solved real
    // velocities; release adds no impulse, transform correction or reset.
    for(size_t a=0;a<released.size();++a)if(attachmentPending[a])released[a]=1;
    std::fill(forces.begin(),forces.end(),V(0));
    double speed=0;
    for(auto v:velocities)speed=std::max(speed,glm::length(v));
    if(speed<.015&&!targetMoving&&settings.material.airDrag==0)quietSeconds+=dt;
    else quietSeconds=0;
    if(quietSeconds>1)sleeping=true;
    JUDAS_PROFILE_COUNTER("Deformable nodes",double(stats.nodes),ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Deformable contacts",double(stats.contacts),ProfileCounterMode::Sum);
}
void DeformableInstance::MapRender(double alpha,MeshData& mesh)const{
    JUDAS_PROFILE_SCOPE("Deformable render mapping");
    if(mesh.vertices.size()!=asset->render.vertices.size())mesh=asset->render;
    alpha=std::clamp(alpha,0.,1.);
    if(asset->fracture){mesh.indices.clear();mesh.primitives.clear();for(int material=0;material<2;++material){unsigned first=unsigned(mesh.indices.size());for(unsigned f=0;f<asset->triangles.size();++f)if(FaceVisible(f)&&(asset->fracture->faceBond[f]<0?0:1)==material)for(unsigned k=0;k<3;++k)mesh.indices.push_back(f*3+k);unsigned count=unsigned(mesh.indices.size())-first;if(count)mesh.primitives.push_back({first,count,material});}}
    for(size_t i=0;i<mesh.vertices.size();++i){auto b=asset->binding[i];
        V p(0);
        for(int k=0;k<4;++k)p+=glm::mix(previous[b.nodes[k]],positions[b.nodes[k]],alpha)*b.weights[k];
        mesh.vertices[i].position=glm::vec3(p);
        mesh.vertices[i].normal=glm::vec3(0);
        mesh.vertices[i].tangent=glm::vec4(0);
        }
    for(size_t i=0;i<mesh.indices.size();i+=3){auto ia=mesh.indices[i],ib=mesh.indices[i+1],ic=mesh.indices[i+2];
        auto& a=mesh.vertices[ia];
        auto& b=mesh.vertices[ib];
        auto& c=mesh.vertices[ic];
        auto e1=b.position-a.position,e2=c.position-a.position;
        auto normal=glm::cross(e1,e2);
        auto uv1=b.uv-a.uv,uv2=c.uv-a.uv;
        float den=uv1.x*uv2.y-uv1.y*uv2.x;
        glm::vec3 tangent=std::abs(den)>1e-10f?(e1*uv2.y-e2*uv1.y)/den:glm::vec3(1,0,0);
        for(auto n:{ia,ib,ic}){mesh.vertices[n].normal+=normal;
            mesh.vertices[n].tangent+=glm::vec4(tangent,(den<0?-1.f:1.f)*glm::length(normal));
            }}
    for(auto& v:mesh.vertices){if(glm::length(v.normal)>1e-9f)v.normal=glm::normalize(v.normal);
        glm::vec3 tangent=glm::vec3(v.tangent)-v.normal*glm::dot(v.normal,glm::vec3(v.tangent));
        if(glm::length(tangent)>1e-9f)tangent=glm::normalize(tangent);
        v.tangent=glm::vec4(tangent,v.tangent.w<0?-1.f:1.f);
        }
}
DeformableHit DeformableInstance::Raycast(V origin,V direction,double maximum)const{
    DeformableHit hit;
    if(!settings.enabled||!finite(origin)||!finite(direction)||!std::isfinite(maximum)||maximum<0||glm::length(direction)<1e-12)return hit;
    direction=glm::normalize(direction);
    double best=maximum;
    for(unsigned t=0;t<asset->triangles.size();++t){if(!FaceVisible(t))continue;auto f=asset->triangles[t];
        V a=positions[f.x],e1=positions[f.y]-a,e2=positions[f.z]-a,p=glm::cross(direction,e2);
        double det=glm::dot(e1,p);
        if(std::abs(det)<1e-12)continue;
        V s=origin-a;
        double u=glm::dot(s,p)/det;
        if(u<0||u>1)continue;
        V q=glm::cross(s,e1);
        double v=glm::dot(direction,q)/det;
        if(v<0||u+v>1)continue;
        double d=glm::dot(e2,q)/det;
        if(d<0||d>best)continue;
        best=d;
        hit.hit=true;
        hit.distance=d;
        hit.point=origin+direction*d;
        hit.normal=glm::normalize(glm::cross(e1,e2));
        if(glm::dot(hit.normal,direction)>0)hit.normal=-hit.normal;
        hit.location={generation,t,V(1-u-v,u,v),asset->fracture?fracture.revision:0};
    }return hit;
}
void DeformableInstance::Persist(SaveArchive& ar,bool materialFrame,bool attachmentLimits){
    if(asset->fracture)fracture.Persist(ar,*asset->fracture);
    ar(settings.enabled,sleeping,quietSeconds);
    settings.material.Save(ar);
    std::string validation;
    ar.Require(ValidDeformableSettings(settings,validation),"invalid saved deformable material");
    ar.Require(validMasses(*asset,settings.material.density),"invalid saved deformable derived mass");
    ar(settings.attachments,positions,velocities,plasticRest,released);
    // Attachment::Save deliberately retains its original field layout. Only
    // State.2 / participant.4 carries optional strength fields; legacy formats
    // keep unlimited connections rather than inventing historical strength.
    if(attachmentLimits)for(auto& attachment:settings.attachments)ar(attachment.forceLimit,attachment.torqueLimit);
    else if(ar.reading)for(auto& attachment:settings.attachments)attachment.forceLimit=attachment.torqueLimit=0;
    ar.Require(ValidDeformableSettings(settings,validation),"invalid saved deformable attachments");
    for(const auto& attachment:settings.attachments)ar.Require(asset->groups.count(attachment.group),"saved attachment group missing");
    ar.Require(positions.size()==asset->nodes.size()&&velocities.size()==positions.size()&&plasticRest.size()==asset->solids.size()&&released.size()==settings.attachments.size(),"deformable snapshot topology mismatch");
    for(auto m:plasticRest)ar.Require(glm::determinant(m)>1e-12,"saved deformable rest inversion");
    for(auto value:released)ar.Require(value<=1,"invalid saved attachment release state");
    ar.Require(quietSeconds>=0,"invalid saved deformable sleep time");
    // Logical owner transforms may move prescribed supports without replacing
    // the simulated material. Its original constitutive/plastic reference must
    // survive cold reconstruction from the CURRENT owner transform.
    if(materialFrame){M reference(initial);ar(reference);
        for(int a=0;a<3;++a)for(int b=0;b<3;++b)ar.Require(std::abs(glm::dot(reference[a],reference[b])-(a==b?1.:0.))<1e-5,"invalid saved deformable material frame");
        ar.Require(glm::determinant(reference)>0,"invalid saved deformable material orientation");
        if(ar.reading)for(int k=0;k<3;++k)initial[k]=glm::dvec4(reference[k],0);
    }
    if(ar.reading){selfCandidatesValid=false;previous=start=positions;
        attachmentState.assign(settings.attachments.size(),{});attachmentPending.clear();attachmentImpulse.clear();attachmentMoment.clear();
        forces.assign(positions.size(),V(0));
        lastGravity.assign(positions.size(),V(0));
        for(size_t i=0;i<mass.size();++i){mass[i]=asset->measures[i]*asset->massWeights[i]*settings.material.density;
            inverseMass[i]=1/mass[i];
            }std::fill(lambda.begin(),lambda.end(),0);
        std::fill(volumeLambda.begin(),volumeLambda.end(),0);
        error.clear();
        }
}
