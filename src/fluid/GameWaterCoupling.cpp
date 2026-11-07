#include "GameWaterCoupling.h"
#include <algorithm>
#include <cmath>

namespace JudasWater {
float Grid::ImmersedBox(const SolidBox &box, glm::vec3 &center) const {
    center = box.center;
    if (cells_.empty())
        return 0;
    const auto position = ToLocal(box.center);
    const auto q = glm::conjugate(settings_.rotation) * box.rotation;
    const auto ext = glm::abs(q * glm::vec3(box.half.x, 0, 0)) +
                     glm::abs(q * glm::vec3(0, box.half.y, 0)) +
                     glm::abs(q * glm::vec3(0, 0, box.half.z));
    const float h = settings_.cellSize;
    const auto bounds = glm::vec3(settings_.cells) * h;
    if (glm::any(glm::lessThan(position + ext, glm::vec3(0))) ||
        glm::any(glm::greaterThan(position - ext, bounds)))
        return 0;
    const auto lo = glm::ivec3(glm::floor((position - ext) / h)),
               hi = glm::ivec3(glm::floor((position + ext) / h));
    // Nearby exposed columns give the water level around this excluded body.
    // A witness must overlap its vertical range: remote water below a dry gap
    // cannot classify the object as immersed.
    double sum = 0, weight = 0;
    auto blocked = [&](glm::vec3 target) {
        const auto start =
            glm::clamp(position, glm::vec3(h * 1e-4f), bounds - glm::vec3(h * 1e-4f));
        const unsigned count =
            std::max(1u, unsigned(std::ceil(glm::length(target - start) / (h * .25f))));
        for (unsigned k = 0; k <= count; ++k) {
            auto p = start + (target - start) * (float(k) / count);
            auto at = glm::ivec3(glm::floor(p / h));
            int i = Index(at.x, at.y, at.z);
            if (i < 0)
                continue;
            auto sub = glm::clamp(glm::ivec3(glm::floor(glm::fract(p / h) * 4.f)), glm::ivec3(0),
                                  glm::ivec3(3));
            if (staticSamples_[i] & (std::uint64_t(1) << ((sub.z * 4 + sub.y) * 4 + sub.x)))
                return true;
        }
        return false;
    };
    for (int z = lo.z - 1; z <= hi.z + 1; ++z)
        for (int x = lo.x - 1; x <= hi.x + 1; ++x) {
            if (x < 0 || z < 0 || x >= settings_.cells.x || z >= settings_.cells.z)
                continue;
            if (x > lo.x && x < hi.x && z > lo.z && z < hi.z)
                continue;
            for (int y = std::min(settings_.cells.y - 1, hi.y + 1); y >= std::max(0, lo.y - 1);
                 --y) {
                const auto &c = cells_[Index(x, y, z)];
                if (c.water < .05 * c.staticCapacity || c.capacity <= 0)
                    continue;
                float level = waterHeight_[Index(x, y, z)];
                if (level < position.y - ext.y - h * .25f)
                    continue;
                // Only cells touching the body's footprint may support its
                // extrapolated interior field; walls block that extension.
                float dx = std::max(0.f, std::abs((x + .5f) * h - position.x) - ext.x);
                float dz = std::max(0.f, std::abs((z + .5f) * h - position.z) - ext.z);
                if (dx > h * .5f + 1e-5f || dz > h * .5f + 1e-5f)
                    continue;
                auto witness = Center(Index(x, y, z));
                witness.y = glm::clamp(position.y, y * h + h * 1e-4f,
                                       std::max(y * h + h * 1e-4f, level - h * 1e-4f));
                if (blocked(witness))
                    continue;
                double w = 1. / (h * h + dx * dx + dz * dz);
                sum += w * level;
                weight += w;
                break;
            }
        }
    if (weight == 0) {
        center = box.center;
        return 0;
    }
    const float level = float(sum / weight);
    double fraction = 0;
    glm::vec3 moment(0);
    // Smooth finite-volume quadrature in the vertical direction, rather than
    // binary wet/dry point impulses. Force is bounded by displaced body volume.
    for (int z = 0; z < 4; ++z)
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                glm::vec3 p =
                    position + q * (2.f * box.half * ((glm::vec3(x, y, z) + .5f) / 4.f - .5f));
                if (glm::any(glm::lessThan(p, glm::vec3(0))) ||
                    glm::any(glm::greaterThan(p, bounds)))
                    continue;
                const float halfHeight = (std::abs((q * glm::vec3(box.half.x, 0, 0)).y) +
                                          std::abs((q * glm::vec3(0, box.half.y, 0)).y) +
                                          std::abs((q * glm::vec3(0, 0, box.half.z)).y)) /
                                         4.f;
                float wet = glm::clamp(
                    (level - p.y + halfHeight) / (2 * std::max(halfHeight, 1e-6f)), 0.f, 1.f);
                fraction += wet;
                moment += wet * p;
            }
    center = fraction > 0 ? ToWorld(moment / float(fraction)) : box.center;
    return float(8. * box.half.x * box.half.y * box.half.z * fraction / 64.);
}
SolidBox ReadBox(const PhysicsWorld &world, BodyHandle handle, const glm::vec3 &half) {
    const auto pose = world.GetTransform(handle);
    return {handle.id,
            pose.position,
            half,
            world.GetLinearVelocity(handle),
            world.GetAngularVelocity(handle),
            pose.rotation};
}
BodyLoad ApplyBoxLoad(Grid &grid, PhysicsWorld &world, BodyHandle handle, const glm::vec3 &half,
                      float dt) {
    auto box = ReadBox(world, handle, half);
    BodyLoad load;
    load.immersedVolume = grid.ImmersedBox(box, load.center);
    if (load.immersedVolume <= 0)
        return load;
    const float rho = grid.Configuration().density, mass = world.GetMass(handle),
                volume = 8 * half.x * half.y * half.z;
    const auto relative = box.velocity - grid.Velocity(load.center);
    const float area = std::pow(volume, 2.f / 3.f) * (load.immersedVolume / volume);
    const float radius = glm::length(half) + grid.Configuration().cellSize;
    const float fluidMass = grid.NearbyMass(load.center, radius);
    const float coefficient = .5f * rho * area * glm::length(relative);
    const auto drag =
        -relative *
        (coefficient /
         (1 + dt * coefficient * (1 / std::max(mass, 1e-6f) + 1 / std::max(fluidMass, 1e-6f))));
    load.force = -rho * load.immersedVolume * grid.Configuration().gravity + drag;
    world.ApplyForceAtPoint(handle, load.force, load.center);
    grid.AddImpulse(load.center, -drag * dt, radius);
    // Analytic implicit drag is dissipative, without an arbitrary speed cap.
    const auto omega = box.angularVelocity;
    load.torque = -omega * (rho * volume * area * .15f /
                            (1 + dt * rho * volume * area * .15f / std::max(mass, 1e-6f)));
    world.ApplyTorque(handle, load.torque, false);
    return load;
}
} // namespace JudasWater
