#pragma once

#include "Broadphase.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

// Derived overlap data only. An edge says that two current FAT PROXIES overlap;
// it never certifies geometry, response, sleeping, filters or contact identity.
// The caller supplies all live proxies on each update and filters edges fresh.
// One changed proxy invalidates all its incident edges; unrelated edges survive.
class BroadphasePairCache {
public:
    using Pair = std::pair<unsigned, unsigned>;
    struct Proxy {
        unsigned slot = 0, generation = 0;
        int proxy = DynamicAabbTree::kNull;
        Aabb bound;
        bool queryEligible = false;
    };
    struct Diagnostics {
        std::size_t changed = 0, treeQueries = 0, visitedLeaves = 0;
        bool reused = false, overflow = false;
    };
    explicit BroadphasePairCache(std::size_t maxPairs = 1u << 20,
                                 std::size_t maxSlots = 1u << 20)
        : maxPairs_(maxPairs), maxSlots_(maxSlots) {}

    template<class Query>
    bool Update(const std::vector<Proxy> &proxies, Query &&query) {
        diagnostics_ = {};
        if (disabled_) return false;
        for (unsigned slot : changed_) changedMarks_[slot] = false;
        changed_.clear();
        if (++epoch_ == 0) { Disable(); return false; }
        std::size_t required = 0;
        for (const auto &proxy : proxies) {
            if (std::size_t(proxy.slot) >= maxSlots_ || !Finite(proxy.bound)) {
                Disable(); return false;
            }
            required = std::max(required, std::size_t(proxy.slot) + 1);
        }
        if (required > states_.size()) {
            states_.resize(required);
            changedMarks_.resize(required, false);
        }
        nextActive_.clear();
        bool anyEligible = false;
        for (const auto &proxy : proxies) {
            auto &state = states_[proxy.slot];
            if (!state.live || state.value.generation != proxy.generation ||
                state.value.proxy != proxy.proxy ||
                state.value.queryEligible != proxy.queryEligible ||
                !SameBound(state.value.bound, proxy.bound))
                MarkChanged(proxy.slot);
            state.value = proxy;
            anyEligible |= proxy.queryEligible;
            state.live = true;
            state.seen = epoch_;
            nextActive_.push_back(proxy.slot);
        }
        for (unsigned slot : active_) {
            auto &state = states_[slot];
            if (state.seen != epoch_) { state.live = false; MarkChanged(slot); }
        }
        active_.swap(nextActive_);
        diagnostics_.changed = changed_.size();
        if (!anyEligible) { pairs_.clear(); return true; }
        if (changed_.empty()) { diagnostics_.reused = true; return true; }

        pairs_.erase(std::remove_if(pairs_.begin(), pairs_.end(), [&](const Pair &pair) {
            return changedMarks_[pair.first] || changedMarks_[pair.second];
        }), pairs_.end());
        additions_.clear();
        bool overflow = false;
        for (unsigned slot : changed_) {
            const auto &state = states_[slot];
            if (!state.live) continue;
            ++diagnostics_.treeQueries;
            query(state.value.bound, [&](unsigned other) {
                ++diagnostics_.visitedLeaves;
                if (overflow || other == slot || other >= states_.size()) return;
                const auto &neighbor = states_[other];
                if (!neighbor.live || neighbor.seen != epoch_ ||
                    (!state.value.queryEligible && !neighbor.value.queryEligible)) return;
                // Both changed endpoints query the same overlap. Keep only
                // its lower-slot discovery; untouched endpoints query nothing.
                if (changedMarks_[other] && other < slot) return;
                if (pairs_.size() + additions_.size() >= maxPairs_) {
                    overflow = true; return;
                }
                additions_.emplace_back(std::min(slot, other), std::max(slot, other));
            });
            if (overflow) { Disable(); diagnostics_.overflow = true; return false; }
        }
        std::sort(additions_.begin(), additions_.end());
        additions_.erase(std::unique(additions_.begin(), additions_.end()), additions_.end());
        const auto oldSize = pairs_.size();
        pairs_.insert(pairs_.end(), additions_.begin(), additions_.end());
        std::inplace_merge(pairs_.begin(), pairs_.begin() + oldSize, pairs_.end());
        return true;
    }
    const std::vector<Pair> &Pairs() const { return pairs_; }
    const Diagnostics &Stats() const { return diagnostics_; }
    bool Disabled() const { return disabled_; }
    std::size_t StorageBytes() const {
        return states_.capacity()*sizeof(State) + changedMarks_.capacity()*sizeof(unsigned char) +
               (active_.capacity()+nextActive_.capacity()+changed_.capacity())*sizeof(unsigned) +
               (pairs_.capacity()+additions_.capacity())*sizeof(Pair);
    }
    void Clear() {
        states_.clear(); changedMarks_.clear(); active_.clear(); nextActive_.clear(); changed_.clear();
        pairs_.clear(); additions_.clear(); epoch_=0; disabled_=false; diagnostics_={};
    }
private:
    struct State { Proxy value; std::uint64_t seen=0; bool live=false; };
    static bool Finite(const Aabb &bound) {
        for(unsigned k=0;k<3;++k)
            if(!std::isfinite(bound.min[k]) || !std::isfinite(bound.max[k])) return false;
        return true;
    }
    static bool SameBound(const Aabb &a,const Aabb &b) {
        for(unsigned k=0;k<3;++k)
            if(std::memcmp(&a.min[k],&b.min[k],sizeof(float)) ||
               std::memcmp(&a.max[k],&b.max[k],sizeof(float))) return false;
        return true;
    }
    void MarkChanged(unsigned slot) {
        if(!changedMarks_[slot]) { changedMarks_[slot]=true; changed_.push_back(slot); }
    }
    void Disable() { disabled_=true; pairs_.clear(); additions_.clear(); }
    std::size_t maxPairs_,maxSlots_;
    bool disabled_=false;
    std::uint64_t epoch_=0;
    std::vector<State> states_;
    std::vector<unsigned char> changedMarks_;
    std::vector<unsigned> active_,nextActive_,changed_;
    std::vector<Pair> pairs_,additions_;
    Diagnostics diagnostics_;
};
