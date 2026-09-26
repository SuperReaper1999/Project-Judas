// Milestone 32: rigid-contact regression suite for the accumulated-impulse
// solver (src/ContactSolver.h) running inside the real PhysicsWorld.
//
// Every fixture measures physical invariants over a meaningful duration —
// drift, spin, rest height, slide acceleration against the analytic value,
// rebound ratio, momentum and energy — rather than one magic coordinate,
// and several are repeated in a rigidly rotated universe. Nothing here
// sleeps bodies or clamps velocities: residual motion is reported as it is.
//
// The sections that use only pre-M32 PhysicsWorld API are also compiled
// against the accepted Milestone 31 sources (JUDAS_PRE_M32) to produce the
// "before" column quoted in docs/ARCHITECTURE.md.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "PhysicsWorld.h"

namespace {

int g_failures = 0;
void Check(bool condition, const std::string& label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}
bool Finite(const glm::vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

constexpr float kDt = 1.0f / 60.0f;
constexpr float kG = 9.81f;
const glm::quat kIdentity(1.0f, 0.0f, 0.0f, 0.0f);
const glm::quat kArbitraryRotation = glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));
const glm::vec3 kArbitraryOffset(137.0f, -42.0f, 88.0f);

// A little world under uniform gravity `-up * g`, described in a frame
// (rotation R, offset T) so the same fixture can be built in any rotated
// universe: world = T + R * local.
struct Frame {
    glm::quat R = kIdentity;
    glm::vec3 T{0.0f};
    glm::vec3 ToWorld(const glm::vec3& local) const { return T + R * local; }
    glm::vec3 ToLocal(const glm::vec3& world) const { return glm::conjugate(R) * (world - T); }
    glm::vec3 Dir(const glm::vec3& local) const { return R * local; }
    glm::vec3 DirLocal(const glm::vec3& world) const { return glm::conjugate(R) * world; }
};

void StepWithGravity(PhysicsWorld& world, const std::vector<BodyHandle>& dynamic,
                     const glm::vec3& gravity) {
    for (const BodyHandle h : dynamic) world.ApplyLinearAcceleration(h, gravity, kDt);
    world.Step(kDt);
}

float AngleBetween(const glm::quat& a, const glm::quat& b) {
    const float d = std::min(1.0f, std::abs(glm::dot(glm::normalize(a), glm::normalize(b))));
    return 2.0f * std::acos(d);
}

// --- A: a box resting on a large flat box -------------------------------
struct RestingResult {
    float tangentialDrift = 0.0f;   // over the measurement window, metres
    float spin = 0.0f;              // orientation change over the window, radians
    float restGap = 0.0f;           // centre height above the ground's top face
    float maxSpeed = 0.0f;          // largest speed seen during the window
    bool finite = true;
};

RestingResult RestingBox(const Frame& f, float friction, float seconds) {
    PhysicsWorld world;
    world.Init();
    const glm::vec3 groundHalf(10.0f, 0.5f, 10.0f);
    world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f)), f.R, groundHalf, friction, 0.0f);
    const glm::vec3 boxHalf(0.5f);
    const BodyHandle box = world.CreateDynamicBox(f.ToWorld(glm::vec3(0.3f, 1.0f + 0.02f, -0.2f)), boxHalf, 5.0f,
                                                  friction, 0.0f);
    world.ResetBody(box, f.ToWorld(glm::vec3(0.3f, 1.0f + 0.02f, -0.2f)), f.R);
    const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
    for (int i = 0; i < 120; ++i) StepWithGravity(world, {box}, gravity);  // settle 2 s
    const BodyTransform start = world.GetTransform(box);
    RestingResult r;
    const int steps = static_cast<int>(seconds / kDt);
    for (int i = 0; i < steps; ++i) {
        StepWithGravity(world, {box}, gravity);
        r.maxSpeed = std::max(r.maxSpeed, glm::length(world.GetLinearVelocity(box)));
        r.finite = r.finite && Finite(world.GetTransform(box).position);
    }
    const BodyTransform end = world.GetTransform(box);
    const glm::vec3 moved = f.DirLocal(end.position - start.position);
    r.tangentialDrift = glm::length(glm::vec2(moved.x, moved.z));
    r.spin = AngleBetween(start.rotation, end.rotation);
    r.restGap = f.ToLocal(end.position).y - groundHalf.y;
    return r;
}

