// Physical contact and optional stand-off clearance are different contracts.
// These controls use production FluidWorld and the native particle-row adapter;
// no replacement pressure law, damping, velocity cap or iteration tuning.
#include "FluidWorld.h"
#include "UniformGravity.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, failures = 0;
void Check(bool value, const std::string& description) {
    ++checks;
    failures += !value;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", description.c_str());
}
bool Near(glm::vec3 a, glm::vec3 b, float tolerance = 2e-5f) {
    return glm::length(a - b) <= tolerance;
}
FluidBoxCollider Box(unsigned id, glm::vec3 position, glm::vec3 half,
                     const glm::quat& rotation = {1, 0, 0, 0}) {
    FluidBoxCollider box;
    box.owner = {id, 73001};
    box.previousPose = box.currentPose = {position, rotation};
    box.halfExtents = half;
    return box;
}

void CapturedTargets(const glm::quat& rotation) {
    PhysicsWorld world;
    world.Init();
    const float h = (1.f / 30) / 3;
    const glm::vec3 original = rotation * glm::vec3(-.263729572f, -.226283818f, -.252897322f);
    const auto floorNormal = rotation * glm::vec3(0, 1, 0);
    const auto bodyNormal = rotation * glm::vec3(.00485873409f, -.997732878f, .0671230927f);
    const auto bodyVelocity = rotation * glm::vec3(.0629138947f, -6.1229682f, -5.90452909f);
    std::vector<PhysicsWorld::ContactParticle> particles{{{0, 0, 0}, original, 125}};
    std::vector<PhysicsWorld::ParticleBoundaryContact> rows{
        {0, {}, {0, 0, 0}, floorNormal, {0, 0, 0}, false},
        {0, {}, {0, 0, 0}, bodyNormal, bodyVelocity, false}};
    rows[0].minimumNormalVelocity = -.125327468f / h;
    rows[1].minimumNormalVelocity = -.180000007f / h;
    std::vector<glm::vec3> impulses;
    // This is deliberately NOT h: cached rigid support and fluid clearance
    // targets have separate time horizons.
    world.SolveParticleContacts(particles, rows, impulses, nullptr, .17f);
    Check(particles[0].velocity == original, "captured separated rows add no velocity or energy");
    Check(impulses.size() == 2 && impulses[0] == glm::vec3(0) && impulses[1] == glm::vec3(0),
          "captured gap targets need zero impulse");
    Check(rows[1].wallVelocity == bodyVelocity, "separation is not encoded as fake wall velocity");
    const auto velocity = particles[0].velocity;
    Check(.125327468f + h * glm::dot(floorNormal, velocity) > 0 &&
              .180000007f + h * glm::dot(bodyNormal, velocity - bodyVelocity) > 0,
          "captured unchanged velocity has positive predicted physical gaps");

    // Explicit negative targets are real lower bounds, not disabled contacts.
    particles[0].velocity = -floorNormal;
    rows.resize(1);
    rows[0].minimumNormalVelocity = -.5f;
    world.SolveParticleContacts(particles, rows, impulses);
    Check(Near(particles[0].velocity, -.5f * floorNormal), "speculative row enforces its exact negative normal target");
    particles[0].velocity = -floorNormal;
    rows[0].minimumNormalVelocity = 0;
    world.SolveParticleContacts(particles, rows, impulses);
    Check(Near(particles[0].velocity, glm::vec3(0)), "default touching target preserves inelastic hard response");
}

