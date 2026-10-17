#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

// Exact unordered-pair membership, with no encounter-order policy. Impact
// discovery still owns its ordered rows. Compact worlds avoid allocating one
// hash node per pair per event; large/sparse slot spaces retain the bounded-
// memory hash path. Capacity is reused only inside one fixed-step event pass.
class ImpactPairVisitSet {
public:
    void Begin(std::size_t slots) {
        compact = slots <= kCompactSlots;
        if (compact) {
            const auto pairs = slots * (slots - (slots != 0)) / 2;
            bits.assign((pairs + 63) / 64, 0);
        } else {
            fallback.clear();
        }
    }
    bool Contains(std::uint64_t key) const {
        if (!compact) return fallback.count(key) != 0;
        const auto index = Index(key);
        return (bits[index / 64] & (std::uint64_t(1) << (index % 64))) != 0;
    }
    void Insert(std::uint64_t key) {
        if (!compact) { fallback.insert(key); return; }
        const auto index = Index(key);
        bits[index / 64] |= std::uint64_t(1) << (index % 64);
    }
    std::size_t CompactBytes() const { return bits.capacity() * sizeof(std::uint64_t); }
private:
    static constexpr std::size_t kCompactSlots = 4096; // at most 1 MiB
    static std::uint64_t Index(std::uint64_t key) {
        const auto first = key >> 32, second = key & 0xffffffffu;
        return second * (second - 1) / 2 + first; // caller supplies first < second
    }
    bool compact = true;
    std::vector<std::uint64_t> bits;
    std::unordered_set<std::uint64_t> fallback;
};
