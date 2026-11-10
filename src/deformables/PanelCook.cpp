#include "PanelCook.h"
#include "SkeletalAnimation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <map>
namespace {
using V=glm::dvec3;
MeshVertex RestVertex(const MeshData& source,unsigned index,const std::vector<glm::mat4>& palette,float& orientation){
    auto vertex=source.vertices.at(index);orientation=1;
    if(palette.empty())return vertex;
    const auto& weights=source.skinVertices.at(index);glm::mat4 transform(0);
    for(unsigned k=0;k<4;++k){transform+=palette.at(weights.joints[k])*weights.weights[k];transform+=palette.at(weights.joints1[k])*weights.weights1[k];}
    auto linear=glm::mat3(transform);float determinant=glm::determinant(linear);
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-12f)throw std::invalid_argument("singular imported panel rest frame");
    auto unit=[](glm::vec3 p){float length=glm::length(p);return length>1e-12f?p/length:glm::vec3(0);};
    orientation=determinant<0?-1.f:1.f;vertex.position=glm::vec3(transform*glm::vec4(vertex.position,1));
    vertex.normal=unit(glm::transpose(glm::inverse(linear))*vertex.normal);
    vertex.tangent=glm::vec4(unit(linear*glm::vec3(vertex.tangent)),vertex.tangent.w*orientation);
    return vertex;
}
std::vector<glm::mat4> RestPalette(const MeshData& source){
    if(!source.skeletal)return {};
    if(source.skinVertices.size()!=source.vertices.size())throw std::invalid_argument("incomplete imported panel rest-skin stream");
    return ResolveSkinMatrices(source.skeletal->skeleton,source.skeletal->skeleton.rest);
}
MeshData FlattenRest(const MeshData& source){
    auto output=source;auto palette=RestPalette(source);std::vector<float> orientation(source.vertices.size(),1);
    for(unsigned i=0;i<output.vertices.size();++i)output.vertices[i]=RestVertex(source,i,palette,orientation[i]);
    for(size_t t=0;t+2<output.indices.size();t+=3)if(orientation.at(output.indices[t])+orientation.at(output.indices[t+1])+orientation.at(output.indices[t+2])<0)std::swap(output.indices[t+1],output.indices[t+2]);
    output.skeletal.reset();output.skinVertices.clear();return output;
}

