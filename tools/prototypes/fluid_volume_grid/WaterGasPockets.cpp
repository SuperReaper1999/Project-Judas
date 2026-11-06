#include "WaterGasPockets.h"
#include <algorithm>
#include <cmath>
namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Finite(Real a){return std::isfinite(a.hi)&&std::isfinite(a.lo);}
bool Positive(double x){return x>0.&&std::isfinite(x);}
bool Fail(std::string& e,const char* why){e=why;return false;}
}
bool RectangularGasMassConductance(double width,double height,double length,
    double density,double pressure,double viscosity,double& result,std::string& error){
    if(!Positive(width)||!Positive(height)||!Positive(length)||!Positive(density)||
       !Positive(pressure)||!Positive(viscosity))return Fail(error,"invalid rectangular gas throat");
    if(height>width)std::swap(height,width);
    const double pi=std::acos(-1.);double series=0.;
    for(int n=1;n<=63;n+=2)series+=std::tanh(n*pi*width/(2.*height))/std::pow(double(n),5.);
    const double shape=1.-192./std::pow(pi,5.)*(height/width)*series;
    const double viscous=density*width*height*height*height*shape/(12.*viscosity*length);
    const double acoustic=width*height/std::sqrt(pressure/density);
    if(!Positive(viscous)||!Positive(acoustic))return Fail(error,"positive gas throat exceeds numeric conductance range");
    // Avoid forming a reciprocal/product of extremely small conductances.
    const double small=std::min(viscous,acoustic),large=std::max(viscous,acoustic);
    const double conductance=small/(1.+small/large);
    if(!Positive(conductance))return Fail(error,"gas throat resistance is unrepresentable");
    result=conductance;error.clear();return true;
}
bool AdvanceWaterGasPockets(const std::vector<Real>& source,const std::vector<Real>& volume,
    const std::vector<WaterGasLink>& links,const std::vector<WaterGasVent>& vents,
    const WaterGasSettings& settings,WaterGasResult& output,std::string& error){
    const auto n=source.size();
    if(volume.size()!=n||!Positive(settings.dt)||!Positive(settings.referenceDensity)||
       !Positive(settings.referencePressure)||!Positive(settings.massTolerance)||
       !(settings.relativeTolerance>0.&&settings.relativeTolerance<1.))return Fail(error,"invalid gas-pocket state or settings");
    if(!n){
        if(!links.empty()||!vents.empty())return Fail(error,"empty gas state has nonempty connections");
        output={};error.clear();return true;
    }
    std::vector<double> storage(n),rhs(n),tolerance(n);std::vector<Real> exactStorage(n),exactRhs(n);
    std::vector<bool> small(n);double maximumVolume=0.;
    for(std::size_t i=0;i<n;++i){
        if(!Finite(source[i])||source[i].Value()<0.||!Finite(volume[i])||!(volume[i].Value()>0.))
            return Fail(error,"gas-pocket closure requires positive capacity and independently preserved nonnegative mass");
        maximumVolume=std::max(maximumVolume,volume[i].Value());
        exactStorage[i]=Real(settings.referenceDensity)*volume[i]/Real(settings.referencePressure);
        exactRhs[i]=source[i]-Real(settings.referenceDensity)*volume[i];
        storage[i]=exactStorage[i].Value();
        tolerance[i]=std::min(settings.massTolerance,settings.relativeTolerance*settings.referenceDensity*volume[i].Value());
        if(!Positive(storage[i])||!Positive(tolerance[i]))return Fail(error,"gas-pocket storage exceeds numeric range");
    }
    std::vector<CutHydroacousticGraphLink> matrix;
    for(auto link:links){
        if(link.left>=n||link.right>=n||link.left==link.right||!Positive(link.massConductance))return Fail(error,"invalid finite gas connection");
        double g=settings.dt*link.massConductance;
        if(!Positive(g))return Fail(error,"finite gas connection exceeds numeric range");
        matrix.push_back({link.left,link.right,g});
    }
    for(auto vent:vents){
        if(vent.node>=n||!Positive(vent.massConductance)||!std::isfinite(vent.gaugePressure)||
           !(vent.gaugePressure>=-settings.referencePressure))return Fail(error,"invalid atmospheric gas port");
        double g=settings.dt*vent.massConductance;
        if(!Positive(g))return Fail(error,"atmospheric port exceeds numeric range");
        storage[vent.node]+=g;exactRhs[vent.node]=exactRhs[vent.node]+Real(g)*Real(vent.gaugePressure);
    }
    for(std::size_t i=0;i<n;++i){rhs[i]=exactRhs[i].Value();small[i]=volume[i].Value()<maximumVolume*.125;}
    CutCondensedResult solution;
    if(!SolveCutCondensedPressure(storage,matrix,rhs,tolerance,small,solution,error))return false;
    WaterGasResult next;next.pressure=std::move(solution.pressure);next.mass=source;
    for(auto link:links){Real amount=Real(settings.dt*link.massConductance)*(next.pressure[link.left]-next.pressure[link.right]);
        next.linkMass.push_back(amount);next.mass[link.left]=next.mass[link.left]-amount;next.mass[link.right]=next.mass[link.right]+amount;}
    for(auto vent:vents){Real amount=Real(settings.dt*vent.massConductance)*(next.pressure[vent.node]-Real(vent.gaugePressure));
        next.ventMass.push_back(amount);next.mass[vent.node]=next.mass[vent.node]-amount;next.externalMass=next.externalMass+amount;}
    Real total=next.externalMass;
    for(std::size_t i=0;i<n;++i){
        Real expected=Real(settings.referenceDensity)*volume[i]+exactStorage[i]*next.pressure[i];
        Real residual=next.mass[i]-expected;
        if(!Finite(next.mass[i])||next.mass[i].Value()<0.||!Finite(residual))return Fail(error,"gas-pocket mass or EOS admission failed");
        next.maximumMassResidual=std::max(next.maximumMassResidual,std::abs(residual.Value()));
        next.maximumNormalizedResidual=std::max(next.maximumNormalizedResidual,std::abs(residual.Value())/tolerance[i]);
        total=total+next.mass[i]-source[i];
    }
    if(next.maximumNormalizedResidual>1.||std::abs(total.Value())>settings.massTolerance)return Fail(error,"gas-pocket original mass ledger failed");
    output=std::move(next);error.clear();return true;
}
}
