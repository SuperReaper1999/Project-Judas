#include "Deformable.h"
#include "SaveArchive.h"
#include "PerformanceProfiler.h"
#include <algorithm>
#include <set>
#include <numeric>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
namespace {
bool finite(glm::dvec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
}
void FracturePart::Save(SaveArchive& a){a(key,nodes,tets,proxyCenter,proxyHalf);}
void FractureBond::Save(SaveArchive& s){s(key,a,b,pairs,normal,centroid,area);}
void FractureCook::Save(SaveArchive& s){s(rigid,tension,shear,compression,compliance,pieceBudget,parts,bonds);}
void FractureCook::SaveStructural(SaveArchive& s){
    s(structural);
    for(auto& b:bonds)s(b.tension,b.shear,b.compression);
}
bool PrepareFractureCook(DeformableAsset& a,std::string& error){
    if(!a.fracture)return true;
    auto& c=*a.fracture;
    auto fail=[&](const char* message){error=message;return false;};
    if(a.kind!=DeformableKind::Solid||c.parts.size()<2||c.parts.size()>256||c.bonds.size()>1024||c.pieceBudget<c.parts.size()||c.pieceBudget>256)return fail("fracture partition/piece budget invalid");
    for(double x:{c.tension,c.shear,c.compression,c.compliance})if(!std::isfinite(x)||x<0)return fail("invalid fracture strengths/compliance");
    // Structural section geometry is shared by rigid joint reactions and soft
    // cohesive reactions. The instance owns the force/moment observation; the
    // immutable section model does not prescribe either motion representation.
    if(c.tension<=0||c.shear<=0)return fail("fracture tension/shear must be positive Pa");
    c.nodePart.assign(a.nodes.size(),~0u);std::vector<unsigned> tetPart(a.tetrahedra.size(),~0u);std::set<std::string> keys;
    for(unsigned p=0;p<c.parts.size();++p){auto& part=c.parts[p];
        if(part.key.empty()||!keys.insert(part.key).second||part.nodes.empty()||part.tets.empty()||!finite(part.proxyCenter)||!finite(part.proxyHalf)||glm::any(glm::lessThanEqual(part.proxyHalf,glm::dvec3(0))))return fail("invalid/duplicate fracture part/proxy");
        for(auto n:part.nodes){if(n>=a.nodes.size()||c.nodePart[n]!=~0u)return fail("fracture nodes overlap or exceed topology");c.nodePart[n]=p;
            if(glm::any(glm::greaterThan(glm::abs(a.nodes[n]-part.proxyCenter),part.proxyHalf+glm::dvec3(1e-7))))return fail("fracture proxy does not cover material");}
        for(auto t:part.tets){if(t>=tetPart.size()||tetPart[t]!=~0u)return fail("fracture material partitions overlap");tetPart[t]=p;for(int k=0;k<4;++k)if(c.nodePart[a.tetrahedra[t][k]]!=p)return fail("shared physical node across fracture parts");}
    }
    if(std::find(c.nodePart.begin(),c.nodePart.end(),~0u)!=c.nodePart.end()||std::find(tetPart.begin(),tetPart.end(),~0u)!=tetPart.end())return fail("fracture partition does not cover material exactly once");
    for(auto weight:a.massWeights)if(weight!=1)return fail("fracture requires uniform cell density/mass weights");
    for(unsigned p=0;p<c.parts.size();++p)for(unsigned q=p+1;q<c.parts.size();++q){auto& x=c.parts[p];auto& y=c.parts[q];if(glm::all(glm::lessThan(glm::abs(x.proxyCenter-y.proxyCenter),x.proxyHalf+y.proxyHalf-glm::dvec3(1e-7))))return fail("overlapping fracture cell proxies: cook nonoverlapping physical cells");}
    if(a.render.vertices.size()!=a.triangles.size()*3||a.binding.size()!=a.render.vertices.size())return fail("fracture requires one bound render vertex per oriented boundary corner");
    for(unsigned f=0;f<a.triangles.size();++f)for(unsigned k=0;k<3;++k){auto& binding=a.binding[f*3+k];if(binding.nodes.x!=a.triangles[f][k]||binding.weights!=glm::dvec4(1,0,0,0))return fail("unsupported fracture render binding; bake boundary-corner binding");}
    keys.clear();c.faceBond.assign(a.triangles.size(),-1);std::set<std::pair<unsigned,unsigned>> interfaces;
    for(unsigned b=0;b<c.bonds.size();++b){auto& bond=c.bonds[b];
        if(bond.key.empty()||!keys.insert(bond.key).second||bond.a>=c.parts.size()||bond.b>=c.parts.size()||bond.a==bond.b||!interfaces.insert(std::minmax(bond.a,bond.b)).second||bond.pairs.size()<3||bond.pairs.size()>16||!finite(bond.normal)||glm::length(bond.normal)<.999||glm::length(bond.normal)>1.001||!finite(bond.centroid)||!std::isfinite(bond.area)||bond.area<=0)return fail("invalid/duplicate fracture interface");
        std::set<unsigned> sideA,sideB;
        for(auto pair:bond.pairs){if(pair.x>=a.nodes.size()||pair.y>=a.nodes.size()||c.nodePart[pair.x]!=bond.a||c.nodePart[pair.y]!=bond.b||!sideA.insert(pair.x).second||!sideB.insert(pair.y).second||glm::length(a.nodes[pair.x]-a.nodes[pair.y])>1e-7)return fail("invalid fracture interface node correspondence");}
        for(double strength:{bond.tension,bond.shear,bond.compression})if(!std::isfinite(strength)||strength<0)return fail("invalid interface strength");
        if(!c.structural&&(bond.tension||bond.shear||bond.compression))return fail("interface strengths require section stress model");
        bond.sectionPoints.clear();bond.sectionWeights.assign(bond.pairs.size(),0);bond.sectionInverse=glm::dmat3(0);bond.polarMoment=0;
        double areaA=0,areaB=0;glm::dmat3 second(0);glm::dvec3 first(0);
        for(unsigned f=0;f<a.triangles.size();++f){auto n=a.triangles[f];bool left=sideA.count(n.x)&&sideA.count(n.y)&&sideA.count(n.z),right=sideB.count(n.x)&&sideB.count(n.y)&&sideB.count(n.z);
            if(!left&&!right)continue;
            if(c.faceBond[f]>=0)return fail("overlapping fracture interfaces");
            c.faceBond[f]=int(b);
            auto cross=glm::cross(a.nodes[n.y]-a.nodes[n.x],a.nodes[n.z]-a.nodes[n.x]);double area=.5*glm::length(cross);
            if(glm::dot(glm::normalize(cross),bond.normal)*(left?1.:-1.)<.999||std::abs(glm::dot((a.nodes[n.x]+a.nodes[n.y]+a.nodes[n.z])/3.-bond.centroid,bond.normal))>1e-7)return fail("fracture interface normal/centroid does not match material faces");
            if(left){areaA+=area;
                if(c.structural){glm::dvec3 r[3]={a.nodes[n.x]-bond.centroid,a.nodes[n.y]-bond.centroid,a.nodes[n.z]-bond.centroid};
                    auto sum=r[0]+r[1]+r[2];first+=area*sum/3.;
                    for(unsigned k=0;k<3;++k)for(unsigned pair=0;pair<bond.pairs.size();++pair)if(bond.pairs[pair].x==n[k])bond.sectionWeights[pair]+=area/(3*bond.area);
                    second+=area/12.*(glm::outerProduct(sum,sum)+glm::outerProduct(r[0],r[0])+glm::outerProduct(r[1],r[1])+glm::outerProduct(r[2],r[2]));
                }
            }else areaB+=area;}
        if(std::abs(areaA-bond.area)>1e-6*bond.area||std::abs(areaB-bond.area)>1e-6*bond.area)return fail("fracture interface area/face coverage mismatch");
        if(c.structural){
            // The authored nominal area may differ from the measured area
            // within the accepted tolerance. Centroid weights must still sum
            // to one, otherwise global translation creates a false moment.
            double weightSum=std::accumulate(bond.sectionWeights.begin(),bond.sectionWeights.end(),0.);
            for(auto& weight:bond.sectionWeights)weight/=weightSum;
            if(glm::length(first)>1e-7*bond.area)return fail("structural interface must use its area centroid");
            // Invert only the two in-plane modes; the added normal mode is
            // algebraic regularization, never artificial physical stiffness.
            bond.polarMoment=second[0][0]+second[1][1]+second[2][2];
            // Dimensionless conditioning test: centimetre and metre sections
            // must not acquire different validity merely from unit scale.
            auto augmented=second/bond.polarMoment+glm::outerProduct(bond.normal,bond.normal);
            if(!std::isfinite(glm::determinant(augmented))||glm::determinant(augmented)<=1e-12)return fail("degenerate structural section");
            bond.sectionInverse=glm::inverse(augmented)/bond.polarMoment;
            for(auto pair:bond.pairs)bond.sectionPoints.push_back(a.nodes[pair.x]-bond.centroid);
        }
    }
    return true;
}
DeformableAsset MakeFractureBlock(glm::uvec3 cells,glm::dvec3 size,bool rigid){
    if(!cells.x||!cells.y||!cells.z||size.x<=0||size.y<=0||size.z<=0||uint64_t(cells.x)*cells.y*cells.z>256)throw std::invalid_argument("fracture helper dimensions/piece budget");
    DeformableAsset a;a.kind=DeformableKind::Solid;a.fracture=std::make_shared<FractureCook>();auto& c=*a.fracture;c.rigid=rigid;
    auto partId=[&](unsigned x,unsigned y,unsigned z){return (z*cells.y+y)*cells.x+x;};
    glm::dvec3 extent=size/glm::dvec3(cells);
    for(unsigned z=0;z<cells.z;++z)for(unsigned y=0;y<cells.y;++y)for(unsigned x=0;x<cells.x;++x){auto cell=MakeDeformableBlock({1,1,1},extent);FracturePart part;
        part.key="part-"+std::to_string(partId(x,y,z));part.proxyCenter=extent*(glm::dvec3(x,y,z)+.5)-size*.5;part.proxyHalf=extent*.5;
        unsigned base=unsigned(a.nodes.size());for(auto p:cell.nodes){part.nodes.push_back(unsigned(a.nodes.size()));a.nodes.push_back(p+part.proxyCenter);}
        for(auto t:cell.tetrahedra){part.tets.push_back(unsigned(a.tetrahedra.size()));a.tetrahedra.push_back(t+glm::uvec4(base));}
        if(x==0)for(auto n:cell.groups.at("left"))a.groups["left"].push_back(n+base);
        if(x+1==cells.x)for(auto n:cell.groups.at("right"))a.groups["right"].push_back(n+base);
        if(y==0)for(auto n:cell.groups.at("bottom"))a.groups["bottom"].push_back(n+base);
        a.groups[part.key]=part.nodes;c.parts.push_back(std::move(part));
    }
    // Only a compact pair of common support/load groups is needed; per-part
    // identity is in the partition, not hundreds of attachment group names.
    for(auto it=a.groups.begin();it!=a.groups.end();)if(it->first.rfind("part-",0)==0)it=a.groups.erase(it);else ++it;
    for(unsigned z=0;z<cells.z;++z)for(unsigned y=0;y<cells.y;++y)for(unsigned x=0;x<cells.x;++x)for(unsigned axis=0;axis<3;++axis){glm::uvec3 next{x,y,z};++next[axis];if(next[axis]>=cells[axis])continue;
        FractureBond bond;bond.a=partId(x,y,z);bond.b=partId(next.x,next.y,next.z);bond.key="interface-"+std::to_string(c.bonds.size());bond.normal=glm::dvec3(0);bond.normal[axis]=1;
        auto& p=c.parts[bond.a];auto& q=c.parts[bond.b];bond.centroid=(p.proxyCenter+q.proxyCenter)*.5;bond.area=extent[(axis+1)%3]*extent[(axis+2)%3];
        for(auto i:p.nodes)for(auto j:q.nodes)if(glm::length(a.nodes[i]-a.nodes[j])<1e-8)bond.pairs.push_back({i,j});
        c.bonds.push_back(std::move(bond));
    }
    std::string error;if(!PrepareDeformableAsset(a,error))throw std::invalid_argument(error);return a;
}
void FractureState::Initialize(const FractureCook& c){revision=1;broken.assign(c.bonds.size(),0);removed.assign(c.parts.size(),0);pendingRemoved.assign(c.parts.size(),0);pending.assign(c.bonds.size(),0);multipliers.resize(c.bonds.size()*16);tensionDemand.assign(c.bonds.size(),0);shearDemand.assign(c.bonds.size(),0);error.clear();Connectivity(c);}
void FractureState::BeginSubstep(){std::fill(multipliers.begin(),multipliers.end(),glm::dvec3(0));}
void FractureState::Observe(const FractureCook& c,unsigned b,glm::dvec3 force,glm::dvec3 normal){
    if(b>=broken.size()||broken[b]||removed[c.bonds[b].a]||removed[c.bonds[b].b])return;
    double traction=glm::dot(force,normal)/c.bonds[b].area,shear=glm::length(force-normal*glm::dot(force,normal))/c.bonds[b].area;
    tensionDemand[b]=std::max(tensionDemand[b],std::max(traction,0.));shearDemand[b]=std::max(shearDemand[b],shear);
    if(traction>c.tension||shear>c.shear||(c.compression>0&&-traction>c.compression))pending[b]=1;
}
// Resultant traction model on the actual authored planar section. Linear
// normal stress satisfies axial force and both bending moments exactly. Shear
// uses a uniform resultant plus a polar-moment torsion approximation (not a
// Saint-Venant continuum solution). Evaluate extrema at section vertices.
void FractureState::ObserveSection(const FractureCook& c,unsigned b,glm::dvec3 force,glm::dvec3 moment){
    if(!c.structural||b>=c.bonds.size()||broken[b]||removed[c.bonds[b].a]||removed[c.bonds[b].b])return;
    const auto& section=c.bonds[b];auto n=section.normal;
    double axial=glm::dot(force,n)/section.area;
    auto gradient=section.sectionInverse*glm::cross(n,moment);
    auto shearForce=(force-n*glm::dot(force,n))/section.area;
    double twist=glm::dot(moment,n)/section.polarMoment;
    double tensile=0,compressive=0,shearPeak=0;
    for(auto r:section.sectionPoints){double stress=axial+glm::dot(gradient,r);
        tensile=std::max(tensile,stress);compressive=std::max(compressive,-stress);
        shearPeak=std::max(shearPeak,glm::length(shearForce+twist*glm::cross(n,r)));
    }
    tensionDemand[b]=std::max(tensionDemand[b],tensile);shearDemand[b]=std::max(shearDemand[b],shearPeak);
    double ts=section.tension>0?section.tension:c.tension,ss=section.shear>0?section.shear:c.shear,cs=section.compression>0?section.compression:c.compression;
    if(tensile>ts||shearPeak>ss||(cs>0&&compressive>cs))pending[b]=1;
}
bool FractureState::Request(const FractureCook& c,unsigned b,uint64_t expected){if(expected!=revision||b>=broken.size()||broken[b]||removed[c.bonds[b].a]||removed[c.bonds[b].b])return false;pending[b]=2;return true;}
void FractureState::Connectivity(const FractureCook& c){JUDAS_PROFILE_SCOPE("Fracture connectivity");component.resize(c.parts.size());std::iota(component.begin(),component.end(),0);auto root=[&](unsigned p){while(component[p]!=p)p=component[p];return p;};
    for(unsigned b=0;b<c.bonds.size();++b)if(!broken[b]&&!removed[c.bonds[b].a]&&!removed[c.bonds[b].b]){auto a=root(c.bonds[b].a),d=root(c.bonds[b].b);component[std::max(a,d)]=std::min(a,d);}
    for(unsigned p=0;p<component.size();++p)component[p]=removed[p]?~0u:root(p);
}
bool FractureState::RequestRemoval(const FractureCook& c,unsigned part,uint64_t expected){if(expected!=revision||part>=c.parts.size()||removed[part])return false;pendingRemoved[part]=1;return true;}
bool FractureState::Commit(const FractureCook& c){JUDAS_PROFILE_SCOPE("Fracture topology commit");committed.clear();if(c.parts.size()>c.pieceBudget){error="fracture piece budget exhausted; prior topology retained";return false;}
    bool removal=false;for(unsigned p=0;p<removed.size();++p)if(pendingRemoved[p]){removed[p]=1;removal=true;for(unsigned b=0;b<c.bonds.size();++b)if(c.bonds[b].a==p||c.bonds[b].b==p)pending[b]=2;}
    std::fill(pendingRemoved.begin(),pendingRemoved.end(),0);
    for(unsigned b=0;b<broken.size();++b)if(pending[b]&&!broken[b]){broken[b]=pending[b];committed.push_back(b);}
    std::fill(pending.begin(),pending.end(),0);if(committed.empty()&&!removal)return false;++revision;Connectivity(c);return true;
}
bool FractureState::FaceVisible(const DeformableAsset& a,unsigned f)const{if(f>=a.triangles.size())return false;auto& c=*a.fracture;if(removed[c.nodePart[a.triangles[f].x]])return false;return c.faceBond[f]<0||broken[c.faceBond[f]];}
void FractureState::Persist(SaveArchive& s,const FractureCook& c){s(revision,broken,removed,tensionDemand,shearDemand);s.Require(revision>0&&broken.size()==c.bonds.size()&&removed.size()==c.parts.size()&&tensionDemand.size()==broken.size()&&shearDemand.size()==broken.size(),"fracture snapshot topology mismatch");for(auto b:broken)s.Require(b<=2,"invalid fracture failure cause");for(auto r:removed)s.Require(r<=1,"invalid fracture removal state");for(auto d:tensionDemand)s.Require(std::isfinite(d)&&d>=0,"invalid saved tensile demand");for(auto d:shearDemand)s.Require(std::isfinite(d)&&d>=0,"invalid saved shear demand");if(s.reading){pending.assign(broken.size(),0);pendingRemoved.assign(c.parts.size(),0);multipliers.assign(c.bonds.size()*16,glm::dvec3(0));committed.clear();Connectivity(c);}}

// Reusable DCC/offline route: explicit material nodes/cells/interfaces, never
// welding a visual seam or guessing adjacency from imported vertex positions.
// Parsing is a bounded text record reader; geometry and mass use M62 validation.
bool ImportFracturePartition(const std::string& text,DeformableAsset& output,std::string& error){
    try{if(text.size()>8*1024*1024)throw std::runtime_error("partition source exceeds 8 MiB");
        std::istringstream in(text);std::string token;unsigned version=0;in>>token>>version;if(token!="JudasFractureSource"||version!=1)throw std::runtime_error("expected JudasFractureSource 1");
        DeformableAsset a;a.kind=DeformableKind::Solid;a.fracture=std::make_shared<FractureCook>();auto& c=*a.fracture;
        auto vector=[&](glm::dvec3& v){in>>v.x>>v.y>>v.z;};auto list=[&](std::vector<unsigned>& v){unsigned count=0;in>>count;if(count>4096)throw std::runtime_error("partition list exceeds 4096");v.resize(count);for(auto& n:v)in>>n;};
        bool end=false;while(in>>token){if(token=="node"){glm::dvec3 v;vector(v);a.nodes.push_back(v);if(a.nodes.size()>4096)throw std::runtime_error("partition node limit");}
            else if(token=="tet"){glm::uvec4 t;in>>t.x>>t.y>>t.z>>t.w;a.tetrahedra.push_back(t);if(a.tetrahedra.size()>8192)throw std::runtime_error("partition tetrahedron limit");}
            else if(token=="part"){FracturePart p;in>>std::quoted(p.key);vector(p.proxyCenter);vector(p.proxyHalf);list(p.nodes);list(p.tets);c.parts.push_back(std::move(p));if(c.parts.size()>256)throw std::runtime_error("partition piece limit");}
            else if(token=="bond"){FractureBond b;unsigned n=0;in>>std::quoted(b.key)>>b.a>>b.b;vector(b.normal);vector(b.centroid);in>>b.area>>n;if(n>16)throw std::runtime_error("interface correspondence limit");b.pairs.resize(n);for(auto& p:b.pairs)in>>p.x>>p.y;c.bonds.push_back(std::move(b));if(c.bonds.size()>1024)throw std::runtime_error("interface limit");}
            else if(token=="group"){std::string name;std::vector<unsigned> nodes;in>>std::quoted(name);list(nodes);if(!a.groups.emplace(name,std::move(nodes)).second)throw std::runtime_error("duplicate group");}
            else if(token=="settings"){in>>c.rigid>>c.tension>>c.shear>>c.compression>>c.compliance>>c.pieceBudget;}
            else if(token=="structural"){in>>c.structural;}
            else if(token=="strength"){std::string key;double tension,shear,compression;in>>std::quoted(key)>>tension>>shear>>compression;
                auto b=std::find_if(c.bonds.begin(),c.bonds.end(),[&](const auto& x){return x.key==key;});
                if(b==c.bonds.end())throw std::runtime_error("strength requires an already authored interface");
                b->tension=tension;b->shear=shear;b->compression=compression;
            }
            else if(token=="end"){end=true;break;}else throw std::runtime_error("unknown partition record: "+token);
            if(!in)throw std::runtime_error("truncated partition record: "+token);
        }
        if(!end||(in>>token))throw std::runtime_error("missing end/trailing partition source data");
        if(!PrepareDeformableAsset(a,error))return false;
        output=std::move(a);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