// One stationary particle remains physically separate from a descending box
// throughout this small interval. Only their preferred clearance layers overlap.
void Squeeze(const glm::quat& rotation, int substeps, bool reversed, bool callback, bool corner) {
    const std::string label = std::string(corner ? "corner" : "face") + "/" +
        std::to_string(substeps) + "substeps/" + (reversed ? "reverse/" : "forward/") +
        (callback ? "native" : "fallback");
    const float dt = .04f, h = dt / substeps;
    FluidSettings settings;
    settings.particleRadius = .18f;
    settings.substeps = substeps;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    fluid.AddParticle(rotation * glm::vec3(corner ? .594f : 0, .18f, 0), {}, 125);
    const auto tilt = glm::angleAxis(-.08f, glm::vec3(1, 0, 0));
    auto floor = Box(1, rotation * glm::vec3(0, -.25f, 0), {2, .25f, 2}, rotation);
    auto falling = Box(2, rotation * glm::vec3(0, .48f, 0), {.5f, .1f, .5f}, rotation * tilt);
    falling.currentPose.position = rotation * glm::vec3(0, .38f, 0);
    std::vector<FluidBoxCollider> boxes = reversed ? std::vector<FluidBoxCollider>{falling, floor}
                                                : std::vector<FluidBoxCollider>{floor, falling};
    PhysicsWorld native;
    native.Init();
    unsigned yielded = 0, cornerNormals = 0;
    bool targets = true, points = true, normals = true, wallVelocity = true;
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
            if (c.minimumNormalVelocity < 0) {
                ++yielded;
                const auto& box = c.owner.id == 1 ? floor : falling;
                const auto center = glm::mix(box.previousPose.position, box.currentPose.position, c.stepFraction);
                const auto inverse = glm::inverse(box.currentPose.rotation);
                const auto local = inverse * (c.particlePosition - center);
                const auto closestLocal = glm::clamp(local, -box.halfExtents, box.halfExtents);
                const auto closest = center + box.currentPose.rotation * closestLocal;
                const auto offset = c.particlePosition - closest;
                const float gap = glm::length(offset);
                targets &= gap > 0 && std::abs(c.minimumNormalVelocity + gap / h) < 3e-4f;
                points &= Near(c.point, closest, 3e-5f);
                normals &= gap > 0 && Near(c.normal, offset / gap, 3e-4f);
                wallVelocity &= Near(c.wallVelocity,
                    (box.currentPose.position - box.previousPose.position) / dt, 3e-4f);
                if (c.owner.id == 2 && std::abs((glm::inverse(rotation) * c.normal).x) > .1f)
                    ++cornerNormals;
            }
        }
        native.SolveParticleContacts(input, rows, impulses, nullptr, .17f);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity gravity({0, 0, 0});
    fluid.Step(dt, gravity, boxes, {}, {}, nullptr, callback ? response : FluidVelocityBatchResponse{});
    const auto& p = fluid.Particles()[0];
    Check(fluid.Particles().size() == 1 && p.mass == 125, label + " preserves particle count/mass");
    // Use the authored physical point, not the solver's output, to predict
    // the exact single active speculative bound. With one 40 ms substep,
    // this tilted box would close a further 0.32 mm beyond the remaining gap
    // at unchanged velocity. Its legitimate minimum response is about 8 mm/s;
    // an exact-zero oracle would incorrectly reject that physical contract.
    const auto knownPosition = rotation * glm::vec3(corner ? .594f : 0, .18f, 0);
    const auto localKnown = glm::inverse(falling.currentPose.rotation) *
        (knownPosition - falling.currentPose.position);
    const auto closestKnown = glm::clamp(localKnown, -falling.halfExtents, falling.halfExtents);
    const float knownGap = glm::length(localKnown - closestKnown);
    const auto knownNormal = glm::normalize(falling.currentPose.rotation * (localKnown - closestKnown));
    const auto actualWallVelocity = (falling.currentPose.position - falling.previousPose.position) / dt;
    const float requiredImpulseVelocity = std::max(0.f,
        -knownGap / h + glm::dot(knownNormal, actualWallVelocity));
    const auto expectedVelocity = requiredImpulseVelocity * knownNormal;
    Check(Near(p.velocity, expectedVelocity, 3e-5f),
          label + " unavailable stand-off enforces the independently predicted physical gap bound");
    for (const auto& box : boxes) {
        const auto local = glm::inverse(box.currentPose.rotation) * (p.position - box.currentPose.position);
        Check(!glm::all(glm::lessThan(glm::abs(local), box.halfExtents)), label + " keeps actual physical solid exclusion");
    }
    Check(fluid.GetDiagnostics().unresolvedGeometryCount == 0, label + " retains verified geometry");
    if (callback) {
        Check(yielded > 0, label + " reports genuine yielding clearance rows");
        Check(targets, label + " uses actual physical gap and fluid substep horizon");
        Check(points && normals, label + " revalidates nearest physical feature at final position");
        Check(wallVelocity, label + " retains actual material velocity");
        if (corner) Check(cornerNormals > 0, label + " reorients outside-face witness toward real corner");
    }
    std::printf("MEASURE %s velocity=(%.9g,%.9g,%.9g) yielded=%u corner=%u\n",
        label.c_str(), p.velocity.x, p.velocity.y, p.velocity.z, yielded, cornerNormals);
}

