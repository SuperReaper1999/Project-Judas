#pragma once
#include "CutDirectedTransport.h"
#include <limits>

namespace judas::volume_grid {

struct CutFastTransportStats {
    std::size_t solves = 0, corrections = 0, sweeps = 0, fallbacks = 0;
};

// Mixed-precision corrections over the same positive condensed operator.
// All publication still requires the original compensated residual enclosure.
class CutFastDirectedTransport {
public:
    bool Build(const std::vector<double>&, const std::vector<CutDirectedEdge>&,
               const std::vector<CutCompensated>&, std::string&);
    bool SolveFraction(const std::vector<CutCompensated>&, double,
                       std::vector<CutCompensated>&, std::string&,
                       double quantityBudget = std::numeric_limits<double>::infinity());
    const CutFastTransportStats& Stats() const { return stats_; }

private:
    bool SolveChecked(const std::vector<CutCompensated>&, double,
                      std::vector<CutCompensated>&, std::string&, double quantityBudget);
    bool Correction(const std::vector<CutCompensated>&,
                    std::vector<CutCompensated>&);
    struct Entry { std::size_t column; double value; };
    CutDirectedTransport reference_;
    std::vector<std::size_t> row_, nodes_;
    std::vector<Entry> entries_;
    std::vector<CutCompensated> diagonal_;
    std::vector<double> reciprocal_, lower_;
    double barrierMaximum_ = 0.;
    CutFastTransportStats stats_;
};
}
