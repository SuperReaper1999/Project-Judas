#pragma once

#include "CutVelocityDual.h"
#include "CutMassMomentum.h"
#include "CutDualLinear.h"

namespace judas::volume_grid {

struct CutDualMomentumAudit {
    double maximumNormalizedResidual=0., maximumMassResidual=0.;
    double beforeEnergy=0., afterEnergy=0., pressureWork=0., gravityWork=0.;
    double unsteadyLoss=0., mixingLoss=0., externalEnergy=0., energyResidual=0.;
    double maximumSpeed=0.;
};

// One frozen nonlinear iterate, not a new engine physics service. The same
// admitted primal mass transfers supply a positive dual momentum operator.
// Pressure changes its RHS; factors remain owned by this coefficient state.
class CutDualMomentum {
public:
    bool Build(const CutVelocityDual&,const CutSpatialField&,const CutDynamicSettings&,
        const std::vector<CutCompensated>& sourceCellMass,
        const std::vector<CutCompensated>& endCellMass,
        const std::vector<CutCompensated>& primalMassFlux,
        const std::vector<CutCompensated>& portMassFlux,
        const std::vector<CutCompensated>& sourceVelocity,std::string&,
        CutMomentumBackend backend=CutMomentumBackend::Fast,bool prepareLinear=true);
    bool Flow(const std::vector<CutCompensated>& pressure,
        std::vector<CutCompensated>& faceVolume,std::vector<CutCompensated>& portVolume,
        std::string&,std::vector<CutCompensated>* velocity=nullptr);
    bool Derivative(const std::vector<double>& pressureDirection,
        std::vector<CutCompensated>& faceVolume,std::vector<CutCompensated>& portVolume,std::string&);
    bool Audit(const std::vector<CutCompensated>& pressure,
        const std::vector<CutCompensated>& faceVolume,const std::vector<CutCompensated>& portVolume,
        CutDualMomentumAudit&,std::string&,std::vector<CutCompensated>* originalRows=nullptr) const;
    bool SolveImpulse(const std::vector<CutCompensated>&,std::vector<CutCompensated>&,std::string&);
    std::vector<CutCompensated> PressureImpulse(const std::vector<CutCompensated>&) const;
    std::vector<CutCompensated> FrozenTangent(const std::vector<double>& pressure,const std::vector<double>& velocity)const;
    const std::vector<CutCompensated>& Diagonal()const{return diagonal_;}
    const std::vector<std::size_t>& FreeNodes()const{return free_;}
    const std::vector<double>& FaceConductance() const { return faceConductance_; }
    const std::vector<double>& PortConductance() const { return portConductance_; }
    const std::vector<CutCompensated>& SourceMass() const { return startMass_; }
    const std::vector<CutCompensated>& EndMass() const { return endMass_; }
    bool Matches(const CutSpatialField& f,const CutDynamicSettings& s) const {
        return field_==&f&&settings_.dt==s.dt&&settings_.gravity==s.gravity&&
            settings_.waterDensity==s.waterDensity&&settings_.gasDensity==s.gasDensity&&
            settings_.soundSpeed==s.soundSpeed&&faceConductance_.size()==f.geometry.faces.size()&&
            portConductance_.size()==f.boundaries.size();
    }
private:
    bool Solve(const std::vector<CutCompensated>&,bool derivative,
        std::vector<CutCompensated>&,std::vector<CutCompensated>&,std::string&,
        std::vector<CutCompensated>*);
    bool Prepare(std::string&);
    const CutVelocityDual* geometry_=nullptr;
    const CutSpatialField* field_=nullptr;
    CutDynamicSettings settings_;
    CutMomentumBackend backend_=CutMomentumBackend::Fast;
    CutDualLinear fast_;
    CutDirectedTransport reference_;
    std::vector<std::size_t> free_,index_;
    std::vector<double> capacity_,faceConductance_,portConductance_;
    std::vector<CutCompensated> startMass_,endMass_,sourceVelocity_,density_,base_,diagonal_,massResidual_;
    std::vector<CutTransportFace> edges_;
    std::vector<CutTransportGasPort> ports_;
    std::vector<CutDirectedEdge> physicalMatrix_,referenceMatrix_;
    std::vector<CutCompensated> exterior_;
    bool ready_=false;
};
} // namespace judas::volume_grid
