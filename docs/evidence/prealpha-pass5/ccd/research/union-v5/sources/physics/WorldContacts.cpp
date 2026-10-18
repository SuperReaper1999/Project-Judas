#include "WorldInternal.h"

using namespace JudasPhysicsDetail;

auto PhysicsWorld::Impl::Observe(unsigned a, unsigned b, const Contact &c, float impulse,
                                 bool preStep) -> void {
    auto ha = MakeHandle(a), hb = MakeHandle(b);
    bool flip = ha.id > hb.id;
    if (flip)
        std::swap(ha, hb);
    auto key = std::make_pair(ha.id, hb.id);
    auto &e = (preStep ? preStepQueryTouches : stepTouches)[key];
    e.a = ha;
    e.b = hb;
    e.sensor = bodies[a].sensor || bodies[b].sensor;
    e.point = c.point;
    e.normal = flip ? -c.normal : c.normal;
    const auto va =
        bodies[a].rigidBody.linearVelocity +
        glm::cross(bodies[a].rigidBody.angularVelocity, c.point - bodies[a].rigidBody.position);
    const auto vb =
        bodies[b].rigidBody.linearVelocity +
        glm::cross(bodies[b].rigidBody.angularVelocity, c.point - bodies[b].rigidBody.position);
    e.relativeVelocity = flip ? va - vb : vb - va;
    e.normalImpulse += impulse;
}

auto PhysicsWorld::Impl::FinishTouches() -> void {
    touchEvents.clear();
    // Sensor overlap is discrete endpoint geometry, never speculative skin
    // or TOI response. Reuse the same tree, layer policy and narrowphase.
    if (std::any_of(aliveSlots.begin(), aliveSlots.end(), [&](unsigned i) {
            return (bodies[i].sensor || bodies[i].isKinematic) && bodies[i].enabled;
        })) {
        GenerateCandidatePairs();
        for (auto [a, b] : candidatePairs)
            if (bodies[a].sensor || bodies[b].sensor ||
                ((bodies[a].isKinematic || bodies[b].isKinematic) &&
                 bodies[a].rigidBody.IsStatic() && bodies[b].rigidBody.IsStatic())) {
                const auto &oa = Orientation(bodies[a]);
                const auto &ob = Orientation(bodies[b]);
                for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                    for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                        auto m = ComputeContacts(
                            PrimitiveAt(bodies[a].shape, bodies[a].rigidBody, pa, &oa),
                            PrimitiveAt(bodies[b].shape, bodies[b].rigidBody, pb, &ob), 0, &oa,
                            &ob);
                        if (m.count)
                            Observe(a, b, m.points[0]);
                    }
            }
    }
    for (auto &[key, e] : stepTouches) {
        const auto old = previousTouches.find(key);
        if (old != previousTouches.end() && old->second.sensor != e.sensor) {
            auto exit = old->second;
            exit.phase = PhysicsWorld::TouchPhase::Exit;
            touchEvents.push_back(exit);
        }
        e.phase = old != previousTouches.end() && old->second.sensor == e.sensor
                      ? PhysicsWorld::TouchPhase::Stay
                      : PhysicsWorld::TouchPhase::Enter;
        touchEvents.push_back(e);
    }
    for (auto &[key, e] : previousTouches)
        if (!stepTouches.count(key)) {
            auto exit = e;
            exit.phase = PhysicsWorld::TouchPhase::Exit;
            touchEvents.push_back(exit);
        }
    std::stable_sort(touchEvents.begin(), touchEvents.end(), [](const auto &a, const auto &b) {
        return std::tie(a.a.id, a.b.id) < std::tie(b.a.id, b.b.id);
    });
    previousTouches = stepTouches;
}

