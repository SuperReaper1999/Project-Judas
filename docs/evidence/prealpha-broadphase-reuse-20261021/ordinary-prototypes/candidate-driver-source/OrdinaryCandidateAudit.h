#pragma once

#include <cstdint>
#include <ostream>
#include <stdexcept>

// Private diagnostic observer. No engine layout or numerical state is changed.
namespace ordinary_candidate_audit {
struct Stats {
    std::uint64_t steps = 0;
    std::uint64_t certified = 0;
    std::uint64_t invalidated = 0;
    std::uint64_t mismatches = 0;
    std::uint64_t predictionPairs = 0;
    std::uint64_t postIntegrationPairs = 0;
    std::uint64_t integrationReinsertions = 0;
};
inline thread_local Stats stats;
inline void Reset() { stats = {}; }
inline void Header(std::ostream &out) {
    out << "step,steps,certified,invalidated,mismatches,prediction_pairs,post_integration_pairs,integration_reinsertions\n";
}
inline void WriteStep(std::ostream &out, std::uint64_t step) {
    out << step << ',' << stats.steps << ',' << stats.certified << ',' << stats.invalidated
        << ',' << stats.mismatches << ',' << stats.predictionPairs << ','
        << stats.postIntegrationPairs << ',' << stats.integrationReinsertions << '\n';
}
template <class Pairs>
inline void Observe(bool certified, const Pairs &before, const Pairs &after,
                    std::uint64_t reinsertions) {
    ++stats.steps;
    stats.certified += certified;
    stats.invalidated += !certified;
    stats.predictionPairs += before.size();
    stats.postIntegrationPairs += after.size();
    stats.integrationReinsertions += reinsertions;
    if (certified && before != after) {
        ++stats.mismatches;
        throw std::logic_error("private candidate-reuse certificate failed");
    }
}
} // namespace ordinary_candidate_audit
