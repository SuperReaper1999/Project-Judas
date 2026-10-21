// Milestone 32: broadphase correctness against a brute-force oracle.
//
// The oracle lives here, not in the engine: every pair of live bodies is
// handed to the SAME narrowphase PhysicsWorld uses (src/Narrowphase.h), and
// the resulting set of actually-touching pairs must equal the set
// PhysicsWorld::FindCollidingPairs reports through its dynamic AABB tree.
// False-positive candidates are allowed (the broadphase is conservative);
// a single false negative fails the suite.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Broadphase.h"
#include "Contacts.h"
#include "Narrowphase.h"
#include "PhysicsWorld.h"
#include "RigidBody.h"

namespace {

int g_failures = 0;
void Check(bool condition, const std::string& label) {
    std::printf("  %s %s\n", condition ? "OK  " : "FAIL", label.c_str());
    if (!condition) ++g_failures;
}

struct Random {
    unsigned int state;
    explicit Random(unsigned int seed) : state(seed) {}
    float Next() {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
    }
    float Range(float a, float b) { return a + (b - a) * Next(); }
    glm::quat Rotation() {
        const glm::vec3 axis = glm::normalize(glm::vec3(Range(-1, 1), Range(-1, 1), Range(-1, 1)) + glm::vec3(1e-3f));
        return glm::angleAxis(Range(0.0f, 6.2831853f), axis);
    }
};

using PairSet = std::set<std::pair<unsigned int, unsigned int>>;

std::pair<unsigned int, unsigned int> Key(BodyHandle a, BodyHandle b) {
    return {std::min(a.id, b.id), std::max(a.id, b.id)};
}

// Exhaustive all-pairs -> narrowphase: the reference.
PairSet OraclePairs(const PhysicsWorld& world) {
    PairSet result;
    const std::vector<BodyHandle> bodies = world.AliveBodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        Shape shapeA;
        BodyTransform poseA;
        world.GetBodyShape(bodies[i], shapeA, poseA);
        RigidBody a;
        a.position = poseA.position;
        a.orientation = poseA.rotation;
        const bool dynamicA = world.IsDynamicBody(bodies[i]);
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            if (!dynamicA && !world.IsDynamicBody(bodies[j])) continue;  // static-static never collides
            Shape shapeB;
            BodyTransform poseB;
            world.GetBodyShape(bodies[j], shapeB, poseB);
            RigidBody b;
            b.position = poseB.position;
            b.orientation = poseB.rotation;
            bool touching = false;
            for (int pa = 0; pa < PrimitiveCount(shapeA) && !touching; ++pa) {
                const PrimitivePose ca = PrimitiveAt(shapeA, a, pa);
                for (int pb = 0; pb < PrimitiveCount(shapeB) && !touching; ++pb) {
                    const PrimitivePose cb = PrimitiveAt(shapeB, b, pb);
                    const ContactManifold m = ComputeContacts(ca, cb);
                    for (int p = 0; p < m.count; ++p) touching = touching || m.points[p].hit;
                }
            }
            if (touching) result.insert(Key(bodies[i], bodies[j]));
        }
    }
    return result;
}

PairSet BroadphasePairs(const PhysicsWorld& world) {
    PairSet result;
    for (const PhysicsWorld::CollidingPair& p : world.FindCollidingPairs()) result.insert(Key(p.a, p.b));
    return result;
}

struct Comparison {
    std::size_t oracle = 0;
    std::size_t broadphase = 0;
    std::size_t missed = 0;     // in the oracle, not reported: a false negative
    std::size_t spurious = 0;   // reported but not touching: the narrowphase must never allow this
};

Comparison Compare(const PhysicsWorld& world) {
    const PairSet oracle = OraclePairs(world);
    const PairSet reported = BroadphasePairs(world);
    Comparison c;
    c.oracle = oracle.size();
    c.broadphase = reported.size();
    for (const auto& p : oracle) if (!reported.count(p)) ++c.missed;
    for (const auto& p : reported) if (!oracle.count(p)) ++c.spurious;
    return c;
}

std::vector<CompoundBox> CupBoxes() {
    return {{{0.0f, -0.15f, 0.0f}, {0.18f, 0.015f, 0.18f}},
            {{-0.165f, 0.04f, 0.0f}, {0.015f, 0.18f, 0.18f}},
            {{0.165f, 0.04f, 0.0f}, {0.015f, 0.18f, 0.18f}},
            {{0.0f, 0.04f, -0.165f}, {0.15f, 0.18f, 0.015f}},
            {{0.0f, 0.04f, 0.165f}, {0.15f, 0.18f, 0.015f}}};
}