auto PhysicsWorld::Impl::PrepareInitialContactGeometry(float dt) -> void {
    initialContactGeometry.clear();
    JobSystem *jobs = contactJobs;
    JUDAS_PROFILE_COUNTER("Physics initial geometry worker lanes", 0, ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics parallel initial manifolds", 0, ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics initial worker completed manifolds", 0,
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics initial geometry scratch bytes",
                          initialContactGeometry.capacity() * sizeof(InitialContactGeometry),
                          ProfileCounterMode::Latest);
    if (!jobs || jobs->IsShutDown() || contactJobLanes <= 1 || candidatePairs.size() < 256)
        return;
    const auto eligible = [&](const std::pair<unsigned, unsigned> &pair) {
        const auto &a = bodies[pair.first];
        const auto &b = bodies[pair.second];
        const auto simple = [](const Shape &shape) {
            return !shape.asset &&
                   (shape.type == ShapeType::Box || shape.type == ShapeType::Sphere);
        };
        return !a.sensor && !b.sensor && !a.queryOnly && !b.queryOnly && simple(a.shape) &&
               simple(b.shape) &&
               !((a.sleeping || a.rigidBody.IsStatic()) && (b.sleeping || b.rigidBody.IsStatic()));
    };
    std::size_t count = 0;
    for (const auto &pair : candidatePairs)
        if (eligible(pair) && ++count > kMaxInitialContactPairs)
            return;
    if (count < 256)
        return;
    const auto capacity = initialContactGeometry.capacity();
    initialContactGeometry.reserve(std::size_t(count));
    if (initialContactGeometry.capacity() != capacity) {
        ++cacheCounters.allocations;
        cacheCounters.allocatedBytes +=
            initialContactGeometry.capacity() * sizeof(InitialContactGeometry);
    }
    for (const auto &pair : candidatePairs)
        if (eligible(pair)) {
            auto &a = bodies[pair.first];
            auto &b = bodies[pair.second];
            const float margin =
                (glm::length(a.rigidBody.linearVelocity - b.rigidBody.linearVelocity) +
                 glm::length(a.rigidBody.angularVelocity) * a.boundingRadius +
                 glm::length(b.rigidBody.angularVelocity) * b.boundingRadius) *
                dt;
            InitialContactGeometry output;
            output.a = pair.first;
            output.b = pair.second;
            output.margin = margin;
            output.orientationA = &Orientation(a);
            output.orientationB = &Orientation(b);
            initialContactGeometry.push_back(std::move(output));
        }
    // Do not inspect the pool's worker vector while Shutdown may join and
    // clear it. The borrowed pool bounds execution; unused queued jobs are
    // cancelled below after the owner exhausts the range cursor.
    const unsigned lanes =
        unsigned(std::min(std::size_t(contactJobLanes), initialContactGeometry.size() / 64));
    std::array<InitialGeometryDiagnostics, JobSystem::kMaxWorkers + 1> diagnostics{};
    std::atomic<std::size_t> next{0};
    const auto compute = [&](InitialContactGeometry &output) {
        const auto &a = bodies[output.a];
        const auto &b = bodies[output.b];
        const auto childA = PrimitiveAt(a.shape, a.rigidBody, 0, output.orientationA);
        const auto childB = PrimitiveAt(b.shape, b.rigidBody, 0, output.orientationB);
        output.manifold = ComputeContacts(childA, childB, output.margin, output.orientationA,
                                          output.orientationB);
        output.touching = false;
        for (int p = 0; p < output.manifold.count; ++p) {
            const auto &c = output.manifold.points[p];
            output.touching = output.touching ||
                              (c.hasLocalAnchors ? c.signedSeparation <= 0 : c.penetration >= 0);
        }
        if (!output.touching && output.manifold.count)
            output.touching =
                ComputeContacts(childA, childB, 0, output.orientationA, output.orientationB).count >
                0;
        output.ready = true;
    };
    auto run = [&](unsigned lane) {
        const auto contactsBefore = GetContactGeometryDiagnostics();
        const auto cookedBefore = GetCollisionGeometryStats();
        JUDAS_PROFILE_SCOPE("Physics initial geometry worker batch");
        try {
            for (;;) {
                const auto first = next.fetch_add(8, std::memory_order_relaxed);
                if (first >= initialContactGeometry.size())
                    break;
                for (auto i = first; i < std::min(first + 8, initialContactGeometry.size()); ++i) {
                    compute(initialContactGeometry[i]);
                    ++diagnostics[lane].manifolds;
                }
            }
        } catch (...) {
            AddContactDiagnostics(diagnostics[lane].contacts, GetContactGeometryDiagnostics(),
                                  contactsBefore);
            AddCookedDiagnostics(diagnostics[lane].cooked, GetCollisionGeometryStats(),
                                 cookedBefore);
            throw;
        }
        AddContactDiagnostics(diagnostics[lane].contacts, GetContactGeometryDiagnostics(),
                              contactsBefore);
        AddCookedDiagnostics(diagnostics[lane].cooked, GetCollisionGeometryStats(), cookedBefore);
    };
    std::array<JobHandle, JobSystem::kMaxWorkers> handles{};
    unsigned handleCount = 0;
    std::exception_ptr submitError;
    try {
        for (unsigned lane = 1; lane < lanes; ++lane) {
            auto handle = jobs->Submit([&, lane](JobContext &) { run(lane); }, JobPriority::High,
                                       "Physics initial contact geometry");
            if (handle.IsValid())
                handles[handleCount++] = handle;
        }
    } catch (...) {
        submitError = std::current_exception();
    }
    std::exception_ptr mainError;
    try {
        run(0);
    } catch (...) {
        mainError = std::current_exception();
    }
    // The owner has exhausted the shared range cursor. A job still queued
    // behind unrelated resource work can no longer claim useful ranges.
    // Cancel only these jobs; running jobs retain their claimed outputs.
    for (unsigned i = 0; i < handleCount; ++i)
        if (jobs->StateOf(handles[i]) == JobState::Queued)
            jobs->Cancel(handles[i]);
    JobHandle failed;
    for (unsigned i = 0; i < handleCount; ++i)
        if (jobs->Wait(handles[i]) == JobState::Failed && !failed.IsValid())
            failed = handles[i];
    std::string workerError = failed.IsValid() ? jobs->ErrorOf(failed) : std::string{};
    if (failed.IsValid() && workerError.empty())
        workerError = "initial geometry job did not complete";
    for (unsigned i = 0; i < handleCount; ++i)
        jobs->Forget(handles[i]);
    // Lane zero executes on the owner; its TLS counters are already included
    // in the native Step's before/after snapshots.
    for (unsigned lane = 1; lane < lanes; ++lane) {
        AddContactDiagnostics(parallelContactDiagnostics, diagnostics[lane].contacts);
        AddCookedDiagnostics(parallelCookedDiagnostics, diagnostics[lane].cooked);
    }
    if (submitError)
        std::rethrow_exception(submitError);
    if (mainError)
        std::rethrow_exception(mainError);
    if (!workerError.empty())
        throw std::runtime_error(workerError);
    // Shutdown may cancel jobs before they start; incomplete outputs then
    // run through the unchanged owner-thread geometry path after all joins.
    // Never fall back concurrently with a worker writing an output slot.
    for (auto &output : initialContactGeometry)
        if (!output.ready)
            compute(output);
    std::size_t workerManifolds = 0;
    for (unsigned lane = 1; lane < lanes; ++lane)
        workerManifolds += diagnostics[lane].manifolds;
    JUDAS_PROFILE_COUNTER("Physics initial worker completed manifolds", workerManifolds,
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics initial geometry worker lanes", lanes,
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics parallel initial manifolds", initialContactGeometry.size(),
                          ProfileCounterMode::Latest);
    JUDAS_PROFILE_COUNTER("Physics initial geometry scratch bytes",
                          initialContactGeometry.capacity() * sizeof(InitialContactGeometry),
                          ProfileCounterMode::Latest);
}

