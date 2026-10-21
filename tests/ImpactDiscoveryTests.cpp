// Exact event-time geometry and isolated CCD query-batch controls.
// Private access stays in this fixture; no public test API or solver tuning.
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cfenv>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define private public
#include "physics/WorldInternal.h"
#undef private

namespace {
unsigned checks = 0, failures = 0;
bool staleBoundControl = false;
bool staleQueryControl = false;

void Check(bool value, const std::string& label) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL " << label << '\n';
    }
}

unsigned Slot(BodyHandle handle) {
    return handle.id & PhysicsWorld::Impl::kSlotMask;
}

void BeginMotion(PhysicsWorld& physics, double duration = 1) {
    auto& world = *physics.m_impl;
    world.stepDuration = float(duration);
    world.impactQueryAnchorTime = 0;
    for (unsigned slot : world.aliveSlots) {
        auto& body = world.bodies[slot];
        if (body.HasMotion())
            body.motion.Begin(world.MakeHandle(slot).id, body.rigidBody, duration);
        world.InvalidateImpactPose(slot);
    }
}

// Sample the authoritative public motion representation independently of the
// world's pose/bound caches. No bound or discovery helper supplies this oracle.
RigidBody Sample(const PhysicsWorld::Impl& world, unsigned slot, double time) {
    const auto& body = world.bodies[slot];
    return body.HasMotion() ? body.motion.EvaluateAfterEvents(time) : body.rigidBody;
}

Aabb QueryAt(const PhysicsWorld::Impl& world, unsigned slot, double time) {
    const auto pose = Sample(world, slot, time);
    return ShapeAabb(world.bodies[slot].shape, pose.position, pose.orientation);
}

ContactManifold ActualContacts(const PhysicsWorld::Impl& world, unsigned a, unsigned b,
                               double time) {
    return ComputeContacts(PrimitiveAt(world.bodies[a].shape, Sample(world, a, time), 0),
                           PrimitiveAt(world.bodies[b].shape, Sample(world, b, time), 0), 0);
}

bool Separated(PhysicsWorld::Impl& world, unsigned a, unsigned b, const Aabb& query,
               double start, double finish) {
    if (!staleBoundControl)
        return world.ImpactDiscoverySeparated(a, b, query, start, finish);
    // Deliberately broken negative control: physical storage may still describe
    // the step anchor while the other body's event-time pose has already moved.
    // Preserve the production eligibility boundary and replace ONLY the bound.
    const auto simple = [](const auto& body) {
        return !body.shape.asset &&
               (body.shape.type == ShapeType::Box || body.shape.type == ShapeType::Sphere) &&
               body.shape.pivotOffset == glm::vec3(0) && std::isfinite(body.boundingRadius) &&
               body.boundingRadius > 0;
    };
    if (!simple(world.bodies[a]) || !simple(world.bodies[b]))
        return false;
    const auto& stale = world.CurrentBound(world.bodies[b]);
    return rigid_motion_bounds::FiniteBound(query) && rigid_motion_bounds::FiniteBound(stale) &&
           !query.Overlaps(stale);
}

void ContactMustSurvive(PhysicsWorld& physics, BodyHandle first, BodyHandle second,
                        double time, const std::string& label) {
    auto& world = *physics.m_impl;
    const auto a = Slot(first), b = Slot(second);
    const auto contact = ActualContacts(world, a, b, time);
    const auto query = QueryAt(world, a, time);
    Check(contact.count > 0 && !contact.uncertain, label + " has independently resolved contact");
    Check(!Separated(world, a, b, query, time, 1), label + " retains actual narrowphase contact");
    std::cout << "CONTACT " << label << " count=" << contact.count;
    if (contact.count)
        std::cout << " separation=" << std::setprecision(17) << contact.points[0].signedSeparation;
    std::cout << '\n';
}