// A random clutter of spheres, rotated boxes, compounds; static and dynamic;
// separated and overlapping, packed into a region so many pairs touch.
std::vector<BodyHandle> Populate(PhysicsWorld& world, Random& rng, int count, float extent,
                                 const glm::quat& R = glm::quat(1, 0, 0, 0), const glm::vec3& T = glm::vec3(0.0f)) {
    std::vector<BodyHandle> handles;
    for (int i = 0; i < count; ++i) {
        const glm::vec3 local(rng.Range(-extent, extent), rng.Range(-extent, extent), rng.Range(-extent, extent));
        const glm::vec3 p = T + R * local;
        const glm::quat q = R * rng.Rotation();
        const int kind = static_cast<int>(rng.Next() * 6.0f);
        BodyHandle h;
        switch (kind) {
            case 0: h = world.CreateDynamicSphere(p, rng.Range(0.1f, 0.8f), 2.0f, 0.5f, 0.1f); break;
            case 1: h = world.CreateStaticSphere(p, rng.Range(0.2f, 1.5f), 0.5f, 0.1f); break;
            case 2:
                h = world.CreateStaticBox(p, q, glm::vec3(rng.Range(0.1f, 2.0f), rng.Range(0.1f, 0.6f), rng.Range(0.1f, 2.0f)),
                                          0.5f, 0.1f);
                break;
            case 3:
                h = world.CreateDynamicCompoundBoxes(p, CupBoxes(), 5.0f, 0.5f, 0.1f);
                world.ResetBody(h, p, q);
                break;
            default:
                h = world.CreateDynamicBox(p, glm::vec3(rng.Range(0.1f, 0.9f), rng.Range(0.1f, 0.9f), rng.Range(0.1f, 0.9f)),
                                           3.0f, 0.5f, 0.1f);
                world.ResetBody(h, p, q);
                break;
        }
        handles.push_back(h);
    }
    return handles;
}

// --- A: the tree itself ----------------------------------------------------
void TestTree() {
    std::printf("A: dynamic AABB tree against brute-force box overlap\n");
    DynamicAabbTree tree;
    Random rng(7u);
    std::vector<int> proxies;
    std::vector<Aabb> boxes;
    const auto randomBox = [&]() {
        const glm::vec3 c(rng.Range(-50, 50), rng.Range(-50, 50), rng.Range(-50, 50));
        const glm::vec3 h(rng.Range(0.05f, 3.0f), rng.Range(0.05f, 3.0f), rng.Range(0.05f, 3.0f));
        return Aabb{c - h, c + h};
    };
    bool valid = true;
    bool exact = true;
    for (int i = 0; i < 2000; ++i) {
        boxes.push_back(randomBox());
        proxies.push_back(tree.CreateProxy(boxes.back(), static_cast<unsigned int>(i)));
    }
    for (int round = 0; round < 200; ++round) {
        const std::size_t k = static_cast<std::size_t>(rng.Next() * static_cast<float>(boxes.size()));
        boxes[k] = randomBox();
        tree.MoveProxy(proxies[k], boxes[k]);
        if (round % 3 == 0) {
            // destroy and recreate another
            const std::size_t d = static_cast<std::size_t>(rng.Next() * static_cast<float>(boxes.size()));
            tree.DestroyProxy(proxies[d]);
            boxes[d] = randomBox();
            proxies[d] = tree.CreateProxy(boxes[d], static_cast<unsigned int>(d));
        }
        valid = valid && tree.Validate();
        const Aabb query = randomBox().Expanded(5.0f);
        std::set<unsigned int> got;
        tree.Query(query, [&](int proxy) { got.insert(tree.UserData(proxy)); });
        std::set<unsigned int> expected;
        for (std::size_t i = 0; i < boxes.size(); ++i) if (boxes[i].Overlaps(query)) expected.insert(static_cast<unsigned int>(i));
        exact = exact && got == expected;
    }
    std::printf("    2000 proxies, height %d (log2 n = %.1f)\n", tree.Height(), std::log2(2000.0));
    Check(valid, "tree structure stays valid through inserts, moves, removals");
    Check(exact, "tree queries return exactly the brute-force overlapping set");
    Check(tree.Height() <= 3 * static_cast<int>(std::ceil(std::log2(2000.0))), "tree height stays logarithmic");
}