void TestRestingBox() {
    std::printf("A: box resting on a large flat box (30 s after 2 s settle)\n");
    for (const float friction : {0.8f, 0.3f}) {
        const RestingResult a = RestingBox(Frame{}, friction, 30.0f);
        const RestingResult b = RestingBox(Frame{kArbitraryRotation, kArbitraryOffset}, friction, 30.0f);
        std::printf("    mu %.1f: drift %.6f m (%.4f mm/s), spin %.6f rad, rest gap %.5f m, max speed %.6f m/s | "
                    "rotated universe: drift %.6f m, spin %.6f rad, gap %.5f m\n",
                    friction, a.tangentialDrift, a.tangentialDrift / 30.0f * 1000.0f, a.spin, a.restGap,
                    a.maxSpeed, b.tangentialDrift, b.spin, b.restGap);
        Check(a.finite && b.finite, "resting box state stays finite");
        Check(a.tangentialDrift < 0.001f && b.tangentialDrift < 0.001f,
              "resting box drifts less than 1 mm in 30 s (M29 measured ~5 mm/s)");
        Check(a.spin < 1.0e-3f && b.spin < 1.0e-3f, "resting box does not develop a spin");
        Check(std::abs(a.restGap - 0.5f) < 0.006f && std::abs(b.restGap - 0.5f) < 0.006f,
              "rest height is the half-height within the penetration slop");
        Check(std::abs(a.restGap - b.restGap) < 1.0e-3f && std::abs(a.tangentialDrift - b.tangentialDrift) < 1.0e-3f,
              "rotated universe rests identically");
    }
}

// --- B: a stack ----------------------------------------------------------
struct StackResult {
    float topDrift = 0.0f;
    float maxLateralOffset = 0.0f;   // largest horizontal offset of any box from the stack axis
    float maxHeightError = 0.0f;     // |actual - ideal| centre heights
    float finalMaxSpeed = 0.0f;
    bool finite = true;
};

StackResult Stack(const Frame& f, int count, float seconds) {
    PhysicsWorld world;
    world.Init();
    const glm::vec3 groundHalf(10.0f, 0.5f, 10.0f);
    world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f)), f.R, groundHalf, 0.7f, 0.0f);
    const glm::vec3 half(0.5f, 0.35f, 0.5f);
    std::vector<BodyHandle> boxes;
    for (int i = 0; i < count; ++i) {
        const glm::vec3 local(0.0f, groundHalf.y + half.y + i * (2.0f * half.y + 0.01f) + 0.005f, 0.0f);
        const BodyHandle h = world.CreateDynamicBox(f.ToWorld(local), half, 4.0f, 0.7f, 0.0f);
        world.ResetBody(h, f.ToWorld(local), f.R);
        boxes.push_back(h);
    }
    const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
    for (int i = 0; i < 120; ++i) StepWithGravity(world, boxes, gravity);
    const glm::vec3 topStart = world.GetTransform(boxes.back()).position;
    StackResult r;
    const int steps = static_cast<int>(seconds / kDt);
    for (int i = 0; i < steps; ++i) {
        StepWithGravity(world, boxes, gravity);
        for (const BodyHandle h : boxes) r.finite = r.finite && Finite(world.GetTransform(h).position);
    }
    for (int i = 0; i < count; ++i) {
        const glm::vec3 local = f.ToLocal(world.GetTransform(boxes[static_cast<std::size_t>(i)]).position);
        const float ideal = groundHalf.y + half.y + i * 2.0f * half.y;
        r.maxHeightError = std::max(r.maxHeightError, std::abs(local.y - ideal));
        r.maxLateralOffset = std::max(r.maxLateralOffset, glm::length(glm::vec2(local.x, local.z)));
        r.finalMaxSpeed = std::max(r.finalMaxSpeed, glm::length(world.GetLinearVelocity(boxes[static_cast<std::size_t>(i)])));
    }
    const glm::vec3 moved = f.DirLocal(world.GetTransform(boxes.back()).position - topStart);
    r.topDrift = glm::length(glm::vec2(moved.x, moved.z));
    return r;
}