glm::dvec3 Barycentric(const V& p,const V& a,const V& b,const V& c,unsigned u,unsigned v){
    glm::dmat2 e(glm::dvec2(b[u]-a[u],b[v]-a[v]),glm::dvec2(c[u]-a[u],c[v]-a[v]));
    if(std::abs(glm::determinant(e))<1e-14)return V(-1);
    auto w=glm::inverse(e)*glm::dvec2(p[u]-a[u],p[v]-a[v]);return {1-w.x-w.y,w.x,w.y};
}
}
bool CookDeformablePanel(const MeshData& source,const PanelCookSettings& settings,DeformableAsset& output,std::string& error){
    try{
        MeshData flattened;if(source.skeletal)flattened=FlattenRest(source);
        const auto& mesh=source.skeletal?flattened:source;
        if(!std::isfinite(settings.padding)||settings.padding<0||settings.padding>1||settings.normalAxis>2||settings.columns<2||settings.rows<2||settings.columns>32||settings.rows>32)throw std::invalid_argument("panel grid requires 2..32 rows/columns and normal axis X/Y/Z");
        if(mesh.vertices.empty()||mesh.vertices.size()>65536||mesh.indices.empty()||mesh.indices.size()>65536||mesh.indices.size()%3)throw std::invalid_argument("select a bounded panel region (at most 65536 vertices/indices)");
        const unsigned n=settings.normalAxis,u=n==0?1:0,v=n==2?1:2;
        V low(std::numeric_limits<double>::max()),high(-low);
        for(const auto& vertex:mesh.vertices){V p=vertex.position;for(int k=0;k<3;++k)if(!std::isfinite(p[k]))throw std::invalid_argument("nonfinite panel source");low=glm::min(low,p);high=glm::max(high,p);}
        for(auto index:mesh.indices)if(index>=mesh.vertices.size())throw std::invalid_argument("panel source index out of bounds");
        if(high[u]-low[u]<1e-6||high[v]-low[v]<1e-6)throw std::invalid_argument("panel projection has no area; choose another normal axis");
        low[u]-=settings.padding;low[v]-=settings.padding;high[u]+=settings.padding;high[v]+=settings.padding;
        auto a=MakeDeformableSheet(settings.columns,settings.rows,high[u]-low[u],high[v]-low[v]);
        for(unsigned y=0;y<settings.rows;++y)for(unsigned x=0;x<settings.columns;++x){
            V p(0);p[u]=glm::mix(low[u],high[u],double(x)/(settings.columns-1));p[v]=glm::mix(high[v],low[v],double(y)/(settings.rows-1));
            double nearest=std::numeric_limits<double>::max(),height=0;
            for(const auto& vertex:mesh.vertices){V d=V(vertex.position)-p;double distance=d[u]*d[u]+d[v]*d[v];if(distance<nearest){nearest=distance;height=vertex.position[n];}}
            bool hit=false;
            for(size_t t=0;t<mesh.indices.size();t+=3){V p0=mesh.vertices[mesh.indices[t]].position,p1=mesh.vertices[mesh.indices[t+1]].position,p2=mesh.vertices[mesh.indices[t+2]].position;
                auto b=Barycentric(p,p0,p1,p2,u,v);if(glm::any(glm::lessThan(b,V(-1e-8))))continue;
                double h=p0[n]*b.x+p1[n]*b.y+p2[n]*b.z;if(!hit||h>height)height=h;hit=true;
            }
            p[n]=height;a.nodes[y*settings.columns+x]=p;
        }
        a.groups.clear();
        for(unsigned y=0;y<settings.rows;++y)for(unsigned x=0;x<settings.columns;++x){unsigned index=y*settings.columns+x;
            if(!x)a.groups["left"].push_back(index);
            if(x+1==settings.columns)a.groups["right"].push_back(index);
            if(!y)a.groups["top"].push_back(index);
            if(y+1==settings.rows)a.groups["bottom"].push_back(index);
        }
        a.render=mesh;a.binding.clear();a.panelBinding=true;
        for(const auto& vertex:mesh.vertices){bool found=false;
            for(auto t:a.triangles){auto b=Barycentric(V(vertex.position),a.nodes[t.x],a.nodes[t.y],a.nodes[t.z],u,v);
                if(glm::any(glm::lessThan(b,V(-1e-8))))continue;
                for(unsigned k=0;k<3;++k)b[k]=std::max(0.,b[k]);
                b/=b.x+b.y+b.z;
                a.binding.push_back({{t.x,t.y,t.z,t.z},{b,0}});found=true;break;
            }
            if(!found)throw std::invalid_argument("panel source lies outside fitted grid");
        }
        if(!PrepareDeformableAsset(a,error))return false;
        output=std::move(a);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void MapDeformablePanel(const DeformableInstance& material,double alpha,MeshData& mesh){
    const auto& asset=*material.asset;
    if(mesh.vertices.size()!=asset.render.vertices.size())mesh=asset.render;
    alpha=std::clamp(alpha,0.,1.);
    for(unsigned i=0;i<mesh.vertices.size();++i){const auto& b=asset.binding[i];
        V p=glm::mix(material.previous[b.nodes.x],material.positions[b.nodes.x],alpha),q=glm::mix(material.previous[b.nodes.y],material.positions[b.nodes.y],alpha),r=glm::mix(material.previous[b.nodes.z],material.positions[b.nodes.z],alpha);
        V rp=asset.nodes[b.nodes.x],rq=asset.nodes[b.nodes.y],rr=asset.nodes[b.nodes.z];
        auto normal=glm::cross(q-p,r-p);double area=glm::length(normal);
        // Degenerate instantaneous triangles have no meaningful local frame.
        // Keep a finite weighted surface, and let physical diagnostics report it.
        if(area<1e-14){mesh.vertices[i].position=glm::vec3(p*b.weights.x+q*b.weights.y+r*b.weights.z);continue;}
        normal/=area;V rn=glm::normalize(glm::cross(rq-rp,rr-rp)),restX=glm::normalize(rq-rp),nowX=glm::normalize(q-p),restY=glm::cross(rn,restX),nowY=glm::cross(normal,nowX);
        auto rotate=[&](V value){return nowX*glm::dot(value,restX)+nowY*glm::dot(value,restY)+normal*glm::dot(value,rn);};
        const auto& source=asset.render.vertices[i];
        V residual=V(source.position)-(rp*b.weights.x+rq*b.weights.y+rr*b.weights.z);
        auto& mapped=mesh.vertices[i];mapped.position=glm::vec3(p*b.weights.x+q*b.weights.y+r*b.weights.z+rotate(residual));
        mapped.normal=glm::vec3(rotate(V(source.normal)));mapped.tangent=glm::vec4(glm::vec3(rotate(V(glm::vec3(source.tangent)))),source.tangent.w);
    }
}
bool SelectDeformablePanelRegion(const MeshData& source,const std::string& part,MeshData& output,std::string& error){
    try{
    if(part.empty()){output=FlattenRest(source);error.clear();return true;}
    auto palette=RestPalette(source);
    MeshData selected;selected.sourceNodes=source.sourceNodes;
    std::map<unsigned,unsigned> vertices;std::map<int,int> materials;
    std::vector<float> orientation;
    for(const auto& primitive:source.primitives)if(primitive.part==part){
        if(primitive.first>source.indices.size()||primitive.count>source.indices.size()-primitive.first||primitive.count%3){error="invalid source panel primitive";return false;}
        int slot=-1;
        if(primitive.material>=0){
            if(size_t(primitive.material)>=source.materials.size()){error="source panel material out of range";return false;}
            auto [m,added]=materials.emplace(primitive.material,int(selected.materials.size()));slot=m->second;
            if(added){selected.materials.push_back(source.materials[primitive.material]);selected.materialKeys.push_back(size_t(primitive.material)<source.materialKeys.size()?source.materialKeys[primitive.material]:std::to_string(primitive.material));}
        }
        unsigned first=unsigned(selected.indices.size());
        for(unsigned i=primitive.first;i<primitive.first+primitive.count;++i){auto sourceIndex=source.indices[i];
            if(sourceIndex>=source.vertices.size()){error="source panel index out of range";return false;}
            auto [v,added]=vertices.emplace(sourceIndex,unsigned(selected.vertices.size()));
            if(added){float sign=1;selected.vertices.push_back(RestVertex(source,sourceIndex,palette,sign));orientation.push_back(sign);}
            selected.indices.push_back(v->second);
        }
        selected.primitives.emplace_back(first,primitive.count,slot,primitive.part,primitive.node);
    }
    if(selected.indices.empty()){error="imported panel part not found: "+part;return false;}
    for(size_t t=0;t<selected.indices.size();t+=3)if(orientation[selected.indices[t]]+orientation[selected.indices[t+1]]+orientation[selected.indices[t+2]]<0)std::swap(selected.indices[t+1],selected.indices[t+2]);
    output=std::move(selected);error.clear();return true;
    }catch(const std::exception& exception){error=exception.what();return false;}
}
