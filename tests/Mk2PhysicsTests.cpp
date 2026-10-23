// Judas Physics MK2 prototype acceptance (docs/MK2_PHYSICS_ARCHITECTURE.md).
// Physical invariants of the substepped simultaneous solve (the default),
// measured in the real PhysicsWorld. MK1 is not exercised here.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "CollisionShapes.h"
#include "PhysicsWorld.h"

namespace {
int g_failures = 0;
void Check(bool condition, const std::string& label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}
constexpr float kDt = 1.0f / 60.0f;

void StepWithGravity(PhysicsWorld& world, const std::vector<BodyHandle>& bodies,
                     const glm::vec3& gravity) {
    for (const auto h : bodies) world.ApplyLinearAcceleration(h, gravity, kDt);
    world.Step(kDt);
}

// room1024's four-body columns, including a box balanced on a sphere (an
// unstable equilibrium that only a symmetric response keeps upright), dropped
// from the authored 4 cm gaps at the authored off-origin column position.
void TestColumns() {
    std::printf("A: authored columns land symmetrically and sleep\n");
    for (const char* pattern : {"SBBS", "BBSB", "BSBB"}) {
        PhysicsWorld w;
        w.Init();
        w.CreateStaticBox({0, -0.25f, 0}, {42, 0.25f, 42}, 0.65f, 0.025f);
        const float ys[] = {0.34f, 1.02f, 1.7f, 2.38f};
        const glm::vec2 column(0.37f, 10.74f);
        std::vector<BodyHandle> b;
        for (int i = 0; i < 4; ++i) {
            const glm::vec3 p(column.x, ys[i], column.y);
            b.push_back(pattern[i] == 'S' ? w.CreateDynamicSphere(p, 0.3f, 8, 0.65f, 0.025f)
                                          : w.CreateDynamicBox(p, glm::vec3(0.3f), 8, 0.65f, 0.025f));
        }
        for (int s = 0; s < 900; ++s) StepWithGravity(w, b, {0, -9.81f, 0});
        float lateral = 0;
        int asleep = 0;
        for (auto h : b) {
            const auto p = w.GetTransform(h).position;
            lateral = std::max(lateral, glm::length(glm::vec2(p.x, p.z) - column));
            asleep += w.IsSleeping(h);
        }
        std::printf("    %s lateral %.6f m, asleep %d/4\n", pattern, lateral, asleep);
        Check(lateral < 1e-4f && asleep == 4, std::string(pattern) + " column stays upright and sleeps");
    }
}

// A five-box stack in a rotated, offset universe: physically identical to the
// axis-aligned one, so it must stay aligned and sleep.
void TestRotatedStack() {
    std::printf("B: rotated stacks stay aligned; tall stacks do not sink\n");
    const glm::quat R = glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));
    for (int n : {5, 20}) {
        const bool rotated = n == 5;
        const glm::quat q = rotated ? R : glm::quat(1, 0, 0, 0);
        const glm::vec3 T = rotated ? glm::vec3(137, -42, 88) : glm::vec3(0);
        PhysicsWorld w;
        w.Init();
        const glm::vec3 ground(10, 0.5f, 10), half(0.5f, 0.35f, 0.5f);
        w.CreateStaticBox(T, q, ground, 0.7f, 0);
        std::vector<BodyHandle> b;
        for (int i = 0; i < n; ++i) {
            const glm::vec3 local(0, ground.y + half.y + i * (2 * half.y + 0.01f) + 0.005f, 0);
            auto h = w.CreateDynamicBox(T + q * local, half, 4, 0.7f, 0);
            w.ResetBody(h, T + q * local, q);
            b.push_back(h);
        }
        for (int s = 0; s < 1320; ++s) StepWithGravity(w, b, q * glm::vec3(0, -9.81f, 0));
        float lateral = 0, height = 0;
        int asleep = 0;
        for (int i = 0; i < n; ++i) {
            const glm::vec3 l = glm::conjugate(q) * (w.GetTransform(b[i]).position - T);
            lateral = std::max(lateral, glm::length(glm::vec2(l.x, l.z)));
            height = std::max(height, std::abs(l.y - (ground.y + half.y + i * 2 * half.y)));
            asleep += w.IsSleeping(b[i]);
        }
        std::printf("    %d boxes%s: lateral %.6f m, height error %.5f m, asleep %d\n", n,
                    rotated ? " (rotated universe)" : "", lateral, height, asleep);
        if (rotated)
            Check(lateral < 1e-3f && asleep == n, "rotated five-box stack stays aligned and sleeps");
        else
            Check(height < 0.02f && asleep == n, "twenty-box stack sinks under 1 mm per contact and sleeps");
    }
}