void TestTreePairs() {
    std::printf("A2: unordered active-leaf pair traversal against independent overlap and query oracles\n");
    DynamicAabbTree tree;
    DynamicAabbTree::PairQueryScratch scratch;
    using ProxyPairSet = std::set<std::pair<int, int>>;
    std::vector<int> proxies;
    std::vector<Aabb> boxes;
    const auto pairKey = [](int a, int b) { return std::make_pair(std::min(a, b), std::max(a, b)); };
    const auto verify = [&](const std::vector<int>& active, const std::string& label) {
        const std::set<int> activeSet(active.begin(), active.end());
        ProxyPairSet brute;
        for (std::size_t a = 0; a < proxies.size(); ++a)
            for (std::size_t b = a + 1; b < proxies.size(); ++b)
                if ((activeSet.count(proxies[a]) || activeSet.count(proxies[b])) &&
                    boxes[a].Overlaps(boxes[b]))
                    brute.insert(pairKey(proxies[a], proxies[b]));
        ProxyPairSet original;
        for (const int activeProxy : active)
            tree.Query(tree.FatAabb(activeProxy), [&](int other) {
                if (other != activeProxy)
                    original.insert(pairKey(activeProxy, other));
            });
        ProxyPairSet result;
        std::size_t callbacks = 0;
        bool noSelfPairs = true;
        tree.QueryPairs(active, scratch, [&](int a, int b) {
            ++callbacks;
            noSelfPairs = noSelfPairs && a != b;
            result.insert(pairKey(a, b));
        });
        Check(result == brute && result == original && callbacks == result.size() && noSelfPairs,
              label + ": exact pairs, no duplicates or self-pairs");
        return result;
    };
    verify({}, "empty tree");
    Check(scratch.marks.empty() && scratch.stack.empty(), "unused pair traversal allocates no scratch");
    const auto add = [&](const Aabb& box, unsigned userData) {
        boxes.push_back(box);
        proxies.push_back(tree.CreateProxy(box, userData));
    };
    add({{-1, -1, -1}, {1, 1, 1}}, 4000000000u);
    verify(proxies, "single leaf");
    add({{1, -1, -1}, {3, 1, 1}}, 17);
    add({{1, 1, -1}, {3, 3, 1}}, 17);
    add({{1, 1, 1}, {3, 3, 3}}, 0);
    const auto touching = verify(proxies, "closed face, edge and corner contacts; duplicate user data");
    Check(touching.size() == 6, "all six boundary-touching pairs are actually exercised");
    verify({}, "inactive overlapping leaves");
    verify({proxies[0], proxies[0]}, "one active leaf, duplicated active ID");
    verify({proxies[3]}, "changed active set does not retain previous epoch marks");
    scratch.epoch = std::numeric_limits<std::uint64_t>::max();
    std::fill(scratch.marks.begin(), scratch.marks.end(), 1);
    verify({proxies[0]}, "epoch wrap clears stale marks");

    Random rng(7493u);
    const auto randomBox = [&]() {
        const glm::vec3 center(rng.Range(-5, 5), rng.Range(-5, 5), rng.Range(-5, 5));
        const glm::vec3 half(rng.Range(.1f, 2), rng.Range(.1f, 2), rng.Range(.1f, 2));
        return Aabb{center - half, center + half};
    };
    for (unsigned i = 0; i < 124; ++i)
        add(randomBox(), i % 7 == 0 ? 0 : 700000u - i * 13);
    verify(proxies, "tree growth and all leaves active");
    std::vector<int> active;
    for (std::size_t i = 0; i < proxies.size(); i += 5)
        active.push_back(proxies[i]);
    verify(active, "mixed active and inactive subtrees");
    verify({proxies.back()}, "single sparse active path in grown tree");
    bool valid = true, reused = false;
    for (unsigned round = 0; round < 12; ++round) {
        const auto moving = static_cast<std::size_t>(round * 17) % proxies.size();
        boxes[moving] = randomBox();
        tree.MoveProxy(proxies[moving], boxes[moving]);
        const auto replaced = static_cast<std::size_t>(round * 23 + 1) % proxies.size();
        const int removed = proxies[replaced];
        tree.DestroyProxy(removed);
        bool rejectsDead = false;
        try {
            tree.QueryPairs({removed}, scratch, [](int, int) {});
        } catch (const std::invalid_argument&) {
            rejectsDead = true;
        }
        Check(rejectsDead && !scratch.inUse, "destroyed proxy is rejected without poisoning scratch");
        boxes[replaced] = randomBox();
        proxies[replaced] = tree.CreateProxy(boxes[replaced], round * 977u);
        reused = reused || proxies[replaced] == removed;
        active.clear();
        for (std::size_t i = round % 4; i < proxies.size(); i += 4)
            active.push_back(proxies[i]);
        verify(active, "move/destroy/reuse round " + std::to_string(round));
        valid = valid && tree.Validate();
    }
    Check(valid && reused, "pair queries preserve tree invariants and exercise actual proxy reuse");
    bool rejectsInvalid = false;
    try {
        tree.QueryPairs({-1}, scratch, [](int, int) {});
    } catch (const std::invalid_argument&) {
        rejectsInvalid = true;
    }
    Check(rejectsInvalid && !scratch.inUse, "invalid active ID is rejected and query guard is restored");

    const auto expected = verify(proxies, "reference before nested queries");
    ProxyPairSet nestedResult;
    bool nestedChecked = false, sameScratchRejected = false, ordinaryExact = false;
    tree.QueryPairs(proxies, scratch, [&](int a, int b) {
        nestedResult.insert(pairKey(a, b));
        if (nestedChecked)
            return;
        nestedChecked = true;
        try {
            tree.QueryPairs(proxies, scratch, [](int, int) {});
        } catch (const std::logic_error&) {
            sameScratchRejected = true;
        }
        std::set<int> ordinary, ordinaryBrute;
        tree.Query(boxes.front(), [&](int proxy) { ordinary.insert(proxy); });
        for (std::size_t i = 0; i < proxies.size(); ++i)
            if (boxes.front().Overlaps(boxes[i]))
                ordinaryBrute.insert(proxies[i]);
        ordinaryExact = ordinary == ordinaryBrute;
        DynamicAabbTree::PairQueryScratch nestedScratch;
        ProxyPairSet independentlyNested;
        tree.QueryPairs(proxies, nestedScratch,
                        [&](int first, int second) { independentlyNested.insert(pairKey(first, second)); });
        Check(independentlyNested == expected, "nested pair query with separate scratch remains exact");
    });
    Check(nestedChecked && sameScratchRejected && ordinaryExact && nestedResult == expected && !scratch.inUse,
          "nested ordinary queries are safe, same-scratch nesting fails explicitly, outer pairs stay exact");
    struct CallbackFailure {};
    bool callbackFailed = false;
    try {
        tree.QueryPairs(proxies, scratch, [](int, int) { throw CallbackFailure{}; });
    } catch (const CallbackFailure&) {
        callbackFailed = true;
    }
    Check(callbackFailed && !scratch.inUse, "throwing callback releases scratch for later queries");
    verify(proxies, "query after callback failure");
}