void TestStack() {
    std::printf("B: five-box stack (20 s after 2 s settle)\n");
    const StackResult a = Stack(Frame{}, 5, 20.0f);
    const StackResult b = Stack(Frame{kArbitraryRotation, kArbitraryOffset}, 5, 20.0f);
    std::printf("    top drift %.6f m, max lateral offset %.6f m, max height error %.5f m, final max speed %.6f m/s | "
                "rotated: %.6f / %.6f / %.5f / %.6f\n",
                a.topDrift, a.maxLateralOffset, a.maxHeightError, a.finalMaxSpeed,
                b.topDrift, b.maxLateralOffset, b.maxHeightError, b.finalMaxSpeed);
    Check(a.finite && b.finite, "stack stays finite");
    Check(a.topDrift < 0.002f && b.topDrift < 0.002f, "top of the stack drifts less than 2 mm in 20 s");
    // The offset includes how each box landed during the 1 cm drop at the
    // start; the drift check above is the at-rest measurement.
    Check(a.maxLateralOffset < 0.01f && b.maxLateralOffset < 0.01f, "stack stays aligned (landing offset < 1 cm)");
    Check(a.maxHeightError < 0.03f && b.maxHeightError < 0.03f, "stack does not sink or inflate");
    Check(a.finalMaxSpeed < 0.02f && b.finalMaxSpeed < 0.02f, "stack is at rest");
}

// --- C: a sphere resting -------------------------------------------------
void TestRestingSphere() {
    std::printf("C: sphere resting on a flat box (30 s)\n");
    for (const Frame& f : {Frame{}, Frame{kArbitraryRotation, kArbitraryOffset}}) {
        PhysicsWorld world;
        world.Init();
        world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f)), f.R, glm::vec3(10.0f, 0.5f, 10.0f), 0.6f, 0.0f);
        const BodyHandle ball = world.CreateDynamicSphere(f.ToWorld(glm::vec3(1.0f, 1.0f, 2.0f)), 0.5f, 3.0f, 0.6f, 0.0f);
        const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
        for (int i = 0; i < 120; ++i) StepWithGravity(world, {ball}, gravity);
        const glm::vec3 start = world.GetTransform(ball).position;
        for (int i = 0; i < 1800; ++i) StepWithGravity(world, {ball}, gravity);
        const glm::vec3 moved = f.DirLocal(world.GetTransform(ball).position - start);
        const float gap = f.ToLocal(world.GetTransform(ball).position).y - 0.5f;
        std::printf("    drift %.7f m, gap %.5f m, speed %.7f m/s, angular %.7f rad/s\n",
                    glm::length(glm::vec2(moved.x, moved.z)), gap,
                    glm::length(world.GetLinearVelocity(ball)), glm::length(world.GetAngularVelocity(ball)));
        // A sphere has no rolling resistance: it rolls on ANY slope. In the
        // universe translated 137 m from the origin, float spacing (~1e-5 m)
        // tilts the representable "flat" plane by ~3e-5 rad, which is what
        // the sub-millimetre residual there measures; the untranslated
        // universe reads exactly zero.
        Check(glm::length(glm::vec2(moved.x, moved.z)) < 1.0e-3f && std::abs(gap - 0.5f) < 0.006f,
              "resting sphere neither rolls (beyond float resolution) nor sinks");
    }
}

// --- D: static and kinetic friction on an incline ------------------------
struct InclineResult {
    float displacement = 0.0f;    // along the slope, downhill positive
    float acceleration = 0.0f;    // measured from the final speed
};

InclineResult Incline(const Frame& f, float angleDegrees, float friction, float seconds) {
    PhysicsWorld world;
    world.Init();
    const glm::quat tilt = glm::angleAxis(glm::radians(angleDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
    const glm::quat slopeRotation = f.R * tilt;
    const glm::vec3 slopeHalf(20.0f, 0.5f, 5.0f);
    world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f)), slopeRotation, slopeHalf, friction, 0.0f);
    const glm::vec3 boxHalf(0.4f);
    const glm::vec3 localStart = tilt * glm::vec3(0.0f, slopeHalf.y + boxHalf.y + 0.003f, 0.0f);
    const BodyHandle box = world.CreateDynamicBox(f.ToWorld(localStart), boxHalf, 6.0f, friction, 0.0f);
    world.ResetBody(box, f.ToWorld(localStart), slopeRotation);
    const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
    const glm::vec3 downhill = f.Dir(tilt * glm::vec3(-1.0f, 0.0f, 0.0f));
    for (int i = 0; i < 10; ++i) StepWithGravity(world, {box}, gravity);   // establish contact
    const glm::vec3 start = world.GetTransform(box).position;
    const float startSpeed = glm::dot(world.GetLinearVelocity(box), downhill);
    const int steps = static_cast<int>(seconds / kDt);
    for (int i = 0; i < steps; ++i) StepWithGravity(world, {box}, gravity);
    InclineResult r;
    r.displacement = glm::dot(world.GetTransform(box).position - start, downhill);
    r.acceleration = (glm::dot(world.GetLinearVelocity(box), downhill) - startSpeed) / (steps * kDt);
    return r;
}

