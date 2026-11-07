#include "GameWaterSurface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace JudasWater {
int SurfaceField::Index(int x,int y,int z) const {
    return x<0||y<0||z<0||x>=cells.x||y>=cells.y||z>=cells.z ? -1 : (z*cells.y+y)*cells.x+x;
}
namespace {
std::array<float,4> Weights(float t) {
    const float a=1-t;
    return {a*a*a/6,(3*t*t*t-6*t*t+4)/6,(-3*t*t*t+3*t*t+3*t+1)/6,t*t*t/6};
}
std::array<float,4> Derivatives(float t) {
    return {-.5f*(1-t)*(1-t),1.5f*t*t-2*t,-1.5f*t*t+t+.5f,.5f*t*t};
}
}
SurfaceField::Sample SurfaceField::At(float x,float z,float height,float lower) const {
    Sample out; out.height=height;
    const float px=x/spacing-.5f,pz=z/spacing-.5f;
    const int ix=int(std::floor(px)),iz=int(std::floor(pz));
    const auto wx=Weights(px-ix),wz=Weights(pz-iz),dx=Derivatives(px-ix),dz=Derivatives(pz-iz);
    float sum=0,bottom=0,total=0;glm::vec2 derivative(0),weightDerivative(0),velocity(0);
    for(int b=0;b<4;++b) for(int a=0;a<4;++a) {
        // Extend to the finite region edge, without joining vertically separate pools.
        const int xx=std::clamp(ix+a-1,0,cells.x-1),zz=std::clamp(iz+b-1,0,cells.z-1);
        int selected=-1;float distance=lower<0?spacing*1.6f:std::numeric_limits<float>::max();
        for(int yy=0;yy<cells.y;) {
            const int j=Index(xx,yy,zz);const auto v=intervals[j];
            if(v.z<=0){yy=std::max(yy+1,int(v.w));continue;}
            if(v.x>height)break;
            if(v.y>lower&&std::abs(v.y-height)<distance){selected=j;distance=std::abs(v.y-height);}
            yy=std::max(yy+1,int(std::ceil(v.y/spacing-.0001f)));
        }
        if(selected<0)continue;
        const float w=wx[a]*wz[b],h=intervals[selected].y;
        const glm::vec2 d(dx[a]*wz[b],wx[a]*dz[b]);
        sum+=w*h;total+=w;derivative+=d*h;weightDerivative+=d;
        bottom+=w*intervals[selected].x;
        velocity+=flow[selected]*w;
    }
    out.coverage=total;
    if(total>1e-6f) {
        out.height=sum/total;
        out.bottom=bottom/total;
        out.gradient=(derivative*total-sum*weightDerivative)/(total*total*spacing);
        out.velocity=velocity/total;
    }
    return out;
}
bool SurfaceField::SameTopology(const SurfaceField& other) const {
    if(cells!=other.cells||spacing!=other.spacing||origin!=other.origin||rotation!=other.rotation||
       intervals.size()!=other.intervals.size())return false;
    std::vector<glm::vec2> a,b;
    auto runs=[](const SurfaceField& f,int x,int z,std::vector<glm::vec2>& result){
        result.clear();
        for(int y=0;y<f.cells.y;++y){const auto v=f.intervals[f.Index(x,y,z)];
            if(v.z>0&&(result.empty()||result.back()!=glm::vec2(v)))result.emplace_back(v.x,v.y);}
    };
    for(int z=0;z<cells.z;++z)for(int x=0;x<cells.x;++x){
        runs(*this,x,z,a);runs(other,x,z,b);if(a.size()!=b.size())return false;
        for(unsigned i=0;i<a.size();++i)
            if(std::abs(a[i].x-b[i].x)>spacing*.1f||std::abs(a[i].y-b[i].y)>spacing*.7f)return false;
    }
    return true;
}
SurfaceField BuildSurfaceField(const Grid &grid) {
    SurfaceField field;
    const auto &c=grid.Configuration();field.cells=c.cells;field.spacing=c.cellSize;
    field.origin=grid.ToWorld({0,0,0});field.rotation=c.rotation;
    const auto &cells=grid.Cells();const auto n=c.cells;const float h=c.cellSize;
    field.intervals.resize(cells.size(),glm::vec4(-1,-1,0,0));field.flow.resize(cells.size());
    if(cells.empty())return field;
    std::vector<float> levels(cells.size(),-1);
    // Sub-2 mm films remain physical water but do not spawn shimmering sheets.
    const double epsilon=std::max(double(h*h*.002f),double(h*h*h)*c.minimumMobileFraction);
    for(int i=0;i<int(cells.size());++i)
        if(cells[i].water>epsilon)grid.CellSurface(i,levels[i]);
    const auto physical=levels;
    // A swimmer or floating solid excludes physical volume. Its visible hull
    // occludes the water; that exclusion must not punch a voxel-shaped air hole
    // in the optical medium around an immersed camera.
    std::vector<unsigned char> excluded(cells.size());
    std::vector<int> distance(cells.size(),-1),front;
    std::vector<float> support(cells.size());
    std::vector<unsigned> witnesses(cells.size());
    for(std::size_t i=0;i<cells.size();++i)
        excluded[i]=cells[i].staticCapacity-cells[i].capacity>=h*h*h*.02f;
    const std::array<glm::ivec2,4> neighbours={glm::ivec2(-1,0),{1,0},{0,-1},{0,1}};
    for(int z=0;z<n.z;++z)for(int y=0;y<n.y;++y)for(int x=0;x<n.x;++x) {
        const int i=grid.Index(x,y,z);
        if(!excluded[i])continue;
        float sum=0;int count=0;
        for(const auto offset:neighbours) {
            const int j=grid.Index(x+offset.x,y,z+offset.y);
            if(j>=0&&!excluded[j]&&physical[j]>y*h&&grid.StaticOpening(i,j)>0){sum+=physical[j];++count;}
        }
        if(count){levels[i]=std::max(levels[i],sum/count);distance[i]=0;front.push_back(i);}
    }
    // Continue through the whole connected exclusion, not merely its outer
    // cell ring. Multi-source breadth-first fronts average equal-distance wet
    // witnesses deterministically. Never fill ordinary dry cells, rise into a
    // dry layer, or cross a closed static aperture. This remains optical data.
    for(std::size_t at=0;at<front.size();++at){
        const int i=front[at],x=i%n.x,y=(i/n.x)%n.y,z=i/(n.x*n.y);
        for(const auto offset:neighbours){
            const int j=grid.Index(x+offset.x,y,z+offset.y);
            if(j<0||!excluded[j]||grid.StaticOpening(i,j)<=0)continue;
            if(distance[j]<0){distance[j]=distance[i]+1;front.push_back(j);}
            if(distance[j]==distance[i]+1){
                support[j]+=levels[i];++witnesses[j];
                levels[j]=std::max(physical[j],support[j]/witnesses[j]);
            }
        }
    }
    for(int z=0;z<n.z;++z)for(int x=0;x<n.x;++x)for(int y=0;y<n.y;) {
        if(levels[grid.Index(x,y,z)]<0){++y;continue;}
        const int first=y;float top=levels[grid.Index(x,y,z)];
        // Consecutive occupied coarse cells form one visible water column.
        // Their unresolved partial-cell voids are not separate glass sheets.
        // A genuinely dry cell still separates layers/cave air, independently
        // of moving-solid continuation and of the physical material ledger.
        while(y+1<n.y&&levels[grid.Index(x,y+1,z)]>=0)
            top=levels[grid.Index(x,++y,z)];
        for(int k=first;k<=y;++k) {
            const int i=grid.Index(x,k,z);const auto v=glm::conjugate(c.rotation)*grid.Velocity(grid.ToWorld({(x+.5f)*h,(k+.5f)*h,(z+.5f)*h}));
            field.intervals[i]={first*h,top,1,glm::length(v)};
            field.flow[i]={v.x,v.z};
        }
        ++y;
    }
    // Dry entries carry a next-wet-run link. Interpolation visits connected
    // columns rather than scanning every cell or assuming all tops occupy the
    // same Y cell. Wet entries retain physical activity in their fourth lane.
    for(int z=0;z<n.z;++z)for(int x=0;x<n.x;++x){
        int next=n.y;
        for(int y=n.y-1;y>=0;--y){auto& v=field.intervals[field.Index(x,y,z)];if(v.z>0)next=y;else v.w=float(next);}
    }
    return field;
}
MeshData BuildPoolSurface(const SurfaceField &field,unsigned subdivisions) {
    MeshData mesh;if(field.intervals.empty())return mesh;
    const auto n=field.cells;const float h=field.spacing;
    subdivisions=std::clamp(subdivisions,1u,6u);
    // One patch per visible column run, independent of its top's Y cell.
    // Height differences within connected water must not spawn overlapping
    // patches and false curtains extending to the pool floor.
    struct Patch {int x,z;float bottom,top;};
    std::vector<Patch> sources;
    std::vector<std::vector<Patch>> columns(std::size_t(n.x)*n.z);
    for(int z=0;z<n.z;++z)for(int x=0;x<n.x;++x)for(int y=0;y<n.y;){
        const auto v=field.intervals[field.Index(x,y,z)];
        if(v.z<=0){y=std::max(y+1,int(v.w));continue;}
        Patch patch{x,z,v.x,v.y};sources.push_back(patch);columns[z*n.x+x].push_back(patch);
        y=std::max(y+1,int(std::ceil(v.y/h-.0001f)));
    }
    for(const auto& patch:sources)for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){
        const int x=patch.x+dx,z=patch.z+dz;if(x<0||z<0||x>=n.x||z>=n.z)continue;
        auto& column=columns[z*n.x+x];
        const bool overlap=std::any_of(column.begin(),column.end(),[&](const auto& v){return v.bottom<patch.top&&v.top>patch.bottom;});
        if(!overlap)column.push_back({x,z,patch.bottom,patch.top});
    }
    struct Point {MeshVertex vertex;float coverage;};
    auto point=[&](float x,float z,float height,float lower) {
        const auto s=field.At(x,z,height,lower);MeshVertex v;
        v.position=field.origin+field.rotation*glm::vec3(x,s.height,z);
        v.normal=field.rotation*glm::normalize(glm::vec3(-s.gradient.x,1,-s.gradient.y));
        v.uv={x,z};v.uv1=s.velocity;
        // Private water-vertex payload: stable layer reference for GPU height
        // reconstruction. No surface mass or collision data is stored here.
        v.tangent.w=height;v.tangent.y=lower;
        return Point{v,s.coverage};
    };
    mesh.vertices.reserve(sources.size()*subdivisions*subdivisions*6);
    for(const auto& column:columns)for(const auto &patch:column) {
        const int x=patch.x,z=patch.z;
        const float reference=patch.top,lower=patch.bottom;
        // Adjacent quads share their corner samples. Retain one bounded patch
        // grid rather than evaluating the same cubic stencil four times and
        // allocating two clipping vectors for every little quad.
        std::array<Point,49> samples;
        const unsigned stride=subdivisions+1;
        for(unsigned b=0;b<=subdivisions;++b)for(unsigned a=0;a<=subdivisions;++a)
            samples[b*stride+a]=point((x+float(a)/subdivisions)*h,(z+float(b)/subdivisions)*h,reference,lower);
        for(unsigned b=0;b<subdivisions;++b)for(unsigned a=0;a<subdivisions;++a) {
            const std::array<Point,4> polygon={samples[b*stride+a],samples[(b+1)*stride+a],samples[(b+1)*stride+a+1],samples[b*stride+a+1]};
            std::array<Point,8> clipped;unsigned count=0;
            for(unsigned i=0;i<polygon.size();++i) {
                const auto &p=polygon[i],&q=polygon[(i+1)%polygon.size()];
                const bool inside=p.coverage>=.35f,next=q.coverage>=.35f;
                if(inside)clipped[count++]=p;
                if(inside!=next) {
                    const float t=(.35f-p.coverage)/(q.coverage-p.coverage);
                    Point r;r.coverage=.35f;r.vertex.position=glm::mix(p.vertex.position,q.vertex.position,t);
                    r.vertex.normal=glm::normalize(glm::mix(p.vertex.normal,q.vertex.normal,t));
                    r.vertex.uv=glm::mix(p.vertex.uv,q.vertex.uv,t);r.vertex.uv1=glm::mix(p.vertex.uv1,q.vertex.uv1,t);
                    r.vertex.tangent.w=reference;r.vertex.tangent.y=lower;clipped[count++]=r;
                }
            }
            for(unsigned i=1;i+1<count;++i)
                for(const auto *v:{&clipped[0].vertex,&clipped[i].vertex,&clipped[i+1].vertex})mesh.vertices.push_back(*v);
            // Close only exposed outlines, including the finite region edge.
            // Opaque terrain naturally hides these faces; no tiled voxel walls.
            for(unsigned i=0;i<count;++i){
                const auto &p=clipped[i],&q=clipped[(i+1)%count];
                if(glm::length(q.vertex.uv-p.vertex.uv)<1e-5f)continue;
                auto edge=[&](int axis,float value){return std::abs(p.vertex.uv[axis]-value)<1e-5f&&std::abs(q.vertex.uv[axis]-value)<1e-5f;};
                if(!((p.coverage<.351f&&q.coverage<.351f)||edge(0,0)||edge(1,0)||edge(0,n.x*h)||edge(1,n.z*h)))continue;
                auto pTop=p.vertex,qTop=q.vertex,pBottom=p.vertex,qBottom=q.vertex;
                const auto outward=glm::normalize(glm::cross(qTop.position-pTop.position,field.rotation*glm::vec3(0,1,0)));
                pBottom.position=field.origin+field.rotation*glm::vec3(pTop.uv.x,field.At(pTop.uv.x,pTop.uv.y,reference,lower).bottom,pTop.uv.y);
                qBottom.position=field.origin+field.rotation*glm::vec3(qTop.uv.x,field.At(qTop.uv.x,qTop.uv.y,reference,lower).bottom,qTop.uv.y);
                for(auto* v:{&pTop,&qTop,&pBottom,&qBottom}){v->normal=outward;v->tangent.z=1;}
                pBottom.tangent.x=qBottom.tangent.x=0;
                for(const auto* v:{&pBottom,&qBottom,&qTop,&pBottom,&qTop,&pTop})mesh.vertices.push_back(*v);
            }
        }
    }
    return mesh;
}
MeshData BuildPoolSurface(const Grid &grid){return BuildPoolSurface(BuildSurfaceField(grid));}
} // namespace JudasWater
