#!/usr/bin/env python3
"""Generate an isolated, unbuilt parallel dirty-TOI experiment.

Production source is read only. The context's CCD arithmetic is copied exactly;
the experiment changes ownership of query scratch and publication of independent
dirty-key answers, not chronological event or solver order.
"""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
SOURCE = ROOT / "src" / "physics"


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


internal = (SOURCE / "WorldInternal.h").read_text()
queries = (SOURCE / "WorldImpactQueries.h").read_text()
ccd = (SOURCE / "WorldCCDSearch.cpp").read_text()
events = (SOURCE / "WorldImpactEvents.cpp").read_text()
inputs = {name: sha((SOURCE / name).read_text()) for name in (
    "WorldInternal.h", "WorldImpactQueries.h", "WorldCCDSearch.cpp", "WorldImpactEvents.cpp")}

# A nested type can name private Impl/Body without widening the public API.
private_header = internal.replace(
    "struct PhysicsWorld::Impl {", "struct PhysicsWorld::Impl {\n    struct ImpactQueryContext;", 1)
(HERE / "WorldInternal.h").write_text(private_header)

decls = internal[internal.index("    RigidBody PoseAt("):
                 internal.index("    void ResolveInitialImpacts(")]
decls = decls.replace("    static bool ContinuationPrimitive(const Body &body);", "")
context = '''#pragma once

// Isolated experiment only. Bodies/motions/assets are borrowed immutable until
// the owner joins every query; samples and diagnostics belong to this one lane.
struct PhysicsWorld::Impl::ImpactQueryContext {
    const Impl& owner;
    const std::vector<Body>& bodies;
    std::vector<ImpactPoseSample> impactPoseSamples, impactAnchorSamples;
    std::vector<PoseBound> staticBounds;
    std::vector<std::optional<PreparedShapeBounds>> staticPreparations;
    double impactQueryAnchorTime = 0;
    std::size_t impactPoseEvaluations = 0, impactPoseReuses = 0,
        impactOrientationBuilds = 0, impactTruthRequests = 0,
        impactTruthReuses = 0, impactBoundsRejected = 0,
        impactActualBoundsRejected = 0;
    WorkDiagnostics work;
    PhysicsWorld::StepStats stats;
    CacheCounters cacheCounters;

    explicit ImpactQueryContext(const Impl& world)
        : owner(world), bodies(world.bodies), impactPoseSamples(bodies.size()),
          impactAnchorSamples(bodies.size()), staticBounds(bodies.size()),
          staticPreparations(bodies.size()) {}

    void Reset(double anchor) {
        impactQueryAnchorTime = anchor;
        work = {}; work.active = owner.work.active;
        stats = {}; cacheCounters = {};
        impactPoseEvaluations = impactPoseReuses = impactOrientationBuilds =
            impactTruthRequests = impactTruthReuses = impactBoundsRejected =
            impactActualBoundsRejected = 0;
        // A previous batch may precede a velocity-changing physical event.
        // Keep allocations, discard every trajectory-dependent sample.
        for (auto& sample : impactPoseSamples) {
            sample.time = sample.boundStart = std::numeric_limits<double>::quiet_NaN();
            sample.tightValid = false;
        }
        for (auto& sample : impactAnchorSamples)
            sample.time = std::numeric_limits<double>::quiet_NaN();
    }

    const Aabb& CurrentBound(const Body& body) {
        // The original static adapter uses this same prepared ShapeAabb path.
        // A matching owner cache is immutable; otherwise prepare lane-private.
        if (Matches(body.currentBound, body.rigidBody.position, body.rigidBody.orientation))
            return body.currentBound.bound;
        const auto slot = std::size_t(&body - bodies.data());
        auto& cached = staticBounds[slot];
        if (!Matches(cached, body.rigidBody.position, body.rigidBody.orientation)) {
            const ContactPreparedOrientation orientation(body.rigidBody.orientation);
            auto& prepared = staticPreparations[slot];
            if (!prepared) prepared.emplace();
            PrepareShapeBounds(body.shape, orientation, *prepared);
            cached = {body.rigidBody.position, body.rigidBody.orientation,
                      ShapeAabb(*prepared, body.rigidBody.position), true};
        }
        return cached.bound;
    }
    static bool ContinuationPrimitive(const Body& body) {
        return Impl::ContinuationPrimitive(body);
    }
    void EnsureImpactPoseStorage();
''' + decls + "};\n\n"
query_defs = queries.replace("#pragma once", "", 1).replace(
    "PhysicsWorld::Impl::", "PhysicsWorld::Impl::ImpactQueryContext::")
ccd_defs = ccd.replace('#include "WorldInternal.h"', "", 1).replace(
    "PhysicsWorld::Impl::", "PhysicsWorld::Impl::ImpactQueryContext::")
context += query_defs + "\n" + ccd_defs
(HERE / "ParallelImpactQueries.h").write_text(context)