void ClearGapAndClosedBoundary() {
    PhysicsWorld physics;
    physics.Init();
    const auto first = physics.CreateDynamicBox({0, 0, 0}, {.5f, .5f, .5f}, 1, 0, 0);
    const auto distant = physics.CreateStaticSphere({3, 0, 0}, .5f, 0, 0);
    const auto boundary = physics.CreateStaticSphere({1, 0, 0}, .5f, 0, 0);
    BeginMotion(physics);
    auto& world = *physics.m_impl;
    const auto query = QueryAt(world, Slot(first), 0);
    Check(ActualContacts(world, Slot(first), Slot(distant), 0).count == 0,
          "clear-gap fixture is independently separated");
    Check(Separated(world, Slot(first), Slot(distant), query, 0, 1),
          "clear gap actually takes the new rejection path");
    ContactMustSurvive(physics, first, boundary, 0, "closed sphere-box boundary");
}

void RotatedAndTranslatedContact() {
    PhysicsWorld physics;
    physics.Init();
    const auto rotation = glm::angleAxis(.7f, glm::normalize(glm::vec3(1, 2, 3)));
    // A large offset and an arbitrary axis prevent an origin/world-Y fixture
    // from accidentally substituting for the real represented geometry.
    const glm::vec3 offset(10000, -20000, 5000);
    const auto first = physics.CreateShape(Shape::Box({.5f, .5f, .5f}),
                                           {offset, rotation}, true, 1, 0, 0);
    const auto second = physics.CreateStaticSphere(offset + rotation * glm::vec3(.7f, 0, 0),
                                                    .25f, 0, 0);
    BeginMotion(physics);
    ContactMustSurvive(physics, first, second, .375, "rotated translated near-contact");
}

void MovingEndpointAtEventTime() {
    PhysicsWorld physics;
    physics.Init();
    const auto first = physics.CreateDynamicBox({0, 0, 0}, {.5f, .5f, .5f}, 1, 0, 0);
    const auto second = physics.CreateDynamicSphere({3, 0, 0}, .5f, 1, 0, 0);
    physics.SetLinearVelocity(second, {-4, 0, 0});
    BeginMotion(physics);
    auto& world = *physics.m_impl;
    const auto query = QueryAt(world, Slot(first), .5);
    Check(!query.Overlaps(world.CurrentBound(world.bodies[Slot(second)])),
          "moving fixture genuinely defeats the stale physical-storage bound");
    Check(Sample(world, Slot(second), .5).position == glm::vec3(1, 0, 0),
          "moving endpoint reaches the exact authored event position");
    ContactMustSurvive(physics, first, second, .5, "moving endpoint at event time");
    // An earlier cached enclosure remains a valid superset at a later event.
    world.RemainingImpactBound(Slot(second), 0, 1);
    ContactMustSurvive(physics, first, second, .625, "earlier enclosure reused at later event");
}

void RotatingFutureEnclosure() {
    PhysicsWorld physics;
    physics.Init();
    const auto first = physics.CreateDynamicBox({0, 0, 0}, {.25f, .25f, .25f}, 1, 0, 0);
    const auto rotation = glm::angleAxis(float(std::acos(-1.0) / 2), glm::vec3(0, 0, 1));
    const auto second = physics.CreateShape(Shape::Box({1.2f, .1f, .1f}),
                                            {{1, 0, 0}, rotation}, true, 1, 0, 0);
    physics.SetAngularVelocity(second, {0, 0, -4});
    BeginMotion(physics);
    auto& world = *physics.m_impl;
    Check(ActualContacts(world, Slot(first), Slot(second), 0).count == 0,
          "rotating fixture starts without actual contact");
    // At the interior half-time the Cayley rotation carries the bar through
    // the box. At the endpoint it has already swung diagonally past it.
    Check(ActualContacts(world, Slot(first), Slot(second), .5).count > 0,
          "rotating fixture's interior geometry really enters the query");
    Check(!Separated(world, Slot(first), Slot(second), QueryAt(world, Slot(first), 0), 0, 1),
          "overlapping future enclosure conservatively retains the current miss");
}