// All three expanded faces meet at the starting point. Geometric stand-off
// can therefore be satisfied exactly while the zero-gap velocity constraints
// still demand an instantaneous wedge jet from a distant moving solid.
void CompetingPreferredSkins(const glm::quat& rotation, bool callback, bool moving) {
    const float dt = .01f, radius = .18f;
    const glm::vec3 point(0, radius, radius);
    const glm::vec3 inward(0, -.001f, -.001f);
    const auto normal = glm::normalize(glm::vec3(.2527f, -.0102f, -.9675f));
    const auto axis = glm::normalize(glm::cross(glm::vec3(0, 1, 0), normal));
    const auto oblique = glm::angleAxis(std::acos(normal.y), axis);
    FluidSettings settings;
    settings.particleRadius = radius;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    fluid.AddParticle(rotation * point, rotation * inward, 125);
    const auto floor = Box(21, rotation * glm::vec3(0, -.25f, 0), {2, .25f, 2}, rotation);
    const auto wall = Box(22, rotation * glm::vec3(0, 0, -.25f), {2, 2, .25f}, rotation);
    // A one-micrometre soft overlap ensures endpoint slab ownership despite
    // rotation roundoff; this is inside the existing feasibility roundoff band.
    auto movingBox = Box(23, rotation * (point - normal * (radius + .1f - 1e-6f)), {.5f, .1f, .5f}, rotation * oblique);
    if (moving) movingBox.previousPose.position -= rotation * (normal * (6 * dt));
    unsigned negativeTargets = 0;
    bool physicalGaps = true;
    PhysicsWorld native;
    native.Init();
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            negativeTargets += c.minimumNormalVelocity < 0;
            const auto& box = c.owner.id == 21 ? floor : c.owner.id == 22 ? wall : movingBox;
            const auto local = glm::inverse(box.currentPose.rotation) * (c.particlePosition - box.currentPose.position);
            const auto closest = glm::clamp(local, -box.halfExtents, box.halfExtents);
            const auto physicalGap = glm::length(local - closest);
            physicalGaps &= std::abs(physicalGap - radius) < 3e-5f;
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity zero({0, 0, 0});
    fluid.Step(dt, zero, {floor, wall, movingBox}, {}, {}, nullptr,
               callback ? response : FluidVelocityBatchResponse{});
    const auto velocity = fluid.Particles()[0].velocity;
    const std::string label = std::string("competing feasible skins/") +
        (callback ? "native/" : "fallback/") + (moving ? "closing" : "stationary");
    // Known answer: with 18 cm of physical clearance and only 6 cm of wall
    // travel, no physical surface reaches this particle in the interval.
    Check(moving ? Near(velocity, rotation * inward, 3e-5f) :
              glm::length(velocity) <= glm::length(inward) + 3e-5f,
          label + " does not invent instantaneous wedge escape");
    if (callback) {
        Check(physicalGaps, label + " individually fits every preferred clearance at publication");
        if (moving) Check(negativeTargets >= 2, label + " yields mutually competing soft preferences");
        else Check(negativeTargets == 0, label + " stationary compatible preferences retain zero targets");
    }
    Check(fluid.GetDiagnostics().unresolvedGeometryCount == 0, label + " preserves real geometry exclusion");
    std::printf("MEASURE %s velocity=(%.9g,%.9g,%.9g) negativeTargets=%u\n",
        label.c_str(), velocity.x, velocity.y, velocity.z, negativeTargets);
}

// Two orthogonal preferred skins have no competition. They retain the
// established zero-inward-velocity stand-off behaviour, even though their
// physical surfaces are still a radius away.
void OrthogonalPreferredSkins(const glm::quat& rotation, bool callback) {
    FluidSettings settings;
    settings.particleRadius = .18f;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    fluid.AddParticle(rotation * glm::vec3(0, .18f, .18f),
                      rotation * glm::vec3(0, -.001f, -.001f), 125);
    const auto floor = Box(31, rotation * glm::vec3(0, -.25f, 0), {2, .25f, 2}, rotation);
    const auto wall = Box(32, rotation * glm::vec3(0, 0, -.25f), {2, 2, .25f}, rotation);
    PhysicsWorld native;
    native.Init();
    unsigned rowCount = 0;
    bool zeroTargets = true;
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            ++rowCount;
            zeroTargets &= c.minimumNormalVelocity == 0;
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity zero({0, 0, 0});
    fluid.Step(.01f, zero, {floor, wall}, {}, {}, nullptr,
               callback ? response : FluidVelocityBatchResponse{});
    Check(Near(fluid.Particles()[0].velocity, {}, 3e-5f),
          "feasible orthogonal soft skins retain zero inward velocity");
    Check(Near(fluid.Particles()[0].position, rotation * glm::vec3(0, .18f, .18f), 3e-5f),
          "feasible orthogonal soft skins preserve preferred geometry");
    if (callback) Check(rowCount == 2 && zeroTargets,
                        "both noncompeting soft skins retain ordinary zero targets");
}