// --- B: static scenes ------------------------------------------------------
void TestStaticScenes() {
    std::printf("B: randomised scenes, broadphase + narrowphase == all-pairs + narrowphase\n");
    std::size_t totalMissed = 0, totalSpurious = 0, totalPairs = 0;
    for (unsigned int seed = 1; seed <= 12; ++seed) {
        PhysicsWorld world;
        world.Init();
        Random rng(seed * 977u);
        Populate(world, rng, 60 + static_cast<int>(seed) * 10, 2.0f + 0.5f * static_cast<float>(seed));
        const Comparison c = Compare(world);
        totalMissed += c.missed;
        totalSpurious += c.spurious;
        totalPairs += c.oracle;
    }
    std::printf("    12 scenes: %zu touching pairs, %zu missed, %zu spurious\n", totalPairs, totalMissed, totalSpurious);
    Check(totalPairs > 100, "fixtures genuinely contain many touching pairs");
    Check(totalMissed == 0, "no false negatives");
    Check(totalSpurious == 0, "every reported pair actually touches");
}

// --- C: moving bodies ------------------------------------------------------
void TestMovingScenes() {
    std::printf("C: moving bodies under gravity, compared after every step\n");
    std::size_t missed = 0, spurious = 0, touching = 0, reinsertions = 0;
    for (unsigned int seed = 1; seed <= 4; ++seed) {
        PhysicsWorld world;
        world.Init();
        Random rng(seed * 31u);
        world.CreateStaticBox(glm::vec3(0.0f, -6.0f, 0.0f), glm::vec3(20.0f, 0.5f, 20.0f), 0.5f, 0.1f);
        const std::vector<BodyHandle> bodies = Populate(world, rng, 80, 4.0f);
        for (const BodyHandle h : bodies) {
            if (world.IsDynamicBody(h)) {
                world.SetLinearVelocity(h, glm::vec3(rng.Range(-6, 6), rng.Range(-6, 6), rng.Range(-6, 6)));
                world.SetAngularVelocity(h, glm::vec3(rng.Range(-4, 4), rng.Range(-4, 4), rng.Range(-4, 4)));
            }
        }
        for (int step = 0; step < 150; ++step) {
            for (const BodyHandle h : bodies) world.ApplyLinearAcceleration(h, glm::vec3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);
            world.Step(1.0f / 60.0f);
            reinsertions += world.LastStepStats().proxyReinsertions;
            const Comparison c = Compare(world);
            missed += c.missed;
            spurious += c.spurious;
            touching += c.oracle;
        }
    }
    std::printf("    600 steps: %zu touching pair-steps, %zu missed, %zu spurious, %zu proxy reinsertions\n",
                touching, missed, spurious, reinsertions);
    Check(touching > 1000 && reinsertions > 100, "fixture exercises many contacts and proxy moves");
    Check(missed == 0 && spurious == 0, "moving broadphase never misses a narrowphase collision");
}

