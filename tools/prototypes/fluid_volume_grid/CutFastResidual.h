#pragma once
#include "CutExactResidual.h"
namespace judas::volume_grid {
// Identical error-free expansion order/enclosures, with in-place small storage.
// Dynamic overflow remains available; there is no precision or degree cutoff.
bool BoundCutDirectedResidualFast(const std::vector<double>&,
 const std::vector<CutDirectedEdge>&,const std::vector<CutCompensated>&,
 const std::vector<CutCompensated>&,const std::vector<CutCompensated>&,
 std::vector<CutResidualInterval>&,std::string&);
}
namespace judas::volume_grid {
// Conservative floating filter; ambiguous rows fall back to exact expansions.
bool BoundCutDirectedResidualFiltered(const std::vector<double>&,
 const std::vector<CutDirectedEdge>&,const std::vector<CutCompensated>&,
 const std::vector<CutCompensated>&,const std::vector<CutCompensated>&,
 const std::vector<double>& rowTargets,
 std::vector<CutResidualInterval>&,std::string&, bool proveNegative = false);
}
