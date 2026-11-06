#pragma once
#include "CutFastDirectedTransport.h"

namespace judas::volume_grid {
struct CutTransportFace { std::size_t left, right; CutCompensated volume; };
struct CutTransportGasPort { std::size_t cell; CutCompensated volume; };

// An owner-local prepared transport evaluation. Interior faces advect both
// materials; gas-only ports are impermeable to water and use gas density.
// Tangents reuse exactly the factors from the successful physical evaluation.
class CutSelectiveTransport {
public:
    bool Build(const std::vector<double>& capacity,
               const std::vector<CutTransportFace>&,
               const std::vector<CutTransportGasPort>&,
               const std::vector<CutCompensated>& initialWater,
               const std::vector<CutCompensated>& initialMass,
               double ambientDensity, double quantityBudget, std::string&);
    bool Tangent(const std::vector<CutCompensated>& faceDerivative,
                 const std::vector<CutCompensated>& portDerivative,
                 std::vector<CutCompensated>& waterDerivative,
                 std::vector<CutCompensated>& massDerivative, std::string&);
    const std::vector<CutCompensated>& Water() const { return water_; }
    const std::vector<CutCompensated>& Mass() const { return mass_; }
    std::vector<CutCompensated> GasPortMass() const;
    // The prepared water operator has no exterior edge at any gas-only port.
    std::vector<CutCompensated> WaterPortVolume() const { return std::vector<CutCompensated>(ports_.size()); }
    CutFastTransportStats Stats() const;
private:
    bool Prepare(const std::vector<double>&, const std::vector<CutTransportFace>&,
                 const std::vector<CutTransportGasPort>&,
                 const std::vector<CutCompensated>&, const std::vector<CutCompensated>&,
                 double, double, std::string&);
    std::vector<double> capacity_;
    std::vector<CutTransportFace> faces_;
    std::vector<CutTransportGasPort> ports_;
    std::vector<CutCompensated> water_, mass_, alpha_, gamma_;
    double ambientDensity_ = 0.;
    bool sharedOperator_ = false;
    CutFastDirectedTransport waterSolver_, gasSolver_;
};
}
