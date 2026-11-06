#include "CutDualMomentum.h"
#include <algorithm>
#include <cmath>

namespace judas::volume_grid {
namespace {
using Real=CutCompensated;
bool Finite(Real x){return std::isfinite(x.hi)&&std::isfinite(x.lo);}
bool Fail(std::string& e,const char* text){e=text;return false;}
}
bool CutDualMomentum::Build(const CutVelocityDual& geometry,const CutSpatialField& field,
    const CutDynamicSettings& settings,const std::vector<Real>& sourceMass,
    const std::vector<Real>& endMass,const std::vector<Real>& massFlux,
    const std::vector<Real>& portFlux,const std::vector<Real>& velocity,std::string& error,
    CutMomentumBackend backend,bool prepareLinear){
    if(!geometry.Matches(field)||!(settings.dt>0.&&std::isfinite(settings.dt))||
        (!velocity.empty()&&velocity.size()!=geometry.Nodes().size()))return Fail(error,"invalid dual momentum source");
    CutDualMomentum next;next.geometry_=&geometry;next.field_=&field;next.settings_=settings;next.backend_=backend;
    if(!geometry.Mass(sourceMass,next.startMass_,error)||!geometry.Mass(endMass,next.endMass_,error)||
        !geometry.Carriers(massFlux,portFlux,next.edges_,next.ports_,error))return false;
    const auto n=geometry.Nodes().size();next.index_.assign(n,SIZE_MAX);next.sourceVelocity_=velocity;next.sourceVelocity_.resize(n);
    next.massResidual_=next.endMass_;next.diagonal_=next.endMass_;next.base_.resize(n);next.density_.resize(n);
    for(std::size_t i=0;i<n;++i){
        if(!Finite(next.sourceVelocity_[i])||!(next.startMass_[i].Value()>0.&&next.endMass_[i].Value()>0.))return Fail(error,"invalid dual momentum mass or velocity");
        if(geometry.Nodes()[i].fixed){if(next.sourceVelocity_[i].Value()!=0.)return Fail(error,"stationary normal wall has nonzero velocity");}
        else{next.index_[i]=next.free_.size();next.free_.push_back(i);next.capacity_.push_back(geometry.Capacity()[i]);}
        next.massResidual_[i]=next.massResidual_[i]-next.startMass_[i];
        next.density_[i]=next.endMass_[i]/Real(geometry.Capacity()[i]);
        next.base_[i]=next.startMass_[i]*next.sourceVelocity_[i]+next.endMass_[i]*Real(settings.dt*settings.gravity[geometry.Nodes()[i].axis]);
    }
    std::vector<CutDirectedEdge> matrixEdges,physicalEdges;std::vector<Real> exterior(next.free_.size());
    auto boundary=[&](std::size_t node,Real flux){
        next.massResidual_[node]=next.massResidual_[node]+flux;
        if(flux.Value()>0.){next.diagonal_[node]=next.diagonal_[node]+flux;if(next.index_[node]!=SIZE_MAX)exterior[next.index_[node]]=exterior[next.index_[node]]+flux/next.density_[node];}
    };
    for(const auto& edge:next.edges_){
        const bool forward=edge.volume.Value()>=0.;const auto from=forward?edge.left:edge.right,to=forward?edge.right:edge.left;
        const Real amount=forward?edge.volume:Real()-edge.volume;
        next.massResidual_[from]=next.massResidual_[from]+amount;next.massResidual_[to]=next.massResidual_[to]-amount;
        next.diagonal_[from]=next.diagonal_[from]+amount;
        if(next.index_[from]!=SIZE_MAX){
            if(next.index_[to]!=SIZE_MAX){matrixEdges.push_back({next.index_[from],next.index_[to],amount/next.density_[from]});physicalEdges.push_back({next.index_[from],next.index_[to],amount});}
            else exterior[next.index_[from]]=exterior[next.index_[from]]+amount/next.density_[from];
        }
    }
    for(const auto& port:next.ports_)boundary(port.cell,port.volume);
    for(auto r:next.massResidual_)if(!Finite(r)||std::abs(r.Value())>settings.waterDensity*settings.volumeTolerance)
        return Fail(error,"velocity-dual mass carriers fail original material budget");
    next.physicalMatrix_=std::move(physicalEdges);next.referenceMatrix_=std::move(matrixEdges);next.exterior_=std::move(exterior);
    if(prepareLinear&&!next.Prepare(error))return false;
    next.faceConductance_.resize(field.geometry.faces.size());next.portConductance_.resize(field.boundaries.size());
    for(std::size_t i=0;i<n;++i){const auto& node=geometry.Nodes()[i];if(node.fixed)continue;
        const double g=settings.dt*settings.dt*node.area*node.area/next.diagonal_[i].Value();
        if(!(g>0.&&std::isfinite(g)))return Fail(error,"unrepresentable dual pressure response");
        if(node.face!=SIZE_MAX)next.faceConductance_[node.face]=g;else next.portConductance_[node.port]=g;
    }
    *this=std::move(next);error.clear();return true;
}

bool CutDualMomentum::Prepare(std::string& error){
    if(ready_)return true;
    if(!free_.empty()){
        if(backend_==CutMomentumBackend::Fast){std::vector<Real> diagonal;for(auto i:free_)diagonal.push_back(diagonal_[i]);if(!fast_.Build(diagonal,physicalMatrix_,error))return false;}
        else if(!reference_.Build(capacity_,referenceMatrix_,exterior_,error))return false;
    }
    ready_=true;return true;
}

bool CutDualMomentum::SolveImpulse(const std::vector<Real>& impulse,std::vector<Real>& output,std::string& error){
    if(!geometry_||impulse.size()!=geometry_->Nodes().size())return Fail(error,"dual impulse dimensions mismatch");
    if(!Prepare(error))return false;
    std::vector<Real> rhs,solution,u(impulse.size());double minimumDensity=1e300;
    for(auto i:free_){rhs.push_back(impulse[i]);minimumDensity=std::min(minimumDensity,density_[i].Value());}
    if(!free_.empty()){
        if(backend_==CutMomentumBackend::Fast){if(!fast_.Solve(rhs,solution,error))return false;}
        else if(!reference_.SolveFraction(rhs,1e-14*minimumDensity,solution,error))return false;
        for(std::size_t j=0;j<free_.size();++j)u[free_[j]]=backend_==CutMomentumBackend::Fast?solution[j]:solution[j]/density_[free_[j]];
    }
    output=std::move(u);error.clear();return true;
}

std::vector<Real> CutDualMomentum::PressureImpulse(const std::vector<Real>& p)const{
    std::vector<Real> result(geometry_->Nodes().size());
    for(auto i:free_){const auto& node=geometry_->Nodes()[i];Real head;
        if(node.face!=SIZE_MAX){auto face=field_->geometry.faces[node.face];head=p[face.left]-p[face.right];}
        else head=Real(node.sign)*p[field_->boundaries[node.port].fragment];
        result[i]=Real(settings_.dt*node.area)*head;
    }return result;
}

std::vector<Real> CutDualMomentum::FrozenTangent(const std::vector<double>& dp,const std::vector<double>& du)const{
    auto impulse=PressureImpulse(std::vector<Real>(dp.begin(),dp.end()));std::vector<Real> rows(du.size());
    for(auto j:free_)rows[j]=endMass_[j]*Real(du[j])-impulse[j];
    for(const auto& edge:edges_){bool forward=edge.volume.Value()>=0.;auto from=forward?edge.left:edge.right,to=forward?edge.right:edge.left;
        Real carried=(forward?edge.volume:Real()-edge.volume)*Real(du[from]);rows[from]=rows[from]+carried;rows[to]=rows[to]-carried;}
    for(const auto& port:ports_)if(port.volume.Value()>0.)rows[port.cell]=rows[port.cell]+port.volume*Real(du[port.cell]);
    return rows;
}

bool CutDualMomentum::Solve(const std::vector<Real>& p,bool derivative,std::vector<Real>& face,
    std::vector<Real>& port,std::string& error,std::vector<Real>* velocity){
    if(!geometry_||p.size()!=field_->parent.size())return Fail(error,"dual pressure source dimensions mismatch");
    for(auto x:p)if(!Finite(x))return Fail(error,"nonfinite dual pressure source");
    if(!Prepare(error))return false;
    auto impulse=PressureImpulse(p);std::vector<Real> rhs(free_.size()),solution,u(geometry_->Nodes().size());
    double minimumDensity=1e300;
    for(std::size_t j=0;j<free_.size();++j){auto i=free_[j];rhs[j]=impulse[i]+(derivative?Real():base_[i]);minimumDensity=std::min(minimumDensity,density_[i].Value());}
    if(!free_.empty()){
        const double budget=1e-14*minimumDensity;
        if(backend_==CutMomentumBackend::Fast){if(!fast_.Solve(rhs,solution,error))return false;}
        else if(!reference_.SolveFraction(rhs,budget,solution,error))return false;
        for(std::size_t j=0;j<free_.size();++j){auto i=free_[j];u[i]=backend_==CutMomentumBackend::Fast?solution[j]:solution[j]/density_[i];if(!Finite(u[i]))return Fail(error,"nonfinite dual velocity response");}
    }
    std::vector<Real> q(field_->geometry.faces.size()),v(field_->boundaries.size());
    for(auto i:free_){const auto& node=geometry_->Nodes()[i];Real volume=Real(settings_.dt*node.area)*u[i];
        if(node.face!=SIZE_MAX)q[node.face]=volume;else v[node.port]=Real(node.sign)*volume;
    }
    face=std::move(q);port=std::move(v);if(velocity)*velocity=std::move(u);error.clear();return true;
}
bool CutDualMomentum::Flow(const std::vector<Real>& p,std::vector<Real>& q,std::vector<Real>& v,std::string& e,std::vector<Real>* u){return Solve(p,false,q,v,e,u);}
bool CutDualMomentum::Derivative(const std::vector<double>& p,std::vector<Real>& q,std::vector<Real>& v,std::string& e){return Solve(std::vector<Real>(p.begin(),p.end()),true,q,v,e,nullptr);}

bool CutDualMomentum::Audit(const std::vector<Real>& p,const std::vector<Real>& q,const std::vector<Real>& v,
    CutDualMomentumAudit& output,std::string& error,std::vector<Real>* originalRows)const{
    if(!geometry_||p.size()!=field_->parent.size()||q.size()!=field_->geometry.faces.size()||v.size()!=field_->boundaries.size())return Fail(error,"dual audit dimensions mismatch");
    const auto n=geometry_->Nodes().size();std::vector<Real> u(n);auto impulse=PressureImpulse(p);
    for(auto i:free_){const auto& node=geometry_->Nodes()[i];u[i]=(node.face!=SIZE_MAX?q[node.face]:Real(node.sign)*v[node.port])/Real(settings_.dt*node.area);}
    std::vector<Real> residual(n);for(auto i:free_)residual[i]=endMass_[i]*u[i]-base_[i]-impulse[i];
    Real before,after,pressureWork,gravityWork,temporal,mixing,external,defectWork;
    for(std::size_t i=0;i<n;++i){
        before=before+Real(.5)*startMass_[i]*sourceVelocity_[i]*sourceVelocity_[i];
        after=after+Real(.5)*endMass_[i]*u[i]*u[i];Real delta=u[i]-sourceVelocity_[i];
        temporal=temporal+Real(.5)*startMass_[i]*delta*delta;
        pressureWork=pressureWork+impulse[i]*u[i];
        gravityWork=gravityWork+endMass_[i]*Real(settings_.dt*settings_.gravity[geometry_->Nodes()[i].axis])*u[i];
        defectWork=defectWork-Real(.5)*massResidual_[i]*u[i]*u[i];
    }
    for(const auto& edge:edges_){const bool forward=edge.volume.Value()>=0.;auto from=forward?edge.left:edge.right,to=forward?edge.right:edge.left;Real amount=forward?edge.volume:Real()-edge.volume;
        const Real carried=amount*u[from];residual[from]=residual[from]+carried;residual[to]=residual[to]-carried;
        Real delta=u[from]-u[to];mixing=mixing+Real(.5)*amount*delta*delta;
    }
    for(const auto& port:ports_){const auto i=port.cell;
        if(port.volume.Value()>=0.){residual[i]=residual[i]+port.volume*u[i];external=external+Real(.5)*port.volume*u[i]*u[i];}
        else mixing=mixing-Real(.5)*port.volume*u[i]*u[i];
    }
    CutDualMomentumAudit next;
    for(auto i:free_){
        if(!Finite(residual[i])||!Finite(u[i]))return Fail(error,"nonfinite dual momentum audit");
        const double scale=diagonal_[i].Value()*std::max(1.,std::abs(u[i].Value()));
        next.maximumNormalizedResidual=std::max(next.maximumNormalizedResidual,std::abs(residual[i].Value())/scale);
        next.maximumSpeed=std::max(next.maximumSpeed,std::abs(u[i].Value()));
    }
    for(auto r:massResidual_)next.maximumMassResidual=std::max(next.maximumMassResidual,std::abs(r.Value()));
    const Real workError=after-before-pressureWork-gravityWork+temporal+mixing+external-defectWork;
    next.beforeEnergy=before.Value();next.afterEnergy=after.Value();next.pressureWork=pressureWork.Value();next.gravityWork=gravityWork.Value();
    next.unsteadyLoss=temporal.Value();next.mixingLoss=mixing.Value();next.externalEnergy=external.Value();next.energyResidual=workError.Value();
    if(!Finite(workError))return Fail(error,"nonfinite dual work ledger");
    if(originalRows)*originalRows=std::move(residual);
    output=next;error.clear();return true;
}
} // namespace judas::volume_grid
