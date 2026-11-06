#include "WaterCellAudit.h"
#include <algorithm>
#include <cmath>
#include <sstream>
namespace judas::volume_grid {
bool AuditWaterCell(double volume,double density,double dt,const GridVec& gravity,
    const std::vector<WaterCellPatch>& patches,const WaterCellProjection& response,
    const std::vector<CutCompensated>& flux,const std::vector<CutCompensated>& pressure,
    WaterCellAudit& output,std::string& error,bool relativeHead,CutCompensated physicalHeadOffset){
    using Real=CutCompensated;const auto n=patches.size(),m=response.free.size();
    if(!(volume>0.&&density>0.&&dt>0.)||response.mass.size()!=n*n||flux.size()!=m||pressure.size()!=m){error="invalid liquid cell audit";return false;}
    std::vector<Real> velocity(n),previous(n),p(n),head(n),deltaMomentum(n);WaterCellAudit next;next.wallPressure.resize(n);
    for(std::size_t i=0;i<n;++i)previous[i]=Real(patches[i].sign*response.sourceVelocity[patches[i].axis]);
    for(std::size_t i=0;i<m;++i){auto f=response.free[i];velocity[f]=flux[i]/Real(patches[f].area);p[f]=pressure[i];head[f]=pressure[i];if(!relativeHead)for(int a=0;a<3;++a)head[f]=head[f]-Real(density*gravity[a])*Real(patches[f].offset[a]);}
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)deltaMomentum[i]=deltaMomentum[i]+Real(response.mass[i*n+j])*(velocity[j]-previous[j]);
    Real centre;
    if(m){auto index=response.reference,patch=response.free[index];centre=head[patch]+deltaMomentum[patch]/Real(dt*patches[patch].area);}
    std::array<Real,3> mean,reaction;
    for(std::size_t i=0;i<n;++i){auto f=patches[i];Real hydro;
        for(int a=0;a<3;++a)hydro=hydro+Real(density*gravity[a])*Real(f.offset[a]);
        if(f.fixed){p[i]=centre+physicalHeadOffset+hydro-deltaMomentum[i]/Real(dt*f.area);next.wallPressure[i]=p[i].Value();reaction[f.axis]=reaction[f.axis]+Real(f.sign*f.area)*p[i];}
        else {Real residual=deltaMomentum[i]-Real(dt*f.area)*(centre-head[i]);next.maximumMomentumResidual=std::max(next.maximumMomentumResidual,std::abs(residual.Value()));}
        Real q=velocity[i]*Real(f.area);for(int a=0;a<3;++a)mean[a]=mean[a]+Real(f.offset[a])*q/Real(volume);
    }
    Real before,after,loss,work;
    for(std::size_t i=0;i<n;++i){Real force=Real(dt*patches[i].area)*(centre-head[i]);
        work=work+force*velocity[i];
        for(std::size_t j=0;j<n;++j){Real mass(response.mass[i*n+j]);before=before+Real(.5)*previous[i]*mass*previous[j];after=after+Real(.5)*velocity[i]*mass*velocity[j];loss=loss+Real(.5)*(velocity[i]-previous[i])*mass*(velocity[j]-previous[j]);}
    }
    next.centrePressure=(centre+physicalHeadOffset).Value();next.beforeEnergy=before.Value();next.afterEnergy=after.Value();next.implicitLoss=loss.Value();next.work=work.Value();next.workResidual=(after-before+loss-work).Value();
    for(int a=0;a<3;++a){next.meanVelocity[a]=mean[a].Value();next.wallReaction[a]=reaction[a].Value();}
    double scale=std::max({std::abs(next.beforeEnergy),std::abs(next.afterEnergy),std::abs(next.work),1e-30});
    if(!std::isfinite(next.workResidual)||std::abs(next.workResidual)>1e-10*scale||next.maximumMomentumResidual>1e-10*std::max(density*volume,1e-30)){std::ostringstream detail;detail.precision(17);double maximumSpeed=0.;for(auto v:velocity)maximumSpeed=std::max(maximumSpeed,std::abs(v.Value()));detail<<"liquid cell original momentum/work audit failed volume="<<volume<<" workResidual="<<next.workResidual<<" workScale="<<scale<<" momentumResidual="<<next.maximumMomentumResidual<<" momentumBudget="<<1e-10*std::max(density*volume,1e-30)<<" maximumPatchSpeed="<<maximumSpeed;error=detail.str();return false;}
    output=std::move(next);error.clear();return true;
}
}
