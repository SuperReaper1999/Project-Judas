#include "CollisionAsset.h"
#include "CollisionGeometry.h"
#include "GameWaterRuntimeInternal.h"
#include "RadialTerrain.h"
#include "RuntimeWorld.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace JudasWater {
bool WorldSystem::Region::RefreshGravity(RuntimeWorld &world, Grid &target, std::string &error) {
    const auto &c = target.Configuration();
    std::vector<glm::vec3> signature;
    for (int z : {0, 1})
        for (int y : {0, 1})
            for (int x : {0, 1})
                signature.push_back(world.SampleEntityGravity(
                    owner, target.ToWorld(glm::vec3(c.cells) * c.cellSize * glm::vec3(x, y, z))));
    if (signature == gravitySignature)
        return true;
    std::vector<glm::vec3> field;
    if (std::any_of(signature.begin(), signature.end(), [&](auto g) { return g != c.gravity; })) {
        field.reserve(target.Cells().size());
        for (int i = 0; i < int(target.Cells().size()); ++i)
            field.push_back(world.SampleEntityGravity(owner, target.ToWorld(target.Center(i))));
    }
    if (!target.SetGravityField(field, error))
        return false;
    gravitySignature = std::move(signature);
    return true;
}
namespace {
bool SameShape(const Shape &a, const Shape &b) {
    if (a.type != b.type || a.radius != b.radius || a.halfHeight != b.halfHeight ||
        a.halfExtents != b.halfExtents || a.asset != b.asset || a.terrain != b.terrain ||
        a.pivotOffset != b.pivotOffset || a.boxes.size() != b.boxes.size())
        return false;
    for (std::size_t i = 0; i < a.boxes.size(); ++i) {
        const auto &x = a.boxes[i];
        const auto &y = b.boxes[i];
        if (x.localCenter != y.localCenter || x.halfExtents != y.halfExtents ||
            x.rotation != y.rotation || x.type != y.type || x.radius != y.radius ||
            x.asset != y.asset)
            return false;
    }
    return true;
}
bool Occupied(const PrimitivePose &p, glm::vec3 point, float shell) {
    const auto center = PrimitiveCenter(p);
    const auto local = glm::transpose(PrimitiveRotation(p)) * (glm::dvec3(point) - center);
    const auto &shape = p.shape;
    if (shape.type == ShapeType::Box)
        return glm::all(glm::lessThanEqual(glm::abs(local), glm::dvec3(shape.halfExtents) + 1e-6));
    if (shape.type == ShapeType::Sphere)
        return glm::dot(local, local) <= shape.radius * shape.radius;
    if (shape.type == ShapeType::Capsule) {
        auto v = local;
        v.y -= glm::clamp(v.y, -double(shape.halfHeight), double(shape.halfHeight));
        return glm::dot(v, v) <= shape.radius * shape.radius;
    }
    if (shape.type == ShapeType::Terrain)
        return shape.terrain && shape.terrain->Sample(glm::vec3(local)).signedDistance <= 0;
    const auto distance = PointGeometry(glm::dvec3(point), p, shell);
    return distance.valid && (distance.contains || distance.gap <= shell);
}
} // namespace

std::vector<SolidBox> ReadBodySolids(const PhysicsWorld &physics, BodyHandle handle) {
    Shape shape;
    BodyTransform pose;
    std::vector<SolidBox> result;
    if (!physics.GetBodyShape(handle, shape, pose))
        return result;
    RigidBody parent;
    parent.position = pose.position;
    parent.orientation = pose.rotation;
    for (int k = 0; k < PrimitiveCount(shape); ++k) {
        const auto p = PrimitiveAt(shape, parent, k);
        SolidBox solid;
        solid.id = (std::uint64_t(handle.id) << 32) | std::uint32_t(k);
        solid.center = glm::vec3(PrimitiveCenter(p));
        solid.rotation = glm::quat_cast(glm::mat3(PrimitiveRotation(p)));
        solid.velocity = physics.GetPointVelocity(handle, solid.center);
        solid.angularVelocity = physics.GetAngularVelocity(handle);
        solid.half = p.shape.halfExtents;
        if (p.shape.type == ShapeType::Sphere || p.shape.type == ShapeType::Capsule) {
            solid.kind = p.shape.type == ShapeType::Sphere ? SolidBox::Kind::Sphere
                                                           : SolidBox::Kind::Capsule;
            solid.radius = p.shape.radius;
            solid.halfHeight = p.shape.halfHeight;
            solid.half = {solid.radius, solid.radius + solid.halfHeight, solid.radius};
        } else if (p.shape.type == ShapeType::ConvexHull && p.shape.asset) {
            solid.kind = SolidBox::Kind::Convex;
            solid.asset = p.shape.asset;
            solid.half =
                glm::vec3(glm::max(glm::abs(solid.asset->minimum), glm::abs(solid.asset->maximum)));
        } else if (p.shape.type != ShapeType::Box) {
            throw std::runtime_error(
                "grid water supports moving boxes, spheres, capsules and convex "
                "compounds; moving "
                "triangle surfaces require a solid-volume representation");
        }
        result.push_back(std::move(solid));
    }
    return result;
}