// --- D: destroyed / reused slots ----------------------------------------------
void TestDestroyedAndReused() {
    std::printf("D: destroyed bodies, reused slots and stale handles\n");
    PhysicsWorld world;
    world.Init();
    Random rng(99u);
    std::vector<BodyHandle> bodies = Populate(world, rng, 120, 3.0f);
    std::vector<BodyHandle> destroyed;
    for (std::size_t i = 0; i < bodies.size(); i += 3) {
        world.DestroyBody(bodies[i]);
        destroyed.push_back(bodies[i]);
    }
    const Comparison afterDestroy = Compare(world);
    // Recreate into the freed slots, overlapping where the old bodies were.
    std::vector<BodyHandle> recreated = Populate(world, rng, static_cast<int>(destroyed.size()), 3.0f);
    const Comparison afterRecreate = Compare(world);
    bool staleInvalid = true;
    bool slotsReused = false;
    for (const BodyHandle stale : destroyed) {
        staleInvalid = staleInvalid && !world.IsDynamicBody(stale) && world.GetBodyBoxes(stale).empty();
        Shape shape;
        BodyTransform pose;
        staleInvalid = staleInvalid && !world.GetBodyShape(stale, shape, pose);
        for (const BodyHandle fresh : recreated) {
            if ((fresh.id & 0xFFFFFu) == (stale.id & 0xFFFFFu)) slotsReused = true;
        }
    }
    bool staleNeverReported = true;
    for (const PhysicsWorld::CollidingPair& p : world.FindCollidingPairs()) {
        for (const BodyHandle stale : destroyed) {
            staleNeverReported = staleNeverReported && p.a.id != stale.id && p.b.id != stale.id;
        }
    }
    std::size_t queried = world.QueryBodiesInAabb(glm::vec3(-100.0f), glm::vec3(100.0f)).size();
    std::printf("    destroyed %zu, recreated %zu; touching pairs %zu -> %zu; query sees %zu of %zu live bodies\n",
                destroyed.size(), recreated.size(), afterDestroy.oracle, afterRecreate.oracle, queried,
                world.AliveBodyCount());
    Check(afterDestroy.missed == 0 && afterDestroy.spurious == 0 && afterRecreate.missed == 0 &&
          afterRecreate.spurious == 0, "oracle agreement survives destruction and slot reuse");
    Check(slotsReused && staleInvalid && staleNeverReported, "reused slots never answer to stale handles");
    Check(queried == world.AliveBodyCount(), "broadphase query sees exactly the live bodies");
}