// The walls remain physically separated, but their prescribed closing speed
// makes a real (negative-target) speculative impulse necessary. Every row must
// remain satisfied after all other rows: a later wedge impulse may not undo an
// earlier wall bound.
void SimultaneousYieldingResponse(const glm::quat& rotation, bool callback, bool narrow = false,
                                 bool reversed = false) {
    const float dt = .01f, radius = .18f;
    const glm::vec3 point(0, radius, radius);
    // The 30-degree wedge converges through ordinary PGS. The original
    // captured narrower wedge needs a certified projection at the same budget.
    const glm::vec3 normal = narrow ?
        glm::normalize(glm::vec3(.2527f, -.0102f, -.9675f)) :
        glm::vec3(.5f, 0, -std::sqrt(.75f));
    const auto axis = glm::normalize(glm::cross(glm::vec3(0, 1, 0), normal));
    const auto oblique = glm::angleAxis(std::acos(normal.y), axis);
    FluidSettings settings;
    settings.particleRadius = radius;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    const auto incoming = rotation * glm::vec3(0, -.001f, -.001f);
    constexpr float particleMass = 125;
    fluid.AddParticle(rotation * point, incoming, particleMass);
    const auto floor = Box(41, rotation * glm::vec3(0, -.25f, 0), {2, .25f, 2}, rotation);
    const auto wall = Box(42, rotation * glm::vec3(0, 0, -.25f), {2, 2, .25f}, rotation);
    auto moving = Box(43, rotation * (point - normal * (radius + .1f - 1e-6f)),
                      {.5f, .1f, .5f}, rotation * oblique);
    moving.previousPose.position -= rotation * (normal * (40 * dt));
    PhysicsWorld native;
    native.Init();
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity zero({0, 0, 0});
    std::vector<FluidBoxCollider> boxes{floor, wall, moving};
    if (reversed) std::reverse(boxes.begin(), boxes.end());
    std::vector<FluidContactImpulse> reactions;
    fluid.Step(dt, zero, boxes, {}, {}, &reactions,
               callback ? response : FluidVelocityBatchResponse{});
    const auto& particle = fluid.Particles()[0];
    float residual = 0;
    bool outside = true;
    for (const auto& box : boxes) {
        const auto local = glm::inverse(box.currentPose.rotation) * (particle.position - box.currentPose.position);
        const auto closest = glm::clamp(local, -box.halfExtents, box.halfExtents);
        const float gap = glm::length(local - closest);
        outside &= gap > 0;
        const auto n = glm::normalize(box.currentPose.rotation * (local - closest));
        const auto wallVelocity = (box.currentPose.position - box.previousPose.position) / dt;
        residual = std::max(residual, -gap / dt - glm::dot(n, particle.velocity - wallVelocity));
    }
    const std::string label = callback ? "native" : "fallback";
    Check(outside, label + " actual-impulse wedge retains physical solid exclusion");
    Check(glm::length(particle.velocity) > 1, label + " actual-impulse wedge exercises active speculative response");
    if (narrow) {
        const auto diagnostics = fluid.GetDiagnostics();
        const auto nativeStats = native.GetParticleContactStats();
        const bool cap = callback ? nativeStats.iterationCapReached :
                                    diagnostics.fallbackContactIterationCaps > 0;
        const float reported = callback ? nativeStats.maximumClosingSpeed :
                                          diagnostics.maximumFallbackContactResidual;
        Check(!cap && residual <= 1e-5f * 40.f,
              label + " narrow wedge satisfies all physical rows within the solver residual budget");
        Check(std::abs(reported - residual) <= 64 * std::numeric_limits<float>::epsilon() *
                  std::max(40.f, glm::length(particle.velocity)),
              label + " narrow wedge diagnostics agree with independent physical residual");
        const auto rowAt = [&](const FluidBoxCollider& box) {
            const auto local = glm::inverse(box.currentPose.rotation) *
                (particle.position - box.currentPose.position);
            const auto difference = glm::dvec3(box.currentPose.rotation *
                (local - glm::clamp(local, -box.halfExtents, box.halfExtents)));
            const auto n = glm::normalize(difference);
            const auto boundaryVelocity = glm::dvec3(
                (box.currentPose.position - box.previousPose.position) / dt);
            return std::pair{n, -glm::length(difference) / dt -
                glm::dot(n, glm::dvec3(incoming) - boundaryVelocity)};
        };
        // Analytic two-plane witness in the wall-normal plane. The floor is
        // strictly slack, and both multipliers are positive.
        const auto [a, demandA] = rowAt(wall);
        const auto [b, demandB] = rowAt(moving);
        const auto [f, demandFloor] = rowAt(floor);
        const auto side = b - glm::dot(a, b) * a;
        const double pushB = (demandB - demandA * glm::dot(a, b)) / glm::dot(side, side);
        const double pushA = demandA - pushB * glm::dot(a, b);
        const auto correction = demandA * a + pushB * side;
        Check(pushA > 0 && pushB > 0 && glm::dot(correction, f) > demandFloor &&
              glm::length(glm::dvec3(particle.velocity - incoming) - correction) <= 2e-5 * 40,
              label + " narrow wedge agrees with the least-change two-plane velocity");
        glm::dvec3 reaction(0);
        for (const auto& row : reactions) reaction += glm::dvec3(row.impulse);
        Check(glm::length(reaction + double(particleMass) * glm::dvec3(particle.velocity - incoming))
                  <= 2e-5 * particleMass * 40,
              label + " narrow wedge published reactions close particle momentum");
        std::printf("MEASURE narrow active-impulse wedge/%s physical_residual=%.9g reported=%.9g cap=%d\n",
            label.c_str(), residual, reported, int(cap));
    } else {
        Check(residual <= 2e-5f * std::max(40.f, glm::length(particle.velocity)),
              label + " later yielding row cannot undo an earlier gap bound");
        std::printf("MEASURE actual-impulse wedge/%s velocity=(%.9g,%.9g,%.9g) residual=%.9g\n",
            label.c_str(), particle.velocity.x, particle.velocity.y, particle.velocity.z, residual);
    }
}