void TestIncline() {
    std::printf("D: static and kinetic friction on a 20-degree incline\n");
    const float angle = 20.0f;
    const float s = std::sin(glm::radians(angle));
    const float c = std::cos(glm::radians(angle));
    // High friction (tan 20 = 0.364 < 0.8): must hold.
    const InclineResult hold = Incline(Frame{}, angle, 0.8f, 10.0f);
    const InclineResult holdRotated = Incline(Frame{kArbitraryRotation, kArbitraryOffset}, angle, 0.8f, 10.0f);
    // Low friction: slides at g (sin - mu cos).
    const InclineResult slide = Incline(Frame{}, angle, 0.1f, 1.5f);
    const InclineResult slideRotated = Incline(Frame{kArbitraryRotation, kArbitraryOffset}, angle, 0.1f, 1.5f);
    const float expected = kG * (s - 0.1f * c);
    std::printf("    mu 0.8: creep %.6f m in 10 s (rotated %.6f m); mu 0.1: a = %.4f m/s^2 (rotated %.4f), "
                "analytic %.4f\n", hold.displacement, holdRotated.displacement, slide.acceleration,
                slideRotated.acceleration, expected);
    Check(std::abs(hold.displacement) < 0.002f && std::abs(holdRotated.displacement) < 0.002f,
          "static friction holds a box on a slope below its friction angle (< 2 mm in 10 s)");
    Check(std::abs(slide.acceleration - expected) < 0.05f * expected &&
          std::abs(slideRotated.acceleration - expected) < 0.05f * expected,
          "kinetic friction slide acceleration matches g(sin - mu cos) within 5%");
}

// --- E: moving support ----------------------------------------------------
void TestMovingSupport() {
    std::printf("E: box carried by a moving platform (frictionless floor, 5 s)\n");
    for (const Frame& f : {Frame{}, Frame{kArbitraryRotation, kArbitraryOffset}}) {
        PhysicsWorld world;
        world.Init();
        world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f)), f.R, glm::vec3(40.0f, 0.5f, 10.0f), 0.0f, 0.0f);
        const glm::vec3 platformHalf(2.0f, 0.25f, 2.0f);
        const glm::vec3 platformLocal(-20.0f, 0.5f + platformHalf.y + 0.002f, 0.0f);
        const BodyHandle platform = world.CreateDynamicBox(f.ToWorld(platformLocal), platformHalf, 200.0f, 0.8f, 0.0f);
        world.ResetBody(platform, f.ToWorld(platformLocal), f.R);
        const glm::vec3 boxLocal = platformLocal + glm::vec3(0.5f, platformHalf.y + 0.3f + 0.002f, 0.0f);
        const BodyHandle box = world.CreateDynamicBox(f.ToWorld(boxLocal), glm::vec3(0.3f), 3.0f, 0.8f, 0.0f);
        world.ResetBody(box, f.ToWorld(boxLocal), f.R);
        const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
        for (int i = 0; i < 60; ++i) StepWithGravity(world, {platform, box}, gravity);
        const glm::vec3 push = f.Dir(glm::vec3(2.0f, 0.0f, 0.0f));
        world.SetLinearVelocity(platform, push);
        world.SetLinearVelocity(box, push);
        const glm::vec3 relativeStart = world.GetTransform(box).position - world.GetTransform(platform).position;
        float maxRelative = 0.0f;
        for (int i = 0; i < 300; ++i) {
            StepWithGravity(world, {platform, box}, gravity);
            const glm::vec3 relative = world.GetTransform(box).position - world.GetTransform(platform).position;
            maxRelative = std::max(maxRelative, glm::length(relative - relativeStart));
        }
        const float travelled = glm::dot(f.DirLocal(world.GetTransform(platform).position) - f.DirLocal(f.ToWorld(platformLocal) - f.T),
                                         glm::vec3(1.0f, 0.0f, 0.0f));
        std::printf("    platform travelled %.3f m, box relative drift %.6f m\n", travelled, maxRelative);
        Check(travelled > 9.0f && maxRelative < 0.003f, "a resting box rides a moving support without slipping");
    }
}

