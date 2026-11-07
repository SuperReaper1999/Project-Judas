#include "fluid/GameWaterSurface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>
namespace JudasWater {
MeshData BuildReferenceSurface(const SurfaceField &field,unsigned subdivisions) {
    MeshData mesh;if(field.intervals.empty())return mesh;
    const auto n=field.cells;const float h=field.spacing;
    subdivisions=std::clamp(subdivisions,1u,6u);
    // Only top patches and a one-cell shoreline halo are refined. Never tessellate
    // the entire three-dimensional simulation volume.
    std::set<std::array<int,3>> patches;
    for(int z=0;z<n.z;++z)for(int y=0;y<n.y;++y)for(int x=0;x<n.x;++x) {
        const auto v=field.intervals[field.Index(x,y,z)];
        if(v.z<=0||v.y>(y+1)*h+h*.001f)continue;
        for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx)
            if(x+dx>=0&&z+dz>=0&&x+dx<n.x&&z+dz<n.z)patches.insert({x+dx,y,z+dz});
    }
    struct Point {MeshVertex vertex;float coverage;};
    auto point=[&](float x,float z,float height) {
        const auto s=field.At(x,z,height);MeshVertex v;
        v.position=field.origin+field.rotation*glm::vec3(x,s.height,z);
        v.normal=field.rotation*glm::normalize(glm::vec3(-s.gradient.x,1,-s.gradient.y));
        v.uv={x,z};v.uv1=s.velocity;
        // Private water-vertex payload: stable layer reference for GPU height
        // reconstruction. No surface mass or collision data is stored here.
        v.tangent.w=height;
        return Point{v,s.coverage};
    };
    for(const auto &patch:patches) {
        const int x=patch[0],y=patch[1],z=patch[2];
        const auto own=field.intervals[field.Index(x,y,z)];
        if(own.z>0&&own.y>(y+1)*h+h*.001f)continue;
        float reference=own.z>0?own.y:(y+1)*h;
        for(unsigned b=0;b<subdivisions;++b)for(unsigned a=0;a<subdivisions;++a) {
            const float x0=(x+float(a)/subdivisions)*h,x1=(x+float(a+1)/subdivisions)*h;
            const float z0=(z+float(b)/subdivisions)*h,z1=(z+float(b+1)/subdivisions)*h;
            std::vector<Point> polygon={point(x0,z0,reference),point(x0,z1,reference),point(x1,z1,reference),point(x1,z0,reference)};
            std::vector<Point> clipped;
            for(unsigned i=0;i<polygon.size();++i) {
                const auto &p=polygon[i],&q=polygon[(i+1)%polygon.size()];
                const bool inside=p.coverage>=.35f,next=q.coverage>=.35f;
                if(inside)clipped.push_back(p);
                if(inside!=next) {
                    const float t=(.35f-p.coverage)/(q.coverage-p.coverage);
                    Point r;r.coverage=.35f;r.vertex.position=glm::mix(p.vertex.position,q.vertex.position,t);
                    r.vertex.normal=glm::normalize(glm::mix(p.vertex.normal,q.vertex.normal,t));
                    r.vertex.uv=glm::mix(p.vertex.uv,q.vertex.uv,t);r.vertex.uv1=glm::mix(p.vertex.uv1,q.vertex.uv1,t);clipped.push_back(r);
                    clipped.back().vertex.tangent.w=reference;
                }
            }
            for(unsigned i=1;i+1<clipped.size();++i)
                for(const auto *v:{&clipped[0].vertex,&clipped[i].vertex,&clipped[i+1].vertex})mesh.vertices.push_back(*v);
            // Close only exposed outlines, including the finite region edge.
            // Opaque terrain naturally hides these faces; no tiled voxel walls.
            for(unsigned i=0;i<clipped.size();++i){
                const auto &p=clipped[i],&q=clipped[(i+1)%clipped.size()];
                if(glm::length(q.vertex.uv-p.vertex.uv)<1e-5f)continue;
                auto edge=[&](int axis,float value){return std::abs(p.vertex.uv[axis]-value)<1e-5f&&std::abs(q.vertex.uv[axis]-value)<1e-5f;};
                if(!((p.coverage<.351f&&q.coverage<.351f)||edge(0,0)||edge(1,0)||edge(0,n.x*h)||edge(1,n.z*h)))continue;
                auto pTop=p.vertex,qTop=q.vertex,pBottom=p.vertex,qBottom=q.vertex;
                const auto outward=glm::normalize(glm::cross(qTop.position-pTop.position,field.rotation*glm::vec3(0,1,0)));
                pBottom.position=field.origin+field.rotation*glm::vec3(pTop.uv.x,field.At(pTop.uv.x,pTop.uv.y,reference).bottom,pTop.uv.y);
                qBottom.position=field.origin+field.rotation*glm::vec3(qTop.uv.x,field.At(qTop.uv.x,qTop.uv.y,reference).bottom,qTop.uv.y);
                for(auto* v:{&pTop,&qTop,&pBottom,&qBottom}){v->normal=outward;v->tangent.z=1;}
                pBottom.tangent.x=qBottom.tangent.x=0;
                for(const auto* v:{&pBottom,&qBottom,&qTop,&pBottom,&qTop,&pTop})mesh.vertices.push_back(*v);
            }
        }
    }
    return mesh;
}

}
