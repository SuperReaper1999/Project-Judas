#pragma once
#include "WorldInternal.h"

// Private event transaction. Body addresses stay stable as rows are appended;
// nothing here is published to the authoritative world until closure succeeds.
struct PhysicsWorld::Impl::IncrementalImpact {
    std::vector<RigidBody> bodies, incoming;
    std::vector<std::pair<glm::vec3, glm::vec3>> original;
    std::vector<unsigned char> initialized, expanded, gravityConsumed;
    std::vector<unsigned> slots, forcedExpansion;
    ImpactPairVisitSet visited;
    bool Visit(unsigned a,unsigned b) {
        const auto key=(std::uint64_t(a)<<32)|b;
        if(visited.Contains(key))return false;
        visited.Insert(key);return true;
    }
    std::vector<ImpactContact> rows;
    std::vector<JudasPhysicsDetail::ContactKey> keys;
    std::vector<std::array<unsigned, 4>> pairs;
    // Declared after body storage: prepared pointers die before their bodies.
    ImpactSolver solver;
    ImpactResult result;
    bool capture = false;
    void Reset(std::size_t size) {
        // Clear every prepared row/frame before a resize can relocate bodies.
        // Nothing from the previous event is a physical continuation cache.
        solver.Clear();
        rows.clear();
        keys.clear();
        pairs.clear();
        bodies.resize(size);
        incoming.resize(size);
        original.resize(size);
        initialized.assign(size, 0);
        expanded.assign(size, 0);
        gravityConsumed.assign(size, 0);
        slots.clear();
        forcedExpansion.clear();
        visited.Begin(size);
        result = {};
        capture = false;
    }
};
