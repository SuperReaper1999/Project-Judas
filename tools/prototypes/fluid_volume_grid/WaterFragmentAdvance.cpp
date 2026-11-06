#include "WaterFragmentAdvance.h"
#include "CutDualLinear.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;using Clock=std::chrono::steady_clock;
double Milliseconds(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
bool Fail(std::string& e,const char* s){e=s;return false;}
bool Momentum(WaterMaterialState& after,
    const WaterFragmentGeometry& geometry,const WaterFragmentFlux& flux,
    double density,std::string& error){
    const auto n=geometry.liquid.regions.size();if(!n)return true;
    std::vector<Real> volume(n),diagonal(n);std::vector<CutDirectedEdge> edges;
    for(auto& f:after.fragments)if(f.water){auto r=geometry.liquid.cellWater[f.carrier];volume[r]=volume[r]+MaterialVolume(f);}
    for(std::size_t i=0;i<n;++i)diagonal[i]=Real(density)*volume[i];
    for(auto e:flux.edges)if(e.shared!=SIZE_MAX){auto from=e.from,to=e.to;Real amount=e.amount;if(amount.Value()<0.){std::swap(from,to);amount=Real()-amount;}Real mass=Real(density)*amount;if(mass.Value()>0.){edges.push_back({from,to,mass});diagonal[from]=diagonal[from]+mass;}}
    CutDualLinear transport;if(!transport.Build(diagonal,edges,error))return false;
    std::vector<GridVec> velocity(n);
    for(int axis=0;axis<3;++axis){std::vector<Real> rhs(n),solution;Real start,end;
        for(std::size_t r=0;r<n;++r){auto id=geometry.liquid.regions[r].sourceCell;rhs[r]=Real(density)*geometry.waterVolume[r]*Real(flux.pressureVelocity[id][axis]);start=start+rhs[r];}
        if(!transport.Solve(rhs,solution,error))return false;
        for(std::size_t r=0;r<n;++r){velocity[r][axis]=solution[r].Value();end=end+Real(density)*volume[r]*solution[r];}
        if(std::abs((end-start).Value())>1e-10*std::max(std::abs(start.Value()),1.))return Fail(error,"fragment momentum transport did not conserve its physical impulse");
    }
    for(auto& f:after.fragments)if(f.water)f.velocity=velocity[geometry.liquid.cellWater[f.carrier]];
    return true;
}
}
bool AdvanceWaterFragments(const CutSpatialField& field,const WaterMaterialState& source,
    double density,const GridVec& gravity,const WaterGasSettings& requested,
    WaterMaterialState& output,WaterFragmentAdvanceStats& stats,std::string& error,std::size_t budget){
    if(!(requested.dt>0.&&std::isfinite(requested.dt))||!budget)return Fail(error,"invalid fragment interval or explicit work budget");
    auto started=Clock::now();WaterMaterialState current=source;WaterFragmentAdvanceStats work;work.minimumInterval=requested.dt;Real elapsed;
    while((Real(requested.dt)-elapsed).Value()>0.){
        if(work.intervals+work.rejected>=budget){work.totalMs=Milliseconds(started);stats=work;return Fail(error,"fragment advance exhausted its reported interface work budget before completing the interval");}
        WaterGasSettings settings=requested;settings.dt=(Real(requested.dt)-elapsed).Value();auto t=Clock::now();WaterFragmentGeometry geometry;
        if(!PrepareWaterFragmentGeometry(field,current,density,gravity,settings,geometry,error)){work.geometryMs+=Milliseconds(t);work.totalMs=Milliseconds(started);stats=work;return false;}work.geometryMs+=Milliseconds(t);
        WaterGasProjectionResult seed;bool haveSeed=false,accepted=false;int numericalRefinements=0;
        bool locatingEvent=false;double lower=0.,upper=settings.dt;
        auto nextEventTrial=[&](){double midpoint=lower+(upper-lower)*.5;
            // Once bracketed to floating-point time resolution, retry the
            // upper endpoint. Exact geometric completion still needs to pass
            // the original momentum/work audit; time tolerance cannot admit it.
            if(upper-lower<=64.*std::numeric_limits<double>::epsilon()*upper)midpoint=upper;
            return midpoint;
        };
        while(!accepted){
            if(work.intervals+work.rejected>=budget){work.totalMs=Milliseconds(started);stats=work;return Fail(error,"fragment advance exhausted its reported crossing work budget");}
            WaterGasProjectionResult pressure;t=Clock::now();bool projected=ProjectWaterGasPressure(geometry.liquid,geometry.air,settings,pressure,error,haveSeed?&seed:nullptr);work.pressureMs+=Milliseconds(t);
            if(!projected){work.totalMs=Milliseconds(started);stats=work;return false;}seed=pressure;haveSeed=true;
            std::vector<std::pair<std::size_t,Real>> fixed;WaterFragmentFlux flux;WaterMaterialState next;bool represented=false;WaterInterfaceEventReport event;
            t=Clock::now();
            for(std::size_t correction=0;correction<=geometry.interfaces.size();++correction){
                if(!BalanceWaterFragmentFlux(current,geometry,pressure,settings,fixed,flux,error))break;
                WaterMaterialState moving=current;for(std::size_t i=0;i<moving.fragments.size();++i)if(moving.fragments[i].water)moving.fragments[i].velocity=flux.pressureVelocity[i];
                std::vector<Real> mass(current.fragments.size());for(std::size_t g=0;g<geometry.air.nodeCell.size();++g)mass[geometry.air.nodeCell[g]]=pressure.gasMass[g];
                represented=ApplyWaterInterfaceEvents(field,moving,flux.sweeps,mass,next,event,error,&geometry.carrier);
                if(represented)break;
                if(event.failedInterface==SIZE_MAX)break;
                auto id=event.failedInterface;Real amount=Real(flux.sweeps[id].volume.Value()>0.?1.:-1.)*event.availableVolume;bool replaced=false;
                for(auto& entry:fixed)if(entry.first==id){if((entry.second-amount).Value()==0.){event.failedInterface=SIZE_MAX;break;}entry.second=amount;replaced=true;}
                if(event.failedInterface==SIZE_MAX)break;
                if(!replaced)fixed.push_back({id,amount});
                ++work.geometricConstraints;
            }
            work.transportMs+=Milliseconds(t);
            if(!represented){
                if(numericalRefinements<3){settings.massTolerance*=.01;settings.relativeTolerance*=.01;++numericalRefinements;if(requested.trace)std::cout<<"FRAGMENT_REFINE_PRESSURE count="<<numericalRefinements<<" dt="<<settings.dt<<std::endl;continue;}
                if(fixed.empty()){work.totalMs=Milliseconds(started);stats=work;return false;}
                // Locate the finite-feature endpoint instead of publishing
                // a sequence of ever smaller prefixes approaching it forever.
                // An accepted endpoint consumes its exact geometric capacity;
                // conservative recovery must pass the unchanged physical audit.
                upper=settings.dt;locatingEvent=true;++work.rejected;
                double trial=nextEventTrial();
                if(!(trial>0.&&trial!=settings.dt)){work.totalMs=Milliseconds(started);stats=work;return Fail(error,"interface event exhausted representable time resolution without an admissible endpoint");}
                settings.dt=trial;
                if(requested.trace)std::cout<<"FRAGMENT_EVENT_BRACKET lower="<<lower<<" upper="<<upper<<" trial="<<trial<<" reason="<<error<<std::endl;
                continue;
            }
            if(locatingEvent&&fixed.empty()&&settings.dt<upper){
                lower=settings.dt;double trial=nextEventTrial();++work.rejected;
                if(!(trial>lower)){work.totalMs=Milliseconds(started);stats=work;return Fail(error,"interface endpoint could not be localized at representable time precision");}
                settings.dt=trial;
                if(requested.trace)std::cout<<"FRAGMENT_EVENT_BRACKET lower="<<lower<<" upper="<<upper<<" trial="<<trial<<std::endl;
                continue;
            }
            t=Clock::now();if(!Momentum(next,geometry,flux,density,error)){work.totalMs=Milliseconds(started);stats=work;return false;}work.transportMs+=Milliseconds(t);
            next.externalGasMass=current.externalGasMass+pressure.externalGasMass;
            Real water,gas=next.externalGasMass;for(auto& f:next.fragments){if(f.water)water=water+MaterialVolume(f);else gas=gas+f.gasMass;}
            if(std::abs((water-source.importedWater-source.waterImportError).Value())>1e-14||std::abs((gas-source.importedGasMass).Value())>requested.massTolerance){work.totalMs=Milliseconds(started);stats=work;return Fail(error,"whole fragment interval violated its cold material ledger");}
            current=std::move(next);elapsed=elapsed+Real(settings.dt);work.advancedTime=elapsed;work.minimumInterval=std::min(work.minimumInterval,settings.dt);++work.intervals;accepted=true;
            if(requested.trace)std::cout<<"FRAGMENT_ACCEPT time="<<elapsed.Value()<<" dt="<<settings.dt<<" pieces="<<current.fragments.size()<<" intervals="<<work.intervals<<" retries="<<work.rejected<<std::endl;
        }
    }
    work.totalMs=Milliseconds(started);output=std::move(current);stats=work;error.clear();return true;
}
}
