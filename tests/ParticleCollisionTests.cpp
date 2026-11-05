#include "particles/CollisionSnapshot.h"

#include "CollisionAsset.h"
#include "Narrowphase.h"
#include "RadialTerrain.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

using namespace JudasParticles;
namespace {
unsigned checks = 0, failures = 0;
constexpr double pi = 3.14159265358979323846;
const Aabb domain{glm::vec3(-256), glm::vec3(256)};
void Check(bool condition, const char* message) {
    ++checks;
    failures += !condition;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
}
bool Near(glm::dvec3 a, glm::dvec3 b, double tolerance = 2e-4) {
    return glm::length(a - b) <= tolerance;
}
void Capture(CollisionSnapshot& snapshot, const PhysicsWorld& world, double dt,
             const Aabb& bounds = domain, const PhysicsQueryFilter& filter = {}) {
    std::string error;
    if (!snapshot.Capture(world, bounds, dt, error, filter,
                          [](BodyHandle) { return std::uint64_t(77); }))
        throw std::runtime_error(error);
}
std::shared_ptr<CollisionAsset> Triangle(double height = 0, bool twoSided = false) {
    MeshData mesh;
    mesh.vertices.resize(3);
    mesh.vertices[0].position = {-2, float(height), -2};
    mesh.vertices[1].position = {0, float(height), 2};
    mesh.vertices[2].position = {2, float(height), -2};
    mesh.indices = {0, 1, 2};
    auto asset = std::make_shared<CollisionAsset>();
    CollisionCookSettings settings;
    settings.twoSided = twoSided;
    std::string error;
    if (!CookCollision(mesh, settings, *asset, error))
        throw std::runtime_error(error);
    return asset;
}
void Analytic() {
    PhysicsWorld world;
    world.Init();
    const auto box = world.CreateStaticBox({0, 0, 0}, {.5f, .5f, .5f}, 0, 0);
    CollisionSnapshot snapshot;
    Capture(snapshot, world, 1);
    const auto hit = snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1);
    Check(hit.status == CollisionQueryStatus::Hit && std::abs(hit.fraction - .35) < 1e-4 &&
              hit.body.id == box.id && hit.entity == 77 && Near(hit.normal, {0, 1, 0}) &&
              Near(hit.point, {0, .5, 0}),
          "sphere/box impact matches independent plane time, normal and surface witness");
    const auto overlap = snapshot.OverlapSphere({0, 0, 0}, .2);
    Check(overlap.status == CollisionQueryStatus::Hit && overlap.initialOverlap &&
              std::abs(overlap.penetration - .7) < 1e-6,
          "initial containment reports actual sphere/box penetration");
    Check(snapshot.SweepSphere({2, 2, 0}, {2, -2, 0}, .1).status == CollisionQueryStatus::Miss,
          "finite box side negative control has no infinite response plane");
    const auto corner = snapshot.SweepSphere({2, 2, 0}, {0, 0, 0}, .1);
    const double known = (2 - (.5 + .1 / std::sqrt(2.))) / 2;
    Check(corner.status == CollisionQueryStatus::Hit && std::abs(corner.fraction - known) < 2e-4 &&
              Near(corner.normal, glm::normalize(glm::dvec3(1, 1, 0))),
          "rounded finite corner uses sphere distance rather than an expanded-box false contact");
    Check(Near(CollisionResponse({2, -4, 0}, hit, .5, .2), {.8, 2, 0}, 1e-6),
          "bounce plus Coulomb friction matches independent one-way impulse");
    Check(Near(CollisionResponse({2, -4, 0}, hit, 1, 0, true), {2, 0, 0}, 1e-6),
          "slide removes only closing normal speed");
    Check(Near(CollisionResponse({2, 4, 0}, hit, 1, 1), {2, 4, 0}, 1e-6),
          "separating particle receives no collision impulse");
    Check(snapshot.SweepSphere({300, 0, 0}, {301, 0, 0}, .1).status == CollisionQueryStatus::OutsideDomain,
          "query outside the captured region is explicit, never a false miss");
    Check(snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1, 0, 1, 0).status == CollisionQueryStatus::Exhausted,
          "exhausted collision policy is explicit, never a false miss");
    const auto point = snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, 0);
    Check(point.status == CollisionQueryStatus::Hit && std::abs(point.fraction - .375) < 1e-8,
          "radius-zero swept particle retains analytic ray timing");
    const auto before = world.GetTransform(box);
    const auto repeated = snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1);
    Check(repeated.fraction == hit.fraction && repeated.point == hit.point &&
              world.GetTransform(box).position == before.position &&
              world.GetLinearVelocity(box) == glm::vec3(0),
          "repeat collision is deterministic and cannot mutate the authoritative rigid world");
    world.DestroyBody(box);
    world.CreateStaticBox({0, 0, 0}, {1, .001f, 1}, 0, 0);
    Capture(snapshot, world, 1);
    const auto thin = snapshot.SweepSphere({0, 100, 0}, {0, -100, 0}, .01);
    Check(thin.status == CollisionQueryStatus::Hit &&
              std::abs(thin.fraction - (100 - .011) / 200) < 1e-6,
          "long sweep detects a two-millimetre wall without endpoint sampling tunnelling");
}
void FiniteMeshAndReplacement() {
    PhysicsWorld world;
    world.Init();
    auto original = Triangle();
    const auto surface = world.CreateShape(Shape::Cooked(original, ""), {}, false, 0, 0, 0);
    CollisionSnapshot snapshot;
    Capture(snapshot, world, 1);
    const auto hit = snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1);
    Check(hit.status == CollisionQueryStatus::Hit && std::abs(hit.fraction - .475) < 1e-4 &&
              hit.feature == 0 && Near(hit.normal, {0, 1, 0}) && hit.body.id == surface.id,
          "cooked terrain triangle reports native front-face feature and known impact time");
    Check(snapshot.SweepSphere({3, 2, 0}, {3, -2, 0}, .1).status == CollisionQueryStatus::Miss,
          "finite terrain triangle negative control cannot collide beyond its edge");
    Check(snapshot.SweepSphere({0, -2, 0}, {0, 2, 0}, .1).status == CollisionQueryStatus::Miss,
          "one-sided cooked triangle rejects back-to-front traversal");
    const auto anchor = snapshot.MakeAnchor(hit);
    const auto retained = snapshot;
    Capture(snapshot, world, 1);
    Check(snapshot.CaptureToken() != retained.CaptureToken() &&
              CollisionSnapshot(snapshot).CaptureToken() == snapshot.CaptureToken(),
          "recapture publishes a fresh sharing token while immutable copies retain the same sample identity");
    glm::dvec3 p, n;
    Check(snapshot.ResolveAnchor(anchor, 1, p, n) && Near(p, {0, 0, 0}),
          "unchanged immutable geometry retains anchor identity across captures");
    Check(world.ReplaceStaticCollision(surface, Triangle(1)), "native terrain collision replacement succeeds");
    Capture(snapshot, world, 1);
    Check(!snapshot.ResolveAnchor(anchor, 1, p, n),
          "same-body geometry replacement invalidates old particle attachment");
    const auto changed = snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1);
    Check(changed.status == CollisionQueryStatus::Hit && std::abs(changed.fraction - .225) < 1e-4 &&
              changed.geometryRevision != hit.geometryRevision,
          "published replacement changes actual physical collision and geometry revision");
    const auto old = retained.SweepSphere({0, 2, 0}, {0, -2, 0}, .1);
    Check(old.status == CollisionQueryStatus::Hit && old.fraction == hit.fraction,
          "retained snapshot owns its old geometry after native publication replaces it");
    const auto changedAnchor = snapshot.MakeAnchor(changed);
    world.DestroyBody(surface);
    const auto recycled = world.CreateShape(Shape::Cooked(Triangle(1, true), ""), {}, false, 0, 0, 0);
    Capture(snapshot, world, 1);
    Check(recycled.id != surface.id && !snapshot.ResolveAnchor(changedAnchor, 1, p, n),
          "reused native body slot cannot inherit an old generation's particle attachment");
    const auto reverse = snapshot.SweepSphere({0, -2, 0}, {0, 2, 0}, .1);
    Check(reverse.status == CollisionQueryStatus::Hit && Near(reverse.normal, {0, -1, 0}),
          "two-sided geometry independently accepts the reverse side with the opposite normal");
    CollisionCaptureLimits limits;
    limits.maxTriangles = 0;
    const auto token = snapshot.CaptureToken();
    std::string error;
    Check(!snapshot.Capture(world, domain, 1, error, {}, {}, limits) && !error.empty() &&
              snapshot.SweepSphere({0, -2, 0}, {0, 2, 0}, .1).fraction == reverse.fraction,
          "failed capture is transactional and retains the previous accepted collision sample");
    Check(snapshot.CaptureToken() == token, "failed capture cannot replace the published sharing identity");
}
void Moving() {
    PhysicsWorld world;
    world.Init();
    auto mover = world.CreateStaticBox({-3, 0, 0}, {.5f, .5f, .5f}, 0, 0);
    Check(world.SetMotionType(mover, BodyMotionType::Kinematic) &&
              world.SetKinematicVelocity(mover, {6, 0, 0}, {0, 0, 0}),
          "moving collider uses real native prescribed motion");
    world.Step(1);
    CollisionSnapshot snapshot;
    Capture(snapshot, world, 1, {glm::vec3(-.25f), glm::vec3(.25f)});
    auto hit = snapshot.SweepSphere({0, 0, 0}, {0, 0, 0}, .1);
    Check(hit.status == CollisionQueryStatus::Hit && std::abs(hit.fraction - .4) < 5e-4 &&
              Near(hit.normal, {1, 0, 0}) && Near(hit.surfaceVelocity, {6, 0, 0}),
          "departed collider's recorded path hits a stationary particle inside a small domain");
    Check(Near(CollisionResponse({0, 0, 0}, hit, .5, 0), {9, 0, 0}),
          "moving-boundary bounce uses relative velocity rather than a static wall");
    const auto anchor = snapshot.MakeAnchor(hit);
    glm::dvec3 point, normal, velocity;
    Check(snapshot.ResolveAnchor(anchor, .5, point, normal, &velocity) &&
              Near(point, {.5, 0, 0}, 5e-4) && Near(velocity, {6, 0, 0}),
          "body-local attachment follows the captured center-of-mass trajectory");
    world.DestroyBody(mover);
    mover = world.CreateStaticBox({0, 0, 0}, {2, .05f, .05f}, 0, 0);
    world.SetMotionType(mover, BodyMotionType::Kinematic);
    world.SetKinematicVelocity(mover, {0, 0, 0}, {0, 0, float(pi)});
    world.Step(1);
    Capture(snapshot, world, 1);
    hit = snapshot.SweepSphere({0, 1, 0}, {0, 1, 0}, .05);
    Check(hit.status == CollisionQueryStatus::Hit &&
              std::abs(hit.fraction - std::acos(.1) / pi) < 5e-4 &&
              Near(hit.surfaceVelocity, glm::cross(glm::dvec3(0, 0, pi), hit.point)),
          "rotating thin bar hits between equivalent endpoint geometries with actual angular surface motion");
    Check(snapshot.SweepSphere({0, 1, 0}, {0, 1, 0}, .05, 0, 1, 1).status ==
              CollisionQueryStatus::Exhausted,
          "insufficient moving-sweep budget is diagnosed instead of skipping an interior impact");
    world.DestroyBody(mover);
    auto triangle = world.CreateShape(Shape::Cooked(Triangle(), ""), {}, false, 0, 0, 0);
    Check(!world.SetMotionType(triangle, BodyMotionType::Kinematic) &&
              !world.SetKinematicVelocity(triangle, {0, 1, 0}, {0, 0, 0}) &&
              world.GetMotionType(triangle) == BodyMotionType::Static,
          "native concave owner rejects unsupported motion atomically before snapshot publication");
    world.Step(1);
    Capture(snapshot, world, 1);
    Check(snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1).status == CollisionQueryStatus::Hit,
          "rejected motion leaves the original finite static collision usable");
}
void FilteringAndFrames() {
    PhysicsWorld world;
    world.Init();
    const auto box = world.CreateStaticBox({0, 0, 0}, {.5f, .5f, .5f}, 0, 0);
    world.SetCollisionFilter(box, 3, kAllCategories);
    world.SetBodyTags(box, CategoryBit(2));
    PhysicsQueryFilter filter;
    filter.includeLayers = CategoryBit(3);
    filter.requiredTags = CategoryBit(2);
    CollisionSnapshot snapshot;
    Capture(snapshot, world, 1, domain, filter);
    Check(snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1).status == CollisionQueryStatus::Hit,
          "snapshot honors the actual native layer and tag query policy");
    filter.excludedTags = CategoryBit(2);
    Capture(snapshot, world, 1, domain, filter);
    Check(snapshot.Colliders().empty() &&
              snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1).status == CollisionQueryStatus::Miss,
          "excluded-tag control removes geometry before particle work");
    Capture(snapshot, world, 1);
    const auto before = snapshot.SweepSphere({0, 2, 0}, {0, -2, 0}, .1);
    world.ShiftOrigin({10, 20, 30});
    Capture(snapshot, world, 1);
    const auto shifted = snapshot.SweepSphere({-10, -18, -30}, {-10, -22, -30}, .1);
    Check(shifted.status == CollisionQueryStatus::Hit && shifted.fraction == before.fraction &&
              Near(shifted.point + glm::dvec3(10, 20, 30), before.point) && shifted.normal == before.normal,
          "origin translation preserves actual collision time, normal and local geometry");
    world.DestroyBody(box);
    auto radial = std::make_shared<RadialTerrain>(10, [](glm::vec3) { return 0.f; }, 0);
    world.CreateStaticTerrain({0, 0, 0}, {1, 0, 0, 0}, radial, 0, 0);
    Capture(snapshot, world, 1);
    const auto planet = snapshot.SweepSphere({12, 0, 0}, {8, 0, 0}, .1);
    Check(planet.status == CollisionQueryStatus::Hit && std::abs(planet.fraction - .475) < 1e-4 &&
              Near(planet.normal, {1, 0, 0}),
          "static radial CPU fallback follows actual terrain normal without a universal Y-up plane");
}
} // namespace

int main() {
    try {
        Analytic();
        FiniteMeshAndReplacement();
        Moving();
        FilteringAndFrames();
    } catch (const std::exception& error) {
        Check(false, error.what());
    }
    std::printf("Particle collision: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