// Linear and angular momentum about the world origin through a fast,
// spinning impact (both bodies free, no gravity).
void TestMomentum() {
    std::printf("C: impacts conserve linear and angular momentum\n");
    for (auto rotation : {glm::quat(1, 0, 0, 0), glm::angleAxis(.8f, glm::normalize(glm::vec3(1, 2, -3)))}) {
        PhysicsWorld world;
        world.Init();
        BodyTransform pose;
        pose.position = rotation * glm::vec3(-.5f, .1f, 0);
        pose.rotation = rotation;
        const auto a = world.CreateShape(Shape::Box({.2f, .5f, .9f}), pose, true, 3, 0, 0);
        const auto c = world.CreateDynamicSphere(rotation * glm::vec3(.2f, 0, 0), .2f, 5, 0, 0);
        world.SetLinearVelocity(a, rotation * glm::vec3(30, 0, 0));
        world.SetAngularVelocity(a, rotation * glm::vec3(1, 2, 3));
        const auto linear = [&] {
            return 3.0 * glm::dvec3(world.GetLinearVelocity(a)) + 5.0 * glm::dvec3(world.GetLinearVelocity(c));
        };
        const auto angular = [&] {
            glm::dvec3 total(0);
            for (auto [h, m] : {std::pair<BodyHandle, double>{a, 3}, {c, 5}}) {
                const auto p = world.GetTransform(h);
                total += glm::cross(glm::dvec3(p.position), m * glm::dvec3(world.GetLinearVelocity(h))) +
                         glm::dmat3(world.GetInertiaWorld(h)) * glm::dvec3(world.GetAngularVelocity(h));
            }
            return total;
        };
        const auto l0 = linear(), h0 = angular();
        world.Step(kDt);
        const double dl = glm::length(linear() - l0), dh = glm::length(angular() - h0);
        const bool touched = glm::length(world.GetLinearVelocity(c)) > 1.0f;
        std::printf("    linear error %.3g, angular error %.3g, struck sphere moving %d\n", dl, dh, int(touched));
        Check(touched && dl < 1e-4 && dh < 1e-4, "fast spinning impact conserves momentum");
    }
}

// Head-on simultaneous arrivals at a resting sphere: symmetric response,
// conserved momentum, no energy gain.
void TestSimultaneous() {
    std::printf("D: simultaneous collisions\n");
    for (float e : {0.0f, 1.0f}) {
        PhysicsWorld w;
        w.Init();
        const auto left = w.CreateDynamicSphere({-1.0f, 0, 0}, 0.25f, 1, 0, e);
        const auto mid = w.CreateDynamicSphere({0, 0, 0}, 0.25f, 1, 0, e);
        const auto right = w.CreateDynamicSphere({1.0f, 0, 0}, 0.25f, 1, 0, e);
        w.SetLinearVelocity(left, {3, 0, 0});
        w.SetLinearVelocity(right, {-3, 0, 0});
        const auto energy = [&] {
            double k = 0;
            for (auto h : {left, mid, right}) k += 0.5 * glm::dot(glm::dvec3(w.GetLinearVelocity(h)), glm::dvec3(w.GetLinearVelocity(h)));
            return k;
        };
        const double k0 = energy();
        double peak = k0;
        for (int s = 0; s < 40; ++s) {
            w.Step(kDt);
            peak = std::max(peak, energy());
        }
        const glm::vec3 vl = w.GetLinearVelocity(left), vm = w.GetLinearVelocity(mid), vr = w.GetLinearVelocity(right);
        const float momentum = glm::length(vl + vm + vr);
        std::printf("    e %.0f: left %.3f mid %.3f right %.3f, momentum %.2g, energy %.3f -> %.3f (peak %.3f)\n", e,
                    vl.x, vm.x, vr.x, momentum, k0, energy(), peak);
        Check(momentum < 1e-4f, "simultaneous impacts conserve momentum");
        Check(std::abs(vl.x + vr.x) < 1e-4f && std::abs(vm.x) < 1e-4f, "symmetric arrivals give a symmetric response");
        Check(peak <= k0 * (1 + 1e-5) + 1e-9, "no kinetic energy is created");
        if (e == 0) Check(std::abs(vl.x) < 0.05f, "inelastic arrivals come to rest together");
        else Check(vl.x < -2.5f && vr.x > 2.5f, "elastic arrivals rebound with their approach speed");
    }
}