bool WorldSystem::Region::PrepareGeometry(RuntimeWorld &world, Grid &target,
                                          std::vector<SolidBox> &solids,
                                          std::vector<BodyHandle> &moving, std::string &error) {
    const auto &c = target.Configuration();
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (int z : {0, 1})
        for (int y : {0, 1})
            for (int x : {0, 1}) {
                auto p = target.ToWorld(glm::vec3(c.cells) * c.cellSize * glm::vec3(x, y, z));
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }
    std::vector<StaticGeometry> current;
    auto terrain = world.HasRuntimeLandscapes() ? world.LiveTerrain().Solids(lo, hi)
                                               : std::vector<TerrainV2::SolidSnapshot>{};
    const auto handles = world.Physics().QueryBodiesInAabb(lo, hi);
    size_t physicalSources = terrain.size();
    for (auto handle : handles) {
        // Native heightfield/volume meshes are collision boundaries, not a
        // closed solid. Their source supplies both below-surface occupancy and
        // excavation air. Never also treat them as isolated thin shells.
        if (world.HasRuntimeLandscapes() && world.LiveTerrain().OwnsGenerated(handle))
            continue;
        if (++physicalSources > 512) {
            error = "grid water collision region exceeds 512 sources; split the active region";
            return false;
        }
        Shape shape;
        BodyTransform pose;
        if (!world.Physics().GetBodyShape(handle, shape, pose))
            continue;
        if (world.Physics().IsDynamicBody(handle) || world.Physics().IsKinematicBody(handle) ||
            world.Physics().IsQueryOnlyBody(handle)) {
            try {
                auto parts = ReadBodySolids(world.Physics(), handle);
                solids.insert(solids.end(), parts.begin(), parts.end());
            } catch (const std::exception &e) {
                error = e.what();
                return false;
            }
            if (world.Physics().IsDynamicBody(handle))
                moving.push_back(handle);
        } else
            current.push_back({handle, std::move(shape), pose});
    }
    bool same = current.size() == statics.size();
    for (std::size_t i = 0; same && i < current.size(); ++i)
        same = current[i].handle.id == statics[i].handle.id &&
               current[i].handle.world == statics[i].handle.world &&
               current[i].pose.position == statics[i].pose.position &&
               current[i].pose.rotation == statics[i].pose.rotation &&
               SameShape(current[i].shape, statics[i].shape);
    same &= terrain.size() == terrains.size();
    for (size_t i = 0; same && i < terrain.size(); ++i)
        same = terrain[i].SameGeometry(terrains[i]);
    if (!same) {
        ++geometryBakes;
        std::vector<PrimitivePose> primitives;
        for (const auto &entry : current) {
            RigidBody body;
            body.position = entry.pose.position;
            body.orientation = entry.pose.rotation;
            for (int k = 0; k < PrimitiveCount(entry.shape); ++k)
                primitives.push_back(PrimitiveAt(entry.shape, body, k));
        }
        target.BakeStatic([&](glm::vec3 p) {
            for (const auto &source : terrain)
                if (source.Contains(p))
                    return true;
            for (const auto &primitive : primitives)
                if (Occupied(primitive, p, c.cellSize * .125f))
                    return true;
            return false;
        });
    }
    pendingStatics = std::move(current);
    pendingTerrains = std::move(terrain);
    target.SetSolids(solids);
    return true;
}
} // namespace JudasWater
