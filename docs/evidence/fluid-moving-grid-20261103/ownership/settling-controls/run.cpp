#include "OwnedEvolvingVolume.h"
#include "GridWaterQuery.h"
#include "GridCellPhase.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
using namespace judas::volume_grid;
int main(int argc,char** argv){
    const std::string_view mode=argc==2?argv[1]:"original";
    const int divisions=mode=="half-step"?2:1;
    const double gravity=mode=="zero-g"?0.:-9.81;
    if(mode!="flat" && mode!="original" && mode!="half-step" && mode!="zero-g")return 3;
    // Identical particle parcel sizes, omission mask, box, and warmup clock
    // from half_025. Grid chart starts at world (-1,0,-1).
    const GridLayout g{{8,12,8},.25};
    const GridVec half{double(.3f),double(.3f),double(.3f)}, center{1,double(.7f),1};
    BoxMotion box{1};
    for(int a=0;a<3;++a){box.lo[a]=center[a]-half[a];box.hi[a]=center[a]+half[a];}
    MovingVolumeState s;
    for(int a=0;a<3;++a)s.velocity[a].resize(g.FaceCount(a));
    s.water.resize(g.CellCount());
    constexpr double omission=.25 * double(.36f);
    for(int j=0;j<10;++j)for(int k=0;k<8;++k)for(int i=0;i<8;++i){
        const GridVec at{(i+.5)*.25,(j+.5)*.25,(k+.5)*.25};
        bool removed=true;
        for(int a=0;a<3;++a)removed=removed && std::abs(at[a]-center[a]) < half[a]+omission;
        if(!removed)s.water[g.Cell(i,j,k)]=.25*.25*.25;
    }
    const double mass=std::accumulate(s.water.begin(),s.water.end(),0.);
    const double solidVolume=(box.hi[0]-box.lo[0])*(box.hi[1]-box.lo[1])*(box.hi[2]-box.lo[2]);
    const double height=(mass+solidVolume)/4;
    const double equilibrium=9.81*(18.-solidVolume*center[1]+999*(2*height*height-solidVolume*center[1]));
    if(mode=="flat"){
        GridPhaseGeometry flat;
        MovingGridGeometry geometry;
        std::string error;
        if(!BuildGridPhaseGeometry(g,{box},1,height,flat,error)||
           !BuildMovingGridGeometry(g,{box},geometry,error)){std::cerr<<error<<'\n';return 1;}
        s.water=std::move(flat.water);
        for(std::size_t c=0;c<s.water.size();++c)s.water[c]=std::min(s.water[c],geometry.startCapacity[c]);
    }
    std::cout<<std::setprecision(12)<<"mode "<<mode<<" equilibrium_height "<<height<<" potential "<<equilibrium<<'\n';
    std::cout<<std::setprecision(12)<<"authored water "<<mass<<" m3, parcels "<<mass/(.25*.25*.25)<<'\n';
    HalfDualVolumeState owned;
    for(auto& v:owned.velocity)v.resize(2*g.CellCount());
    std::string error;
    const auto energy=[&](const MovingVolumeState& material,const HalfDualVolumeState& momentum){
        GridCellPhase phase;
        GridWaterSample wet;
        if(!ReconstructGridCellPhase(g,{box},1,material.water,phase,error) ||
           !QueryGridWaterVolume(g,{box},1,phase.waterPlane,{0,0,0},{2,3,2},wet,error))
            throw std::runtime_error(error);
        double kinetic=0;
        for(size_t c=0;c<g.CellCount();++c){
            const double m=.5*(phase.capacity[c]+999*material.water[c]);
            for(int a=0;a<3;++a)for(int half=0;half<2;++half){
                const double u=momentum.velocity[a][2*c+half];
                kinetic+=.5*m*u*u;
            }
        }
        const double solidVolume=(box.hi[0]-box.lo[0])*(box.hi[1]-box.lo[1])*(box.hi[2]-box.lo[2]);
        const double potential=9.81*(18.-solidVolume*(box.lo[1]+box.hi[1])*.5+999*wet.centroid[1]*wet.wetVolume);
        return std::array<double,3>{kinetic,potential,kinetic+potential};
    };
    const auto initialEnergy=energy(s,owned);
    std::cout<<"initial kinetic,potential,total "<<initialEnergy[0]<<','<<initialEnergy[1]<<','<<initialEnergy[2]<<'\n';
    for(int i=1;i<=120;++i){
        int accepted=0,rejected=0;
        const auto begin=std::chrono::steady_clock::now();
        for(int split=0;split<divisions;++split){
            OwnedEvolvingVolumeInterval next;
            const double dt=(i==1?1./60:1./30)/divisions;
            if(!AdvanceOwnedEvolvingVolumeInterval(g,{box},s,owned,dt,1,gravity,7,next,error)){
                std::cout<<"REJECT at "<<i<<'.'<<split<<" reason "<<error<<'\n';
                return 2;
            }
            accepted+=next.substeps;
            rejected+=next.rejectedAttempts;
            s=std::move(next.final.phase.fluid.state);
            owned=std::move(next.final.halfVelocity);
        }
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        double mean=0, peak=0, com=0,rms=0,vertical=0,surfaceSpeed=0,surfaceMass=0,deepSpeed=0,deepMass=0;
        for(int z=0;z<g.cells[2];++z)for(int y=0;y<g.cells[1];++y)for(int x=0;x<g.cells[0];++x){
            const auto cell=g.Cell(x,y,z);
            GridVec u;
            for(int a=0;a<3;++a){
                int coord[3]={x,y,z};
                const double low=s.velocity[a][g.Face(a,x,y,z)];
                ++coord[a];
                u[a]=.5*(low+s.velocity[a][g.Face(a,coord[0],coord[1],coord[2])]);
            }
            const double speed=std::sqrt(std::inner_product(u.begin(),u.end(),u.begin(),0.));
            mean+=speed*s.water[cell];peak=std::max(peak,speed);
            rms+=speed*speed*s.water[cell];
            vertical+=std::abs(u[1])*s.water[cell];
            if(y<=4){deepSpeed+=speed*s.water[cell];deepMass+=s.water[cell];}
            else {surfaceSpeed+=speed*s.water[cell];surfaceMass+=s.water[cell];}
            com+=(y+.5)*.25*s.water[cell];
        }
        const auto measured=energy(s,owned);
        std::cout<<i<<','<<mean/mass<<','<<peak<<','<<com/mass<<','<<accepted<<','<<rejected<<','<<ms<<','
                 <<std::accumulate(s.water.begin(),s.water.end(),0.)-mass<<','<<measured[0]<<','
                 <<measured[1]-equilibrium<<','<<measured[2]-equilibrium<<','<<std::sqrt(rms/mass)<<','
                 <<vertical/mass<<','<<deepSpeed/deepMass<<','<<surfaceSpeed/surfaceMass<<'\n';
    }
}
