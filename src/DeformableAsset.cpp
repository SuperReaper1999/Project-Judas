#include "Deformable.h"
#include "SaveArchive.h"
#include "Scene.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace {
bool finite(glm::dvec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
        }
template<class T>std::string text(const T& v){std::ostringstream s;
        s<<std::setprecision(17)<<v;
        return s.str();
        }
double volume(const glm::dmat3& m){return glm::determinant(m)/6;
        }
glm::uvec3 key(glm::uvec3 f){if(f.x>f.y)std::swap(f.x,f.y);
        if(f.y>f.z)std::swap(f.y,f.z);
        if(f.x>f.y)std::swap(f.x,f.y);
        return f;
        }
struct Less3{bool operator()(glm::uvec3 a,glm::uvec3 b)const{return std::tie(a.x,a.y,a.z)<std::tie(b.x,b.y,b.z);
            }};
void renderBoundary(DeformableAsset& a){
    a.render={};
        a.binding.clear();
    for(auto f:a.triangles)for(unsigned n:{f.x,f.y,f.z}){
        MeshVertex v;
            v.position=glm::vec3(a.nodes[n]);
            v.uv={float(a.nodes[n].x),float(a.nodes[n].y)};
        a.render.indices.push_back(unsigned(a.render.vertices.size()));
            a.render.vertices.push_back(v);
        DeformableBinding b;
            b.nodes=glm::uvec4(n);
            a.binding.push_back(b);
    }
}
void payload(SaveArchive& ar,DeformableAsset& a){
    ar(a.kind,a.nodes,a.massWeights,a.triangles,a.tetrahedra,a.groups,a.sourceAsset,a.sourceFingerprint);
    uint32_t n=uint32_t(a.render.vertices.size());
        ar(n);
        ar.Require(n<=65536,"deformable render vertex limit");
        if(ar.reading)a.render.vertices.resize(n);
    for(auto& v:a.render.vertices)ar(v.position,v.normal,v.uv,v.tangent);
    ar(a.render.indices,a.binding);
        n=uint32_t(a.render.primitives.size());
        ar(n);
        ar.Require(n<=64,"deformable material slot limit");
        if(ar.reading)a.render.primitives.resize(n);
        for(auto& p:a.render.primitives)ar(p.first,p.count,p.material);
}
}
void DeformableMaterial::Save(SaveArchive& a){a(density,stretchCompliance,shearCompliance,bendCompliance,volumeCompliance,damping,thickness,friction,airDrag,yieldStrain,plasticRate,maximumPlasticStrain,airVelocity);
    }
void DeformableAttachment::Save(SaveArchive& a){a(group,joint,kind,target,offset,enabled);
    }
void DeformableBinding::Save(SaveArchive& a){a(nodes,weights);
    }
