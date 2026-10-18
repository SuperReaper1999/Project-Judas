#include "OriginLimits.h"
#include "RuntimeWorld.h"
#include "WorldPersistence.h"

#include <cstdio>
#include <limits>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool condition, const char* description) {
    ++checks;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition) throw std::runtime_error(description);
}

void Build(RuntimeWorld& world, double origin) {
    Scene scene;
    scene.Settings().worldOrigin = {origin, 0, 0};
    world.legacyGameplay = false;
    std::string error;
    Check(world.Build(scene, nullptr, error), "ordinary empty world constructs at accepted origin");
}

void Rejection(double from, double to) {
    RuntimeWorld world;
    Build(world, from);
    std::string error;
    std::map<std::string, SaveChunk> before, after;
    Check(WorldPersistence::Capture(world, before, error), "capture actual pre-request participant state");
    const auto revision = world.OriginRevision();
    Check(!world.RequestOrigin({to, 0, 0}, error) && error.find("translation") != std::string::npos,
          "unsupported delta is rejected during admission with actionable diagnosis");
    Check(!world.OriginPending() && world.Settings().worldOrigin == glm::dvec3(from, 0, 0) &&
              world.OriginRevision() == revision,
          "rejected request leaves actual chart and pending state untouched");
    Check(world.CommitOrigin() && world.OriginRevision() == revision,
          "commit after rejection is a safe no-op rather than a native exception");
    Check(WorldPersistence::Capture(world, after, error) && before.size() == after.size(),
          "post-rejection snapshot captures the same participant set");
    bool identical = true;
    for (const auto& [name, chunk] : before) {
        const auto it = after.find(name);
        identical &= it != after.end() && it->second.version == chunk.version &&
                     it->second.data == chunk.data;
    }
    Check(identical, "rejected transaction preserves every serialized participant byte");
}

void PositiveAndPending() {
    RuntimeWorld world;
    Build(world, -OriginLimits::MaximumComponent);
    std::string error;
    Check(world.RequestOrigin({0, 0, 0}, error), "exact native translation limit remains admitted");
    Check(!world.RequestOrigin({OriginLimits::MaximumComponent, 0, 0}, error) && world.OriginPending(),
          "invalid replacement cannot consume a previously admitted pending origin");
    Check(world.CommitOrigin() && world.Settings().worldOrigin == glm::dvec3(0) &&
              world.OriginRevision() == 1 && !world.OriginPending(),
          "the original pending transaction still publishes exactly once");
    Check(world.RequestOrigin({512, -512, 512}, error) && world.CommitOrigin() &&
              world.Settings().worldOrigin == glm::dvec3(512, -512, 512) && world.OriginRevision() == 2,
          "ordinary non-Y chart translation remains supported");
    const auto origin = world.Settings().worldOrigin;
    const auto revision = world.OriginRevision();
    Check(!world.RequestOrigin({std::numeric_limits<double>::quiet_NaN(), 0, 0}, error) &&
              !world.RequestOrigin({OriginLimits::MaximumComponent + 1024, 0, 0}, error),
          "nonfinite and out-of-domain absolute destinations still reject");
    Check(world.Settings().worldOrigin == origin && world.OriginRevision() == revision &&
              !world.OriginPending(),
          "invalid absolute requests cannot change the existing chart");
    Check(world.RequestOrigin(origin + glm::dvec3(127, -127, 31), error) && world.CommitOrigin() &&
              world.Settings().worldOrigin == origin && world.OriginRevision() == revision,
          "sub-anchor requests retain the existing documented 512 m quantization");
}
}  // namespace

int main() {
    try {
        Rejection(-OriginLimits::MaximumComponent, OriginLimits::MaximumComponent);
        Rejection(OriginLimits::MaximumComponent, -OriginLimits::MaximumComponent);
        PositiveAndPending();
        PhysicsWorld physics;
        physics.Init();
        const auto body = physics.CreateDynamicSphere({1, 2, 3}, .5f, 1, .5f, 0);
        const auto pose = physics.GetTransform(body);
        bool threw = false;
        try { physics.ShiftOrigin({2 * OriginLimits::MaximumComponent, 0, 0}); }
        catch (const std::invalid_argument&) { threw = true; }
        Check(threw && physics.GetTransform(body).position == pose.position,
              "direct native invalid translation retains its original exception and physical endpoint");
        std::printf("SUMMARY %u checks 0 failures\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL origin admission after %u checks: %s\n", checks, error.what());
        return 1;
    }
}
