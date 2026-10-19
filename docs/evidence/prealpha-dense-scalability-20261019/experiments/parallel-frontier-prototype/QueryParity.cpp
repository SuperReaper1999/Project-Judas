// Isolated Linux-only private-kernel experiment; no shipped API/test changes.
#include <bits/stdc++.h>
#include <cfenv>
#define private public
#include "WorldInternal.h"
#undef private
#include "ParallelImpactQueries.h"

namespace {
struct Query {
    unsigned a, b;
    int pa = 0, pb = 0;
    double start = 0, finish = 1.0 / 60.0, target = 0;
};
bool Same(double a, double b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
std::shared_ptr<CollisionAsset> Sheet() {
    MeshData mesh;
    for (glm::vec3 p : {glm::vec3(-2, 0, -2), glm::vec3(-2, 0, 2),
                        glm::vec3(2, 0, 2), glm::vec3(2, 0, -2)}) {
        MeshVertex v; v.position = p; mesh.vertices.push_back(v);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    auto asset = std::make_shared<CollisionAsset>();
    CollisionCookSettings settings; std::string error;
    if (!CookCollision(mesh, settings, *asset, error)) throw std::runtime_error(error);
    return asset;
}
}

int main(int argc, char**) {
    const bool shared = argc > 1;
    PhysicsWorld world; world.Init();
    auto& state = *world.m_impl;
    std::vector<Query> queries;
    const auto sheet = Sheet();
    constexpr float dt = 1.f / 60;
    for (unsigned i = 0; i < 192; ++i) {
        const unsigned kind = i % 6;
        const glm::vec3 origin = i % 17 == 0 ? glm::vec3(1200, -700, 45) : glm::vec3(0);
        const glm::quat rotation = glm::angleAxis(float(i % 13) * .11f,
                                                  glm::normalize(glm::vec3(1, 2, -3)));
        const float gap = i % 4 == 0 ? .001f : i % 4 == 1 ? .2f : i % 4 == 2 ? 2.f : -.01f;
        const glm::vec3 offset = rotation * glm::vec3(0, 1.f + gap, 0);
        Shape a = kind == 0 || kind == 2 ? Shape::Sphere(.5f) : Shape::Box(glm::vec3(.5f));
        Shape b = kind == 0 ? Shape::Sphere(.5f) : Shape::Box(glm::vec3(.5f));
        BodyTransform aPose{origin + offset, rotation}, bPose{origin, rotation};
        if (kind == 4) {
            CompoundBox c; c.localCenter = glm::vec3(.2f, 0, 0); c.halfExtents = glm::vec3(.5f);
            a = Shape::Compound({c});
        }
        if (kind == 5) {
            b = Shape::Cooked(sheet, "query-parity-sheet");
            aPose.position = origin + rotation * glm::vec3(0, .5f + gap, 0);
        }
        auto ah = world.CreateShape(a, aPose, true, 1, .5f, 0);
        auto bh = world.CreateShape(b, bPose, kind != 5 && i % 3 != 0, 1, .5f, 0);
        auto ai = ah.id & state.kSlotMask, bi = bh.id & state.kSlotMask;
        auto& ba = state.bodies[ai]; auto& bb = state.bodies[bi];
        ba.rigidBody.linearVelocity = rotation * glm::vec3(float(int(i % 3) - 1) * .2f,
                                                           i % 5 == 0 ? 0.f : -35.f, 0);
        ba.rigidBody.angularVelocity = rotation * glm::vec3(i % 7 == 0 ? 18.f : 0,
                                                           i % 11 == 0 ? 9.f : 0, 0);
        if (bb.isDynamic) bb.rigidBody.linearVelocity = rotation * glm::vec3(0, 1, .2f);
        if (i % 19 == 0 && kind != 5) {
            bb.isDynamic = false; bb.isKinematic = true;
            bb.rigidBody.linearVelocity = rotation * glm::vec3(0, 3, 0);
            bb.motion.BeginPrescribed(bh.id, bb.rigidBody, dt, dt * .6f);
        }
        ba.motion.Begin(ah.id, ba.rigidBody, dt);
        if (bb.isDynamic) bb.motion.Begin(bh.id, bb.rigidBody, dt);
        for (bool reverse : {false, true}) for (double start : {0., double(dt) * .15}) {
            Query q{reverse ? bi : ai, reverse ? ai : bi, 0, 0, start, dt, 0};
            queries.push_back(q);
            q.target = .025; queries.push_back(q);
        }
    }
    state.stepDuration = dt;
    std::vector<double> serial(queries.size()), parallel(queries.size());
    std::size_t hits = 0, misses = 0;
    for (std::size_t i = 0; i < queries.size(); ++i) {
        const auto& q = queries[i]; state.impactQueryAnchorTime = q.start;
        serial[i] = state.ImpactTime(q.a, q.b, q.pa, q.pb, q.start, q.finish, q.target);
        (serial[i] <= q.finish ? hits : misses)++;
    }
    JobSystem jobs(3);
    std::vector<std::unique_ptr<PhysicsWorld::Impl::ImpactQueryContext>> lanes;
    for (unsigned lane = 0; lane < 4; ++lane) lanes.push_back(
        std::make_unique<PhysicsWorld::Impl::ImpactQueryContext>(state, shared));
    int failures = 0;
    for (int repetition = 0; repetition < 3; ++repetition) {
        std::array<std::size_t, 4> counts{};
        for (unsigned partition = 0; partition < (shared ? 2u : 1u); ++partition) {
        const double anchor = partition ? double(dt) * .15 : 0.;
        PhysicsWorld::Impl::ImpactQueryContext::SharedInputs inputs;
        if (shared) {
            inputs.anchors.resize(state.bodies.size());
            inputs.remainingBounds.resize(state.bodies.size());
            inputs.tightBounds.resize(state.bodies.size());
            inputs.anchorErrors.resize(state.bodies.size());
            inputs.remainingErrors.resize(state.bodies.size());
            inputs.tightErrors.resize(state.bodies.size());
            state.impactQueryAnchorTime = anchor;
            for (unsigned slot : state.aliveSlots) {
                inputs.anchors[slot] = &state.ImpactPoseAt(slot, anchor);
                inputs.remainingBounds[slot] = &state.RemainingImpactBound(slot, anchor, dt);
                inputs.tightBounds[slot] = &state.ActualImpactBound(slot, *inputs.anchors[slot]);
            }
        }
        std::atomic<std::size_t> next{0};
        auto run = [&](unsigned lane) {
            auto& context = *lanes[lane]; context.Reset(anchor);
            context.shared = shared ? &inputs : nullptr;
            for (;;) {
                auto i = next.fetch_add(1);
                if (i >= queries.size()) break;
                if (shared && queries[i].start != anchor) continue;
                const auto& q = queries[i]; context.impactQueryAnchorTime = q.start;
                parallel[i] = context.ImpactTime(q.a, q.b, q.pa, q.pb, q.start, q.finish, q.target);
                ++counts[lane];
            }
        };
        std::array<JobHandle, 3> handles;
        for (unsigned lane = 1; lane < 4; ++lane) handles[lane - 1] = jobs.Submit(
            [&, lane](JobContext&) { run(lane); }, JobPriority::High, "query parity");
        run(0);
        for (auto handle : handles) {
            if (jobs.Wait(handle) != JobState::Completed) {
                std::cerr << "worker failure " << jobs.ErrorOf(handle) << '\n'; ++failures;
            }
            jobs.Forget(handle);
        }
        }
        for (std::size_t i = 0; i < queries.size(); ++i) if (!Same(serial[i], parallel[i])) {
            if (failures < 20) std::cerr << "TOI mismatch " << i << ' ' << std::setprecision(17)
                                       << serial[i] << ' ' << parallel[i] << '\n';
            ++failures;
        }
        std::cout << "{\"shared\":" << shared << ",\"repetition\":" << repetition << ",\"queries\":" << queries.size()
                  << ",\"laneQueries\":[" << counts[0] << ',' << counts[1] << ',' << counts[2]
                  << ',' << counts[3] << "],\"failures\":" << failures << "}\n";
    }
    if (!hits || !misses) ++failures;
    std::cout << "{\"hits\":" << hits << ",\"misses\":" << misses
              << ",\"failures\":" << failures << "}\n";
    return failures ? 1 : 0;
}
