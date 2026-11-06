#include "CutCoupledResidual.h"
#include <algorithm>
#include <cmath>

namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
std::vector<Real> Mass(const std::vector<Real>& w,const std::vector<Real>& gas,double density){
    std::vector<Real> r;for(std::size_t i=0;i<w.size();++i)r.push_back(Real(density)*w[i]+gas[i]);return r;
}
bool Fail(std::string& e,const char* s){e=s;return false;}
}
bool CutCoupledResidual::Build(const CutSpatialField& f,const CutVelocityDual& dual,const CutDynamicSettings& s,
    const std::vector<Real>& sw,const std::vector<Real>& sm,const std::vector<Real>& su,
    const std::vector<Real>& p,const std::vector<Real>& u,CutMomentumBackend backend,std::string& error){
    CutCoupledResidual next;
    if(!next.Prepare(f,dual,s,sw,sm,su,p,u,backend,error))return false;
    *this=std::move(next);error.clear();return true;
}
bool CutCoupledResidual::Prepare(const CutSpatialField& f,const CutVelocityDual& dual,const CutDynamicSettings& s,
    const std::vector<Real>& sw,const std::vector<Real>& sm,const std::vector<Real>& su,
    const std::vector<Real>& p,const std::vector<Real>& u,CutMomentumBackend backend,std::string& error){
    if(p.size()!=sw.size()||sm.size()!=sw.size()||u.size()!=dual.Nodes().size())return Fail(error,"coupled residual dimension mismatch");
    field_=&f;dual_=&dual;settings_=s;velocity=u;material.pressure=p;
    material.flux.assign(f.geometry.faces.size(),{});material.portFlux.assign(f.boundaries.size(),{});
    for(std::size_t j=0;j<u.size();++j){auto node=dual.Nodes()[j];
        if(!std::isfinite(u[j].hi)||!std::isfinite(u[j].lo)||(node.fixed&&u[j].Value()!=0.))return Fail(error,"invalid coupled velocity");
        if(node.fixed)continue;
        Real q=Real(s.dt*node.area)*u[j];
        if(node.face!=SIZE_MAX)material.flux[node.face]=q;else material.portFlux[node.port]=Real(node.sign)*q;
    }
    faces_.clear();ports_.clear();rho_.clear();
    for(std::size_t i=0;i<p.size();++i){Real r=Real(s.gasDensity)+p[i]/Real(s.soundSpeed*s.soundSpeed);
        if(!(r.Value()>0.&&std::isfinite(r.hi)&&std::isfinite(r.lo)))return Fail(error,"coupled EOS outside positive domain");
        rho_.push_back(r);
    }
    for(std::size_t i=0;i<material.flux.size();++i)faces_.push_back({f.geometry.faces[i].left,f.geometry.faces[i].right,material.flux[i]});
    for(std::size_t i=0;i<material.portFlux.size();++i)ports_.push_back({f.boundaries[i].fragment,material.portFlux[i]});
    if(!transport_.Build(f.geometry.startCapacity,faces_,ports_,sw,sm,s.gasDensity,s.volumeTolerance*1e-4,error))return false;
    material.water=transport_.Water();material.gasMass=transport_.Mass();
    if(!transport_.InteriorMassFlux(s.waterDensity,massFlux_,error))return false;
    portMass_=transport_.GasPortMass();
    if(!momentum.Build(dual,f,s,Mass(sw,sm,s.waterDensity),Mass(material.water,material.gasMass,s.waterDensity),
        massFlux_,portMass_,su,error,backend,false)||!momentum.Audit(p,material.flux,material.portFlux,audit,error,&momentumRows))return false;
    rawVolume.resize(p.size());material.maximumVolumeResidual=material.maximumNormalizedResidual=0.;
    for(std::size_t i=0;i<p.size();++i){rawVolume[i]=Real(f.geometry.startCapacity[i])-material.water[i]-material.gasMass[i]/rho_[i];
        double tolerance=std::min(s.volumeTolerance,s.relativeVolumeTolerance*f.geometry.startCapacity[i]);
        material.maximumVolumeResidual=std::max(material.maximumVolumeResidual,std::abs(rawVolume[i].Value()));
        material.maximumNormalizedResidual=std::max(material.maximumNormalizedResidual,std::abs(rawVolume[i].Value())/tolerance);
    }
    return ApplyCutConservativeBalance(f.geometry.startCapacity,faces_,ports_,rawVolume,volumeRows,error);
}

bool CutCoupledResidual::Tangent(const std::vector<double>& dp,const std::vector<double>& du,
    std::vector<Real>& volume,std::vector<Real>& momentumRowsOut,std::string& error){
    if(!field_||dp.size()!=rho_.size()||du.size()!=velocity.size())return Fail(error,"coupled tangent dimension mismatch");
    const auto& f=*field_;const auto& dual=*dual_;const auto& s=settings_;
    std::vector<Real> dq(faces_.size()),dv(ports_.size()),dw,dm,df,db;
    for(auto j:momentum.FreeNodes()){auto node=dual.Nodes()[j];Real q=Real(s.dt*node.area)*Real(du[j]);
        if(node.face!=SIZE_MAX)dq[node.face]=q;else dv[node.port]=Real(node.sign)*q;
    }
    if(!transport_.MassTangent(dq,dv,s.waterDensity,dw,dm,df,db,error))return false;
    std::vector<Real> raw(dp.size()),massDelta(dp.size()),dualMass;
    for(std::size_t i=0;i<dp.size();++i){raw[i]=Real()-dw[i]-dm[i]/rho_[i]+material.gasMass[i]*Real(dp[i])/(rho_[i]*rho_[i]*Real(s.soundSpeed*s.soundSpeed));
        massDelta[i]=Real(s.waterDensity)*dw[i]+dm[i];}
    if(!ApplyCutConservativeBalanceTangent(f.geometry.startCapacity,faces_,ports_,rawVolume,raw,dq,dv,volume,error)||
        !dual.MassDerivative(massDelta,dualMass,error))return false;
    auto impulse=momentum.PressureImpulse(std::vector<Real>(dp.begin(),dp.end()));
    std::vector<Real> rows(velocity.size());
    for(auto j:momentum.FreeNodes())rows[j]=momentum.EndMass()[j]*Real(du[j])+dualMass[j]*(velocity[j]-Real(s.dt*s.gravity[dual.Nodes()[j].axis]))-impulse[j];
    for(std::size_t k=0;k<df.size();++k)for(auto route:dual.FaceScatter()[k]){
        Real flux=massFlux_[k]*route.fraction,delta=df[k]*route.fraction,carried;
        if(flux.Value()>0.)carried=delta*velocity[route.from]+flux*Real(du[route.from]);
        else if(flux.Value()<0.)carried=delta*velocity[route.to]+flux*Real(du[route.to]);
        else carried=delta*(velocity[route.from]+velocity[route.to])*Real(.5);
        rows[route.from]=rows[route.from]+carried;rows[route.to]=rows[route.to]-carried;
    }
    for(std::size_t k=0;k<db.size();++k)for(auto route:dual.PortScatter()[k]){
        auto j=route.from;Real flux=portMass_[k]*route.fraction,delta=db[k]*route.fraction;
        if(flux.Value()>0.)rows[j]=rows[j]+delta*velocity[j]+flux*Real(du[j]);
        else if(flux.Value()==0.)rows[j]=rows[j]+delta*velocity[j]*Real(.5);
    }
    momentumRowsOut=std::move(rows);error.clear();return true;
}
} // namespace judas::volume_grid