// Thin static wall, fast and spinning primitives, every orientation of a
// small family. A crossing is the centre ending beyond the wall's far face
// within the wall's extent.
void TestFastObjects() {
    std::printf("E: fast and spinning primitives do not tunnel through a 2 cm wall\n");
    int crossings = 0, cases = 0;
    for (int shape = 0; shape < 3; ++shape)
        for (float speed : {5.f, 20.f, 60.f, 150.f, 400.f})
            for (float spin : {0.f, 30.f, 200.f})
                for (int orient = 0; orient < 3; ++orient) {
                    PhysicsWorld w;
                    w.Init();
                    w.CreateStaticBox({0, 0, 0}, {0.01f, 5, 5}, 0.5f, 0);
                    const glm::quat q = glm::angleAxis(0.37f * orient, glm::normalize(glm::vec3(0.3f, 1, 0.2f)));
                    const glm::vec3 start(-3.f, 0.1f * orient, 0.05f * orient);
                    BodyHandle b = shape == 0   ? w.CreateDynamicSphere(start, 0.05f, 1, 0.5f, 0)
                                   : shape == 1 ? w.CreateDynamicBox(start, {0.04f, 0.04f, 0.04f}, 1, 0.5f, 0)
                                                : w.CreateDynamicBox(start, {0.2f, 0.01f, 0.1f}, 1, 0.5f, 0);
                    w.ResetBody(b, start, q);
                    w.SetLinearVelocity(b, {speed, 0, 0});
                    w.SetAngularVelocity(b, {0, spin, spin * 0.3f});
                    bool crossed = false;
                    for (int s = 0; s < 120 && !crossed; ++s) {
                        w.Step(kDt);
                        // Through the wall, not around its 10 m edge: a spinning
                        // body can be thrown sideways along the face and leave it.
                        const glm::vec3 p = w.GetTransform(b).position;
                        crossed = p.x > 0.01f && std::abs(p.y) < 5 && std::abs(p.z) < 5;
                    }
                    ++cases;
                    crossings += crossed;
                    if (crossed)
                        std::printf("    crossed: shape %d speed %.0f spin %.0f orientation %d\n", shape, speed, spin, orient);
                }
    std::printf("    %d cases, %d crossings\n", cases, crossings);
    Check(crossings == 0, "no fast or spinning primitive crosses the wall");
}

// The same scene stepped twice from the same inputs is bitwise identical.
void TestDeterminism() {
    std::printf("F: deterministic replay\n");
    const auto run = [] {
        PhysicsWorld w;
        w.Init();
        w.CreateStaticBox({0, -0.5f, 0}, {5, 0.5f, 5}, 0.6f, 0.2f);
        std::vector<BodyHandle> b;
        for (int i = 0; i < 30; ++i) {
            const glm::vec3 p(0.31f * (i % 5) - 0.6f, 0.4f + 0.45f * (i / 5), 0.17f * (i % 3));
            b.push_back(i % 2 ? w.CreateDynamicSphere(p, 0.2f, 1 + 0.1f * i, 0.6f, 0.2f)
                              : w.CreateDynamicBox(p, {0.2f, 0.15f, 0.18f}, 1 + 0.1f * i, 0.6f, 0.2f));
            w.SetAngularVelocity(b.back(), {0.3f * i, -0.2f, 0.1f});
        }
        for (int s = 0; s < 240; ++s) StepWithGravity(w, b, {0, -9.81f, 0});
        std::vector<float> out;
        for (auto h : b) {
            const auto t = w.GetTransform(h);
            out.insert(out.end(), {t.position.x, t.position.y, t.position.z, t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z});
        }
        return out;
    };
    const auto first = run(), second = run();
    Check(first.size() == second.size() &&
              std::memcmp(first.data(), second.data(), first.size() * sizeof(float)) == 0,
          "two runs of a 30-body pile are bitwise identical");
}