void StationaryOpposedChannel(const glm::quat& rotation, bool callback, bool reversed, float direction) {
    const float radius = .18f;
    FluidSettings settings;
    settings.particleRadius = radius;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    const auto initial = rotation * glm::vec3(direction * .001f, 0, 0);
    fluid.AddParticle({}, initial, 125);
    const auto left = Box(51, rotation * glm::vec3(-radius - .25f, 0, 0), {.25f, 2, 2}, rotation);
    const auto right = Box(52, rotation * glm::vec3(radius + .25f, 0, 0), {.25f, 2, 2}, rotation);
    std::vector<FluidBoxCollider> boxes = reversed ? std::vector<FluidBoxCollider>{right, left} :
                                                   std::vector<FluidBoxCollider>{left, right};
    PhysicsWorld native;
    native.Init();
    unsigned count = 0, negativeTargets = 0;
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            ++count;
            negativeTargets += c.minimumNormalVelocity < 0;
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity zero({0, 0, 0});
    fluid.Step(.01f, zero, boxes, {}, {}, nullptr,
               callback ? response : FluidVelocityBatchResponse{});
    const auto& particle = fluid.Particles()[0];
    Check(Near(particle.position, {}, 3e-5f), "stationary channel exactly 2r preserves fitted stand-off geometry");
    Check(glm::length(particle.velocity) <= glm::length(initial) + 3e-5f,
          "stationary channel exactly 2r does not create energy");
    Check(fluid.GetDiagnostics().unresolvedGeometryCount == 0,
          "stationary channel exactly 2r retains verified physical exclusion");
    if (callback) Check(count > 0 && (count != 1 || negativeTargets == 0),
                        "single retained feasible channel face keeps ordinary zero target");
    std::printf("MEASURE stationary channel/%s velocity=(%.9g,%.9g,%.9g) rows=%u negativeTargets=%u\n",
        callback ? "native" : "fallback", particle.velocity.x, particle.velocity.y, particle.velocity.z,
        count, negativeTargets);
}

