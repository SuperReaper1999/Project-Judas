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

struct CutHydroacousticRowNeighbor { double conductance, pressure; };
// Optional diagnostic only: populated on an original-row balance failure.
// It never replaces or publishes the physical solution on rejection.
struct CutHydroacousticGraphFailure {
    int iterations = 0;
    std::size_t row = 0;
    double residual = 0., normalizedResidual = 0.;
    double storage = 0., source = 0., tolerance = 0., pressure = 0.;
    std::vector<CutHydroacousticRowNeighbor> neighbors;
};

// Same operator and original-row audit, with an independently supplied
// tolerance for each physical owner. A large owner cannot mask a small one.
bool SolveCutHydroacousticGraphLocal(const std::vector<double>& groundStorage,
                                    const std::vector<CutHydroacousticGraphLink>& links,
                                    const std::vector<double>& rhs,
                                    const std::vector<double>& rowTolerance,
                                    CutHydroacousticGraphResult& output, std::string& error,
                                    CutHydroacousticGraphFailure* diagnostic = nullptr);

} // namespace judas::volume_grid