// --- E: rotate the universe --------------------------------------------------
void TestRotatedUniverse() {
    std::printf("E: rotating and translating the whole scene changes no touching pair\n");
    const glm::quat R = glm::angleAxis(glm::radians(53.0f), glm::normalize(glm::vec3(0.4f, 0.8f, -0.3f)));
    const glm::vec3 T(250.0f, -80.0f, 40.0f);
    bool identical = true;
    std::size_t pairs = 0;
    for (unsigned int seed = 3; seed <= 8; ++seed) {
        PhysicsWorld a, b;
        a.Init();
        b.Init();
        Random ra(seed), rb(seed);
        Populate(a, ra, 90, 3.0f);
        Populate(b, rb, 90, 3.0f, R, T);
        // Same creation order -> same slots -> handle ids comparable.
        const PairSet pa = BroadphasePairs(a);
        const PairSet pb = BroadphasePairs(b);
        pairs += pa.size();
        identical = identical && pa == pb && Compare(b).missed == 0;
    }
    std::printf("    %zu touching pairs across six scenes\n", pairs);
    Check(identical, "touching pairs are identical in the rotated, translated universe");
}

// --- F: player sweep reuse -----------------------------------------------------
// The sweep evaluates only broadphase candidates. This oracle repeats
// SweepPlayerShape's march (t = 0 check, 24 substeps, 20 bisections) over
// EVERY live body; the results must be identical, not merely close.
ShapeSweepHit BruteSweep(const PhysicsWorld& world, float radius, float halfHeight, const glm::vec3& from,
                         const glm::vec3& displacement) {
    const std::vector<BodyHandle> bodies = world.AliveBodies();
    const auto closest = [&](float t, BodyHandle& who, glm::vec3& normal) {
        const glm::vec3 c = from + displacement * t;
        const glm::vec3 a = c + glm::vec3(0.0f, -halfHeight, 0.0f);
        const glm::vec3 b = c + glm::vec3(0.0f, halfHeight, 0.0f);
        float best = 3.4e38f;
        for (const BodyHandle h : bodies) {
            Shape shape;
            BodyTransform pose;
            world.GetBodyShape(h, shape, pose);
            RigidBody body;
            body.position = pose.position;
            body.orientation = pose.rotation;
            for (int part = 0; part < PrimitiveCount(shape); ++part) {
                const PrimitivePose prim = PrimitiveAt(shape, body, part);
                CapsuleDistance d;
                if (prim.shape.type == ShapeType::Sphere) {
                    d = CapsuleDistanceToSphere(a, b, radius, prim.body.position, prim.shape.radius);
                } else if (prim.shape.type == ShapeType::Box) {
                    d = CapsuleDistanceToBox(a, b, radius, prim.body.position, prim.body.orientation, prim.shape.halfExtents);
                } else {
                    continue;
                }
                if (d.distance < best) {
                    best = d.distance;
                    normal = d.normal;
                    who = h;
                }
            }
        }
        return best;
    };
    ShapeSweepHit result;
    const float length = glm::length(displacement);
    BodyHandle who;
    glm::vec3 normal(0.0f);
    if (closest(0.0f, who, normal) <= 0.0f) {
        result.hit = true;
        result.normal = normal;
        result.hitBody = who;
        return result;
    }
    float previous = 0.0f;
    for (int step = 1; step <= 24; ++step) {
        const float t = static_cast<float>(step) / 24.0f;
        if (closest(t, who, normal) <= 0.0f) {
            float lo = previous, hi = t;
            BodyHandle refinedWho = who;
            glm::vec3 refinedNormal = normal;
            for (int i = 0; i < 20; ++i) {
                const float mid = (lo + hi) * 0.5f;
                BodyHandle w2;
                glm::vec3 n2(0.0f);
                if (closest(mid, w2, n2) <= 0.0f) {
                    hi = mid;
                    refinedWho = w2;
                    refinedNormal = n2;
                } else {
                    lo = mid;
                }
            }
            result.hit = true;
            result.distance = lo * length;
            result.normal = refinedNormal;
            result.hitBody = refinedWho;
            return result;
        }
        previous = t;
    }
    return result;
}

