#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace judas::volume_grid {

struct RemapTransfer {
    std::size_t from, to;
    double amount; // Negative reverses the orientation.
};

struct ConservativeRemapInput {
    std::vector<double> start, end;
    std::vector<RemapTransfer> transfers;
    std::vector<bool> relayAllowed;
    double balanceTolerance = 0;
};

struct RemapScalarResult {
    std::vector<double> end, transfers;
    double maximumLocalResidual = 0;
    double conservationResidual = 0;
};

struct RemapMomentumResult {
    std::vector<std::array<double, 3>> end, transfers;
    std::array<double, 3> conservationResidual{};
    double beforeKineticEnergy = 0, afterKineticEnergy = 0;
    double mixingLoss = 0, kineticResidual = 0;
};

// The caller owns face admissibility and swept exposure. Ordinary nodes spend
// only their initial carrier. An allowed node that exceeds its initial budget
// may share a mixture of initial and received carrier between its exits and end
// state. This integrated upwind map does not establish the actual chronology of
// a swept face, supply missing pressure inertia, or preserve sharp interfaces.
class ConservativeRemap {
public:
    bool RemapScalar(const std::vector<double>& start, RemapScalarResult& output,
                     std::string& error) const;
    // Build the plan with mass, then supply extensive linear momentum.
    bool RemapMomentum(const std::vector<std::array<double, 3>>& start,
                       RemapMomentumResult& output, std::string& error) const;

private:
    struct Link {
        std::size_t donor, receiver;
        double mass;
        bool reversed;
    };
    struct Block {
        std::vector<std::size_t> nodes, pivots;
        std::vector<double> factors;
    };
    struct MixingLedger {
        double before = 0, after = 0, loss = 0;
    };

    friend bool BuildConservativeRemap(const ConservativeRemapInput&, ConservativeRemap&, std::string&);

    bool Apply(const std::vector<double>& start, RemapScalarResult& output,
               MixingLedger* ledger, std::string& error) const;

    std::vector<double> start_, end_, incoming_, outgoing_;
    std::vector<Link> links_;
    std::vector<std::vector<std::size_t>> arrivals_, departures_;
    std::vector<bool> relay_;
    std::vector<Block> blocks_;
    double tolerance_ = 0;
};

// Cyclic relay blocks use small pivoted dense solves. Oversized or numerically
// unresolved blocks require a smaller transport interval. Failed builds and
// applications leave their supplied output unchanged.
bool BuildConservativeRemap(const ConservativeRemapInput& input, ConservativeRemap& output,
                            std::string& error);

} // namespace judas::volume_grid