// Impacts against exact solutions at the engine's own 60 Hz step: each is
// resolved at its arrival instant (sequential propagation, Poisson
// restitution), so positions and velocities match the analytic trajectory.
void TestImpactAccuracy() {
    std::printf("G: impacts at their arrival instant match exact trajectories\n");
    {
        PhysicsWorld w;
        w.Init();
        w.CreateStaticBox({0, -0.5f, 0}, {10, 0.5f, 10}, 0, 0);
        const auto ball = w.CreateDynamicSphere({0, 0.501f, 0}, 0.5f, 1, 0, 0.5f);
        w.SetLinearVelocity(ball, {0, -1, 0});
        double worst = 0;
        for (int step = 1; step <= 12; ++step) {
            w.Step(kDt);
            const double gap = double(w.GetTransform(ball).position.y) - 0.5;
            worst = std::max(worst, std::abs(gap - 0.5 * (step * double(kDt) - 0.001)));
        }
        std::printf("    bounce: worst endpoint error %.3g m, outgoing %.6f m/s\n", worst,
                    w.GetLinearVelocity(ball).y);
        Check(worst < 1e-5 && std::abs(w.GetLinearVelocity(ball).y - 0.5f) < 1e-5f,
              "a bounce leaves the floor at its arrival instant with e|v|");
    }
    // Spheres in a row: separated (binary impacts at 0.08 s and 0.16 s), and a
    // cradle (a ball striking two touching ones).
    for (const bool cradle : {false, true}) {
        PhysicsWorld w;
        w.Init();
        std::vector<BodyHandle> b;
        const float speed = cradle ? 6.f : 10.f;
        for (const float x : cradle ? std::vector<float>{0.f, 1.5f, 2.5f} : std::vector<float>{0.f, 1.8f, 3.6f})
            b.push_back(w.CreateDynamicSphere({x, 0, 0}, 0.5f, 1, 0, 1));
        w.SetLinearVelocity(b[0], {speed, 0, 0});
        for (int step = 0; step < 12; ++step) w.Step(kDt);
        const double t = 0.2, hit = 0.5 / speed;
        const std::vector<double> ex = cradle ? std::vector<double>{0.5, 1.5, 2.5 + speed * (t - hit)}
                                              : std::vector<double>{0.8, 2.6, 4.0},
                                  ev{0, 0, speed};
        double dx = 0, dv = 0;
        for (int i = 0; i < 3; ++i) {
            dx = std::max(dx, std::abs(double(w.GetTransform(b[i]).position.x) - ex[i]));
            dv = std::max(dv, std::abs(double(w.GetLinearVelocity(b[i]).x) - ev[i]));
        }
        std::printf("    %s: worst position error %.3g m, velocity error %.3g m/s\n",
                    cradle ? "cradle" : "separated chain", dx, dv);
        Check(dx < 1e-4 && dv < 1e-4, cradle ? "a struck cradle passes the speed to its far ball"
                                             : "a separated chain resolves its binary impacts in sequence");
    }
    {
        // A ball rattling between walls 1 m apart at 100 m/s: one or two
        // elastic bounces per step, for one second.
        PhysicsWorld w;
        w.Init();
        w.CreateStaticBox({0.6f, 0, 0}, {0.05f, 4, 4}, 0, 1);
        w.CreateStaticBox({-0.6f, 0, 0}, {0.05f, 4, 4}, 0, 1);
        const auto ball = w.CreateDynamicSphere({0, 0, 0}, 0.05f, 1, 0, 1);
        w.SetLinearVelocity(ball, {100, 0, 0});
        double worst = 0, speedError = 0;
        for (int step = 1; step <= 60; ++step) {
            w.Step(kDt);
            double u = std::fmod(100.0 * step * double(kDt) + 0.5, 2.0); // unfolded, period 2 m
            const double exact = u < 1 ? u - 0.5 : 1.5 - u;
            worst = std::max(worst, std::abs(double(w.GetTransform(ball).position.x) - exact));
            speedError = std::max(speedError, std::abs(double(glm::length(w.GetLinearVelocity(ball))) - 100));
        }
        std::printf("    rattle: worst position error %.3g m, speed error %.3g m/s\n", worst, speedError);
        Check(worst < 1e-3 && speedError < 1e-3, "a rattling elastic ball keeps its speed and exact path");
    }
    {
        // Free spin at 100 rad/s: the first-order quaternion update would turn
        // 2 atan(theta/2) per step, 17% short.
        PhysicsWorld w;
        w.Init();
        const glm::quat q0 = glm::normalize(glm::quat(0.7f, 0.2f, -0.3f, 0.4f));
        const auto body = w.CreateDynamicSphere({0, 0, 0}, 0.5f, 1, 0, 0);
        w.ResetBody(body, {0, 0, 0}, q0);
        w.SetAngularVelocity(body, {0, 0, 100});
        w.Step(kDt);
        const glm::quat exact = glm::angleAxis(100.f * kDt, glm::vec3(0, 0, 1)) * q0;
        const float error = glm::angle(w.GetTransform(body).rotation * glm::conjugate(exact));
        std::printf("    free spin: rotation error %.3g rad\n", error);
        Check(error < 1e-5f, "a free spin turns by its exact rotation");
    }
}

} // namespace