auto PhysicsWorld::Impl::AddContactDiagnostics(ContactGeometryDiagnostics &into,
                                               const ContactGeometryDiagnostics &after,
                                               const ContactGeometryDiagnostics &before) -> void {
#define ADD_DIAG(name) into.name += after.name - before.name
    ADD_DIAG(predicates);
    ADD_DIAG(intervalResolved);
    ADD_DIAG(exactFallbacks);
    ADD_DIAG(unresolved);
    ADD_DIAG(invalidInputs);
    ADD_DIAG(numericGapFallbacks);
    ADD_DIAG(preparedOrientationMisses);
#undef ADD_DIAG
}

auto PhysicsWorld::Impl::AddCookedDiagnostics(CollisionGeometryStats &into,
                                              const CollisionGeometryStats &after,
                                              const CollisionGeometryStats &before) -> void {
    into.nodes += after.nodes - before.nodes;
    into.triangles += after.triangles - before.triangles;
    into.convexTests += after.convexTests - before.convexTests;
}

const std::vector<PhysicsWorld::TouchEvent> &PhysicsWorld::LastStepTouchEvents() const {
    return m_impl->touchEvents;
}

void PhysicsWorld::ClearTouchHistory() {
    m_impl->previousTouches.clear();
    m_impl->stepTouches.clear();
    m_impl->preStepQueryTouches.clear();
    m_impl->collectingPreStepQueries = false;
    m_impl->queryTouchesPending = false;
    m_impl->touchEvents.clear();
}

void PhysicsWorld::BeginPreStepQueryContacts() {
    m_impl->preStepQueryTouches.clear();
    m_impl->collectingPreStepQueries = true;
}

void PhysicsWorld::ObserveQueryContact(BodyHandle h, const ShapeSweepHit &hit,
                                       const glm::vec3 &velocity) {
    auto *a = m_impl->Get(h);
    auto *b = m_impl->Get(hit.hitBody);
    if (!a || !b || !a->enabled || !a->queryOnly ||
        !m_impl->CanCollide(h.id & Impl::kSlotMask, hit.hitBody.id & Impl::kSlotMask))
        return;
    a->rigidBody.linearVelocity = velocity;
    Contact c;
    c.point = hit.point;
    c.normal = hit.normal;
    m_impl->Observe(h.id & Impl::kSlotMask, hit.hitBody.id & Impl::kSlotMask, c, 0,
                    m_impl->collectingPreStepQueries);
}

void PhysicsWorld::FinishQueryTouches() {
    if (!m_impl->collectingPreStepQueries && m_impl->queryTouchesPending) {
        m_impl->FinishTouches();
        m_impl->queryTouchesPending = false;
    }
}
