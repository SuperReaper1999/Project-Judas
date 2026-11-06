#pragma once
#include "CutDirectedTransport.h"

namespace judas::volume_grid {
// Velocity is signed, not a positive material fraction. Solve its original
// momentum rows by compensated refinement over a binary64 ILU/GMRES kernel.
// No phase certificate or physical row is relaxed by this velocity solver.
class CutDualLinear {
public:
    bool Build(const std::vector<CutCompensated>& diagonal,
               const std::vector<CutDirectedEdge>& massEdges,std::string&);
    bool Solve(const std::vector<CutCompensated>& rhs,
               std::vector<CutCompensated>&,std::string&) const;
private:
    struct Entry {std::size_t column;CutCompensated mass;double scaled;};
    void Multiply(const std::vector<double>&,std::vector<double>&)const;
    void Precondition(const std::vector<double>&,std::vector<double>&)const;
    bool Correction(const std::vector<double>&,std::vector<double>&)const;
    std::vector<CutCompensated> diagonal_;
    std::vector<std::size_t> row_,pivot_;
    std::vector<Entry> entries_;
    std::vector<double> factor_;
};
} // namespace judas::volume_grid