// --- F: restitution ------------------------------------------------------
void TestRestitution() {
    std::printf("F: restitution of a dropped sphere\n");
    for (const float e : {0.0f, 0.5f}) {
        for (const Frame& f : {Frame{}, Frame{kArbitraryRotation, kArbitraryOffset}}) {
            PhysicsWorld world;
            world.Init();
            world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f)), f.R, glm::vec3(10.0f, 0.5f, 10.0f), 0.3f, e);
            const BodyHandle ball = world.CreateDynamicSphere(f.ToWorld(glm::vec3(0.0f, 3.0f, 0.0f)), 0.3f, 1.0f, 0.3f, e);
            const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
            const glm::vec3 up = f.Dir(glm::vec3(0.0f, 1.0f, 0.0f));
            float impact = 0.0f;
            float rebound = 0.0f;
            float previous = 0.0f;
            for (int i = 0; i < 90; ++i) {
                StepWithGravity(world, {ball}, gravity);
                const float v = glm::dot(world.GetLinearVelocity(ball), up);
                if (previous < 0.0f && v > previous + 1.0f && impact == 0.0f) impact = -previous;
                if (impact > 0.0f) rebound = std::max(rebound, v);
                previous = v;
            }
            std::printf("    e %.1f: impact %.3f m/s, rebound %.3f m/s (ratio %.3f)\n", e, impact, rebound,
                        impact > 0.0f ? rebound / impact : 0.0f);
            if (e > 0.0f) Check(impact > 5.0f && std::abs(rebound / impact - e) < 0.08f, "rebound ratio matches restitution");
            else Check(impact > 5.0f && rebound < 0.2f, "zero restitution does not bounce");
        }
    }
}

#ifndef JUDAS_PRE_M32
// --- G: a tumbling pile: finite state and no energy creation --------------
void TestPileEnergy() {
    std::printf("G: 40-body pile in a static bin: finite, no mechanical-energy gain\n");
    for (const Frame& f : {Frame{}, Frame{kArbitraryRotation, kArbitraryOffset}}) {
        PhysicsWorld world;
        world.Init();
        world.CreateStaticBox(f.ToWorld(glm::vec3(0.0f, -0.5f, 0.0f)), f.R, glm::vec3(3.0f, 0.5f, 3.0f), 0.6f, 0.1f);
        for (int side = 0; side < 4; ++side) {
            const glm::vec3 local = side < 2 ? glm::vec3(side == 0 ? -3.2f : 3.2f, 2.0f, 0.0f)
                                             : glm::vec3(0.0f, 2.0f, side == 2 ? -3.2f : 3.2f);
            const glm::vec3 half = side < 2 ? glm::vec3(0.2f, 2.5f, 3.4f) : glm::vec3(3.4f, 2.5f, 0.2f);
            world.CreateStaticBox(f.ToWorld(local), f.R, half, 0.6f, 0.1f);
        }
        std::vector<BodyHandle> bodies;
        std::vector<float> masses;
        unsigned int seed = 12345u;
        const auto random = [&]() {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(seed >> 8) / static_cast<float>(1u << 24);
        };
        for (int i = 0; i < 40; ++i) {
            const glm::vec3 local(-2.0f + 4.0f * random(), 0.6f + 0.55f * i, -2.0f + 4.0f * random());
            BodyHandle h;
            float mass;
            if (i % 3 == 0) {
                mass = 2.0f;
                h = world.CreateDynamicSphere(f.ToWorld(local), 0.25f, mass, 0.5f, 0.2f);
            } else {
                mass = 3.0f;
                h = world.CreateDynamicBox(f.ToWorld(local), glm::vec3(0.25f, 0.2f, 0.3f), mass, 0.5f, 0.2f);
                world.ResetBody(h, f.ToWorld(local),
                                f.R * glm::angleAxis(6.28f * random(), glm::normalize(glm::vec3(random(), random() + 0.1f, random()))));
            }
            bodies.push_back(h);
            masses.push_back(mass);
        }
        const glm::vec3 gravity = f.Dir(glm::vec3(0.0f, -kG, 0.0f));
        const glm::vec3 up = f.Dir(glm::vec3(0.0f, 1.0f, 0.0f));
        const auto energy = [&]() {
            double e = 0.0;
            for (std::size_t i = 0; i < bodies.size(); ++i) {
                const glm::vec3 v = world.GetLinearVelocity(bodies[i]);
                const glm::vec3 w = world.GetAngularVelocity(bodies[i]);
                e += 0.5 * masses[i] * glm::dot(v, v) +
                     0.5 * glm::dot(w, world.GetInertiaWorld(bodies[i]) * w) +
                     masses[i] * kG * glm::dot(world.GetTransform(bodies[i]).position - f.T, up);
            }
            return e;
        };
        const double initial = energy();
        double maximumGain = 0.0;
        double previous = initial;
        double maximumStepGain = 0.0;
        bool finite = true;
        for (int i = 0; i < 600; ++i) {
            StepWithGravity(world, bodies, gravity);
            const double e = energy();
            maximumGain = std::max(maximumGain, e - initial);
            maximumStepGain = std::max(maximumStepGain, e - previous);
            previous = e;
            for (const BodyHandle h : bodies) finite = finite && Finite(world.GetTransform(h).position) &&
                                                         Finite(world.GetLinearVelocity(h));
        }
        const PhysicsWorld::StepStats& stats = world.LastStepStats();
        std::printf("    energy %.1f J -> %.1f J, largest gain over start %.3f J, largest one-step gain %.3f J; "
                    "last step: %zu candidates / %zu possible, %zu colliding pairs, %zu points\n",
                    initial, previous, maximumGain, maximumStepGain, stats.candidatePairs, stats.possiblePairs,
                    stats.collidingPairs, stats.contactPoints);
        Check(finite, "pile stays finite");
        Check(maximumGain <= 1.0e-3 * std::abs(initial) + 1.0, "pile never exceeds its initial mechanical energy");
        Check(previous < initial, "pile dissipates energy overall");
    }
}