// Gap 8: an undamped pendulum of 1-3 jointed links conserves its swing energy.
// Joint position correction must not pump energy into chains (a velocity bias
// at any fixed rate did), nor drain a real swing (a soft step did).
void TestJointChainEnergy() {
    std::printf("H: undamped jointed chains neither gain nor lose swing energy\n");
    const glm::vec3 half(0.015f, 0.05f, 0.015f);
    const float mass = 1.0f, g = 9.81f;
    const glm::vec3 inertia = mass / 3.0f * glm::vec3(half.y * half.y + half.z * half.z,
                                                      half.x * half.x + half.z * half.z,
                                                      half.x * half.x + half.y * half.y);
    for (const JointType type : {JointType::Ball, JointType::Hinge})
        for (int links = 1; links <= 3; ++links) {
            PhysicsWorld w;
            w.Init();
            const glm::vec3 pivot(0, 1, 0);
            const glm::quat tilt = glm::angleAxis(0.06f, glm::vec3(0, 0, 1));
            const glm::quat frame = glm::angleAxis(-1.5707963f, glm::vec3(0, 1, 0));
            std::vector<BodyHandle> b;
            for (int i = 0; i < links; ++i)
                b.push_back(w.CreateShape(Shape::Box(half),
                                          {pivot + tilt * glm::vec3(0, -half.y * (1 + 2 * i), 0), tilt},
                                          true, mass, 0, 0));
            for (int i = 0; i < links; ++i) {
                JointSettings j;
                j.type = type;
                j.bodyA = b[i];
                j.anchorA = {0, half.y, 0};
                j.frameA = j.frameB = frame;
                if (i == 0) {
                    j.anchorB = pivot;
                } else {
                    j.bodyB = b[i - 1];
                    j.anchorB = {0, -half.y, 0};
                    w.SetPairCollisionEnabled(b[i], b[i - 1], false);
                }
                w.CreateJoint(j);
            }
            const auto energy = [&] {
                double e = 0;
                for (auto h : b) {
                    const auto pose = w.GetTransform(h);
                    const glm::vec3 v = w.GetLinearVelocity(h);
                    const glm::vec3 wl = glm::conjugate(pose.rotation) * w.GetAngularVelocity(h);
                    e += 0.5 * mass * glm::dot(v, v) + 0.5 * glm::dot(wl * wl, inertia) +
                         mass * g * pose.position.y;
                }
                return e;
            };
            // Mean energy over one-second windows, so the swing phase averages out.
            double early = 0, late = 0, swing = 0;
            double bottom = 0;
            for (int i = 0; i < links; ++i) bottom += mass * g * (pivot.y - half.y * (1 + 2 * i));
            swing = energy() - bottom; // the authored pose, at rest
            int asleep = 0;
            for (int s = 0; s < 3600; ++s) {
                StepWithGravity(w, b, {0, -g, 0});
                const double e = energy();
                if (s >= 540 && s < 600) early += e / 60;
                if (s >= 3540) late += e / 60;
            }
            for (auto h : b) asleep += w.IsSleeping(h);
            const double drift = late - early;
            std::printf("    %s x%d: swing %.3e J, drift 10->60 s %+.3e J (%+.2f%%), asleep %d\n",
                        type == JointType::Ball ? "ball" : "hinge", links, swing, drift,
                        100 * drift / swing, asleep);
            const std::string name = std::string(type == JointType::Ball ? "ball" : "hinge") + " x" +
                                     std::to_string(links);
            Check(asleep == 0, name + " keeps swinging (not put to sleep)");
            Check(drift <= 1e-3 * swing, name + " gains no energy");
            Check(drift >= -0.05 * swing, name + " loses under 5% in 50 s");
        }
}

