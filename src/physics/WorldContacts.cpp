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
    std::vector<TouchEvent> sensorCrossings;
    std::optional<CCDQuery> sensorQuery;
    const auto sensorStart = work.active ? PerformanceProfiler::Now() : 0;
    // Endpoint overlap owns persistent sensor state. A swept-only crossing
    // publishes an enter/exit pair without retaining a phantom overlap into
    // the next step. Search the completed motion ledger, never its uncorrected
    // pre-solve prediction, and never apply a physical sensor impulse.
    if (std::any_of(aliveSlots.begin(), aliveSlots.end(), [&](unsigned i) {
            return (bodies[i].sensor || bodies[i].isKinematic) && bodies[i].enabled;
        })) {
        GenerateCandidatePairs();
        for (auto [a, b] : candidatePairs)
            if (bodies[a].sensor || bodies[b].sensor ||
                ((bodies[a].isKinematic || bodies[b].isKinematic) &&
                 bodies[a].rigidBody.IsStatic() && bodies[b].rigidBody.IsStatic())) {
                if(!bodies[a].sensor&&!bodies[b].sensor&&!OneWayAllows(a,b))continue;
                const auto &oa = Orientation(bodies[a]);
                const auto &ob = Orientation(bodies[b]);
                bool endpointOverlap = false;
                for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                    for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                        auto m = ComputeContacts(
                            PrimitiveAt(bodies[a].shape, bodies[a].rigidBody, pa, &oa),
                            PrimitiveAt(bodies[b].shape, bodies[b].rigidBody, pb, &ob), 0, &oa,
                            &ob);
                        if (m.count) {
                            Observe(a, b, m.points[0]);
                            endpointOverlap = true;
                        }
                    }
                if (endpointOverlap || !(bodies[a].sensor || bodies[b].sensor) ||
                    bodies[a].queryOnly || bodies[b].queryOnly || !(stepDuration > 0))
                    continue;
                const auto handleA = MakeHandle(a), handleB = MakeHandle(b);
                const auto handles = std::minmax(handleA.id, handleB.id);
                // An existing overlap already receives its ordinary exit below.
                const auto previous = previousTouches.find({handles.first, handles.second});
                if (previous != previousTouches.end() && previous->second.sensor)
                    continue;
                // The completed path can reverse after a solid impact. Its
                // endpoint enclosure alone is not an enclosure of that path.
                // Search each common ledger interval, with fresh observations
                // after StepCoupled has assembled the full-step trajectories.
                if (!sensorQuery) {
                    // Physics query workers have joined before publication.
                    // Reuse their bounded scratch, never the solver's memo or
                    // diagnostics. Coupled steps have now rebuilt their full
                    // trajectories, so their old local-time anchor is invalid.
                    if (ccdQueryStorage.empty()) ccdQueryStorage.resize(1);
                    if (!ccdQueryStorage[0]) ccdQueryStorage[0] = std::make_unique<CCDQueryStorage>();
                    auto &scratch = *ccdQueryStorage[0];
                    scratch.Reset();
                    sensorQuery.emplace(*this, scratch);
                    sensorQuery->impactQueryAnchorTime = std::numeric_limits<double>::quiet_NaN();
                }
                std::vector<double> boundaries{0, stepDuration};
                for (auto slot : {a, b})
                    for (const auto &segment : bodies[slot].motion.Segments())
                        if (segment.end > 0 && segment.end < stepDuration)
                            boundaries.push_back(segment.end);
                std::sort(boundaries.begin(), boundaries.end());
                boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
                double first = stepDuration + 1;
                int firstA = 0, firstB = 0;
                for (std::size_t interval = 1; interval < boundaries.size(); ++interval) {
                    for (int pa = 0; pa < PrimitiveCount(bodies[a].shape); ++pa)
                        for (int pb = 0; pb < PrimitiveCount(bodies[b].shape); ++pb) {
                            if (interval == 1) {
                                const auto initial = sensorQuery->PairAt(a, b, pa, pb, 0, 0);
                                if (initial.count && !initial.uncertain) {
                                    first = 0;
                                    firstA = pa;
                                    firstB = pb;
                                    continue;
                                }
                            }
                            const auto hit = sensorQuery->ImpactTime(a, b, pa, pb, boundaries[interval-1],
                                                                     boundaries[interval]);
                            if (hit <= boundaries[interval] && hit < first) {
                                first = hit;
                                firstA = pa;
                                firstB = pb;
                            }
                        }
                    if (first <= boundaries[interval])
                        break;
                }
                if (!(first < stepDuration))
                    continue;
                auto manifold = sensorQuery->PairAt(a, b, firstA, firstB, first, 0);
                if (!manifold.count || manifold.uncertain)
                    continue;
                const auto aa = PoseAt(a, first), bb = PoseAt(b, first);
                const auto &contact = manifold.points[0];
                const auto va = aa.linearVelocity + glm::cross(aa.angularVelocity,
                                                              contact.point - aa.position);
                const auto vb = bb.linearVelocity + glm::cross(bb.angularVelocity,
                                                              contact.point - bb.position);
                TouchEvent event;
                event.a = MakeHandle(a);
                event.b = MakeHandle(b);
                event.sensor = true;
                event.point = contact.point;
                event.normal = contact.normal;
                event.relativeVelocity = vb - va;
                if (event.a.id > event.b.id) {
                    std::swap(event.a, event.b);
                    event.normal = -event.normal;
                    event.relativeVelocity = -event.relativeVelocity;
                }
                sensorCrossings.push_back(event);
            }
    }
    // Query-controlled actors publish only accepted path segments. Final pose
    // reset does not erase those observations; endpoint overlap still owns
    // persistent state and an existing overlap already receives its normal exit.
    for (const auto &[key, event] : querySensorCrossings) {
        const auto *a = Get(event.a), *b = Get(event.b);
        if (!a || !b || !a->enabled || !b->enabled ||
            !CanCollide(event.a.id & kSlotMask, event.b.id & kSlotMask)) continue;
        const auto *sensor = a->sensor ? a : b->sensor ? b : nullptr;
        if (!sensor || sensor->isDynamic || sensor->isKinematic) continue;
        const auto old = previousTouches.find(key);
        const bool entering=old == previousTouches.end() || !old->second.sensor;
        const auto endpoint=stepTouches.find(key);
        if (endpoint != stepTouches.end()) {
            // The endpoint retains overlap ownership. Its first Enter carries
            // the verified entry point/velocity instead of ResetBody's zero
            // velocity. Subsequent Stay observations use endpoint geometry.
            if (entering) endpoint->second=event;
        } else if (entering) sensorCrossings.push_back(event);
    }
    querySensorCrossings.clear();
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
    for (auto event : sensorCrossings) {
        event.phase = PhysicsWorld::TouchPhase::Enter;
        touchEvents.push_back(event);
        event.phase = PhysicsWorld::TouchPhase::Exit;
        touchEvents.push_back(event);
    }
    std::stable_sort(touchEvents.begin(), touchEvents.end(), [](const auto &a, const auto &b) {
        return std::tie(a.a.id, a.b.id) < std::tie(b.a.id, b.b.id);
    });
    previousTouches = stepTouches;
    JUDAS_PROFILE_COUNTER("Physics swept sensor queries",
                          sensorQuery ? double(ccdQueryStorage[0]->stats.impactQueries) : 0,
                          ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics swept sensor crossings", double(sensorCrossings.size()),
                          ProfileCounterMode::Sum);
    if (sensorStart)
        JUDAS_PROFILE_COUNTER("Physics sensor publication CPU ms",
                              double(PerformanceProfiler::Now()-sensorStart)*1e-6,
                              ProfileCounterMode::Sum);
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
            output.margin = ContactMargin(margin);
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
        const ContactPose poseA{a.rigidBody.position, a.rigidBody.orientation, -a.shape.pivotOffset};
        const ContactPose poseB{b.rigidBody.position, b.rigidBody.orientation, -b.shape.pivotOffset};
        output.manifold = PrimitiveContacts(a.shape, poseA, b.shape, poseB, output.margin,
                                            output.orientationA, output.orientationB, true);
        output.touching = false;
        for (int p = 0; p < output.manifold.count; ++p) {
            const auto &c = output.manifold.points[p];
            output.touching = output.touching ||
                              (c.hasLocalAnchors ? c.signedSeparation <= 0 : c.penetration >= 0);
        }
        if (!output.touching && output.manifold.count)
            output.touching =
                PrimitiveContacts(a.shape, poseA, b.shape, poseB, 0, output.orientationA, output.orientationB).count >
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
    m_impl->querySensorCrossings.clear();
    m_impl->collectingPreStepQueries = false;
    m_impl->queryTouchesPending = false;
    m_impl->touchEvents.clear();
}

