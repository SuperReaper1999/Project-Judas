#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace judas::volume_grid {

struct CutHydroacousticGraphLink {
    std::size_t left, right;
    double conductance;
};

struct CutHydroacousticGraphResult {
    std::vector<double> pressure;
    double maximumResidual = 0.;
    int iterations = 0;
};

// Solve (diag(groundStorage) + B G B^T) p = rhs. A connected
// component without storage has a constant pressure gauge. Pin one
// row only after verifying its source compatibility. All original
// rows, including pinned rows, must satisfy the supplied tolerance.
// A failed solve preserves the previous result.
bool SolveCutHydroacousticGraph(const std::vector<double>& groundStorage,
                               const std::vector<CutHydroacousticGraphLink>& links,
                               const std::vector<double>& rhs, double volumeTolerance,
                               CutHydroacousticGraphResult& output, std::string& error);

} // namespace judas::volume_grid