// Opposing normal directions do not alone make preferred skins dynamically
// incompatible. Opening walls admit a stationary reference; a translated
// assembly admits its common material velocity. Both keep ordinary stand-off.
void CompatibleOpposingSkins(const glm::quat& rotation, bool commonTranslation, bool callback,
                              bool roundedEndpoints = false) {
    const float dt = commonTranslation && !roundedEndpoints ? 1.f / 64 : .01f;
    const float radius = .18f;
    const glm::vec3 point(0, radius, radius);
    const auto normal = glm::normalize(glm::vec3(.2527f, -.0102f, -.9675f));
    const auto axis = glm::normalize(glm::cross(glm::vec3(0, 1, 0), normal));
    const auto oblique = glm::angleAxis(std::acos(normal.y), axis);
    const auto translation = commonTranslation ?
        (roundedEndpoints ? rotation * glm::vec3(1.3f, -.4f, .7f) : glm::vec3(3, -1, 2)) :
        glm::vec3(0);
    FluidSettings settings;
    settings.particleRadius = radius;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    fluid.AddParticle(rotation * point, translation + rotation * glm::vec3(0, -.001f, -.001f), 125);
    auto floor = Box(61, rotation * glm::vec3(0, -.25f, 0), {2, .25f, 2}, rotation);
    auto wall = Box(62, rotation * glm::vec3(0, 0, -.25f), {2, 2, .25f}, rotation);
    auto obliqueBox = Box(63, rotation * (point - normal * (radius + .1f - 1e-6f)),
                          {.5f, .1f, .5f}, rotation * oblique);
    if (commonTranslation) {
        floor.currentPose.position += translation * dt;
        wall.currentPose.position += translation * dt;
        obliqueBox.currentPose.position += translation * dt;
    } else {
        // The oblique box moves away. Its skin remains observed from its
        // initial overlap, while the actual physical solid stays separated.
        obliqueBox.previousPose.position += rotation * (normal * (6 * dt));
    }
    const std::vector<FluidBoxCollider> boxes{floor, wall, obliqueBox};
    // An exact common-motion oracle must first establish the actual stored
    // geometry has that motion. Arbitrary float endpoint additions can encode
    // slightly different velocities, even when the requested boost is shared.
    if (commonTranslation && !roundedEndpoints) for (const auto& box : boxes) {
        const auto represented = glm::dvec3(box.currentPose.position) - glm::dvec3(box.previousPose.position);
        Check(represented == glm::dvec3(translation * dt),
              "exact common-motion fixture stores identical COM displacement before physics");
    }
    PhysicsWorld native;
    native.Init();
    unsigned rowCount = 0;
    bool zeroTargets = true, physicalGaps = true, actualVelocities = true, validTargets = true;
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            ++rowCount;
            zeroTargets &= c.minimumNormalVelocity == 0;
            const auto& box = c.owner.id == 61 ? floor : c.owner.id == 62 ? wall : obliqueBox;
            const auto local = glm::inverse(box.currentPose.rotation) * (c.particlePosition - box.currentPose.position);
            const auto closest = glm::clamp(local, -box.halfExtents, box.halfExtents);
            const float gap = glm::length(local - closest);
            physicalGaps &= std::abs(gap - radius) < 3e-5f;
            validTargets &= c.minimumNormalVelocity == 0 ||
                std::abs(c.minimumNormalVelocity + gap / dt) < 3e-4f;
            const auto expectedVelocity = (box.currentPose.position - box.previousPose.position) / dt;
            actualVelocities &= Near(c.wallVelocity, expectedVelocity, 3e-5f);
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity zero({0, 0, 0});
    fluid.Step(dt, zero, boxes, {}, {}, nullptr,
               callback ? response : FluidVelocityBatchResponse{});
    const std::string label = std::string(roundedEndpoints ? "rounded endpoint motion/" :
        commonTranslation ? "exact common translation/" : "opening wedge/") +
        (callback ? "native" : "fallback");
    if (roundedEndpoints) {
        // This intentionally retained input is not an exact common boost:
        // its stored collider COM deltas differ by several float ULPs. It
        // must not create a kick, but a zero-target certificate is optional.
        Check(glm::length(fluid.Particles()[0].velocity - translation) <=
                  glm::length(glm::vec3(0, -.001f, -.001f)) + 3e-5f,
              label + " does not create relative kinetic energy");
    } else {
        Check(Near(fluid.Particles()[0].velocity, translation, 3e-5f),
              label + " compatible preferred skins stop inward relative motion");
    }
    Check(fluid.GetDiagnostics().unresolvedGeometryCount == 0,
          label + " retains verified physical geometry");
    if (callback) {
        Check(rowCount == 3 && (roundedEndpoints ? validTargets : zeroTargets),
              label + (roundedEndpoints ? " retains valid zero or actual physical-gap targets" :
                                          " retains all three feasible preferred zero targets"));
        Check(physicalGaps && actualVelocities,
              label + " uses actual separated features and actual material velocities");
    }
}