batch = r'''
    // Queries read this event boundary only. The owner mutates no body/motion
    // until every worker is joined and answers are published in sorted order.
    std::vector<std::unique_ptr<ImpactQueryContext>> queryContexts;
    const auto scheduleBatch = [&](const std::vector<std::array<unsigned, 4>>& keys) {
        JobSystem* jobs = contactJobs;
        if (!jobs || jobs->IsShutDown() || contactJobLanes <= 1 || keys.size() < 256) {
            for (const auto& key : keys) schedule(key);
            return;
        }
        struct Request {
            std::array<unsigned, 4> key;
            double target = 0, time = 0;
            bool ready = false;
            std::exception_ptr error;
        };
        std::vector<Request> requests;
        requests.reserve(keys.size());
        for (const auto& key : keys) {
            if (!CanRespond(key[0], key[1])) continue;
            const bool mesh = StaticMeshPolyPair(key[0], key[1], int(key[2]), int(key[3]));
            const bool support = !mesh &&
                std::binary_search(supportPairs.begin(), supportPairs.end(), key);
            const double target = !mesh && !support &&
                std::binary_search(spinningPairs.begin(), spinningPairs.end(), key)
                ? SpinPenetrationTarget(key) : 0;
            requests.push_back({key, target, double(dt) + 1, support, {}});
        }
        // Bounded scratch is a fallback condition, never a physics-quality cap.
        constexpr std::size_t kScratchBudget = 32u * 1024u * 1024u;
        const std::size_t bytesPerLane = bodies.size() *
            (2 * sizeof(ImpactPoseSample) + sizeof(PoseBound) +
             sizeof(std::optional<PreparedShapeBounds>));
        const auto memoryLanes = bytesPerLane ? kScratchBudget / bytesPerLane : 1;
        const unsigned lanes = unsigned(std::min({std::size_t(contactJobLanes), memoryLanes,
                                                std::max(std::size_t(1), requests.size() / 64)}));
        if (lanes <= 1) {
            for (const auto& key : keys) schedule(key);
            return;
        }
        while (queryContexts.size() < lanes)
            queryContexts.push_back(std::make_unique<ImpactQueryContext>(*this));
        for (unsigned lane = 0; lane < lanes; ++lane) queryContexts[lane]->Reset(now);
        std::array<InitialGeometryDiagnostics, JobSystem::kMaxWorkers + 1> diagnostics{};
        std::atomic<std::size_t> next{0};
        std::fenv_t ownerEnvironment;
        std::fegetenv(&ownerEnvironment);
        std::array<int, JobSystem::kMaxWorkers + 1> floatingExceptions{};
        auto run = [&](unsigned lane) {
            struct RestoreEnvironment {
                std::fenv_t previous;
                bool active;
                RestoreEnvironment(const std::fenv_t& desired, bool worker) : active(worker) {
                    if (active) { std::fegetenv(&previous); std::fesetenv(&desired); }
                }
                ~RestoreEnvironment() { if (active) std::fesetenv(&previous); }
            } environment(ownerEnvironment, lane != 0);
            auto& query = *queryContexts[lane];
            const auto contactsBefore = GetContactGeometryDiagnostics();
            const auto cookedBefore = GetCollisionGeometryStats();
            JUDAS_PROFILE_SCOPE("Physics independent dirty TOI batch");
            for (;;) {
                const auto first = next.fetch_add(8, std::memory_order_relaxed);
                if (first >= requests.size()) break;
                for (auto i = first; i < std::min(first + 8, requests.size()); ++i) {
                    auto& request = requests[i];
                    if (request.ready) continue;
                    try {
                        const auto& key = request.key;
                        request.time = query.ImpactTime(key[0], key[1], int(key[2]), int(key[3]),
                                                       now, dt, request.target);
                    } catch (...) { request.error = std::current_exception(); }
                    request.ready = true;
                }
            }
            AddContactDiagnostics(diagnostics[lane].contacts,
                                  GetContactGeometryDiagnostics(), contactsBefore);
            AddCookedDiagnostics(diagnostics[lane].cooked,
                                 GetCollisionGeometryStats(), cookedBefore);
            floatingExceptions[lane] = std::fetestexcept(FE_ALL_EXCEPT);
        };
        std::array<JobHandle, JobSystem::kMaxWorkers> handles{};
        unsigned handleCount = 0;
        std::exception_ptr submitError, mainError;
        try {
            for (unsigned lane = 1; lane < lanes; ++lane) {
                auto handle = jobs->Submit([&, lane](JobContext&) { run(lane); }, JobPriority::High,
                                          "Physics dirty TOI search");
                if (handle.IsValid()) handles[handleCount++] = handle;
            }
        } catch (...) { submitError = std::current_exception(); }
        try { run(0); } catch (...) { mainError = std::current_exception(); }
        for (unsigned i = 0; i < handleCount; ++i)
            if (jobs->StateOf(handles[i]) == JobState::Queued) jobs->Cancel(handles[i]);
        JobHandle failed;
        for (unsigned i = 0; i < handleCount; ++i)
            if (jobs->Wait(handles[i]) == JobState::Failed && !failed.IsValid()) failed = handles[i];
        std::string workerError = failed.IsValid() ? jobs->ErrorOf(failed) : std::string{};
        if (failed.IsValid() && workerError.empty()) workerError = "dirty TOI job did not complete";
        for (unsigned i = 0; i < handleCount; ++i) jobs->Forget(handles[i]);
        const auto sumSample = [](PhysicsWorkSample& into, const PhysicsWorkSample& from) {
            into.nanoseconds += from.nanoseconds; into.calls += from.calls;
        };
        for (unsigned lane = 0; lane < lanes; ++lane) {
            const auto& query = *queryContexts[lane];
            const auto& extra = query.stats;
            stats.impactQueries += extra.impactQueries;
            stats.impactSearchIterations += extra.impactSearchIterations;
            stats.impactPeakIterations = std::max(stats.impactPeakIterations, extra.impactPeakIterations);
            stats.impactSearchLimit += extra.impactSearchLimit;
            stats.impactUncertifiedAdvances += extra.impactUncertifiedAdvances;
            stats.impactSearchStalls += extra.impactSearchStalls;
            stats.impactSamplingFallbacks += extra.impactSamplingFallbacks;
            stats.impactSamplingTests += extra.impactSamplingTests;
            stats.impactSamplingResolutionCaps += extra.impactSamplingResolutionCaps;
            stats.impactSamplingMaxInterval = std::max(stats.impactSamplingMaxInterval,
                                                     extra.impactSamplingMaxInterval);
            impactPoseEvaluations += query.impactPoseEvaluations;
            impactPoseReuses += query.impactPoseReuses;
            impactOrientationBuilds += query.impactOrientationBuilds;
            impactTruthRequests += query.impactTruthRequests;
            impactTruthReuses += query.impactTruthReuses;
            impactBoundsRejected += query.impactBoundsRejected;
            impactActualBoundsRejected += query.impactActualBoundsRejected;
            // Sum of lane work is diagnostic CPU time, not owner wall latency.
            sumSample(work.toi, query.work.toi);
            sumSample(work.geometry, query.work.geometry);
            if (lane) {
                AddContactDiagnostics(parallelContactDiagnostics, diagnostics[lane].contacts);
                AddCookedDiagnostics(parallelCookedDiagnostics, diagnostics[lane].cooked);
            }
        }
        if (submitError) std::rethrow_exception(submitError);
        if (mainError) std::rethrow_exception(mainError);
        if (!workerError.empty()) throw std::runtime_error(workerError);
        int combinedExceptions = 0;
        for (unsigned lane = 0; lane < lanes; ++lane) combinedExceptions |= floatingExceptions[lane];
        std::feraiseexcept(combinedExceptions & ~std::fetestexcept(FE_ALL_EXCEPT));
        for (auto& request : requests) {
            if (!request.ready) {
                const auto& key = request.key;
                request.time = ImpactTime(key[0], key[1], int(key[2]), int(key[3]), now, dt,
                                          request.target);
            }
            if (request.error) std::rethrow_exception(request.error);
            const auto found = scheduledIndex.find(request.key);
            if (found != scheduledIndex.end()) scheduledImpacts[found->second].time = request.time;
            else {
                scheduledIndex.emplace(request.key, scheduledImpacts.size());
                scheduledImpacts.push_back({request.key, request.time});
            }
        }
    };
'''

