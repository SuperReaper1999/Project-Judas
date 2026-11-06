#pragma once

#include "CutSpatialTransport.h"
#include "CutSelectiveTransport.h"

namespace judas::volume_grid {

struct CutVelocityShare { std::size_t node; CutCompensated fraction; };
struct CutVelocityScatter { std::size_t from,to;CutCompensated fraction; };
struct CutVelocityNode {
    int axis = 0, sign = 1;
    std::size_t face = SIZE_MAX, port = SIZE_MAX;
    double area = 0.;
    bool fixed = false;
};

// Fluid-only frozen geometry. Material transport represents a cell mixture;
// velocity inertia uses that SAME mixture, rather than a separate sharp plane.
// Each component partitions every cell's mass among its incident aperture
// columns and stationary-wall remainder. No positive aperture is discarded.
class CutVelocityDual {
public:
    bool Build(const CutSpatialField&, std::string&);
    bool Matches(const CutSpatialField& field) const { return sourceField_==&field; }
    const std::vector<CutVelocityNode>& Nodes() const { return nodes_; }
    const std::vector<double>& Capacity() const { return dualCapacity_; }
    const std::vector<std::vector<CutVelocityShare>>& Shares() const { return shares_; }
    const std::vector<std::size_t>& FaceNodes() const { return faceNodes_; }
    const std::vector<std::size_t>& PortNodes() const { return portNodes_; }
    double MaximumPartitionRoundoff() const { return partitionRoundoff_; }
    bool Mass(const std::vector<CutCompensated>& cellMass,
              std::vector<CutCompensated>&, std::string&) const;
    bool MassDerivative(const std::vector<CutCompensated>&,std::vector<CutCompensated>&,std::string&)const;
    const std::vector<std::vector<CutVelocityScatter>>& FaceScatter()const{return faceScatter_;}
    const std::vector<std::vector<CutVelocityScatter>>& PortScatter()const{return portScatter_;}
    bool Carriers(const std::vector<CutCompensated>& primalMassFlux,
                  const std::vector<CutCompensated>& portMassFlux,
                  std::vector<CutTransportFace>&,
                  std::vector<CutTransportGasPort>&, std::string&) const;
private:
    std::vector<CutVelocityNode> nodes_;
    std::vector<double> dualCapacity_;
    std::vector<std::vector<CutVelocityShare>> shares_;
    std::vector<std::size_t> faceNodes_, portNodes_;
    std::vector<std::vector<CutVelocityScatter>> faceScatter_, portScatter_;
    double partitionRoundoff_ = 0.;
    const CutSpatialField* sourceField_=nullptr; // caller retains this frozen chart
};

} // namespace judas::volume_grid