void ChangedTrajectoryInvalidatesEnclosure() {
    PhysicsWorld physics;
    physics.Init();
    const auto first = physics.CreateDynamicBox({0, 0, 0}, {.5f, .5f, .5f}, 1, 0, 0);
    const auto second = physics.CreateDynamicSphere({3, 0, 0}, .5f, 1, 0, 0);
    physics.SetLinearVelocity(second, {1, 0, 0});
    BeginMotion(physics);
    auto& world = *physics.m_impl;
    const auto slot = Slot(second);
    Check(Separated(world, Slot(first), slot, QueryAt(world, Slot(first), 0), 0, 1),
          "outgoing trajectory initially has a valid separation enclosure");
    const Aabb old = world.RemainingImpactBound(slot, 0, 1);
    // This is the event publisher's real ChangeVelocity/Invalidate ordering,
    // not an invented cache revision or an artificial settling step.
    auto& body = world.bodies[slot];
    body.rigidBody = body.motion.EvaluateAfterEvents(.25);
    body.rigidBody.linearVelocity = {-4, 0, 0};
    body.motion.ChangeVelocity(body.rigidBody, .25);
    world.InvalidateImpactPose(slot);
    Check(std::isnan(world.impactPoseSamples[slot].boundStart),
          "published trajectory change invalidates the earlier enclosure");
    const auto query = QueryAt(world, Slot(first), .875);
    Check(!query.Overlaps(old), "old trajectory enclosure would incorrectly reject the new contact");
    ContactMustSurvive(physics, first, second, .875, "changed trajectory after an impact");
}

void UnsupportedAndUncertainBounds() {
    PhysicsWorld physics;
    physics.Init();
    const auto first = physics.CreateDynamicSphere({0, 0, 0}, .5f, 1, 0, 0);
    const auto normal = physics.CreateStaticSphere({4, 0, 0}, .5f, 0, 0);
    const auto compound = physics.CreateDynamicCompoundBoxes(
        {8, 0, 0}, {{{0, 0, 0}, {.5f, .5f, .5f}}}, 1, 0, 0);
    auto pivotShape = Shape::Box({.5f, .5f, .5f});
    pivotShape.pivotOffset = {.25f, 0, 0};
    const auto pivot = physics.CreateShape(pivotShape, {{12, 0, 0}, {1, 0, 0, 0}}, false, 0, 0, 0);
    BeginMotion(physics);
    auto& world = *physics.m_impl;
    const auto a = Slot(first);
    const auto query = QueryAt(world, a, 0);
    Check(!Separated(world, a, Slot(compound), query, 0, 1),
          "compound other endpoint retains its original geometry path");
    Check(!Separated(world, Slot(compound), a, QueryAt(world, Slot(compound), 0), 0, 1),
          "compound expanding endpoint retains its original geometry path");
    Check(world.bodies[Slot(pivot)].shape.pivotOffset != glm::vec3(0),
          "pivot fixture really has a nonzero authored offset");
    Check(!Separated(world, a, Slot(pivot), query, 0, 1),
          "nonzero pivot retains its original geometry path");
    Aabb invalid = query;
    invalid.min.x = std::numeric_limits<float>::quiet_NaN();
    Check(!Separated(world, a, Slot(normal), invalid, 0, 1),
          "nonfinite query is never interpreted as proven separation");
    invalid = query;
    invalid.min.x = invalid.max.x + 1;
    Check(!Separated(world, a, Slot(normal), invalid, 0, 1),
          "inverted query is never interpreted as proven separation");
    world.bodies[Slot(normal)].boundingRadius = std::numeric_limits<float>::infinity();
    Check(!Separated(world, a, Slot(normal), query, 0, 1),
          "nonfinite other radius retains the original geometry path");
}