void PhysicsWorld::BeginPreStepQueryContacts() {
    m_impl->preStepQueryTouches.clear();
    m_impl->querySensorCrossings.clear();
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

void PhysicsWorld::ObserveQueryTranslation(BodyHandle observer,const BodyTransform& from,
                                           const glm::vec3& displacement,const glm::vec3& velocity,
                                           const PhysicsQueryFilter* filter) {
    auto* actor=m_impl->Get(observer);
    if(!actor || !actor->queryOnly || !actor->enabled) return;
    const auto finite=[](glm::vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    const double rotationNorm=glm::dot(glm::dquat(from.rotation),glm::dquat(from.rotation));
    const double length=glm::length(glm::dvec3(displacement));
    if(!finite(from.position)||!finite(from.position+displacement)||!finite(displacement)||!finite(velocity)||
       !std::isfinite(rotationNorm)||!(rotationNorm>0)||!(length>1e-6)) return;
    const auto started=PerformanceProfiler::Get().Active()?PerformanceProfiler::Now():0;
    auto policy=filter?*filter:PhysicsQueryFilter{};
    policy.includeSensors=true;
    const auto first=ShapeAabb(actor->shape,from.position,from.rotation);
    const auto last=ShapeAabb(actor->shape,from.position+displacement,from.rotation);
    const auto candidates=m_impl->QuerySlots(first.Union(last).Expanded(kSweepQueryEpsilon));
    const auto direction=glm::vec3(glm::dvec3(displacement)/length);
    unsigned tested=0;
    for(auto slot:candidates){
        const auto& target=m_impl->bodies[slot];
        if(slot==(observer.id&Impl::kSlotMask)||!target.sensor||target.isDynamic||target.isKinematic||
           !m_impl->CanCollide(observer.id&Impl::kSlotMask,slot)||
           !m_impl->MatchesQuery(slot,policy)) continue;
        const auto handle=m_impl->MakeHandle(slot);
        const auto ids=std::minmax(observer.id,handle.id);
        const Impl::TouchKey key{ids.first,ids.second};
        if(m_impl->querySensorCrossings.count(key)) continue;
        PrimitiveCastHit nearest;
        nearest.distance=length+1;
        for(int part=0;part<PrimitiveCount(target.shape);++part){
            ++tested;
            const auto geometry=PrimitiveAt(target.shape,target.rigidBody,part);
            const auto hit=CastAgainstPrimitive(actor->shape,from,direction,length,geometry,false,
                                               policy.normalAxis,policy.minimumNormalDot);
            if(hit.hit&&(policy.normalAxis==glm::vec3(0)||glm::dot(hit.normal,policy.normalAxis)>policy.minimumNormalDot)&&
               (!nearest.hit||hit.distance<nearest.distance)) nearest=hit;
        }
        if(!nearest.hit) continue;
        TouchEvent event;
        event.a=observer;event.b=handle;event.sensor=true;
        event.point=nearest.point;event.normal=nearest.normal;event.relativeVelocity=-velocity;
        if(event.a.id>event.b.id){
            std::swap(event.a,event.b);event.normal=-event.normal;event.relativeVelocity=-event.relativeVelocity;
        }
        m_impl->querySensorCrossings.emplace(key,event);
        m_impl->queryTouchesPending=true;
    }
    JUDAS_PROFILE_COUNTER("Physics accepted query translations",1,ProfileCounterMode::Sum);
    JUDAS_PROFILE_COUNTER("Physics query sensor primitives",double(tested),ProfileCounterMode::Sum);
    if(started) JUDAS_PROFILE_COUNTER("Physics query sensor search CPU ms",
                                    double(PerformanceProfiler::Now()-started)*1e-6,ProfileCounterMode::Sum);
}

void PhysicsWorld::FinishQueryTouches() {
    if (!m_impl->collectingPreStepQueries && m_impl->queryTouchesPending) {
        m_impl->FinishTouches();
        m_impl->queryTouchesPending = false;
    }
}