// --- H: momentum in an isolated collision ----------------------------------
void TestMomentum() {
    std::printf("H: isolated collisions conserve linear momentum\n");
    PhysicsWorld world;
    world.Init();
    const BodyHandle a = world.CreateDynamicBox(glm::vec3(-2.0f, 0.1f, 0.0f), glm::vec3(0.4f, 0.3f, 0.5f), 4.0f, 0.5f, 0.3f);
    const BodyHandle b = world.CreateDynamicBox(glm::vec3(2.0f, -0.1f, 0.2f), glm::vec3(0.3f), 2.0f, 0.5f, 0.3f);
    const BodyHandle c = world.CreateDynamicSphere(glm::vec3(0.0f, 2.5f, 0.0f), 0.4f, 1.5f, 0.5f, 0.3f);
    world.ResetBody(a, glm::vec3(-2.0f, 0.1f, 0.0f), glm::angleAxis(0.4f, glm::normalize(glm::vec3(1, 2, 3))));
    world.SetLinearVelocity(a, glm::vec3(3.0f, 0.0f, 0.0f));
    world.SetLinearVelocity(b, glm::vec3(-2.0f, 0.1f, 0.0f));
    world.SetLinearVelocity(c, glm::vec3(0.0f, -3.0f, 0.0f));
    const auto momentum = [&]() {
        return 4.0f * world.GetLinearVelocity(a) + 2.0f * world.GetLinearVelocity(b) + 1.5f * world.GetLinearVelocity(c);
    };
    const glm::vec3 start = momentum();
    float maxError = 0.0f;
    int contactSteps = 0;
    for (int i = 0; i < 120; ++i) {
        world.Step(kDt);
        if (world.LastStepContactCount() > 0) ++contactSteps;
        maxError = std::max(maxError, glm::length(momentum() - start));
    }
    std::printf("    %d steps with contact; largest momentum error %.6f kg m/s of %.3f\n", contactSteps, maxError,
                glm::length(start));
    Check(contactSteps > 0 && maxError < 1.0e-3f, "contact impulses are equal and opposite");
}
#endif

}  // namespace

int main() {
    TestRestingBox();
    TestStack();
    TestRestingSphere();
    TestIncline();
    TestMovingSupport();
    TestRestitution();
#ifndef JUDAS_PRE_M32
    TestPileEnergy();
    TestMomentum();
#endif
    std::printf("Rigid contact tests: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