bool ValidDeformableSettings(const DeformableSettings& s,std::string& error){
    const auto& m=s.material;
    if(s.substeps<1||s.substeps>16||s.iterations<1||s.iterations>16||s.collisionLayer>=64||s.attachments.size()>64){error="deformable quality/filter/attachment bounds exceeded";
        return false;
        }
    for(double v:{m.density,m.stretchCompliance,m.shearCompliance,m.bendCompliance,m.volumeCompliance,m.damping,m.thickness,m.friction,m.airDrag,m.yieldStrain,m.plasticRate,m.maximumPlasticStrain})if(!std::isfinite(v)||v<0){error="invalid deformable material";
        return false;
        }
    if(m.density<=0||m.thickness<=0||m.thickness>1||m.friction>2||m.damping>20||m.airDrag>20||m.plasticRate>10||m.maximumPlasticStrain>0.8||!finite(m.airVelocity)){error="deformable material outside supported range";
        return false;
        }
    std::set<std::string> groups;
    for(auto& a:s.attachments)if(a.group.empty()||!groups.insert(a.group).second||int(a.kind)<0||int(a.kind)>2||!finite(a.offset)||(a.kind!=DeformableAttachment::Kind::World&&!a.target)||(a.kind==DeformableAttachment::Kind::Bone&&a.joint.empty())||!std::isfinite(a.forceLimit)||!std::isfinite(a.torqueLimit)||a.forceLimit<0||a.torqueLimit<0||((a.forceLimit>0||a.torqueLimit>0)&&a.kind!=DeformableAttachment::Kind::Body)){error="invalid/duplicate deformable attachment or finite strength";
        return false;
        }
    return true;
}
std::map<std::string,std::string> DeformableProperties(const SceneObject& o){
    std::map<std::string,std::string> p;
    if(!o.deformable)return p;
    const auto& s=*o.deformable;
#define F(v) p["deformable." #v]=text(s.v)
    F(asset);
    F(enabled);
    F(selfContact);
    F(substeps);
    F(iterations);
    F(collisionLayer);
    F(collisionMask);
    F(material.density);
    F(material.stretchCompliance);
    F(material.shearCompliance);
    F(material.bendCompliance);
    F(material.volumeCompliance);
    F(material.damping);
    F(material.thickness);
    F(material.friction);
    F(material.airDrag);
    F(material.yieldStrain);
    F(material.plasticRate);
    F(material.maximumPlasticStrain);
#undef F
    p["deformable.airVelocity"]=text(s.material.airVelocity.x)+" "+text(s.material.airVelocity.y)+" "+text(s.material.airVelocity.z);
    std::ostringstream a;
    a<<std::setprecision(17)<<s.attachments.size();
    for(auto& x:s.attachments)a<<' '<<std::quoted(x.group)<<' '<<int(x.kind)<<' '<<x.target<<' '<<std::quoted(x.joint)<<' '<<x.offset.x<<' '<<x.offset.y<<' '<<x.offset.z<<' '<<x.enabled;
    p["deformable.attachments"]=a.str();
    unsigned finiteCount=0;for(const auto& x:s.attachments)if(x.forceLimit>0||x.torqueLimit>0)++finiteCount;
    if(finiteCount){std::ostringstream limits;limits<<std::setprecision(17)<<finiteCount;
        for(const auto& x:s.attachments)if(x.forceLimit>0||x.torqueLimit>0)limits<<' '<<std::quoted(x.group)<<' '<<x.forceLimit<<' '<<x.torqueLimit;
        p["deformable.attachmentLimits"]=limits.str();
    }
    return p;
}
bool ApplyDeformableProperties(const std::map<std::string,std::string>& p,SceneObject& o,std::string& error){
    if(p.empty()){o.deformable.reset();
        return true;
        }DeformableSettings s;
    SceneObject probe;probe.deformable=s;auto known=DeformableProperties(probe);known["deformable.attachmentLimits"]={};for(const auto& [key,value]:p){(void)value;if(!known.count(key)){error="unknown deformable property: "+key;return false;}}
    auto read=[&](const char* key,auto& v){auto it=p.find(key);
        if(it==p.end()){error=std::string("missing ")+key;
            return false;
            }if constexpr(std::is_same_v<std::decay_t<decltype(v)>,std::string>)v=it->second;
        else{std::istringstream in(it->second);
            if(!(in>>v)){error=std::string("invalid ")+key;
                return false;
                }in>>std::ws;
            if(!in.eof())return false;
            }return true;
        };
#define R(v) if(!read("deformable." #v,s.v))return false
    R(asset);
    R(enabled);
    R(selfContact);
    R(substeps);
    R(iterations);
    R(collisionLayer);
    R(collisionMask);
    R(material.density);
    R(material.stretchCompliance);
    R(material.shearCompliance);
    R(material.bendCompliance);
    R(material.volumeCompliance);
    R(material.damping);
    R(material.thickness);
    R(material.friction);
    R(material.airDrag);
    R(material.yieldStrain);
    R(material.plasticRate);
    R(material.maximumPlasticStrain);
#undef R
    auto air=p.find("deformable.airVelocity"),at=p.find("deformable.attachments");
    if(air==p.end()||at==p.end()){error="missing deformable airflow/attachments";
        return false;
        }
    std::istringstream v(air->second);
    if(!(v>>s.material.airVelocity.x>>s.material.airVelocity.y>>s.material.airVelocity.z))return false;
    v>>std::ws;
    if(!v.eof())return false;
    std::istringstream in(at->second);
    unsigned n;
    if(!(in>>n)||n>64)return false;
    s.attachments.resize(n);
    for(auto& a:s.attachments){int kind;
        if(!(in>>std::quoted(a.group)>>kind>>a.target>>std::quoted(a.joint)>>a.offset.x>>a.offset.y>>a.offset.z>>a.enabled))return false;
        a.kind=DeformableAttachment::Kind(kind);
        }in>>std::ws;
    if(!in.eof())return false;
    if(auto limits=p.find("deformable.attachmentLimits");limits!=p.end()){
        std::istringstream values(limits->second);unsigned count=0;std::set<std::string> seen;
        if(!(values>>count)||count==0||count>s.attachments.size()){error="invalid material attachment strength count";return false;}
        for(unsigned i=0;i<count;++i){std::string group;double force=0,torque=0;
            if(!(values>>std::quoted(group)>>force>>torque)||!seen.insert(group).second){error="invalid/duplicate material attachment strength";return false;}
            auto found=std::find_if(s.attachments.begin(),s.attachments.end(),[&](const auto& a){return a.group==group;});
            if(found==s.attachments.end()){error="material attachment strength names an unknown group";return false;}
            found->forceLimit=force;found->torqueLimit=torque;
        }
        values>>std::ws;if(!values.eof()){error="trailing material attachment strength fields";return false;}
    }
    if(!ValidDeformableSettings(s,error))return false;
    o.deformable=s;
    if(DeformableProperties(o).size()!=p.size()){error="unknown deformable property";
        return false;
        }return true;
}
bool PrepareDeformableAsset(DeformableAsset& a,std::string& error){
    auto fail=[&](const char* s){error=s;
        return false;
        };
    if(a.nodes.size()<3||a.nodes.size()>8192||a.triangles.size()>32768||a.tetrahedra.size()>32768||int(a.kind)<0||int(a.kind)>1)return fail("deformable topology bounds exceeded");
    for(auto p:a.nodes)if(!finite(p))return fail("nonfinite deformable node");
    if(a.massWeights.empty())a.massWeights.assign(a.nodes.size(),1);
    if(a.massWeights.size()!=a.nodes.size())return fail("deformable mass weight count");
    for(auto w:a.massWeights)if(!std::isfinite(w)||w<=0)return fail("invalid deformable mass weight");
    a.measures.assign(a.nodes.size(),0);
    a.cloth.clear();
    a.solids.clear();
    a.bends.clear();
    a.edges.clear();
    a.neighbors.assign(a.nodes.size(),{});
    a.cellSize=0;
    if(a.kind==DeformableKind::Solid){
        if(a.tetrahedra.empty())return fail("solid requires tetrahedra");
        std::map<glm::uvec3,std::pair<unsigned,glm::uvec3>,Less3> faces;
        std::set<std::array<unsigned,4>> seen;
        for(auto t:a.tetrahedra){for(int k=0;k<4;++k)if(t[k]>=a.nodes.size())return fail("tet node outside topology");
            std::array<unsigned,4> sorted{t.x,t.y,t.z,t.w};
            std::sort(sorted.begin(),sorted.end());
            if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end()||!seen.insert(sorted).second)return fail("duplicate/degenerate tet connectivity");
            glm::dmat3 rest(a.nodes[t.y]-a.nodes[t.x],a.nodes[t.z]-a.nodes[t.x],a.nodes[t.w]-a.nodes[t.x]);
            double vol=volume(rest);
            if(!std::isfinite(vol)||vol<=1e-12)return fail("tet orientation/volume invalid (finite positive orientation required)");
            auto inverse=glm::inverse(rest);
            for(int k=0;k<3;++k)if(!finite(inverse[k]))return fail("invalid derived tet rest inverse");
            a.solids.push_back({t,rest,inverse,vol});
            for(int k=0;k<4;++k)a.measures[t[k]]+=vol/4;
            for(auto f:{glm::uvec3(t.y,t.z,t.w),glm::uvec3(t.x,t.w,t.z),glm::uvec3(t.x,t.y,t.w),glm::uvec3(t.x,t.z,t.y)}){auto& item=faces[key(f)];
                ++item.first;
                item.second=f;
                if(item.first>2)return fail("nonmanifold tetrahedral face");
                }
        }
        a.triangles.clear();
        for(auto& [_,f]:faces)if(f.first==1)a.triangles.push_back(f.second);
    }else if(!a.tetrahedra.empty()||a.triangles.empty())return fail("cloth requires triangles and no tetrahedra");
    std::map<std::pair<unsigned,unsigned>,std::vector<unsigned>> edgeFaces;
    std::set<glm::uvec3,Less3> seenFaces;
    for(auto f:a.triangles){for(int k=0;k<3;++k)if(f[k]>=a.nodes.size())return fail("triangle node outside topology");
        if(!seenFaces.insert(key(f)).second)return fail("duplicate triangle");
        auto e1=a.nodes[f.y]-a.nodes[f.x],e2=a.nodes[f.z]-a.nodes[f.x];
        double len=glm::length(e1),area=glm::length(glm::cross(e1,e2))/2;
        if(!std::isfinite(area)||!std::isfinite(len)||area<1e-12)return fail("degenerate/nonfinite surface triangle area");
        if(a.kind==DeformableKind::Cloth){auto x=e1/len;
            double u=glm::dot(x,e2),v=glm::length(e2-u*x);
            auto inverse=glm::inverse(glm::dmat2(len,0,u,v));
            for(int k=0;k<2;++k)for(int j=0;j<2;++j)if(!std::isfinite(inverse[k][j]))return fail("invalid derived cloth rest inverse");
            a.cloth.push_back({f,inverse,area});
            for(int k=0;k<3;++k)a.measures[f[k]]+=area/3;
            }
        for(int k=0;k<3;++k){unsigned i=f[k],j=f[(k+1)%3],op=f[(k+2)%3];
            auto pair=std::minmax(i,j);
            edgeFaces[pair].push_back(op);
            a.neighbors[i].push_back(j);
            a.neighbors[j].push_back(i);
            a.cellSize=std::max(a.cellSize,glm::length(a.nodes[i]-a.nodes[j]));
            }
    }
    for(auto& [edge,opposite]:edgeFaces){if(opposite.size()>2)return fail("nonmanifold surface edge");
        a.edges.push_back({edge.first,edge.second});
        if(a.kind==DeformableKind::Cloth&&opposite.size()==2)a.bends.push_back({opposite[0],opposite[1],glm::length(a.nodes[opposite[0]]-a.nodes[opposite[1]])});
        }
    for(auto& neighbors:a.neighbors){std::sort(neighbors.begin(),neighbors.end());
        neighbors.erase(std::unique(neighbors.begin(),neighbors.end()),neighbors.end());
        }
    // Immutable one-ring exclusions. Runtime marks these bounded lists once
    // per query rather than repeatedly searching four node adjacency lists.
    std::vector<std::vector<unsigned>> nodeFaces(a.nodes.size()),nodeEdges(a.nodes.size());
    for(unsigned i=0;i<a.triangles.size();++i)for(unsigned n:{a.triangles[i].x,a.triangles[i].y,a.triangles[i].z})nodeFaces[n].push_back(i);
    for(unsigned i=0;i<a.edges.size();++i)for(unsigned n:{a.edges[i].x,a.edges[i].y})nodeEdges[n].push_back(i);
    a.excludedFaces.assign(a.nodes.size(),{});a.excludedEdges.assign(a.edges.size(),{});size_t exclusionCount=0;
    auto collect=[&](unsigned node,const auto& incident,std::set<unsigned>& found){found.insert(incident[node].begin(),incident[node].end());for(auto neighbor:a.neighbors[node])found.insert(incident[neighbor].begin(),incident[neighbor].end());};
    for(unsigned n=0;n<a.nodes.size();++n){std::set<unsigned> found;collect(n,nodeFaces,found);a.excludedFaces[n]={found.begin(),found.end()};exclusionCount+=found.size();if(exclusionCount>2000000)return fail("surface adjacency budget exceeded");}
    for(unsigned e=0;e<a.edges.size();++e){std::set<unsigned> found;collect(a.edges[e].x,nodeEdges,found);collect(a.edges[e].y,nodeEdges,found);a.excludedEdges[e]={found.begin(),found.end()};exclusionCount+=found.size();if(exclusionCount>2000000)return fail("surface adjacency budget exceeded");}
    for(auto measure:a.measures)if(!std::isfinite(measure)||measure<=0)return fail("unused/nonfinite simulation node measure");
    if(a.groups.size()>64)return fail("too many attachment groups");
    for(auto& [name,ids]:a.groups){if(name.empty()||ids.empty())return fail("empty attachment group");
        std::set<unsigned> seenIds;
        for(auto id:ids)if(id>=a.nodes.size()||!seenIds.insert(id).second)return fail("invalid/duplicate group node");
        }
    if(a.render.vertices.empty())renderBoundary(a);
    if(a.render.vertices.size()>65536||a.render.indices.size()>65536||a.binding.size()!=a.render.vertices.size()||a.render.indices.size()%3)return fail("render binding/triangle count exceeds payload bounds or is invalid");
    for(const auto& vertex:a.render.vertices){
        if(!finite(glm::dvec3(vertex.position))||!finite(glm::dvec3(vertex.normal))||
           !finite(glm::dvec3(vertex.tangent))||!std::isfinite(vertex.tangent.w)||
           !std::isfinite(vertex.uv.x)||!std::isfinite(vertex.uv.y))return fail("nonfinite deformable render vertex");
    }
    for(auto i:a.render.indices)if(i>=a.render.vertices.size())return fail("render index out of range");
    for(auto b:a.binding){double sum=0;
        for(int k=0;k<4;++k){if(b.nodes[k]>=a.nodes.size()||!std::isfinite(b.weights[k])||b.weights[k]<-1e-8)return fail("invalid render binding");
            sum+=b.weights[k];
            }if(std::abs(sum-1)>1e-8)return fail("render binding must sum to one");
        }
    for(auto p:a.render.primitives)if(p.first>a.render.indices.size()||p.count>a.render.indices.size()-p.first||p.count%3)return fail("invalid render material range");
    return PrepareFractureCook(a,error);
}
std::string EncodeDeformableAsset(const DeformableAsset& input){auto a=input;
    std::string error;
    if(!PrepareDeformableAsset(a,error))throw std::invalid_argument(error);
    SaveArchive ar;
    payload(ar,a);
    if(a.fracture&&a.fracture->HasMaterials()){a.fracture->Save(ar);a.fracture->SaveStructural(ar);a.fracture->SaveMaterials(ar);return "JudasDeformable5\n"+ar.bytes;}
    if(a.fracture){a.fracture->Save(ar);if(a.fracture->structural){a.fracture->SaveStructural(ar);return "JudasDeformable3\n"+ar.bytes;}return "JudasDeformable2\n"+ar.bytes;}
    return "JudasDeformable1\n"+ar.bytes;
    }