// Gap 8, as the imported rig met it: an undamped hinged pendulum hanging from
// a body that rests, by friction, on a static ledge. The resting support must
// not feed the swing.
void TestSupportedPendulumEnergy() {
    std::printf("I: a pendulum on a contact-supported body gains no energy\n");
    const glm::vec3 half(0.015f, 0.05f, 0.015f), legHalf(0.1f);
    const float mass = 1.0f, legMass = 5.0f, g = 9.81f;
    for (int variant = 0; variant < 3; ++variant) {
        const bool limits = variant >= 1, floorBelow = variant == 2;
        PhysicsWorld w;
        w.Init();
        w.CreateStaticBox({0, 0.5f, 0}, glm::vec3(0.5f), 0.65f, 0.0f);
        // A floor 2.5 mm under the hanging foot's lowest point (the rig's case).
        if (floorBelow)
            w.CreateStaticBox({1.0f, 0.95f - 0.0025f - 0.25f, 0}, glm::vec3(0.45f, 0.25f, 0.5f), 0.65f, 0.0f);
        const auto leg = w.CreateDynamicBox({0.4f, 1.0f + legHalf.y, 0}, legHalf, legMass, 0.65f, 0.0f);
        const glm::vec3 pivot(0.53f, 1.05f, 0);
        const glm::quat tilt = glm::angleAxis(0.06f, glm::vec3(0, 0, 1));
        const auto foot = w.CreateShape(Shape::Box(half), {pivot + tilt * glm::vec3(0, -half.y, 0), tilt},
                                        true, mass, 0, 0);
        w.SetPairCollisionEnabled(leg, foot, false);
        JointSettings j;
        j.type = JointType::Hinge;
        j.bodyA = foot;
        j.bodyB = leg;
        j.anchorA = {0, half.y, 0};
        j.anchorB = pivot - glm::vec3(0.4f, 1.0f + legHalf.y, 0);
        j.frameA = j.frameB = glm::angleAxis(-1.5707963f, glm::vec3(0, 1, 0));
        j.limits = limits;
        j.lower = -0.6f;
        j.upper = 0.6f;
        w.CreateJoint(j);
        const std::vector<BodyHandle> b{leg, foot};
        const auto footEnergy = [&] {
            const auto pose = w.GetTransform(foot);
            const glm::vec3 v = w.GetLinearVelocity(foot);
            const glm::vec3 inertia = mass / 3.0f * glm::vec3(half.y * half.y + half.z * half.z,
                                                              half.x * half.x + half.z * half.z,
                                                              half.x * half.x + half.y * half.y);
            // Swing energy about the (slowly settling) pivot, relative to the leg.
            const auto legPose = w.GetTransform(leg);
            const glm::vec3 pivotNow = legPose.position + legPose.rotation * j.anchorB;
            const glm::vec3 vr = v - w.GetLinearVelocity(leg);
            const glm::vec3 wr = glm::conjugate(pose.rotation) * (w.GetAngularVelocity(foot) - w.GetAngularVelocity(leg));
            return 0.5 * mass * glm::dot(vr, vr) + 0.5 * glm::dot(wr * wr, inertia) +
                   mass * g * (pose.position.y - (pivotNow.y - half.y));
        };
        const double swing = footEnergy();
        double early = 0, late = 0;
        for (int s = 0; s < 3600; ++s) {
            StepWithGravity(w, b, {0, -g, 0});
            const double e = footEnergy();
            if (s >= 540 && s < 600) early += e / 60;
            if (s >= 3540) late += e / 60;
        }
        const double drift = late - early;
        std::printf("    limits %d floor %d: swing %.3e J, foot drift 10->60 s %+.3e J (%+.2f%%), leg asleep %d\n",
                    limits, floorBelow, swing, drift, 100 * drift / swing, w.IsSleeping(leg));
        Check(drift <= 1e-3 * swing, std::string("supported pendulum gains no energy, limits ") +
                                         (limits ? "on" : "off") + (floorBelow ? ", floor below" : ""));
    }
}

