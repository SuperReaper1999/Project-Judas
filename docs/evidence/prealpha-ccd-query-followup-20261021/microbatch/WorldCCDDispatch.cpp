#include "WorldInternal.h"
#include <cfenv>

using namespace JudasPhysicsDetail;

namespace {
// Resource jobs may have used a different rounding environment. Query lanes
// follow the native contact contract and restore the borrowed thread afterward.
struct QueryRoundingScope {
    int previous = std::fegetround();
    QueryRoundingScope() {
        if (previous != FE_TONEAREST && std::fesetround(FE_TONEAREST) != 0)
            throw std::runtime_error("CCD worker cannot establish round-to-nearest");
    }
    ~QueryRoundingScope() {
        if (previous != FE_TONEAREST)
            std::fesetround(previous);
    }
};
}


// Queries run against one immutable event boundary. Only this owner publishes
// ordered results after every borrowed job has joined. No solver work is parallel.
auto PhysicsWorld::Impl::EvaluateImpactQueries(std::vector<CCDRequest>& requests,
                                                double start, double finish) -> void {
    const auto serial = [&](CCDRequest& request) {
        if (!request.query) {
            request.time = finish + 1;
            request.ready = true;
            return;
        }
        try {
            const auto& k = request.key;
            request.time = ImpactTime(k[0], k[1], int(k[2]), int(k[3]), start, finish,
                                      request.target);
        } catch (...) {
            request.error = std::current_exception();
        }
        request.ready = true;
    };
    const auto rethrow = [&] {
        for (const auto& request : requests)
            if (request.error)
                std::rethrow_exception(request.error);
    };
    JobSystem* jobs = contactJobs;
    if (!jobs || jobs->IsShutDown() || contactJobLanes < 2 || requests.size() < 32 || requests.size() > 16384 ||
        std::fegetround() != FE_TONEAREST) {
        for (auto& request : requests) {
            serial(request);
            if (request.error)
                std::rethrow_exception(request.error);
        }
        return;
    }
    // Do not turn cheap broadphase misses into jobs. Reuse the existing,
    // outward-proven swept enclosures, and retain the native query for all
    // unsupported shapes or piecewise prescribed trajectories.
    std::vector<std::size_t> pending;
    pending.reserve(requests.size());
    const auto simple = [](const Body& body) {
        return !body.isKinematic && !body.shape.asset &&
               (body.shape.type == ShapeType::Box || body.shape.type == ShapeType::Sphere) &&
               body.shape.pivotOffset == glm::vec3(0) &&
               std::isfinite(body.boundingRadius) && body.boundingRadius > 0;
    };
    for (std::size_t i = 0; i < requests.size(); ++i) {
        auto& request = requests[i];
        const auto a = request.key[0], b = request.key[1];
        bool expensive = request.query && simple(bodies[a]) && simple(bodies[b]);
        if (expensive) {
            const auto& first = RemainingImpactBound(a, start, finish);
            const auto& second = RemainingImpactBound(b, start, finish);
            expensive = !(rigid_motion_bounds::FiniteBound(first) &&
                          rigid_motion_bounds::FiniteBound(second) && !first.Overlaps(second));
        }
        if (expensive)
            pending.push_back(i);
        else
            serial(request);
    }
    if (pending.size() < 32) {
        for (auto i : pending)
            serial(requests[i]);
        rethrow();
        return;
    }
    const unsigned lanes = unsigned(std::min(std::size_t(contactJobLanes), std::max(std::size_t(2), pending.size() / 16)));
    // Limit retained sample payload, including inactive lanes left by an older
    // batch. Above the optional budget every real query still runs serially.
    constexpr std::size_t maximumScratchBytes = 32 * 1024 * 1024;
    std::size_t plannedSamples = 0;
    for (std::size_t lane = 0; lane < std::max(ccdQueryStorage.size(), std::size_t(lanes)); ++lane) {
        const auto* scratch = lane < ccdQueryStorage.size() ? ccdQueryStorage[lane].get() : nullptr;
        for (bool anchor : {false, true}) {
            const auto capacity = !scratch ? 0 : (anchor ? scratch->impactAnchorSamples.capacity()
                                                        : scratch->impactPoseSamples.capacity());
            plannedSamples += lane < lanes ? std::max(capacity, bodies.size()) : capacity;
        }
    }
    if (plannedSamples > maximumScratchBytes / sizeof(ImpactPoseSample)) {
        for (auto i : pending)
            serial(requests[i]);
        rethrow();
        return;
    }
    if (ccdQueryStorage.size() < lanes)
        ccdQueryStorage.resize(lanes);
    for (unsigned lane = 0; lane < lanes; ++lane) {
        if (!ccdQueryStorage[lane]) {
            ccdQueryStorage[lane] = std::make_unique<CCDQueryStorage>();
            ++cacheCounters.allocations;
            cacheCounters.allocatedBytes += sizeof(CCDQueryStorage);
        }
        auto& scratch = *ccdQueryStorage[lane];
        scratch.Reset();
        scratch.work.active = work.active;
        for (auto* samples : {&scratch.impactPoseSamples, &scratch.impactAnchorSamples}) {
            if (samples->capacity() < bodies.size()) {
                // Explicit replacement avoids geometric vector growth defeating
                // the budget. No worker allocates/resizes the shared world.
                std::vector<ImpactPoseSample> replacement(bodies.size());
                samples->swap(replacement);
                ++cacheCounters.allocations;
                cacheCounters.allocatedBytes += samples->capacity() * sizeof(ImpactPoseSample);
            } else {
                samples->resize(bodies.size());
            }
        }
    }
    std::array<InitialGeometryDiagnostics, JobSystem::kMaxWorkers + 1> diagnostics{};
    std::atomic<std::size_t> next{0};
    const auto run = [&](unsigned lane) {
        const auto contactsBefore = GetContactGeometryDiagnostics();
        const auto cookedBefore = GetCollisionGeometryStats();
        JUDAS_PROFILE_SCOPE("Physics CCD query worker batch");
        QueryRoundingScope rounding;
        CCDQuery query(*this, *ccdQueryStorage[lane]);
        for (;;) {
            const auto first = next.fetch_add(8, std::memory_order_relaxed);
            if (first >= pending.size())
                break;
            for (auto i = first; i < std::min(first + 8, pending.size()); ++i) {
                auto& request = requests[pending[i]];
                try {
                    const auto& k = request.key;
                    request.time = query.ImpactTime(k[0], k[1], int(k[2]), int(k[3]),
                                                     start, finish, request.target);
                } catch (...) {
                    request.error = std::current_exception();
                }
                request.ready = true;
                ++diagnostics[lane].manifolds;
            }
        }
        AddContactDiagnostics(diagnostics[lane].contacts, GetContactGeometryDiagnostics(), contactsBefore);
        AddCookedDiagnostics(diagnostics[lane].cooked, GetCollisionGeometryStats(), cookedBefore);
    };
    const auto began = work.active ? Clock::now() : Clock::time_point{};
    std::array<JobHandle, JobSystem::kMaxWorkers> handles{};
    unsigned handleCount = 0;
    std::exception_ptr submitError;
    try {
        for (unsigned lane = 1; lane < lanes; ++lane) {
            const auto handle = jobs->Submit([&, lane](JobContext&) { run(lane); },
                                             JobPriority::High, "Physics CCD queries");
            if (handle.IsValid())
                handles[handleCount++] = handle;
        }
    } catch (...) { submitError = std::current_exception(); }
    std::exception_ptr ownerError;
    try { run(0); } catch (...) { ownerError = std::current_exception(); }
    for (unsigned i = 0; i < handleCount; ++i)
        if (jobs->StateOf(handles[i]) == JobState::Queued)
            jobs->Cancel(handles[i]);
    std::string workerError;
    for (unsigned i = 0; i < handleCount; ++i) {
        if (jobs->Wait(handles[i]) == JobState::Failed && workerError.empty()) {
            workerError = jobs->ErrorOf(handles[i]);
            if (workerError.empty()) workerError = "CCD query job did not complete";
        }
        jobs->Forget(handles[i]);
    }
    if (work.active)
        work.ccdBatchNanoseconds += std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now() - began).count());
    ++work.ccdParallelBatches;
    work.ccdParallelQueries += pending.size();
    work.ccdPeakLanes = std::max(work.ccdPeakLanes, std::size_t(lanes));
    for (unsigned lane = 0; lane < lanes; ++lane) {
        const auto& scratch = *ccdQueryStorage[lane];
        const auto& counts = scratch.stats;
        stats.impactQueries += counts.impactQueries;
        stats.impactSearchIterations += counts.impactSearchIterations;
        stats.impactSearchLimit += counts.impactSearchLimit;
        stats.impactUncertifiedAdvances += counts.impactUncertifiedAdvances;
        stats.impactSearchStalls += counts.impactSearchStalls;
        stats.impactSamplingFallbacks += counts.impactSamplingFallbacks;
        stats.impactSamplingTests += counts.impactSamplingTests;
        stats.impactSamplingResolutionCaps += counts.impactSamplingResolutionCaps;
        stats.impactPeakIterations = std::max(stats.impactPeakIterations, counts.impactPeakIterations);
        stats.impactSamplingMaxInterval = std::max(stats.impactSamplingMaxInterval,
                                                  counts.impactSamplingMaxInterval);
        impactPoseEvaluations += scratch.impactPoseEvaluations;
        impactPoseReuses += scratch.impactPoseReuses;
        impactOrientationBuilds += scratch.impactOrientationBuilds;
        impactTruthRequests += scratch.impactTruthRequests;
        impactTruthReuses += scratch.impactTruthReuses;
        impactBoundsRejected += scratch.impactBoundsRejected;
        impactActualBoundsRejected += scratch.impactActualBoundsRejected;
        // Summed lane CPU time is distinct from elapsed batch wall time.
        work.toi.nanoseconds += scratch.work.toi.nanoseconds;
        work.toi.calls += scratch.work.toi.calls;
        work.geometry.nanoseconds += scratch.work.geometry.nanoseconds;
        work.geometry.calls += scratch.work.geometry.calls;
        cacheCounters.allocations += scratch.cacheCounters.allocations;
        cacheCounters.allocatedBytes += scratch.cacheCounters.allocatedBytes;
        cacheCounters.boundHits += scratch.cacheCounters.boundHits;
        cacheCounters.boundRebuilds += scratch.cacheCounters.boundRebuilds;
        if (lane) {
            work.ccdWorkerQueries += diagnostics[lane].manifolds;
            AddContactDiagnostics(parallelContactDiagnostics, diagnostics[lane].contacts);
            AddCookedDiagnostics(parallelCookedDiagnostics, diagnostics[lane].cooked);
        }
    }
    // Shutdown can cancel a queued lane. All joins precede this owner fallback;
    // no output is ever recomputed concurrently with a borrowed worker.
    for (auto& request : requests)
        if (!request.ready)
            serial(request);
    if (submitError) std::rethrow_exception(submitError);
    if (ownerError) std::rethrow_exception(ownerError);
    if (!workerError.empty()) throw std::runtime_error(workerError);
    rethrow();
}
