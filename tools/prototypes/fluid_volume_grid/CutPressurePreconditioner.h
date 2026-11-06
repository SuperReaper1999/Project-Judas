#pragma once
#include "CutCondensedPressure.h"
namespace judas::volume_grid {
// Reusable approximate pressure accelerator, never a physical publication.
// It retains microscopic storage during condensation, freezes one positive
// operator, and supplies binary64 reduced corrections to the outer Krylov solve.
class CutPressurePreconditioner {
public:
    bool Build(const std::vector<double>& storage,
               const std::vector<CutHydroacousticGraphLink>&,
               const std::vector<bool>& condense, std::string&);
    bool Apply(const std::vector<double>& rhs, std::vector<double>&, std::string&) const;
private:
    struct Elimination {
        std::size_t node;
        CutCompensated diagonal;
        std::vector<std::pair<std::size_t, CutCompensated>> edges;
    };
    std::size_t size_ = 0;
    std::vector<Elimination> steps_;
    std::vector<std::size_t> original_;
    std::vector<double> storage_;
    std::vector<CutHydroacousticGraphLink> links_;
};
}
