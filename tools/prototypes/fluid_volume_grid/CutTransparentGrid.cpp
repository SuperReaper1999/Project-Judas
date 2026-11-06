#include "CutTransparentGrid.h"
#include "CutCondensedPressure.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Fail(std::string& e,const char* s){e=s;return false;}
}
bool CoarsenCutTransparentGrid(const CutSpatialField& f,CutSpatialField& output,
    CutTransparentGridReport& report,std::string& error,double fraction){
    const auto n=f.parent.size();
    if(&f==&output||!n||f.chart.regions.size()!=n||f.water.size()!=n||f.gas.size()!=n||f.gasMass.size()!=n||
        f.geometry.startCapacity.size()!=n||f.geometry.endCapacity!=f.geometry.startCapacity||f.geometry.motion||
        !(fraction>0.&&fraction<.5)||!f.chart.layout.CheckedCellCount())return Fail(error,"invalid transparent-grid initial field");
    if(f.flux.size()!=f.geometry.faces.size())return Fail(error,"transparent-grid flow dimensions mismatch");
    for(std::size_t i=0;i<n;++i){
        if(!(f.geometry.startCapacity[i]>0.&&std::isfinite(f.geometry.startCapacity[i])))return Fail(error,"invalid transparent-grid capacity");
        for(int a=0;a<3;++a)if(!(f.chart.regions[i].lo[a]>=0.&&f.chart.regions[i].hi[a]>f.chart.regions[i].lo[a]&&f.chart.regions[i].hi[a]<=f.chart.layout.cells[a]*f.chart.layout.h))return Fail(error,"invalid transparent-grid bounds");
    }
    for(double q:f.flux)if(q!=0.)return Fail(error,"transparent-grid projection cannot discard momentum");
    std::array<std::vector<double>,3> cuts;std::array<std::size_t,3> size;
    for(int a=0;a<3;++a){auto& c=cuts[a];c={0.,f.chart.layout.cells[a]*f.chart.layout.h};for(auto r:f.chart.regions){c.push_back(r.lo[a]);c.push_back(r.hi[a]);}std::sort(c.begin(),c.end());c.erase(std::unique(c.begin(),c.end()),c.end());size[a]=c.size()-1;}
    std::size_t count=1;for(auto s:size){if(s>262144/count)return Fail(error,"transparent-grid topology budget exceeded");count*=s;}
    auto index=[&](const std::array<std::size_t,3>& c){return (c[2]*size[1]+c[1])*size[0]+c[0];};
    std::vector<std::size_t> owners(count,SIZE_MAX);std::vector<std::array<std::size_t,3>> coordinates(n);
    for(std::size_t i=0;i<n;++i){const auto& r=f.chart.regions[i];auto& c=coordinates[i];double capacity=1.;
        for(int a=0;a<3;++a){c[a]=std::lower_bound(cuts[a].begin(),cuts[a].end(),r.lo[a])-cuts[a].begin();if(c[a]>=size[a]||cuts[a][c[a]+1]!=r.hi[a])return Fail(error,"transparent-grid input is not conforming");capacity*=r.hi[a]-r.lo[a];}
        if(std::abs(capacity-f.geometry.startCapacity[i])>128.*std::numeric_limits<double>::epsilon()*capacity)return Fail(error,"transparent-grid cell has an unresolved solid cut");
        auto j=index(c);if(owners[j]!=SIZE_MAX)return Fail(error,"duplicate transparent-grid cell");owners[j]=i;
    }
    // Rectangular free cells alone do not prove that their shared face is
    // open: reject hidden barriers and partial faces, rather than invent flow.
    std::vector<std::array<bool,3>> connected(n);
    for(auto face:f.geometry.faces){
        if(face.left>=n||face.right>=n||face.axis<0||face.axis>2)return Fail(error,"invalid transparent-grid face");
        auto a=coordinates[face.left],b=coordinates[face.right];auto expected=a;++expected[face.axis];
        const auto& r=f.chart.regions[face.left];double area=(r.hi[(face.axis+1)%3]-r.lo[(face.axis+1)%3])*(r.hi[(face.axis+2)%3]-r.lo[(face.axis+2)%3]);
        if(b!=expected||face.meanArea!=area||face.startArea!=area||face.endArea!=area||connected[face.left][face.axis])return Fail(error,"transparent-grid requires complete open internal faces");
        connected[face.left][face.axis]=true;
    }
    for(std::size_t i=0;i<n;++i)for(int a=0;a<3;++a){auto c=coordinates[i];
        if(++c[a]<size[a]&&owners[index(c)]!=SIZE_MAX&&!connected[i][a])return Fail(error,"transparent-grid cannot erase an internal barrier");
    }
    std::vector<std::array<int,6>> portKinds(n);
    for(auto b:f.boundaries){if(b.fragment>=n||b.axis<0||b.axis>2||(b.sign!=1&&b.sign!=-1)||!b.gasOnly||b.volume!=0.)return Fail(error,"unsupported transparent-grid port");const auto& r=f.chart.regions[b.fragment];if((b.sign>0?r.hi[b.axis]:r.lo[b.axis])!=(b.sign>0?f.chart.layout.cells[b.axis]*f.chart.layout.h:0.))return Fail(error,"transparent-grid port is not exterior");auto& kind=portKinds[b.fragment][2*b.axis+(b.sign>0)];if(kind)return Fail(error,"duplicate transparent-grid port");kind=1;}
    std::array<std::vector<bool>,3> keep;
    for(int a=0;a<3;++a){keep[a].assign(size[a]+1,false);keep[a][0]=keep[a][size[a]]=true;}
    for(std::size_t z=0;z<size[2];++z)for(std::size_t y=0;y<size[1];++y)for(std::size_t x=0;x<size[0];++x){std::array<std::size_t,3> c{x,y,z};auto left=owners[index(c)];
        for(int a=0;a<3;++a){if(c[a]+1==size[a])continue;auto d=c;++d[a];auto right=owners[index(d)];
            bool boundary=(left==SIZE_MAX)!=(right==SIZE_MAX);
            if(left!=SIZE_MAX&&right!=SIZE_MAX)for(int b=0;b<3;++b)if(b!=a)for(int side=0;side<2;++side)
                if(c[b]==(side?size[b]-1:0)&&portKinds[left][2*b+side]!=portKinds[right][2*b+side])boundary=true;
            if(boundary)keep[a][c[a]+1]=true;
        }
    }
    CutTransparentGridReport stats;stats.inputCells=n;stats.minimumCapacityBefore=*std::min_element(f.geometry.startCapacity.begin(),f.geometry.startCapacity.end());
    std::array<std::vector<std::size_t>,3> retained,map;
    for(int a=0;a<3;++a){auto& r=retained[a];for(std::size_t i=0;i<=size[a];++i)r.push_back(i);
        bool removed=true;while(removed){removed=false;
            for(std::size_t j=1;j+1<r.size();++j)if(!keep[a][r[j]]){
                double before=cuts[a][r[j]]-cuts[a][r[j-1]],after=cuts[a][r[j+1]]-cuts[a][r[j]];
                if(std::min(before,after)<fraction*f.chart.layout.h&&before+after<=(1.+fraction)*f.chart.layout.h){r.erase(r.begin()+j);++stats.removedPlanes;removed=true;break;}
            }
        }
        map[a].resize(size[a]);std::size_t j=0;for(std::size_t i=0;i<size[a];++i){while(j+1<r.size()-1&&i>=r[j+1])++j;map[a][i]=j;}
    }
    std::array<std::size_t,3> coarse{retained[0].size()-1,retained[1].size()-1,retained[2].size()-1};
    auto cell=[&](const std::array<std::size_t,3>& c){return (c[2]*coarse[1]+c[1])*coarse[0]+c[0];};
    struct Quantity{Real w,g,m,c;std::size_t parts=0;std::array<int,6> ports{};std::array<Real,6> portArea;};
    std::vector<Quantity> sum(coarse[0]*coarse[1]*coarse[2]);
    std::array<Real,3> oldW,oldM,newW,newM;Real totalW,totalM;
    for(std::size_t i=0;i<n;++i){auto c=coordinates[i];for(int a=0;a<3;++a)c[a]=map[a][c[a]];auto& q=sum[cell(c)];
        if(!(f.water[i]>=0.&&std::isfinite(f.water[i])&&f.gas[i]>=0.&&std::isfinite(f.gas[i])&&f.gasMass[i]>=0.&&std::isfinite(f.gasMass[i])))return Fail(error,"invalid transparent-grid material");
        q.w=q.w+Real(f.water[i]);q.g=q.g+Real(f.gas[i]);q.m=q.m+Real(f.gasMass[i]);q.c=q.c+Real(f.geometry.startCapacity[i]);++q.parts;
        totalW=totalW+Real(f.water[i]);totalM=totalM+Real(f.gasMass[i]);
        const auto& r=f.chart.regions[i];for(int a=0;a<3;++a){Real middle=Real(r.lo[a])+Real(.5)*Real(r.hi[a]-r.lo[a]);oldW[a]=oldW[a]+Real(f.water[i])*middle;oldM[a]=oldM[a]+Real(f.gasMass[i])*middle;
            for(int side=0;side<2;++side)if(portKinds[i][2*a+side]){q.ports[2*a+side]=1;q.portArea[2*a+side]=q.portArea[2*a+side]+Real((r.hi[(a+1)%3]-r.lo[(a+1)%3])*(r.hi[(a+2)%3]-r.lo[(a+2)%3]));}}
    }
    CutSpatialField next;next.chart.layout=f.chart.layout;std::vector<std::size_t> ids(sum.size(),SIZE_MAX);Real finalW,finalM;
    for(std::size_t z=0;z<coarse[2];++z)for(std::size_t y=0;y<coarse[1];++y)for(std::size_t x=0;x<coarse[0];++x){std::array<std::size_t,3> c{x,y,z};auto j=cell(c);auto& q=sum[j];if(!q.parts)continue;
        CutRegion r;double capacity=1.;for(int a=0;a<3;++a){r.lo[a]=cuts[a][retained[a][c[a]]];r.hi[a]=cuts[a][retained[a][c[a]+1]];capacity*=r.hi[a]-r.lo[a];}
        if(std::abs((q.c-Real(capacity)).Value())>256.*std::numeric_limits<double>::epsilon()*capacity)return Fail(error,"transparent plane would bridge or fill a solid");
        auto id=next.parent.size();ids[j]=id;
        std::array<int,3> parent;for(int a=0;a<3;++a)parent[a]=std::clamp(int((r.lo[a]+.5*(r.hi[a]-r.lo[a]))/f.chart.layout.h),0,f.chart.layout.cells[a]-1);
        r.parent=f.chart.layout.Cell(parent[0],parent[1],parent[2]);next.parent.push_back(id);next.chart.regions.push_back(r);next.geometry.startCapacity.push_back(capacity);next.geometry.endCapacity.push_back(capacity);
        next.water.push_back(q.w.Value());next.gas.push_back(q.g.Value());next.gasMass.push_back(q.m.Value());finalW=finalW+Real(next.water.back());finalM=finalM+Real(next.gasMass.back());
        for(int a=0;a<3;++a){Real middle=Real(r.lo[a])+Real(.5)*Real(r.hi[a]-r.lo[a]);newW[a]=newW[a]+Real(next.water.back())*middle;newM[a]=newM[a]+Real(next.gasMass.back())*middle;
            for(int side=0;side<2;++side)if(q.ports[2*a+side]){double area=(r.hi[(a+1)%3]-r.lo[(a+1)%3])*(r.hi[(a+2)%3]-r.lo[(a+2)%3]);
                if(std::abs((q.portArea[2*a+side]-Real(area)).Value())>128.*std::numeric_limits<double>::epsilon()*area)return Fail(error,"transparent-grid merge changes exterior port coverage");
                next.boundaries.push_back({id,a,side?1:-1,0.,0.,true});}}
    }
    for(std::size_t z=0;z<coarse[2];++z)for(std::size_t y=0;y<coarse[1];++y)for(std::size_t x=0;x<coarse[0];++x){std::array<std::size_t,3> c{x,y,z};auto l=ids[cell(c)];if(l==SIZE_MAX)continue;
        for(int a=0;a<3;++a){auto d=c;if(++d[a]>=coarse[a])continue;auto right=ids[cell(d)];if(right==SIZE_MAX)continue;const auto& r=next.chart.regions[l];GridVec lo=r.lo,hi=r.hi;lo[a]=hi[a];double area=(hi[(a+1)%3]-lo[(a+1)%3])*(hi[(a+2)%3]-lo[(a+2)%3]);double length=.5*(r.hi[a]-r.lo[a]+next.chart.regions[right].hi[a]-next.chart.regions[right].lo[a]);
            next.geometry.faces.push_back({l,right,a,lo,hi,area,area,area,0.,length});next.flux.push_back(0.);next.mobility.push_back(area/length);next.originalFace.push_back(SIZE_MAX);}}
    stats.outputCells=next.parent.size();stats.waterError=(finalW-totalW).Value();stats.gasMassError=(finalM-totalM).Value();
    if(std::abs(stats.waterError)>1e-14||std::abs(stats.gasMassError)>1.2e-14)return Fail(error,"transparent-grid original quantity ledger failed");
    stats.minimumCapacityAfter=*std::min_element(next.geometry.startCapacity.begin(),next.geometry.startCapacity.end());for(int a=0;a<3;++a){stats.waterFirstMomentChange[a]=(newW[a]-oldW[a]).Value();stats.gasFirstMomentChange[a]=(newM[a]-oldM[a]).Value();}
    next.chart.faces=next.geometry.faces;output=std::move(next);report=stats;error.clear();return true;
}
}