void ActualStepDiscovery() {
    auto& profiler = PerformanceProfiler::Get();
    const bool enabled = profiler.Enabled(), frozen = profiler.Frozen();
    profiler.Enable(true);
    profiler.Freeze(false);
    profiler.Clear();
    {
        PhysicsWorld physics;
        physics.Init();
        physics.SetSleepingEnabled(false);
        const auto mover = physics.CreateDynamicSphere({0, 0, 0}, .2f, 1, 0, 0);
        physics.CreateStaticBox({1.5f, 0, 0}, {.1f, 2, 2}, 0, 0);
        const auto distant = physics.CreateStaticSphere({0, 5, 0}, .2f, 0, 0);
        auto& world = *physics.m_impl;
        // A retained conservative fat proxy can legitimately exceed present
        // geometry. This controlled broadphase state creates real empty work
        // beside an independently established force-driven collision episode.
        world.tree.MoveProxy(world.bodies[Slot(distant)].proxy,
                             {{-20, -20, -20}, {20, 20, 20}});
        physics.ApplyForce(mover, {7200, 0, 0});
        {
            ProfileFrame frame("incremental discovery bound proof");
            physics.Step(1.f / 60);
        }
        const auto& stats = physics.LastStepStats();
        Check(stats.impactEvents > 0 && world.work.incrementalEvents > 0,
              "bounded real step exercises an actual incremental impact episode");
        Check(world.work.active && world.work.incrementalBoundsRejected > 0,
              "actual growing discovery rejects a proven empty fat-proxy neighbor");
        Check(physics.GetTransform(mover).position.x < 1.3f &&
                  std::abs(physics.GetLinearVelocity(mover).x) < 1.f,
              "real step keeps the wall's collision response and prevents crossing");
        Check(world.tree.Validate(), "real step preserves broadphase ownership");
        std::cout << "STEP impacts=" << stats.impactEvents
                  << " incremental=" << world.work.incrementalEvents
                  << " rejected=" << world.work.incrementalBoundsRejected
                  << " x=" << physics.GetTransform(mover).position.x
                  << " vx=" << physics.GetLinearVelocity(mover).x << '\n';
    }
    profiler.Enable(enabled);
    profiler.Freeze(frozen);
    profiler.Clear();
}
void PoolRounding(JobSystem& jobs, int setTo, int expected = -1) {
    std::mutex mutex;
    std::condition_variable condition;
    unsigned reached = 0;
    bool release = false;
    std::array<int, 3> observed{};
    std::array<JobHandle, 3> handles;
    for (unsigned i = 0; i < handles.size(); ++i)
        handles[i] = jobs.Submit([&, i](JobContext&) {
            observed[i] = std::fegetround();
            std::fesetround(setTo);
            std::unique_lock<std::mutex> lock(mutex);
            ++reached;
            condition.notify_all();
            condition.wait(lock, [&] { return release; });
        });
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&] { return reached == handles.size(); });
        release = true;
    }
    condition.notify_all();
    for (const auto handle : handles) {
        jobs.Wait(handle);
        jobs.Forget(handle);
    }
    if (expected >= 0)
        for (auto value : observed)
            Check(value == expected, "CCD restores each borrowed worker's rounding environment");
}