void TestPlayerSweep() {
    std::printf("F: player capsule sweep through the broadphase == sweep against every body\n");
    PhysicsWorld world;
    world.Init();
    world.CreatePlayerShape(0.3f, 0.6f);
    world.CreateStaticBox(glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(50.0f, 0.5f, 50.0f), 0.8f, 0.0f);
    Random rng(5u);
    for (int i = 0; i < 300; ++i) {
        const glm::vec3 p(rng.Range(-20, 20), rng.Range(0.3f, 3.0f), rng.Range(-20, 20));
        if (i % 4 == 0) {
            world.CreateStaticSphere(p, rng.Range(0.2f, 1.0f), 0.5f, 0.0f);
        } else {
            const BodyHandle h = world.CreateDynamicBox(p, glm::vec3(rng.Range(0.1f, 0.6f)), 4.0f, 0.5f, 0.1f);
            world.ResetBody(h, p, rng.Rotation());
        }
    }
    int identical = 0;
    int hits = 0;
    constexpr int kSweeps = 300;
    for (int i = 0; i < kSweeps; ++i) {
        const glm::vec3 from(rng.Range(-20, 20), rng.Range(0.95f, 4.0f), rng.Range(-20, 20));
        const glm::vec3 displacement(rng.Range(-3, 3), rng.Range(-3, 1), rng.Range(-3, 3));
        const ShapeSweepHit a = world.SweepPlayerShape(from, glm::quat(1, 0, 0, 0), displacement);
        const ShapeSweepHit b = BruteSweep(world, 0.3f, 0.6f, from, displacement);
        if (a.hit) ++hits;
        if (a.hit == b.hit && a.distance == b.distance && a.normal == b.normal && a.hitBody.id == b.hitBody.id) ++identical;
    }
    const ShapeSweepHit clear = world.SweepPlayerShape(glm::vec3(0.0f, 30.0f, 0.0f), glm::quat(1, 0, 0, 0),
                                                       glm::vec3(3.0f, 0.0f, 0.0f));
    std::printf("    %d/%d sweeps identical to the all-bodies oracle (%d hits)\n", identical, kSweeps, hits);
    Check(hits > 50 && identical == kSweeps, "broadphase-limited sweeps are bit-identical to sweeping every body");
    Check(!clear.hit, "a sweep through empty space hits nothing");
}

// --- G: statistics at scale ------------------------------------------------------
void TestScaleStatistics() {
    std::printf("G: 1,500-crate floor: candidate pairs versus all pairs\n");
    PhysicsWorld world;
    world.Init();
    world.CreateStaticBox(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(100.0f, 1.0f, 100.0f), 0.8f, 0.05f);
    std::vector<BodyHandle> crates;
    for (int i = 0; i < 1500; ++i) {
        const float x = static_cast<float>(i % 50) * 1.5f - 37.0f;
        const float z = static_cast<float>(i / 50) * 1.5f - 22.0f;
        crates.push_back(world.CreateDynamicBox(glm::vec3(x, 0.5f, z), glm::vec3(0.5f), 5.0f, 0.6f, 0.15f));
    }
    for (int step = 0; step < 30; ++step) {
        for (const BodyHandle h : crates) world.ApplyLinearAcceleration(h, glm::vec3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);
        world.Step(1.0f / 60.0f);
    }
    const PhysicsWorld::StepStats s = world.LastStepStats();
    std::printf("    %zu bodies, %zu possible pairs, %zu candidates, %zu colliding pairs, %zu points, "
                "tree height %d, %.3f ms/step (broadphase %.3f, narrowphase %.3f, solver %.3f)\n",
                s.bodies, s.possiblePairs, s.candidatePairs, s.collidingPairs, s.contactPoints, s.treeHeight,
                s.totalMilliseconds, s.broadphaseMilliseconds, s.narrowphaseMilliseconds, s.solverMilliseconds);
    std::printf("    geometry predicates %zu, exact fallbacks %zu, unresolved %zu\n",
                s.geometryPredicates, s.geometryExactFallbacks, s.geometryUnresolved);
    std::printf("  cache orientation %zu hits %zu rebuilds; bounds %zu hits %zu rebuilds; shapes %zu; solver frames %zu; bytes %zu; allocations %zu\n",
                s.orientationCacheHits, s.orientationCacheRebuilds, s.boundCacheHits,
                s.boundCacheRebuilds, s.shapeCacheRebuilds, s.solverFrameBuilds,
                s.geometryCacheBytes, s.geometryCacheAllocations);
    Check(s.possiblePairs == 1500u * 1499u / 2u + 1500u, "possible-pair count is the all-pairs count");
    Check(s.candidatePairs < s.possiblePairs / 100, "broadphase hands the narrowphase < 1% of all pairs");
    Check(s.collidingPairs == 1500 && Compare(world).missed == 0, "every crate touches the floor and nothing is missed");
}

}  // namespace

int main() {
    TestTree();
    TestTreePairs();
    TestStaticScenes();
    TestMovingScenes();
    TestDestroyedAndReused();
    TestRotatedUniverse();
    TestPlayerSweep();
    TestScaleStatistics();
    std::printf("Broadphase tests: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
