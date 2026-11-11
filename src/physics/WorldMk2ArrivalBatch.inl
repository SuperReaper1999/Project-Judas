// Connected contacts at a recurrent arrival share one compression solve.
// Included in the MK2 solver TU to reuse its authoritative row primitives.
bool PhysicsWorld::Impl::Mk2ResolveArrivalBatch(unsigned fast, unsigned other, int partFast,
                                                int partOther, double hit,
                                                std::vector<unsigned> &changed,
                                                const DynamicAabbTree &futureIndex) {
    std::optional<PhysicsWorkTimer> discovery;
    discovery.emplace(work.arrivalDiscovery, work.active);
    struct Patch {
        unsigned a, b;
        int pa, pb;
        ContactManifold manifold;
    };
    std::vector<Patch> patches;
    std::vector<unsigned> slots;
    std::vector<char> member(bodies.size(), 0);
    std::set<std::array<unsigned, 4>> seen;
    const auto dynamic = [&](unsigned slot) {
        const auto &b = bodies[slot];
        return b.isDynamic && !b.rigidBody.IsStatic() && !b.isKinematic && !b.simulationSuspended;
    };
    const auto addSlot = [&](unsigned slot) {
        if (!member[slot]) {
            member[slot] = 1;
            slots.push_back(slot);
        }
    };
    const auto addPair = [&](unsigned a, unsigned b, int pa, int pb, bool primary) {
        if (a > b) {
            std::swap(a, b);
            std::swap(pa, pb);
        }
        if (!seen.insert({a, b, unsigned(pa), unsigned(pb)}).second)
            return;
        if (!primary) {
            const auto boundA = ActualImpactBound(a, ImpactPoseAt(a, hit));
            const auto boundB = ActualImpactBound(b, ImpactPoseAt(b, hit));
            if (!boundA.Expanded(2 * float(kMk2Slop)).Overlaps(boundB))
                return;
        }
        // Detection and response must agree on the actual contact features.
        // A proximity manifold may select a different face even when the
        // zero-margin pair is already touching. Use margins only to recover a
        // just-before-touch root or a neighbouring contact within MK2's slop.
        auto m = PairAt(a, b, pa, pb, hit, 0);
        if (!m.count && !m.uncertain)
            m = PairAt(a, b, pa, pb, hit, primary ? 1e-3f : kMk2Slop);
        if (primary && !m.count && !m.uncertain)
            m = PairAt(a, b, pa, pb, hit, 5e-3f);
        if (primary && !m.count && !m.uncertain)
            m = PairAt(a, b, pa, pb, hit, float(kMk2Slop) * 20);
        if (!m.count || m.uncertain)
            return;
        patches.push_back({a, b, pa, pb, m});
        addSlot(a);
        addSlot(b);
    };
    addPair(fast, other, partFast, partOther, true);
    if (patches.empty())
        return false;
    // A static floor is a boundary, never a bridge joining unrelated islands.
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto a = slots[i];
        if (!dynamic(a))
            continue;
        const auto bound = RemainingImpactBound(a, hit, stepDuration);
        const auto found = ImpactCandidates(a, bound);
        std::vector<unsigned> nearby(found.begin(), found.end());
        futureIndex.Query(bound, [&](int proxy) { nearby.push_back(futureIndex.UserData(proxy)); });
        std::sort(nearby.begin(), nearby.end());
        nearby.erase(std::unique(nearby.begin(), nearby.end()), nearby.end());
        for (auto b : nearby) {
            if (a == b || !CanRespond(a, b) || bodies[b].sensor || bodies[b].queryOnly)
                continue;
            for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb)
                    addPair(a, b, pa, pb, false);
        }
    }
    discovery.reset();
    PhysicsWorkTimer solving(work.arrivalSolve, work.active);
    if (work.active) {
        ++work.arrivalBatches;
        work.arrivalPatches += patches.size();
        work.arrivalBodies += slots.size();
    }
    std::sort(slots.begin(), slots.end());
    std::sort(patches.begin(), patches.end(), [](const auto &a, const auto &b) {
        return std::tie(a.a, a.b, a.pa, a.pb) < std::tie(b.a, b.b, b.pa, b.pb);
    });
    std::vector<unsigned> indices(bodies.size());
    std::vector<RigidBody> copies;
    copies.reserve(slots.size());
    for (auto slot : slots) {
        indices[slot] = unsigned(copies.size());
        copies.push_back(PoseAt(slot, hit));
    }
    std::vector<SolverBody> participants(slots.size());
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const auto slot = slots[i];
        auto &p = participants[i];
        p.rb = &copies[i];
        p.dynamic = dynamic(slot);
        if (!p.dynamic) {
            p.rb->inverseMass = 0;
            p.rb->inverseInertiaLocal = glm::mat3(0);
            if (!bodies[slot].isKinematic)
                p.rb->linearVelocity = p.rb->angularVelocity = glm::vec3(0);
        }
        p.invMass = p.rb->inverseMass;
        p.invI = p.rb->ResponseInertiaWorld();
    }
    std::vector<Row> rows;
    std::vector<Block> blocks;
    std::vector<float> bounce;
    for (const auto &patch : patches) {
        Block block;
        block.first = rows.size();
        float approach = 0;
        for (int k = 0; k < patch.manifold.count; ++k) {
            auto c = patch.manifold.points[k];
            Row row{};
            row.a = indices[patch.a];
            row.b = indices[patch.b];
            row.arriving = true;
            const auto arm = [&](const RigidBody &rb, bool first) {
                if (!c.hasLocalAnchors)
                    return glm::vec3(glm::dvec3(c.point) - glm::dvec3(rb.position));
                const auto local = first ? c.localAnchorA : c.localAnchorB;
                return glm::vec3((first ? c.anchorAInWorldFrame : c.anchorBInWorldFrame)
                                     ? local
                                     : ContactRotation(rb.orientation) * local);
            };
            row.rA = row.rA0 = arm(copies[row.a], true);
            row.rB = row.rB0 = arm(copies[row.b], false);
            row.n = c.normal;
            row.friction = std::sqrt(std::max(0.f, bodies[patch.a].friction) *
                                     std::max(0.f, bodies[patch.b].friction));
            row.restitution = std::max(bodies[patch.a].restitution, bodies[patch.b].restitution);
            ContactTangents(copies[row.a], copies[row.b], row);
            PrepareRow(participants[row.a], participants[row.b], row);
            approach = std::min(approach, RowNormalSpeed(participants, row));
            rows.push_back(row);
        }
        block.count = rows.size() - block.first;
        block.usable = block.count >= 2 && block.count <= 4;
        for (std::size_t k = 0; k < block.count; ++k)
            block.usable =
                block.usable && glm::dot(rows[block.first + k].n, rows[block.first].n) > .99999f;
        PrepareBlock(participants, rows, block);
        blocks.push_back(block);
        bounce.push_back(approach < -kMk2RestitutionThreshold ? rows[block.first].restitution : 0);
    }
    const auto initial = copies;
    const auto energy = [&] {
        double value = 0;
        for (const auto &p : participants)
            if (p.dynamic) {
                if (p.rb->IsPlanar()) {
                    value += p.rb->PlanarEnergy();
                    continue;
                }
                const glm::dvec3 v(p.rb->linearVelocity), w(p.rb->angularVelocity);
                value += .5 * glm::dot(v, v) / p.invMass +
                         .5 * glm::dot(w, glm::inverse(glm::dmat3(p.invI)) * w);
            }
        return value;
    };
    const double incoming = energy();
    const auto solve = [&](bool frictionless) {
        std::copy(initial.begin(), initial.end(), copies.begin());
        for (auto &r : rows) {
            r.normalImpulse = r.tangent1 = r.tangent2 = 0;
            if (frictionless)
                r.friction = 0;
        }
        // Compression is coupled across all touching patches, with impulses
        // retained between sweeps rather than replaying restitution per event.
        for (std::size_t iteration = 0; iteration < std::max<std::size_t>(64, blocks.size() * 4);
             ++iteration) {
            if (work.active)
                ++work.arrivalSweeps;
            for (auto &block : blocks)
                SolveBlock(participants, rows, block, 1, false, true);
            bool approaching = false;
            for (const auto &r : rows)
                approaching |= RowNormalSpeed(participants, r) < -kMk2ImpactApproach;
            if (iteration + 1 >= kMk2ImpactSweeps && !approaching)
                break;
        }
        for (std::size_t k = 0; k < blocks.size(); ++k) {
            const auto &b = blocks[k];
            for (std::size_t n = 0; n < b.count; ++n) {
                auto &r = rows[b.first + n];
                const float extra = bounce[k] * r.normalImpulse;
                ApplyAxis(participants[r.a], participants[r.b], r.n, r.wnA, r.wnB, extra);
                r.normalImpulse += extra;
            }
        }
        // Expansion at one patch can close a neighbouring patch. Resolve that
        // secondary compression in the same frozen contact set, without
        // applying restitution a second time or returning to the CCD frontier.
        bool secondary = false;
        for (const auto &r : rows)
            secondary |= RowNormalSpeed(participants, r) < -kMk2ImpactApproach;
        if (secondary) {
            std::vector<glm::vec3> delivered;
            delivered.reserve(rows.size());
            for (auto &r : rows) {
                delivered.emplace_back(r.normalImpulse, r.tangent1, r.tangent2);
                r.normalImpulse = r.tangent1 = r.tangent2 = 0;
            }
            for (std::size_t iteration = 0;
                 iteration < std::max<std::size_t>(64, blocks.size() * 4); ++iteration) {
                if (work.active)
                    ++work.arrivalSweeps;
                for (auto &block : blocks)
                    SolveBlock(participants, rows, block, 1, false, true);
                bool approaching = false;
                for (const auto &r : rows)
                    approaching |= RowNormalSpeed(participants, r) < -kMk2ImpactApproach;
                if (iteration + 1 >= kMk2ImpactSweeps && !approaching)
                    break;
            }
            for (std::size_t n = 0; n < rows.size(); ++n) {
                auto &r = rows[n];
                r.normalImpulse += delivered[n].x;
                r.tangent1 += delivered[n].y;
                r.tangent2 += delivered[n].z;
            }
        }
        double boundaryWork = 0;
        for (const auto &r : rows) {
            const auto impulse = r.n * r.normalImpulse + r.t1 * r.tangent1 + r.t2 * r.tangent2;
            if (!participants[r.a].dynamic)
                boundaryWork -=
                    glm::dot(impulse, copies[r.a].linearVelocity +
                                          glm::cross(copies[r.a].angularVelocity, r.rA));
            if (!participants[r.b].dynamic)
                boundaryWork +=
                    glm::dot(impulse, copies[r.b].linearVelocity +
                                          glm::cross(copies[r.b].angularVelocity, r.rB));
        }
        return energy() <= incoming + boundaryWork +
                               64 * std::numeric_limits<float>::epsilon() *
                                   std::max({1., incoming, std::abs(boundaryWork)});
    };
    if (!solve(false)) {
        if (work.active)
            ++work.arrivalRetries;
        if (!solve(true))
            return false;
    }
    std::vector<unsigned char> loaded(slots.size(), 0);
    for (const auto &row : rows)
        if (row.normalImpulse != 0 || row.tangent1 != 0 || row.tangent2 != 0)
            loaded[row.a] = loaded[row.b] = 1;
    for (std::size_t i = 0; i < slots.size(); ++i)
        if (participants[i].dynamic &&
            (slots[i] == fast || slots[i] == other || (loaded[i] && bodies[slots[i]].sleeping) ||
             copies[i].linearVelocity != initial[i].linearVelocity ||
             copies[i].angularVelocity != initial[i].angularVelocity)) {
            const auto slot = slots[i];
            if (bodies[slot].sleeping) {
                Wake(slot);
                mk2ContinuousAwakened = true;
            }
            bodies[slot].rigidBody = copies[i];
            changed.push_back(slot);
        }
    if (work.active && std::getenv("JUDAS_MK2_ARRIVAL_TRUTH_PROFILE")) {
        bool unchanged = true;
        for (std::size_t n = 0; n < copies.size(); ++n)
            unchanged = unchanged && copies[n].linearVelocity == initial[n].linearVelocity &&
                        copies[n].angularVelocity == initial[n].angularVelocity;
        work.arrivalUnchanged += unchanged;
        for (const auto &patch : patches) {
            const auto manifold = PairAt(patch.a, patch.b, patch.pa, patch.pb, hit, 0);
            const auto &aa = copies[indices[patch.a]];
            const auto &bb = copies[indices[patch.b]];
            for (int k = 0; k < manifold.count; ++k) {
                const auto &c = manifold.points[k];
                const auto arm = [&](const RigidBody &b, bool first) {
                    if (!c.hasLocalAnchors)
                        return glm::dvec3(c.point) - glm::dvec3(b.position);
                    const auto local = first ? c.localAnchorA : c.localAnchorB;
                    return (first ? c.anchorAInWorldFrame : c.anchorBInWorldFrame)
                               ? local
                               : ContactRotation(b.orientation) * local;
                };
                const auto va = glm::dvec3(aa.linearVelocity) +
                                glm::cross(glm::dvec3(aa.angularVelocity), arm(aa, true));
                const auto vb = glm::dvec3(bb.linearVelocity) +
                                glm::cross(glm::dvec3(bb.angularVelocity), arm(bb, false));
                const auto approach = std::max(0., -glm::dot(va - vb, glm::dvec3(c.normal)));
                work.arrivalTruthResidualRows += approach > kMk2ImpactApproach;
                work.arrivalTruthResidualApproach =
                    std::max(work.arrivalTruthResidualApproach, approach);
            }
        }
    }
    if (work.active)
        for (const auto &r : rows) {
            const auto approach = std::max(0.f, -RowNormalSpeed(participants, r));
            work.arrivalResidualRows += approach > kMk2ImpactApproach;
            work.arrivalResidualApproach = std::max(work.arrivalResidualApproach, double(approach));
        }
    ++stats.impactResponseAttempts;
    ++stats.impactResponsesApplied;
    for (std::size_t k = 0; k < patches.size(); ++k) {
        const auto &p = patches[k];
        const auto &block = blocks[k];
        for (std::size_t n = 0; n < block.count; ++n) {
            const auto &r = rows[block.first + n];
            auto c = p.manifold.points[n];
            Observe(p.a, p.b, c, r.normalImpulse);
            const auto handleA = MakeHandle(p.a).id, handleB = MakeHandle(p.b).id;
            const auto handles = std::minmax(handleA, handleB);
            stepTouches[handles].impulseAvailable = true;
            CachedContact observation;
            observation.key = {p.a,  p.b, bodies[p.a].generation, bodies[p.b].generation,
                               p.pa, p.pb};
            observation.localAnchorA =
                glm::transpose(ContactRotation(copies[r.a].orientation)) * glm::dvec3(r.rA);
            observation.normal = r.n;
            observation.normalImpulse = r.normalImpulse;
            observation.tangentImpulse = r.t1 * r.tangent1 + r.t2 * r.tangent2;
            observation.observationRevisionA = bodies[p.a].contactObservationRevision;
            observation.observationRevisionB = bodies[p.b].contactObservationRevision;
            mk2ImpactObservations.push_back(observation);
        }
    }
    return true;
}