void IndependentQueryBatch() {
    PhysicsWorld physics;
    physics.Init();
    std::vector<PhysicsWorld::Impl::CCDRequest> requests;
    for (unsigned i = 0; i < 320; ++i) {
        const glm::vec3 origin(-3, 4.f * i, 0);
        const auto rotation = glm::angleAxis(.13f * float(i % 7), glm::normalize(glm::vec3(1, 2, 3)));
        const auto moving = physics.CreateShape(i % 2 ? Shape::Sphere(.25f) : Shape::Box({.3f, .2f, .25f}),
                                                {origin, rotation}, true, 1, 0, 0);
        physics.SetLinearVelocity(moving, {6, 0, 0});
        const auto wall = physics.CreateShape(Shape::Box({.15f, 1, 1}),
                                              {{0, origin.y, 0}, rotation}, false, 0, 0, 0);
        PhysicsWorld::Impl::CCDRequest hit;
        hit.key = {Slot(moving), Slot(wall), 0, 0};
        requests.push_back(hit);
        if (i % 2 == 0) {
            const auto distant = physics.CreateStaticSphere({50, origin.y, 0}, .25f, 0, 0);
            PhysicsWorld::Impl::CCDRequest miss;
            miss.key = {Slot(moving), Slot(distant), 0, 0};
            requests.push_back(miss);
        }
    }
    BeginMotion(physics);
    auto& world = *physics.m_impl;
    const auto evaluate = [&](JobSystem* jobs, unsigned lanes, double start) {
        physics.SetJobSystem(jobs, lanes);
        world.stats = {};
        world.work = {};
        world.work.active = true;
        auto outputs = requests;
        world.EvaluateImpactQueries(outputs, start, 1);
        return outputs;
    };
    const auto serial = evaluate(nullptr, 1, 0);
    Check(world.ccdQueryStorage.empty(), "serial queries allocate no worker scratch");
    JobSystem jobs(3);
    PoolRounding(jobs, FE_UPWARD);
    const auto parallel = evaluate(&jobs, 4, 0);
    PoolRounding(jobs, FE_TONEAREST, FE_UPWARD);
    std::size_t sampleBytes = 0;
    for (const auto& scratch : world.ccdQueryStorage)
        if (scratch)
            sampleBytes += (scratch->impactPoseSamples.capacity() + scratch->impactAnchorSamples.capacity()) *
                           sizeof(PhysicsWorld::Impl::ImpactPoseSample);
    Check(sampleBytes > 0 && sampleBytes <= 32 * 1024 * 1024 && world.CCDQueryBytes() >= sampleBytes,
          "optional query samples are bounded and included in cache accounting");
    Check(world.work.ccdParallelBatches == 1 && world.work.ccdPeakLanes == 4,
          "large independent search batch uses the bounded job path");
    Check(world.work.ccdParallelQueries == 320,
          "cheap separated enclosures remain outside the worker batch");
    for (std::size_t i = 0; i < parallel.size(); ++i) {
        Check(parallel[i].ready && !parallel[i].error, "every borrowed query joins with a complete result");
        Check(parallel[i].time == serial[i].time, "parallel query preserves the exact serial impact time");
        const auto& key = parallel[i].key;
        const auto a = Sample(world, key[0], 0), b = Sample(world, key[1], 0);
        const bool far = b.position.x == 50;
        if (far) {
            Check(parallel[i].time > 1, "independent distant geometry has no impact");
        } else {
            Check(parallel[i].time > 0 && parallel[i].time < 1,
                  "crossing with separated endpoints is detected within the interval");
            const auto actual = ActualContacts(world, key[0], key[1], parallel[i].time);
            Check(actual.count > 0 && !actual.uncertain,
                  "reported TOI has actual independently constructed zero-margin contact");
            Check(ActualContacts(world, key[0], key[1], 0).count == 0 &&
                      ActualContacts(world, key[0], key[1], 1).count == 0,
                  "test cannot pass using endpoint overlap alone");
        }
        Check(physics.GetTransform(world.MakeHandle(key[0])).position == a.position,
              "query batch does not publish body movement");
    }
    auto ordinaryRequests = requests;
    requests.assign(16385, ordinaryRequests.front());
    const auto oversized = evaluate(&jobs, 4, 0);
    Check(world.work.ccdParallelBatches == 0 && world.stats.impactQueries >= oversized.size(),
          "above-budget batch still searches every query through the serial path");
    Check(std::all_of(oversized.begin(), oversized.end(), [&](const auto& result) {
              return result.ready && !result.error && result.time == serial.front().time;
          }), "above-budget fallback preserves every actual collision");
    requests = std::move(ordinaryRequests);
    JobSystem busy(1);
    std::mutex mutex;
    std::condition_variable condition;
    bool started = false, release = false;
    const auto unrelated = busy.Submit([&](JobContext&) {
        std::unique_lock<std::mutex> lock(mutex);
        started = true;
        condition.notify_all();
        condition.wait(lock, [&] { return release; });
    }, JobPriority::Normal, "unrelated blocked resource");
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&] { return started; });
    }
    try {
        const auto blocked = evaluate(&busy, 4, 0);
        Check(busy.StateOf(unrelated) == JobState::Running,
              "owner drains its CCD work without waiting for unrelated resource jobs");
        for (std::size_t i = 0; i < blocked.size(); ++i)
            Check(blocked[i].time == serial[i].time,
                  "cancelled queued CCD lanes lose no collision result");
    } catch (...) {
        { std::lock_guard<std::mutex> lock(mutex); release = true; }
        condition.notify_all();
        busy.Shutdown();
        throw;
    }
    { std::lock_guard<std::mutex> lock(mutex); release = true; }
    condition.notify_all();
    busy.Wait(unrelated);
    busy.Forget(unrelated);
    busy.Shutdown();
    JobSystem stopping(1);
    const auto blocker = stopping.Submit([](JobContext& context) {
        while (!context.CancelRequested()) std::this_thread::yield();
        context.ReportCancelled();
    });
    while (stopping.StateOf(blocker) != JobState::Running) std::this_thread::yield();
    std::atomic<bool> sawSubmission{false};
    std::thread shutdown([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (stopping.Stats().submitted < 2 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        sawSubmission = stopping.Stats().submitted >= 2;
        stopping.Shutdown();
    });
    try {
        const auto cancelled = evaluate(&stopping, 4, 0);
        for (std::size_t i = 0; i < cancelled.size(); ++i)
            Check(cancelled[i].time == serial[i].time,
                  "concurrent pool shutdown joins CCD readers before owner fallback");
    } catch (...) { shutdown.join(); throw; }
    shutdown.join();
    Check(sawSubmission && stopping.IsShutDown(), "shutdown actually overlaps a submitted CCD batch");
    // A later event changes the motion without changing the entity generation.
    // Reusing an old no-impact or TOI answer would now be wrong.
    for (const auto& request : requests) {
        auto& body = world.bodies[request.key[0]];
        if (body.motion.Segments().back().begin == 0) {
            body.rigidBody.linearVelocity = {0, 10, 0};
            body.motion.ChangeVelocity(body.rigidBody, .25);
            world.InvalidateImpactPose(request.key[0]);
        }
    }
    world.impactQueryAnchorTime = .25;
    const auto changedSerial = evaluate(nullptr, 1, .25);
    auto changedParallel = evaluate(&jobs, 4, .25);
    if (staleQueryControl)
        for (std::size_t i = 0; i < changedParallel.size(); ++i)
            changedParallel[i].time = serial[i].time;
    for (std::size_t i = 0; i < changedParallel.size(); ++i)
        Check(changedSerial[i].time == changedParallel[i].time && changedParallel[i].time > 1,
              "retained worker scratch observes the changed event trajectory");
    jobs.Shutdown();
    const auto stopped = evaluate(&jobs, 4, .25);
    Check(world.work.ccdParallelBatches == 0, "stopped pool retains the serial fallback");
    for (std::size_t i = 0; i < stopped.size(); ++i)
        Check(stopped[i].time == changedSerial[i].time, "stopped-pool query matches independent serial result");
    physics.SetJobSystem(nullptr);
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--stale-bound")
        staleBoundControl = true;
    else if (argc == 2 && std::string(argv[1]) == "--stale-query-result")
        staleQueryControl = true;
    else if (argc != 1) {
        std::cerr << "usage: impact-discovery-tests [--stale-bound|--stale-query-result]\n";
        return 2;
    }
    ClearGapAndClosedBoundary();
    RotatedAndTranslatedContact();
    MovingEndpointAtEventTime();
    RotatingFutureEnclosure();
    ChangedTrajectoryInvalidatesEnclosure();
    UnsupportedAndUncertainBounds();
    ActualStepDiscovery();
    IndependentQueryBatch();
    std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures
              << ",\"stale_bound_control\":" << (staleBoundControl ? "true" : "false")
              << ",\"stale_query_control\":" << (staleQueryControl ? "true" : "false") << "}\n";
    return failures ? 1 : 0;
}