// The imported rig's foot regime: a horizontal arm on a hinge whose axis is
// 2.4 degrees off vertical (a weakly restored swing, so a small energy error
// is a large fraction of it), from a box resting by friction or a static one.
void TestWeakSwingEnergy() {
    std::printf("J: a weakly restored swing gains no energy\n");
    // Variant 2 rolls the arm about its long axis, so the hinge is not one of
    // its principal axes (as the rig's foot): the real angular velocity must
    // be transported to the pose the projection actually reached.
    for (int variant = 0; variant < 3; ++variant) {
        const bool staticLeg = variant == 1, rolled = variant == 2;
        PhysicsWorld w;
        w.Init();
        w.CreateStaticBox({0, 0.5f, 0}, glm::vec3(0.5f), 0.65f, 0.0f);
        const glm::vec3 legC(0.4f, 1.1f, 0), legHalf(0.1f);
        const auto leg = staticLeg ? w.CreateStaticBox(legC, legHalf, 0.65f, 0.0f)
                                   : w.CreateDynamicBox(legC, legHalf, 4, 0.65f, 0.0f);
        // The arm points along +z: the lowest point of its tilted circle.
        const glm::vec3 half(0.04f, 0.015f, 0.05f);
        const glm::vec3 pivot(0.4f, 1.1f, 0.13f);
        const glm::quat roll = rolled ? glm::angleAxis(0.5f, glm::vec3(0, 0, 1)) : glm::quat(1, 0, 0, 0);
        const auto foot = w.CreateShape(Shape::Box(half), {pivot + glm::vec3(0, 0, half.z), roll}, true, 1.2f, 0, 0);
        w.SetPairCollisionEnabled(leg, foot, false);
        const float tilt = 0.042f; // radians off vertical
        const glm::vec3 axis(0, std::cos(tilt), std::sin(tilt));
        JointSettings j;
        j.type = JointType::Hinge;
        j.bodyA = foot;
        j.bodyB = leg;
        j.anchorA = {0, 0, -half.z};
        j.anchorB = pivot - legC;
        j.frameB = glm::rotation(glm::vec3(1, 0, 0), axis);
        j.frameA = glm::conjugate(roll) * j.frameB;
        j.limits = true;
        j.lower = -0.6f;
        j.upper = 0.6f;
        const auto jh = w.CreateJoint(j);
        w.SetAngularVelocity(foot, axis * 0.33f);
        std::vector<BodyHandle> b{foot};
        if (!staticLeg) b.push_back(leg);
        // Peak relative swing rate over the second and the last ten seconds.
        float peak = 0, early = 0, late = 0;
        for (int s = 0; s < 3600; ++s) {
            StepWithGravity(w, b, {0, -9.81f, 0});
            peak = std::max(peak, glm::length(w.GetAngularVelocity(foot) - w.GetAngularVelocity(leg)));
            if (s == 1199) early = peak, peak = 0;
            if (s == 2999) peak = 0;
        }
        late = peak;
        JointState js;
        w.GetJoint(jh, js);
        const char *name = rolled ? "rolled arm" : staticLeg ? "static parent" : "resting parent";
        std::printf("    %s: peak rate 10-20 s %.4f, 50-60 s %.4f rad/s, at %.4f rad\n",
                    name, early, late, js.coordinate);
        Check(early > 0.1f && late <= early * 1.001f,
              std::string("weak swing keeps swinging and gains no energy, ") + name);
    }
}

int main() {
    unsetenv("JUDAS_PHYSICS_MK1");
    TestColumns();
    TestRotatedStack();
    TestMomentum();
    TestSimultaneous();
    TestFastObjects();
    TestDeterminism();
    TestImpactAccuracy();
    TestJointChainEnergy();
    TestSupportedPendulumEnergy();
    TestWeakSwingEnergy();
    std::printf("MK2 physics tests: %s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
