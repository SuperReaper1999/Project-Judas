#include "WaterFragmentFlux.h"
#include "WaterCellAudit.h"
#include <algorithm>
#include <cmath>
#include <numeric>
namespace judas::volume_grid {
namespace {using Real=CutCompensated;bool Fail(std::string& e,const char* s){e=s;return false;}}
bool BalanceWaterFragmentFlux(const WaterMaterialState& state,const WaterFragmentGeometry& geometry,
    const WaterGasProjectionResult& pressure,const WaterGasSettings& settings,
    const std::vector<std::pair<std::size_t,Real>>& fixed,WaterFragmentFlux& output,std::string& error){
    const auto nw=geometry.liquid.regions.size(),ng=geometry.air.volume.size(),n=nw+ng;WaterFragmentFlux next;
    if(pressure.liquidFlux.size()!=nw||pressure.gasVolume.size()!=ng)return Fail(error,"fragment flux epoch mismatch");
    next.liquidFlux=pressure.liquidFlux;next.pressureVelocity.resize(state.fragments.size());
    std::vector<Real> first(geometry.liquid.sharedFaces);std::vector<std::size_t> seen(first.size(),SIZE_MAX);
    std::vector<std::vector<std::pair<std::size_t,int>>> patchEdge(nw);
    for(std::size_t r=0;r<nw;++r){auto& region=geometry.liquid.regions[r];patchEdge[r].resize(region.response.free.size(),{SIZE_MAX,0});
        for(std::size_t a=0;a<region.response.free.size();++a){auto owner=region.owners[region.response.free[a]];if(owner.shared==SIZE_MAX)continue;Real amount=Real(settings.dt)*pressure.liquidFlux[r][a];
            if(seen[owner.shared]==SIZE_MAX){seen[owner.shared]=r;first[owner.shared]=amount;}
            else{auto id=next.edges.size();next.edges.push_back({seen[owner.shared],r,owner.shared,SIZE_MAX,(first[owner.shared]-amount)*Real(.5),region.patches[region.response.free[a]].area});
                for(std::size_t b=0;b<geometry.liquid.regions[seen[owner.shared]].response.free.size();++b)if(geometry.liquid.regions[seen[owner.shared]].owners[geometry.liquid.regions[seen[owner.shared]].response.free[b]].shared==owner.shared)patchEdge[seen[owner.shared]][b]={id,1};
                patchEdge[r][a]={id,-1};}
        }
    }
    auto interfaceArea=[](const WaterFragmentInterface& face){Real area(1.);for(int d=0;d<3;++d)if(d!=face.axis)area=area*(face.hi[d]-face.lo[d]);return area;};
    std::vector<std::size_t> interfaceEdge(geometry.interfaces.size());
    for(std::size_t f=0;f<geometry.interfaces.size();++f){auto face=geometry.interfaces[f];auto r=geometry.liquid.cellWater[face.water];auto& region=geometry.liquid.regions[r];auto found=std::find(region.response.free.begin(),region.response.free.end(),face.patch);if(found==region.response.free.end())return Fail(error,"interface lost its liquid flux patch");auto a=std::size_t(found-region.response.free.begin());
        auto id=patchEdge[r][a].first;
        if(id==SIZE_MAX){id=next.edges.size();next.edges.push_back({r,nw+geometry.air.cellNode[face.gas],SIZE_MAX,f,Real(settings.dt)*pressure.liquidFlux[r][a],region.patches[face.patch].area});patchEdge[r][a]={id,1};}
        interfaceEdge[f]=id;
    }
    std::vector<bool> locked(next.edges.size());
    for(auto value:fixed){if(value.first>=interfaceEdge.size())return Fail(error,"fixed geometric event has no interface");auto id=interfaceEdge[value.first];auto face=geometry.interfaces[value.first];auto r=geometry.liquid.cellWater[face.water];Real amount=value.second*geometry.liquid.regions[r].patches[face.patch].Area()/interfaceArea(face);
        if(locked[id]&&std::abs((next.edges[id].amount-amount).Value())>1e-12*std::max(std::abs(amount.Value()),1e-30))return Fail(error,"simultaneous interface events need distinct normal velocities");
        next.edges[id].amount=amount;locked[id]=true;
    }
    std::vector<std::size_t> parent(n);std::iota(parent.begin(),parent.end(),0);
    auto root=[&](std::size_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
    std::vector<std::size_t> order(next.edges.size());std::iota(order.begin(),order.end(),0);
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return next.edges[a].area>next.edges[b].area;});
    std::vector<std::vector<std::pair<std::size_t,std::size_t>>> tree(n);
    for(auto id:order)if(!locked[id]){auto e=next.edges[id];auto a=root(e.from),b=root(e.to);if(a!=b){parent[b]=a;tree[e.from].push_back({e.to,id});tree[e.to].push_back({e.from,id});}}
    std::vector<Real> target(n),divergence(n);std::vector<bool> active(n);
    for(auto e:next.edges){active[e.from]=active[e.to]=true;divergence[e.from]=divergence[e.from]+e.amount;divergence[e.to]=divergence[e.to]-e.amount;}
    for(std::size_t g=0;g<ng;++g)if(active[nw+g]){
        Real geometric=geometry.air.volume[g]+divergence[nw+g];
        target[nw+g]=geometric.Value()>0.?divergence[nw+g]:pressure.gasVolume[g]-geometry.air.volume[g];
    }
    // Each FREE-edge component has one dependent geometric volume. Select the
    // largest real air capacity; never spend a tiny pocket as the roundoff sink.
    std::vector<std::size_t> roots(n,SIZE_MAX);std::vector<Real> sum(n);
    for(std::size_t i=0;i<n;++i)if(active[i]){auto component=root(i);sum[component]=sum[component]+target[i]-divergence[i];if(roots[component]==SIZE_MAX||(i>=nw&&(roots[component]<nw||geometry.air.volume[i-nw].Value()>geometry.air.volume[roots[component]-nw].Value())))roots[component]=i;}
    std::vector<std::size_t> previous(n,SIZE_MAX),edgeTo(n,SIZE_MAX);std::vector<Real> residual(n);
    for(std::size_t c=0;c<n;++c)if(roots[c]!=SIZE_MAX){auto top=roots[c];if(top>=nw){Real volume=geometry.air.volume[top-nw]+target[top]-sum[c];if(!(volume.Value()>0.))return Fail(error,"conservative flux recovery would erase retained air");target[top]=target[top]-sum[c];}else if(std::abs(sum[c].Value())>settings.massTolerance/settings.referenceDensity)return Fail(error,"sealed water event has incompatible integrated flux");
        std::vector<std::size_t> traversal{top};previous[top]=top;
        for(std::size_t j=0;j<traversal.size();++j)for(auto link:tree[traversal[j]])if(previous[link.first]==SIZE_MAX){previous[link.first]=traversal[j];edgeTo[link.first]=link.second;traversal.push_back(link.first);}
        for(auto i:traversal)residual[i]=target[i]-divergence[i];
        for(std::size_t j=traversal.size();j-->1;){auto i=traversal[j],p=previous[i],edge=edgeTo[i];auto& e=next.edges[edge];e.amount=e.amount+Real(e.from==i?1.:-1.)*residual[i];residual[p]=residual[p]+residual[i];}
    }
    for(std::size_t r=0;r<nw;++r)for(std::size_t a=0;a<patchEdge[r].size();++a){auto e=patchEdge[r][a];if(e.first==SIZE_MAX)return Fail(error,"liquid patch has no conservative carrier");next.liquidFlux[r][a]=Real(e.second)*next.edges[e.first].amount/Real(settings.dt);}
    std::vector<bool> fixedPiece(geometry.interfaces.size());std::vector<Real> fixedAmount(geometry.interfaces.size());
    for(auto value:fixed){fixedPiece[value.first]=true;fixedAmount[value.first]=value.second;}
    std::vector<std::size_t> largest(next.edges.size(),SIZE_MAX);std::vector<Real> areas(next.edges.size()),remaining(next.edges.size());
    for(std::size_t i=0;i<next.edges.size();++i)remaining[i]=next.edges[i].amount;
    next.sweeps.resize(geometry.interfaces.size());
    for(std::size_t i=0;i<geometry.interfaces.size();++i){auto id=interfaceEdge[i];next.sweeps[i].face=geometry.interfaces[i];
        if(fixedPiece[i]){next.sweeps[i].volume=fixedAmount[i];remaining[id]=remaining[id]-fixedAmount[i];continue;}
        Real area=interfaceArea(geometry.interfaces[i]);areas[id]=areas[id]+area;if(largest[id]==SIZE_MAX||area.Value()>interfaceArea(geometry.interfaces[largest[id]]).Value())largest[id]=i;
    }
    auto totalUnfixed=remaining;
    for(std::size_t i=0;i<geometry.interfaces.size();++i)if(!fixedPiece[i]){auto id=interfaceEdge[i];if(i!=largest[id]){next.sweeps[i].volume=totalUnfixed[id]*interfaceArea(geometry.interfaces[i])/areas[id];remaining[id]=remaining[id]-next.sweeps[i].volume;}}
    for(std::size_t id=0;id<next.edges.size();++id)if(next.edges[id].interface!=SIZE_MAX){if(largest[id]!=SIZE_MAX)next.sweeps[largest[id]].volume=remaining[id];else if(std::abs(remaining[id].Value())>1e-28*std::max(std::abs(next.edges[id].amount.Value()),1e-30))return Fail(error,"completed interface pieces do not exhaust their grouped physical flux");}
    for(std::size_t r=0;r<nw;++r){const auto& region=geometry.liquid.regions[r];Real offset=pressure.liquidReferencePressure[r];if(!region.response.free.empty()){auto f=region.response.free[region.response.reference];for(int a=0;a<3;++a)offset=offset-Real(geometry.liquid.density*geometry.liquid.gravity[a])*region.patches[f].Offset(a);}WaterCellAudit audit;
        if(!AuditWaterCell(geometry.waterVolume[r].Value(),geometry.liquid.density,settings.dt,geometry.liquid.gravity,region.patches,region.response,next.liquidFlux[r],pressure.liquidHeadDifference[r],audit,error,true,offset))return false;
        next.maximumMomentumResidual=std::max(next.maximumMomentumResidual,audit.maximumMomentumResidual);next.pressureVelocity[region.sourceCell]=audit.meanVelocity;
    }
    for(std::size_t i=0;i<state.fragments.size();++i)if(state.fragments[i].water)next.pressureVelocity[i]=next.pressureVelocity[geometry.carrier[i]];
    output=std::move(next);error.clear();return true;
}
}