bool DecodeDeformableAsset(const std::vector<unsigned char>& bytes,DeformableAsset& out,std::string& error){try{std::string b(bytes.begin(),bytes.end());
        const std::string magic="JudasDeformable1\n";
        bool material=b.rfind("JudasDeformable5\n",0)==0;
        bool structural=material||b.rfind("JudasDeformable3\n",0)==0;
        bool fracture=structural||b.rfind("JudasDeformable2\n",0)==0;
        if(!fracture&&b.rfind(magic,0)!=0)throw std::runtime_error("invalid deformable asset version");
        DeformableAsset a;
        SaveArchive ar(b.substr(magic.size()));
        payload(ar,a);
        if(fracture){a.fracture=std::make_shared<FractureCook>();a.fracture->Save(ar);if(structural)a.fracture->SaveStructural(ar);if(material)a.fracture->SaveMaterials(ar);}
        ar.Finish();
        if(!PrepareDeformableAsset(a,error))return false;
        out=std::move(a);
        return true;
        }catch(const std::exception& e){error=e.what();
        return false;
        }}
DeformableAsset MakeDeformableSheet(unsigned columns,unsigned rows,double width,double height,unsigned subdivision){
    if(columns<2||rows<2||columns>64||rows>64||subdivision<1||subdivision>4||width<=0||height<=0)throw std::invalid_argument("sheet helper dimensions");
    DeformableAsset a;
    for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x)a.nodes.push_back({width*(double(x)/(columns-1)-.5),height*(1-double(y)/(rows-1)),0});
    for(unsigned y=0;y+1<rows;++y)for(unsigned x=0;x+1<columns;++x){unsigned n=y*columns+x;
        a.triangles.push_back({n,n+columns,n+1});
        a.triangles.push_back({n+1,n+columns,n+columns+1});
        }
    for(unsigned x=0;x<columns;++x)a.groups["top"].push_back(x);
    a.groups["leftShoulder"]={0};
    a.groups["rightShoulder"]={columns-1};
    unsigned w=(columns-1)*subdivision+1,h=(rows-1)*subdivision+1;
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){double fx=double(x)/subdivision,fy=double(y)/subdivision;
        unsigned cx=std::min(unsigned(fx),columns-2),cy=std::min(unsigned(fy),rows-2),n=cy*columns+cx;
        double u=fx-cx,v=fy-cy;
        DeformableBinding b;
        if(u+v<=1){b.nodes={n,n+columns,n+1,0};
            b.weights={1-u-v,v,u,0};
            }else{b.nodes={n+1,n+columns,n+columns+1,0};
            b.weights={1-v,1-u,u+v-1,0};
            }
        MeshVertex mv;
        mv.uv={float(x)/float(w-1),float(y)/float(h-1)};
        for(int k=0;k<4;++k)mv.position+=glm::vec3(a.nodes[b.nodes[k]]*b.weights[k]);
        a.render.vertices.push_back(mv);
        a.binding.push_back(b);
    }
    for(unsigned y=0;y+1<h;++y)for(unsigned x=0;x+1<w;++x){unsigned n=y*w+x;
        for(unsigned i:{n,n+w,n+1,n+1,n+w,n+w+1})a.render.indices.push_back(i);
        }
    std::string error;
    if(!PrepareDeformableAsset(a,error))throw std::invalid_argument(error);
    return a;
}
DeformableAsset MakeDeformableBlock(glm::uvec3 c,glm::dvec3 size){
    if(c.x<1||c.y<1||c.z<1||c.x>16||c.y>16||c.z>16||glm::any(glm::lessThanEqual(size,glm::dvec3(0))))throw std::invalid_argument("block helper dimensions");
    DeformableAsset a;
    a.kind=DeformableKind::Solid;
    auto node=[&](unsigned x,unsigned y,unsigned z){return (z*(c.y+1)+y)*(c.x+1)+x;
        };
    for(unsigned z=0;z<=c.z;++z)for(unsigned y=0;y<=c.y;++y)for(unsigned x=0;x<=c.x;++x){a.nodes.push_back(size*(glm::dvec3(x,y,z)/glm::dvec3(c)-glm::dvec3(.5)));
        if(x==0)a.groups["left"].push_back(node(x,y,z));
        if(x==c.x)a.groups["right"].push_back(node(x,y,z));
        if(x==c.x)a.groups[y>c.y/2?"upperRight":"lowerRight"].push_back(node(x,y,z));
        if(y==0)a.groups["bottom"].push_back(node(x,y,z));
        }
    for(unsigned z=0;z<c.z;++z)for(unsigned y=0;y<c.y;++y)for(unsigned x=0;x<c.x;++x){unsigned v[8];
        for(unsigned k=0;k<8;++k)v[k]=node(x+(k&1),y+((k>>1)&1),z+((k>>2)&1));
        for(auto indices:{glm::uvec4(0,1,3,7),glm::uvec4(0,3,2,7),glm::uvec4(0,2,6,7),glm::uvec4(0,6,4,7),glm::uvec4(0,4,5,7),glm::uvec4(0,5,1,7)})a.tetrahedra.push_back({v[indices.x],v[indices.y],v[indices.z],v[indices.w]});
        }
    std::string error;
    if(!PrepareDeformableAsset(a,error))throw std::invalid_argument(error);
    return a;
}
bool ImportDeformableCloth(const MeshData& mesh,DeformableAsset& out,std::string& error){
    // Weld source POSITION identities, never spatially coincident vertices. OBJ
    // UV/normal splits share an identity; separately authored layers do not.
    DeformableAsset a;
    std::map<uint32_t,unsigned> topology;
    if(!mesh.sourceVertexIds.empty()&&mesh.sourceVertexIds.size()!=mesh.vertices.size()){
        error="cloth source topology stream mismatch"; return false;
    }
    for(unsigned i=0;i<mesh.vertices.size();++i){
        uint32_t identity=mesh.sourceVertexIds.empty()?i:mesh.sourceVertexIds[i];
        auto [it,inserted]=topology.emplace(identity,unsigned(a.nodes.size()));
        if(inserted)a.nodes.push_back(glm::dvec3(mesh.vertices[i].position));
        else if(glm::length(a.nodes[it->second]-glm::dvec3(mesh.vertices[i].position))>1e-7){error="source position identity has inconsistent coordinates";return false;}
        DeformableBinding b; b.nodes=glm::uvec4(it->second); a.binding.push_back(b);
    }
    if(mesh.indices.size()%3){error="cloth import requires triangles";return false;}
    for(size_t i=0;i<mesh.indices.size();i+=3){glm::uvec3 face;
        for(unsigned j=0;j<3;++j){auto index=mesh.indices[i+j];if(index>=a.binding.size()){error="cloth import index out of range";return false;}face[j]=a.binding[index].nodes.x;}
        a.triangles.push_back(face);
    }
    a.render=mesh;
    if(!PrepareDeformableAsset(a,error))return false;
    out=std::move(a);return true;
}
