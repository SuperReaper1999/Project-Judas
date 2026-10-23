// Judas Physics MK2 prototype acceptance (docs/MK2_PHYSICS_ARCHITECTURE.md).
// Physical invariants of the opt-in substepped simultaneous solve, measured
// in the real PhysicsWorld. MK1 remains the default and is not exercised here.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
// small family. A crossing is the centre ending beyond the wall's far face.
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
                        crossed = w.GetTransform(b).position.x > 0.01f;
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
} // namespace

int main() {
    setenv("JUDAS_PHYSICS_MK2", "1", 1);
    TestColumns();
    TestRotatedStack();
    TestMomentum();
    TestSimultaneous();
    TestFastObjects();
    TestDeterminism();
    std::printf("MK2 physics tests: %s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}