// A real drift hits the floor, but never reaches the descending box. An
// unavailable preferred floor radius must not lift the element into that box
// and manufacture a second simultaneous hard collision episode.
void DriftAndProjectionEpisodes(const glm::quat& rotation, bool tilted, bool reversed, bool callback) {
    const float h = 1.f / 90;
    FluidSettings settings;
    settings.particleRadius = .18f;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    FluidWorld fluid(settings);
    const auto initial = rotation * glm::vec3(0, .0201816558838f, 0);
    const auto incoming = rotation * glm::vec3(0, -1.92534899712f, 0);
    fluid.AddParticle(initial, incoming, 125);
    const auto floor = Box(71, rotation * glm::vec3(0, -.25f, 0), {2, .25f, 2}, rotation);
    const float angle = tilted ? -.08f : 0;
    const auto tilt = glm::angleAxis(angle, glm::vec3(1, 0, 0));
    const float centerAbovePlane = .5f / std::cos(angle);
    auto body = Box(72, rotation * glm::vec3(0, .205857753754f + centerAbovePlane, 0),
                    {.5f, .5f, .5f}, rotation * tilt);
    body.currentPose.position = rotation * glm::vec3(0, .14740422368f + centerAbovePlane, 0);
    const std::vector<FluidBoxCollider> boxes = reversed ? std::vector<FluidBoxCollider>{body, floor} :
                                                         std::vector<FluidBoxCollider>{floor, body};
    PhysicsWorld native;
    native.Init();
    unsigned floorRows = 0;
    bool floorTarget = true, bodyTargets = true;
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            if (c.owner.id == 71) {++floorRows; floorTarget &= c.minimumNormalVelocity == 0;}
            if (c.owner.id == 72) {
                const auto local = glm::inverse(body.currentPose.rotation) *
                    (c.particlePosition - body.currentPose.position);
                const auto closest = glm::clamp(local, -body.halfExtents, body.halfExtents);
                const float gap = glm::length(local - closest);
                bodyTargets &= gap > 0 && c.minimumNormalVelocity < 0 &&
                    std::abs(c.minimumNormalVelocity + gap / h) < 3e-4f;
            }
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    UniformGravity zero({0, 0, 0});
    std::vector<FluidContactImpulse> reactions;
    fluid.Step(h, zero, boxes, {}, {}, &reactions,
               callback ? response : FluidVelocityBatchResponse{});
    const auto& particle = fluid.Particles()[0];
    glm::vec3 floorReaction(0), bodyReaction(0);
    for (const auto& reaction : reactions) {
        if (reaction.owner.id == 71) floorReaction += reaction.impulse;
        if (reaction.owner.id == 72) bodyReaction += reaction.impulse;
    }
    const std::string label = std::string("real drift / projection-only body/") +
        (tilted ? "tilted/" : "flat/") + (reversed ? "reverse/" : "forward/") +
        (callback ? "native" : "fallback");
    Check(Near(particle.position, {}, 3e-5f), label + " retains actual floor position without invented body touch");
    Check(Near(particle.velocity, {}, 3e-5f), label + " stops true floor hit without squeeze kick");
    Check(Near(floorReaction, 125.f * incoming, 4e-3f), label + " preserves the real floor collision impulse");
    Check(bodyReaction == glm::vec3(0), label + " creates no reaction from a projection-only body encounter");
    Check(fluid.GetDiagnostics().unresolvedGeometryCount == 0,
          label + " preserves validated physical geometry");
    if (callback) Check(floorRows > 0 && floorTarget && bodyTargets,
                        label + " distinguishes real drift ownership from final separated body rows");
    std::printf("MEASURE episode/%s position=(%.9g,%.9g,%.9g) velocity=(%.9g,%.9g,%.9g) body_impulse=%.9g\n",
        label.c_str(), particle.position.x, particle.position.y, particle.position.z,
        particle.velocity.x, particle.velocity.y, particle.velocity.z, glm::length(bodyReaction));
}

