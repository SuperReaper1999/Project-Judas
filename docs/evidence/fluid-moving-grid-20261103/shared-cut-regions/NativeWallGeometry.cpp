#include "BasinMk2.h"
#include "CollisionGeometry.h"

#include <iomanip>
#include <iostream>

namespace {

using namespace judas::volume_grid::basin_probe;

PrimitivePose Retrieve(PhysicsWorld& world, BodyHandle handle) {
    Shape shape;
    BodyTransform pose;
    if (!world.GetBodyShape(handle, shape, pose))
        throw std::runtime_error("could not retrieve authoritative wall shape");
    RigidBody rigid;
    rigid.position = pose.position;
    rigid.orientation = pose.rotation;
    return PrimitiveAt(shape, rigid, 0);
}

bool Probe(bool rotated) {
    const auto fixture = Fixture::Authored(rotated);
    PhysicsWorld world;
    if (!world.Init())
        return false;
    const auto create = [&](std::size_t index) {
        const auto wall = Fixture::wallDefinitions[index];
        return world.CreateStaticBox(fixture.rotation * wall.center, fixture.rotation, wall.half, 0.f, 0.f);
    };
    const auto xwall = create(2);
    const auto zwall = create(3);
    if (!xwall.IsValid() || !zwall.IsValid())
        return false;
    const auto x = Retrieve(world, xwall);
    const auto z = Retrieve(world, zwall);
    const auto basis = PrimitiveRotation(x);
    const auto local = glm::transpose(basis);
    const auto xCenter = local * PrimitiveCenter(x), zCenter = local * PrimitiveCenter(z);
    const double inside = xCenter.x - x.shape.halfExtents.x;
    const double edge = zCenter.x + z.shape.halfExtents.x;
    const double slit = inside - edge;
    const glm::dvec3 chartPoint((inside + edge) * .5, .25, zCenter.z + .05);
    const auto sample = basis * chartPoint;
    const auto toX = PointGeometry(sample, x);
    const auto toZ = PointGeometry(sample, z);
    const auto boundsX = ShapeAabb(x.shape, x.parentPosition, x.parentOrientation);
    const auto boundsZ = ShapeAabb(z.shape, z.parentPosition, z.parentOrientation);
    PhysicsQueryFilter filter;
    filter.ignoredBodies = {zwall};
    const auto xQuery = world.ClosestPoint(glm::vec3(basis * glm::dvec3(1., .25, -1.05)), 1.f, filter);
    std::cout << "WALL rotation=" << rotated << " inner_x=" << inside << " wall_end_x=" << edge
              << " slit_m=" << slit << " gap_x=" << toX.gap << " gap_z=" << toZ.gap
              << " inside_x=" << toX.contains << " inside_z=" << toZ.contains
              << " aabb_min_x=" << boundsX.min.x << " aabb_max_z_wall_x=" << boundsZ.max.x
              << " public_contains=" << xQuery.contains << " public_gap=" << xQuery.distance
              << " public_surface_x=" << xQuery.point.x << '\n';
    const bool passed = toX.valid && toZ.valid && xQuery.hit &&
        (slit > 0. ? !toX.contains && !toZ.contains &&
            std::abs(toX.gap - .5 * slit) < 1e-14 && std::abs(toZ.gap - .5 * slit) < 1e-14 :
            toX.contains && toZ.contains);
    return passed && (rotated || (inside == double(1.1f) - double(.1f) && edge == 1. &&
        !xQuery.contains && xQuery.distance == static_cast<float>(slit)));
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    const bool level = Probe(false), rotated = Probe(true);
    std::cout << "WALL_SUMMARY level=" << level << " rotated=" << rotated << '\n';
    return level && rotated ? 0 : 1;
}
