#include "VisualParticles.h"
#include "UniformGravity.h"
#include "particles/CollisionSnapshot.h"
#include "particles/CpuParticles.h"
#include "particles/ParticleRandom.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

using namespace JudasParticles;
namespace { int allocationFailureCountdown = -1; }
void* operator new(std::size_t size) {
    if (allocationFailureCountdown == 0) throw std::bad_alloc();
    if (allocationFailureCountdown > 0) --allocationFailureCountdown;
    if (void* storage = std::malloc(size ? size : 1)) return storage;
    throw std::bad_alloc();
}
void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
namespace {
unsigned checks = 0, failures = 0;
const UniformGravity zero({0, 0, 0}), down({0, -10, 0});
void Check(bool condition, const char* message) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
}
bool Near(glm::dvec3 a, glm::dvec3 b, double tolerance = 1e-4) {
    return glm::length(a - b) <= tolerance;
}
std::shared_ptr<CollisionSnapshot> Capture(const PhysicsWorld& world, double dt = .5) {
    auto snapshot = std::make_shared<CollisionSnapshot>();
    std::string error;
    if (!snapshot->Capture(world, {glm::vec3(-100), glm::vec3(100)}, dt, error, {},
                          [](BodyHandle h) { return std::uint64_t(h.id); }))
        throw std::runtime_error(error);
    return snapshot;
}
ParticleEmitterSettings Settings() {
    ParticleEmitterSettings s;
    s.loop = false; s.rate = 0; s.burst = 0; s.maxParticles = 16;
    s.lifetime = 10; s.collision = true; s.collisionRadius = .1f;
    s.size = s.endSize = .2f; s.spread = {0, 0, 0};
    s.velocity = s.velocityVariation = {0, 0, 0};
    s.restitution = 1; s.friction = 0;
    return s;
}
void KnownAnswers() {
    PhysicsWorld world;
    world.Init();
    world.CreateStaticBox({0, -.1f, 0}, {4, .1f, 4}, 0, 0);
    const auto snapshot = Capture(world);
    const auto before = world.GetTransform(snapshot->Bodies().front().handle);
    auto s = Settings();
    CpuParticles cpu;
    std::vector<VisualParticle> particles{{{0, 1, 0}, {0, -4, 0}, 0}};
    Check(cpu.Update(particles, s, .5f, {}, zero, snapshot, {}, 10) && particles.size() == 1 &&
              Near(particles[0].position, {0, 1.2001, 0}, 3e-4) && Near(particles[0].velocity, {0, 4, 0}),
          "CPU bounce matches analytic TOI and integrates the actual remaining interval");
    Check(std::abs(particles[0].age - .5f) < 1e-7f && cpu.Stats().contacts == 1,
          "one fixed step advances age once and reports one real contact");
    Check(world.GetTransform(snapshot->Bodies().front().handle).position == before.position &&
              world.GetLinearVelocity(snapshot->Bodies().front().handle) == glm::vec3(0),
          "visual collision has no hidden reaction on the authoritative rigid body");
    CpuParticles small, large;
    auto a = std::vector<VisualParticle>{{{0, 1, 0}, {0, -4, 0}, 0}}, b = a;
    auto giant = s; giant.size = giant.endSize = 20;
    small.Update(a, s, .5f, {}, zero, snapshot, {}, 1);
    large.Update(b, giant, .5f, {}, zero, snapshot, {}, 1);
    Check(a[0].position == b[0].position && a[0].velocity == b[0].velocity,
          "physical collision radius is independent of visual billboard size");
    CpuParticles slide;
    s.response = ParticleResponse::Slide;
    particles = {{{0, 1, 0}, {2, -4, 0}, 0}};
    slide.Update(particles, s, .5f, {}, zero, snapshot, {}, 1);
    Check(Near(particles[0].position, {1, .1001, 0}, 3e-4) && Near(particles[0].velocity, {2, 0, 0}),
          "slide preserves tangent motion without advancing through the floor");
}
void ReliableResume() {
    PhysicsWorld world;
    world.Init();
    const auto floor = world.CreateStaticBox({0, -.1f, 0}, {4, .1f, 4}, 0, 0);
    auto old = Capture(world);
    auto s = Settings();
    s.response = ParticleResponse::Remove;
    s.impacts = ParticleImpactDelivery::Reliable;
    s.maxImpactCells = 1; s.impactCellSize = .25f; s.impactWeight = 3;
    CpuParticles cpu;
    std::vector<VisualParticle> p{{{-1, 1, 0}, {0, -4, 0}, 0}, {{1, 1, 0}, {0, -4, 0}, 0}};
    Check(!cpu.Update(p, s, .5f, {}, zero, old, {100, 200, 300}, 42) && cpu.PendingStep() &&
              cpu.Stats().backpressure && cpu.Impacts().Pending().size() == 1 &&
              cpu.Impacts().Pending()[0].count == 1,
          "one-cell reliable queue blocks before publishing the second contact");
    Check(!cpu.Control("clear", p) && !cpu.Control("restart", p),
          "clear/restart cannot discard a pending reliable contact");
    Check(!cpu.Update(p, s, .5f, {}, zero, old, {}, 99) &&
              cpu.Impacts().Pending().size() == 1 && cpu.Impacts().Pending()[0].count == 1,
          "retrying a full queue duplicates neither the prior contact nor its weight");
    const auto first = cpu.Impacts().Take();
    Check(first.size() == 1 && first[0].tick == 42 && first[0].weight == 3 &&
              Near(first[0].position, {99, 200, 300}) && first[0].sequence == 1,
          "first reliable event retains exact original tick, absolute point, count and sequence");
    world.ResetBody(floor, {0, 10, 0}, {1, 0, 0, 0});
    const auto newer = Capture(world);
    Check(cpu.Update(p, s, .5f, {}, zero, newer, {999, 999, 999}, 99) && !cpu.PendingStep() && p.empty(),
          "drain resumes the captured historical step and finishes both removals exactly once");
    const auto second = cpu.Impacts().Take();
    Check(second.size() == 1 && second[0].sequence == 2 && second[0].tick == 42 &&
              second[0].weight == 3 && Near(second[0].position, {101, 200, 300}) &&
              cpu.Stats().deliveredImpacts == 2 && cpu.Stats().simulated == 2,
          "resumed event cannot adopt newer geometry/origin/tick or replay the first impact");

    PhysicsWorld corridor;
    corridor.Init();
    corridor.CreateStaticBox({1.1f, 0, 0}, {.1f, 4, 4}, 0, 0);
    corridor.CreateStaticBox({-1.1f, 0, 0}, {.1f, 4, 4}, 0, 0);
    s.response = ParticleResponse::Bounce; s.impactWeight = 1;
    CpuParticles repeated;
    p = {{{0, 0, 0}, {8, 0, 0}, 0}};
    const auto chart = Capture(corridor, 1);
    unsigned total = 0, batches = 0;
    bool done = false;
    while (!done && batches < 8) {
        done = repeated.Update(p, s, 1, {}, zero, chart, {}, 7);
        for (const auto& event : repeated.Impacts().Take()) {
            total += unsigned(event.count);
            Check(event.sequence == batches + 1 && event.tick == 7,
                  "each resumed corridor contact has one ordered historical sequence");
            ++batches;
        }
    }
    Check(done && total == 4 && batches == 4 && repeated.Stats().contacts == 4 &&
              p.size() == 1 && Near(p[0].position, {.8, 0, 0}, .002) && Near(p[0].velocity, {8, 0, 0}),
          "multiple impacts in one step conserve elastic speed and remaining travel through repeated backpressure");
}
void AttachmentsAndSettings() {
    PhysicsWorld world;
    world.Init();
    const auto floor = world.CreateStaticBox({0, -.1f, 0}, {4, .1f, 4}, 0, 0);
    auto s = Settings(); s.response = ParticleResponse::Stick;
    VisualParticlePool pool(s);
    pool.ImportTransient({{{-1, 1, 0}, {0, -10, 0}, 9.7f}, {{1, 1, 0}, {0, -10, 0}, .1f}}, 0, true, 3);
    pool.UpdateWorld(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 1);
    Check(pool.Particles().size() == 2 && Near(pool.Particles()[1].position, {1, .1001, 0}),
          "two distinct particles attach at independent body-local points");
    auto shortLife = s; shortLife.lifetime = .5f;
    pool.SetSettings(shortLife);
    pool.UpdateWorld(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 2);
    Check(pool.Particles().size() == 1 && Near(pool.Particles()[0].position, {1, .1001, 0}),
          "lifetime editing compacts particle and attachment identity together");
    auto local = shortLife; local.localSpace = true;
    pool.SetSettings(local);
    pool.Burst(1, {3, 2, 0}, {1, 0, 0, 0}, {1, 1, 1});
    pool.UpdateWorld(.1f, {3, 2, 0}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 3);
    Check(pool.Particles().size() == 1 && Near(pool.Particles()[0].position, {0, 0, 0}),
          "changing coordinate space cannot transfer an old attachment to a new birth");

    world.SetMotionType(floor, BodyMotionType::Kinematic);
    world.SetKinematicVelocity(floor, {2, 0, 0}, {0, 0, 0});
    world.Step(.1f);
    CpuParticles attached;
    std::vector<VisualParticle> p{{{0, .2f, 0}, {0, -2, 0}, 0}};
    attached.Update(p, s, .1f, {}, zero, Capture(world, .1), {}, 4);
    const double startX = p[0].position.x;
    world.Step(.1f);
    attached.Update(p, s, .1f, {}, zero, Capture(world, .1), {}, 5);
    Check(p.size() == 1 && std::abs(p[0].position.x - startX - .2) < 2e-4,
          "stuck particle follows real translating body's COM-local anchor");
    world.DestroyBody(floor);
    attached.Update(p, s, .1f, {}, down, Capture(world, .1), {}, 6);
    // Existing velocity is the boundary velocity; no persistent anchor survives
    // destruction. A following gravity-enabled step must be free to fall.
    auto falling = s; falling.useGravity = true;
    const auto y = p[0].position.y;
    attached.Update(p, falling, .1f, {}, down, Capture(world, .1), {}, 7);
    Check(p[0].position.y < y - .09f, "destroyed receiver releases attachment and resumes actual gravity motion");
}
void LocalFramesAndBirths() {
    PhysicsWorld world;
    world.Init();
    world.CreateStaticBox({1, 0, 0}, {.1f, 4, 4}, 0, 0);
    auto s = Settings(); s.localSpace = true; s.response = ParticleResponse::Remove;
    VisualParticlePool fresh(s);
    fresh.UpdateWorld(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 1);
    fresh.Burst(1, {2, 0, 0}, {1, 0, 0, 0}, {1, 1, 1});
    fresh.UpdateWorld(.1f, {2, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 2);
    Check(fresh.Particles().size() == 1 && Near(fresh.Particles()[0].position, {}),
          "new local birth starts at its actual current emitter pose without a phantom swept path");
    auto startupSettings=s;startupSettings.burst=1;
    VisualParticlePool startup(startupSettings);
    startup.UpdateWorld(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 1);
    Check(startup.Particles().size()==1&&startup.Particles()[0].age==0,
          "startup emission records an actual birth at the completed tick boundary");
    startup.UpdateWorld(.1f, {2, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 2);
    Check(startup.Particles().empty(),
          "zero-age birth from the previous tick still sweeps its genuine first carrier motion");
    VisualParticlePool older(s);
    older.ImportTransient({{{0, 0, 0}, {0, 0, 0}, .1f}}, 0, true, 2);
    older.UpdateWorld(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 1);
    older.UpdateWorld(.1f, {2, 0, 0}, {1, 0, 0, 0}, {1, 1, 1}, zero, Capture(world, .1), {}, 2);
    Check(older.Particles().empty(), "older local particle still sweeps its real carried trajectory through a wall");
    PhysicsWorld expiryWorld;
    expiryWorld.Init();
    expiryWorld.CreateStaticBox({1.6f, 0, 0}, {.1f, 4, 4}, 0, 0);
    CpuParticles expiring;
    auto expiry = s; expiry.impacts = ParticleImpactDelivery::Reliable;
    std::vector<VisualParticle> dying{{{0, 0, 0}, {0, 0, 0}, 1}};
    expiring.Update(dying, expiry, .1f, {}, zero, Capture(expiryWorld, .1), {}, 1);
    dying[0].age = 9.95f;
    expiring.Update(dying, expiry, .1f, {{2, 0, 0}, {1, 1, 1}, {1, 0, 0, 0}}, zero,
                    Capture(expiryWorld, .1), {}, 2);
    Check(dying.empty() && expiring.Impacts().Pending().empty(),
          "partial-lifetime local particle cannot hit a wall beyond the emitter's actual pre-expiry travel");
    auto free = s; free.collision = false; free.backend = ParticleBackend::Auto;
    free.useGravity = true; free.response = ParticleResponse::Bounce;
    CpuParticles local;
    std::vector<VisualParticle> p{{{0, 0, 0}, {0, 0, 0}, 0}};
    const auto rotation = glm::angleAxis(float(3.141592653589793 / 2), glm::vec3(0, 0, 1));
    local.Update(p, free, .1f, {{5, 6, 7}, {2, 3, 4}, rotation}, down, nullptr, {}, 1);
    const auto worldPoint = glm::dvec3(5, 6, 7) + glm::dvec3(rotation * (p[0].position * glm::vec3(2, 3, 4)));
    Check(Near(worldPoint, {5, 5.9, 7}, 1e-5),
          "local nonuniform scale and rotated frame preserve independent world gravity displacement");
}
void ImpactsAndLifecycle() {
    auto s = Settings(); s.impacts = ParticleImpactDelivery::Reliable; s.maxImpactCells = 1;
    ParticleImpacts queue;
    ParticleImpact record;
    record.receiver = 11; record.geometryRevision = 9; record.count = 2; record.weight = 3;
    record.position = {1, 2, 3}; record.localPosition = {.1, .1, .1};
    record.normal = record.localNormal = {0, 1, 0};
    Check(queue.Add(record, s), "reliable impact accepts a finite receiver cell");
    record.count = 3; record.weight = 5; record.position = {3, 4, 5};
    Check(queue.Add(record, s) && queue.Pending().size() == 1 && queue.Pending()[0].count == 5 &&
              queue.Pending()[0].weight == 8 && Near(queue.Pending()[0].position, {2.2, 3.2, 4.2}),
          "same-cell aggregation preserves count/weight and independently expected count-weighted location");
    ParticleImpacts restored;
    std::string error;
    Check(restored.Restore(queue.Pending(), queue.NextSequence(), error),
          "pending reliable records and next sequence restore as an ordered historical batch");
    auto spacious = s; spacious.maxImpactCells = 8;
    auto newCell = record; newCell.localPosition.x = 4;
    Check(!restored.Add(newCell, spacious) && restored.Pending().size() == 1 &&
              restored.Pending()[0].count == 5,
          "restored historical batch backpressures new records even when spare capacity exists");
    auto invalid = queue.Pending(); invalid[0].sequence = queue.NextSequence();
    Check(!restored.Restore(invalid, queue.NextSequence(), error) && restored.Pending()[0].count == 5,
          "invalid restored sequence cannot partly overwrite accepted impact delivery");
    const auto delivered = restored.Take();
    record.localPosition.x = 1;
    Check(delivered[0].count == 5 && restored.Delivered() == 5 && restored.Add(record, s) &&
              restored.Pending()[0].sequence == 2,
          "restored history drains exactly once and new events continue monotonic sequence");
    auto cosmetic = s; cosmetic.impacts = ParticleImpactDelivery::Cosmetic;
    record.localPosition.x = 2;
    Check(restored.Add(record, cosmetic) && restored.Dropped() == 3 && restored.Pending()[0].count == 3,
          "cosmetic overflow is counted without mutating an accepted prior record");
    for (int failure = 0; failure < 3; ++failure) {
        ParticleImpacts transactional;
        allocationFailureCountdown = failure;
        bool accepted = false;
        try { accepted = transactional.Add(record, s); } catch (const std::bad_alloc&) {}
        allocationFailureCountdown = -1;
        Check(!accepted && transactional.Pending().empty() && transactional.NextSequence() == 1 &&
                  transactional.Add(record, s) && transactional.Pending().size() == 1 &&
                  transactional.Pending()[0].sequence == 1,
              "allocation-failure injection preserves queue/index/sequence and allows a clean retry");
    }
    ParticleImpacts overflow;
    auto huge = record; huge.count = std::numeric_limits<std::uint64_t>::max();
    Check(overflow.Add(huge, s) && !overflow.Add(record, s) &&
              overflow.Pending()[0].count == std::numeric_limits<std::uint64_t>::max(),
          "same-cell count overflow is explicit backpressure without partial mutation");

    auto life = Settings(); life.collision = false; life.backend = ParticleBackend::Auto;
    life.loop = true; life.rate = 10; life.velocity = {1, 0, 0};
    VisualParticlePool pool(life);
    pool.Update(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero);
    const auto initial = pool.Particles()[0];
    Check(pool.Control("pause"), "pause is accepted");
    pool.Update(.5f, {}, {1, 0, 0, 0}, {1, 1, 1}, down);
    Check(pool.Particles().size() == 1 && pool.Particles()[0].position == initial.position &&
              pool.Particles()[0].age == initial.age,
          "paused particle simulation and emission do not advance");
    pool.Control("play"); pool.Update(.1f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero);
    const auto count = pool.Particles().size();
    const float age = pool.Particles()[0].age;
    pool.Control("stop"); pool.Update(.2f, {}, {1, 0, 0, 0}, {1, 1, 1}, zero);
    Check(!pool.Emitting() && pool.Particles().size() == count &&
              std::abs(pool.Particles()[0].age - age - .2f) < 1e-6,
          "stop halts births while live particles continue to simulate");
    Check(pool.Control("restart") && pool.Particles().empty(), "restart clears transient population at a safe boundary");
    pool.Burst(1, {}, {1, 0, 0, 0}, {1, 1, 1});
    Check(pool.Particles().size() == 1 && pool.Particles()[0].position == initial.position &&
              pool.Particles()[0].velocity == initial.velocity,
          "restart resets seeded birth identity rather than resuming an unrelated random stream");
    auto varied = life; varied.seed = 12345; varied.spread = {1, 2, 3};
    varied.velocity = {4, 5, 6}; varied.velocityVariation = {.5f, 1, 1.5f};
    const auto birth = SampleParticleBirth(varied, 1);
    Check(ParticleBirthSeed(12345, 1) == 3020340486u && birth.state == 1310407644u &&
              Near(birth.position, {.04434013366699219, -.26555776596069336, 2.1988415718078613}, 1e-7) &&
              Near(birth.velocity, {3.770275831222534, 4.545860290527344, 5.415308952331543}, 1e-7),
          "keyed RNG matches independent uint32 and binary32 golden values");
    VisualParticlePool matching(varied);
    matching.Burst(1, {}, {1, 0, 0, 0}, {1, 1, 1});
    Check(matching.Particles()[0].position == birth.position && matching.Particles()[0].velocity == birth.velocity,
          "CPU extended births consume the same explicit keyed six-draw contract as GPU");
}
} // namespace
int main() {
    try {
        KnownAnswers(); ReliableResume(); AttachmentsAndSettings();
        LocalFramesAndBirths(); ImpactsAndLifecycle();
    } catch (const std::exception& error) {
        Check(false, error.what());
    }
    std::printf("CPU particles: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