void HardAndFeasible(bool callback) {
    PhysicsWorld native;
    native.Init();
    bool hardTargets = true;
    const FluidVelocityBatchResponse response = [&](std::vector<FluidParticle>& particles,
            const std::vector<FluidVelocityContact>& contacts, std::vector<glm::vec3>& impulses) {
        std::vector<PhysicsWorld::ContactParticle> input;
        for (const auto& p : particles) input.push_back({p.position, p.velocity, p.mass});
        std::vector<PhysicsWorld::ParticleBoundaryContact> rows;
        for (const auto& c : contacts) {
            hardTargets &= c.minimumNormalVelocity == 0;
            PhysicsWorld::ParticleBoundaryContact row{
                c.particleIndex, c.owner, c.point, c.normal, c.wallVelocity, false};
            row.minimumNormalVelocity = c.minimumNormalVelocity;
            rows.push_back(row);
        }
        native.SolveParticleContacts(input, rows, impulses);
        for (std::size_t i = 0; i < particles.size(); ++i) particles[i].velocity = input[i].velocity;
    };
    FluidSettings settings;
    settings.particleRadius = .125f;
    settings.substeps = 1;
    settings.densityIterations = 0;
    settings.velocitySmoothing = 0;
    const auto wall = Box(7, {-.125f, 0, 0}, {.125f, 2, 2});
    UniformGravity zero({0, 0, 0});
    for (const float x : {.125f, .126f}) {
        FluidWorld fluid(settings);
        fluid.AddParticle({x, 0, 0}, {-100, 0, 0}, 1);
        fluid.Step(.01f, zero, {wall}, {}, {}, nullptr, callback ? response : FluidVelocityBatchResponse{});
        Check(Near(fluid.Particles()[0].position, {.125f, 0, 0}), "true swept thin-wall hit retains preferred exterior placement");
        Check(Near(fluid.Particles()[0].velocity, {}), "true swept thin-wall hit cannot tunnel or lose stopping response");
    }
    FluidWorld outward(settings);
    outward.AddParticle({.125f, 0, 0}, {100, 0, 0}, 1);
    outward.Step(.01f, zero, {wall}, {}, {}, nullptr, callback ? response : FluidVelocityBatchResponse{});
    Check(Near(outward.Particles()[0].velocity, {100, 0, 0}), "departing hard feature does not receive a false inward impulse");

    settings.particleRadius = .018f;
    settings.velocitySmoothing = .15f;
    FluidWorld feasible(settings);
    feasible.AddParticle({0, .018f, 0}, {}, 1);
    feasible.AddParticle({0, .06f, 0}, {}, 1);
    UniformGravity gravity({0, -9.81f, 0});
    feasible.Step(1.f / 60, gravity, {Box(8, {0, -.125f, 0}, {2, .125f, 2})}, {}, {}, nullptr,
                  callback ? response : FluidVelocityBatchResponse{});
    Check(feasible.Particles()[0].position.y >= .018f - 2e-6f,
          "feasible preferred floor stand-off is preserved");
    Check(feasible.Particles()[0].velocity.y >= -2e-6f,
          "smoothing does not invalidate a feasible floor normal");
    if (callback) Check(hardTargets, "hard sweeps and feasible skins retain ordinary zero targets");
}
} // namespace

int main() {
    CapturedTargets({1, 0, 0, 0});
    const auto rotated = glm::angleAxis(.71f, glm::normalize(glm::vec3(1, -2, 3)));
    CapturedTargets(rotated);
    HardAndFeasible(false);
    HardAndFeasible(true);
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const bool tilted : {false, true})
            for (const bool reversed : {false, true})
                for (const bool callback : {false, true})
                    DriftAndProjectionEpisodes(rotation, tilted, reversed, callback);
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const bool callback : {false, true})
            for (const bool translated : {false, true})
                CompatibleOpposingSkins(rotation, translated, callback);
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const bool callback : {false, true})
            CompatibleOpposingSkins(rotation, true, callback, true);
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const bool callback : {false, true}) {
            SimultaneousYieldingResponse(rotation, callback);
            SimultaneousYieldingResponse(rotation, callback, true);
            SimultaneousYieldingResponse(rotation, callback, true, true);
            for (const bool reversed : {false, true})
                for (const float direction : {-1.f, 1.f})
                    StationaryOpposedChannel(rotation, callback, reversed, direction);
        }
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const bool callback : {false, true})
            OrthogonalPreferredSkins(rotation, callback);
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const bool callback : {false, true})
            for (const bool moving : {false, true})
                CompetingPreferredSkins(rotation, callback, moving);
    for (const auto& rotation : {glm::quat(1, 0, 0, 0), rotated})
        for (const int substeps : {1, 3})
            for (const bool reverse : {false, true})
                for (const bool callback : {false, true})
                    for (const bool corner : {false, true})
                        Squeeze(rotation, substeps, reverse, callback, corner);
    std::printf("SUMMARY %u checks %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
