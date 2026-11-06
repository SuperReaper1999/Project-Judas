#pragma once
#include "CutSelectiveTransport.h"
namespace judas::volume_grid {
// B(R) = T(q) diag(C)^-1 R. Positive capacity makes T nonsingular;
// this changes the nonlinear coordinates, not the physical root R = 0.
bool ApplyCutConservativeBalance(const std::vector<double>&,
 const std::vector<CutTransportFace>&, const std::vector<CutTransportGasPort>&,
 const std::vector<CutCompensated>& residual, std::vector<CutCompensated>&, std::string&);
// Exact directional derivative T C^-1 dR + dT C^-1 R, with the same
// generalized midpoint convention as material transport at exactly zero flux.
bool ApplyCutConservativeBalanceTangent(const std::vector<double>&,
 const std::vector<CutTransportFace>&, const std::vector<CutTransportGasPort>&,
 const std::vector<CutCompensated>& residual,
 const std::vector<CutCompensated>& residualDerivative,
 const std::vector<CutCompensated>& faceDerivative,
 const std::vector<CutCompensated>& portDerivative,
 std::vector<CutCompensated>&, std::string&);
}