events = events.replace('#include "WorldInternal.h"',
                        '#include "WorldInternal.h"\n#include "ParallelImpactQueries.h"\n#include <cfenv>')
needle = '    std::vector<std::array<unsigned, 4>> dirty;\n'
assert events.count(needle) == 1
events = events.replace(needle, needle + batch, 1)
needle = '        for (const auto &key : dirty)\n            schedule(key);'
assert events.count(needle) == 1
events = events.replace(needle, '        scheduleBatch(dirty);', 1)
(HERE / "WorldImpactEvents.cpp").write_text(events)

manifest = {"status": "generated, unbuilt, unvalidated isolated prototype",
            "inputs": inputs,
            "outputs": {name: sha((HERE / name).read_text()) for name in (
                "WorldInternal.h", "ParallelImpactQueries.h", "WorldImpactEvents.cpp")},
            "productionModified": False,
            "arithmeticChange": "none intended; independent static-bound cache adapter needs parity proof",
            "requiredBeforeAdoption": [
                "compile and query-level serial/parallel exact TOI controls",
                "270-step exact physical trace with worker counts 1/2/4",
                "moving kinematics, cooked mesh, compound, failures and job shutdown",
                "rounding environment propagation, cancellation and worker exception",
                "actual peak memory and cache ownership review",
                "matched uninstrumented dense latency; no speedup assumed"]}
(HERE / "PROTOTYPE.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(json.dumps({"outputs": list(manifest["outputs"]), "status": manifest["status"]}))
