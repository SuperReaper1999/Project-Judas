#include "FluidWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include <glm/gtc/quaternion.hpp>

#include "GravityField.h"
#include "RadialTerrain.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1.0e-7f;

struct Cell {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const Cell& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct CellHash {
    std::size_t operator()(const Cell& cell) const {
        // The grid is only a neighbor index; hash order never affects the
        // order of particle interactions or the authoritative result.
        std::size_t hash = std::hash<int>{}(cell.x);
        hash ^= std::hash<int>{}(cell.y) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(cell.z) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
        return hash;
    }
};

Cell CellFor(const glm::vec3& position, float cellWidth) {
    return Cell{static_cast<int>(std::floor(position.x / cellWidth)),
                static_cast<int>(std::floor(position.y / cellWidth)),
                static_cast<int>(std::floor(position.z / cellWidth))};
}

using NeighborLists = std::vector<std::vector<std::size_t>>;

NeighborLists BuildNeighbors(const std::vector<glm::vec3>& positions, float radius) {
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> grid;
    grid.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        grid[CellFor(positions[i], radius)].push_back(i);
    }

    NeighborLists neighbors(positions.size());
    const float radiusSquared = radius * radius;
    for (std::size_t i = 0; i < positions.size(); ++i) {
        const Cell center = CellFor(positions[i], radius);
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    const auto found = grid.find(Cell{center.x + x, center.y + y, center.z + z});
                    if (found == grid.end()) continue;
                    for (std::size_t j : found->second) {
                        const glm::vec3 separation = positions[i] - positions[j];
                        if (j != i && glm::dot(separation, separation) < radiusSquared) {
                            neighbors[i].push_back(j);
                        }
                    }
                }
            }
        }
    }
    return neighbors;
}

float Poly6(float squaredDistance, float radius) {
    const float squaredRadius = radius * radius;
    if (squaredDistance >= squaredRadius) return 0.0f;
    const float remaining = squaredRadius - squaredDistance;
    const float radius3 = squaredRadius * radius;
    const float radius9 = radius3 * radius3 * radius3;
    return (315.0f / (64.0f * kPi * radius9)) * remaining * remaining * remaining;
}

glm::vec3 SpikyGradient(const glm::vec3& displacement, float radius) {
    const float distance = glm::length(displacement);
    if (distance <= kEpsilon || distance >= radius) return glm::vec3(0.0f);
    const float radius2 = radius * radius;
    const float radius6 = radius2 * radius2 * radius2;
    const float remaining = radius - distance;
    return (-45.0f / (kPi * radius6)) * remaining * remaining *
           (displacement / distance);
}

float DensityAt(std::size_t i, const std::vector<glm::vec3>& positions,
                const std::vector<FluidParticle>& particles,
                const NeighborLists& neighbors, float radius) {
    float density = particles[i].mass * Poly6(0.0f, radius);
    for (std::size_t j : neighbors[i]) {
        const glm::vec3 separation = positions[i] - positions[j];
        density += particles[j].mass * Poly6(glm::dot(separation, separation), radius);
    }
    return density;
}

BodyTransform InterpolatePose(const FluidBoxCollider& box, float alpha) {
    BodyTransform pose;
    pose.position = glm::mix(box.previousPose.position, box.currentPose.position, alpha);
    pose.rotation = glm::normalize(glm::slerp(box.previousPose.rotation,
                                               box.currentPose.rotation, alpha));
    return pose;
}

BodyTransform InterpolatePose(const FluidSphereCollider& sphere, float alpha) {
    BodyTransform pose;
    pose.position = glm::mix(sphere.previousPose.position, sphere.currentPose.position, alpha);
    pose.rotation = glm::normalize(glm::slerp(sphere.previousPose.rotation,
                                               sphere.currentPose.rotation, alpha));
    return pose;
}

BodyTransform InterpolatePose(const FluidTerrainCollider& terrain, float alpha) {
    BodyTransform pose;
    pose.position = glm::mix(terrain.previousPose.position,
                             terrain.currentPose.position, alpha);
    pose.rotation = glm::normalize(glm::slerp(terrain.previousPose.rotation,
                                               terrain.currentPose.rotation, alpha));
    return pose;
}

struct BoxContact {
    bool hit = false;
    BodyHandle owner;
    glm::vec3 projectedPosition{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec3 point{0.0f};
    glm::vec3 wallVelocity{0.0f};
};

struct PreparedBox {
    BodyHandle owner;
    BodyTransform start;
    BodyTransform end;
    glm::quat inverseStart{1.0f, 0.0f, 0.0f, 0.0f};
    glm::quat inverseEnd{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 halfExtents{0.0f};
    glm::vec3 sweptCenter{0.0f};
    float sweptRadius = 0.0f;
};

struct PreparedSphere {
    BodyHandle owner;
    BodyTransform start;
    BodyTransform end;
    float radius = 0.0f;
    glm::vec3 sweptCenter{0.0f};
    float sweptRadius = 0.0f;
};

struct PreparedTerrain {
    BodyHandle owner;
    BodyTransform start;
    BodyTransform end;
    glm::quat inverseStart{1.0f, 0.0f, 0.0f, 0.0f};
    glm::quat inverseEnd{1.0f, 0.0f, 0.0f, 0.0f};
    const RadialTerrain* surface = nullptr;
    glm::vec3 sweptCenter{0.0f};
    float sweptRadius = 0.0f;
};

PreparedBox Prepare(const FluidBoxCollider& box, float alpha0, float alpha1) {
    PreparedBox prepared;
    prepared.owner = box.owner;
    prepared.start = InterpolatePose(box, alpha0);
    prepared.end = InterpolatePose(box, alpha1);
    prepared.inverseStart = glm::inverse(prepared.start.rotation);
    prepared.inverseEnd = glm::inverse(prepared.end.rotation);
    prepared.halfExtents = box.halfExtents;
    prepared.sweptCenter = 0.5f * (prepared.start.position + prepared.end.position);
    prepared.sweptRadius = glm::length(box.halfExtents) +
                           0.5f * glm::distance(prepared.start.position, prepared.end.position);
    return prepared;
}

PreparedSphere Prepare(const FluidSphereCollider& sphere, float alpha0, float alpha1) {
    PreparedSphere prepared;
    prepared.owner = sphere.owner;
    prepared.start = InterpolatePose(sphere, alpha0);
    prepared.end = InterpolatePose(sphere, alpha1);
    prepared.radius = sphere.radius;
    prepared.sweptCenter = 0.5f * (prepared.start.position + prepared.end.position);
    prepared.sweptRadius = sphere.radius +
                           0.5f * glm::distance(prepared.start.position, prepared.end.position);
    return prepared;
}

PreparedTerrain Prepare(const FluidTerrainCollider& terrain, float alpha0, float alpha1) {
    PreparedTerrain prepared;
    prepared.owner = terrain.owner;
    prepared.start = InterpolatePose(terrain, alpha0);
    prepared.end = InterpolatePose(terrain, alpha1);
    prepared.inverseStart = glm::inverse(prepared.start.rotation);
    prepared.inverseEnd = glm::inverse(prepared.end.rotation);
    prepared.surface = terrain.surface;
    prepared.sweptCenter = 0.5f * (prepared.start.position + prepared.end.position);
    prepared.sweptRadius = terrain.surface->BoundRadius() +
                           0.5f * glm::distance(prepared.start.position,
                                                prepared.end.position);
    return prepared;
}

bool FarFromSweptSolid(const glm::vec3& from, const glm::vec3& to,
                       const glm::vec3& sweptCenter, float sweptRadius,
                       float particleRadius) {
    // Conservative bounding spheres for the solid and particle trajectories.
    // Even a fast moving wall remains inside this bound during the substep.
    const glm::vec3 particleMiddle = 0.5f * (from + to);
    const float reach = sweptRadius + particleRadius + 0.5f * glm::distance(from, to);
    const glm::vec3 separation = particleMiddle - sweptCenter;
    return glm::dot(separation, separation) > reach * reach;
}

// A point swept relative to a moving OBB, expanded by the fluid element's
// collision radius. This is geometric contact against the box, regardless
// of whether it is a cup wall, floor, or another ordinary body.
BoxContact CollideBox(const glm::vec3& from, const glm::vec3& to,
                      const PreparedBox& box, float radius, float substepTime) {
    BoxContact result;
    if (FarFromSweptSolid(from, to, box.sweptCenter, box.sweptRadius, radius)) return result;
    const BodyTransform& start = box.start;
    const BodyTransform& end = box.end;
    const glm::vec3 startLocal = box.inverseStart * (from - start.position);
    const glm::vec3 endLocal = box.inverseEnd * (to - end.position);
    const glm::vec3 half = box.halfExtents + glm::vec3(radius);

    auto inside = [&](const glm::vec3& point) {
        return std::abs(point.x) < half.x && std::abs(point.y) < half.y &&
               std::abs(point.z) < half.z;
    };
    const bool startInside = inside(startLocal);
    const bool endInside = inside(endLocal);

    float entry = 0.0f;
    float exit = 1.0f;
    int entryAxis = -1;
    float entrySign = 0.0f;
    const glm::vec3 displacement = endLocal - startLocal;
    bool sweptHit = true;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(displacement[axis]) <= kEpsilon) {
            if (std::abs(startLocal[axis]) > half[axis]) sweptHit = false;
            continue;
        }
        const float t0 = (-half[axis] - startLocal[axis]) / displacement[axis];
        const float t1 = (half[axis] - startLocal[axis]) / displacement[axis];
        const float nearTime = std::min(t0, t1);
        const float farTime = std::max(t0, t1);
        if (nearTime > entry) {
            entry = nearTime;
            entryAxis = axis;
            entrySign = displacement[axis] > 0.0f ? -1.0f : 1.0f;
        }
        exit = std::min(exit, farTime);
        if (entry > exit) sweptHit = false;
    }
    sweptHit = sweptHit && entryAxis >= 0 && entry >= 0.0f && entry <= 1.0f &&
               exit >= 0.0f;
    if (!endInside && !sweptHit) return result;

    int normalAxis = entryAxis;
    float normalSign = entrySign;
    if (startInside || normalAxis < 0) {
        // When an element starts in the collision margin, retain the side
        // it came from. This prevents a thin wall from pulling it through
        // to the other side when the container moves/rotates quickly.
        const glm::vec3& reference = startInside ? startLocal : endLocal;
        float closestFace = half.x - std::abs(reference.x);
        normalAxis = 0;
        for (int axis = 1; axis < 3; ++axis) {
            const float face = half[axis] - std::abs(reference[axis]);
            if (face < closestFace) {
                closestFace = face;
                normalAxis = axis;
            }
        }
        normalSign = reference[normalAxis] >= 0.0f ? 1.0f : -1.0f;
        if (std::abs(reference[normalAxis]) <= kEpsilon) {
            normalSign = displacement[normalAxis] >= 0.0f ? -1.0f : 1.0f;
        }
    }

    glm::vec3 projectedLocal = endLocal;
    projectedLocal[normalAxis] = normalSign * half[normalAxis];
    glm::vec3 localNormal(0.0f);
    localNormal[normalAxis] = normalSign;
    result.hit = true;
    result.projectedPosition = end.position + end.rotation * projectedLocal;
    result.normal = end.rotation * localNormal;
    const glm::vec3 localSurface = glm::clamp(projectedLocal, -box.halfExtents,
                                              box.halfExtents);
    result.point = end.position + end.rotation * localSurface;
    const glm::vec3 previousPoint = start.position + start.rotation * localSurface;
    result.wallVelocity = (result.point - previousPoint) / substepTime;
    return result;
}

BoxContact CollideSphere(const glm::vec3& from, const glm::vec3& to,
                         const PreparedSphere& sphere, float particleRadius,
                         float substepTime) {
    BoxContact result;
    if (FarFromSweptSolid(from, to, sphere.sweptCenter, sphere.sweptRadius,
                          particleRadius)) return result;
    const BodyTransform& start = sphere.start;
    const BodyTransform& end = sphere.end;
    const glm::vec3 startRelative = from - start.position;
    const glm::vec3 endRelative = to - end.position;
    const glm::vec3 displacement = endRelative - startRelative;
    const float combinedRadius = sphere.radius + particleRadius;
    const float combinedRadiusSquared = combinedRadius * combinedRadius;
    const bool startInside = glm::dot(startRelative, startRelative) < combinedRadiusSquared;
    const bool endInside = glm::dot(endRelative, endRelative) < combinedRadiusSquared;

    // A particle tangent to or departing from a sphere is free to leave.
    // Treating t=0 at an exact surface touch as a new impact would pin
    // tangential motion to one point on a curved world.
    if (!endInside && glm::dot(startRelative, displacement) >= 0.0f) return result;

    glm::vec3 outward(0.0f);
    if (startInside) {
        const glm::vec3 reference = glm::dot(startRelative, startRelative) > kEpsilon * kEpsilon
            ? startRelative : endRelative;
        const float length = glm::length(reference);
        if (length <= kEpsilon) return result; // exact centre has no defined normal
        outward = reference / length;
    } else {
        const float a = glm::dot(displacement, displacement);
        const float b = 2.0f * glm::dot(startRelative, displacement);
        const float c = glm::dot(startRelative, startRelative) - combinedRadiusSquared;
        const float discriminant = b * b - 4.0f * a * c;
        if (a <= kEpsilon * kEpsilon || discriminant < 0.0f) {
            if (!endInside) return result;
            const float length = glm::length(endRelative);
            if (length <= kEpsilon) return result;
            outward = endRelative / length;
        } else {
            const float entry = (-b - std::sqrt(discriminant)) / (2.0f * a);
            if (entry < 0.0f || entry > 1.0f) {
                if (!endInside) return result;
                const float length = glm::length(endRelative);
                if (length <= kEpsilon) return result;
                outward = endRelative / length;
            } else {
                outward = glm::normalize(startRelative + entry * displacement);
            }
        }
    }

    result.hit = true;
    result.normal = outward;
    result.projectedPosition = end.position + outward * combinedRadius;
    result.point = end.position + outward * sphere.radius;
    const glm::vec3 previousPoint = start.position + outward * sphere.radius;
    result.wallVelocity = (result.point - previousPoint) / substepTime;
    return result;
}

BoxContact CollideTerrain(const glm::vec3& from, const glm::vec3& to,
                          const PreparedTerrain& terrain, float particleRadius,
                          float substepTime) {
    BoxContact result;
    if (FarFromSweptSolid(from, to, terrain.sweptCenter,
                          terrain.sweptRadius, particleRadius)) return result;

    const glm::vec3 startLocal = terrain.inverseStart * (from - terrain.start.position);
    const glm::vec3 endLocal = terrain.inverseEnd * (to - terrain.end.position);
    // At the exact centre of a radial solid there is no geometric outward
    // normal. Such a state cannot be resolved without inventing a direction.
    if (glm::dot(endLocal, endLocal) <= kEpsilon * kEpsilon) return result;
    const TerrainSample endSample = terrain.surface->Sample(endLocal);
    const bool endInside = endSample.signedDistance < particleRadius;

    glm::vec3 projectedLocal = endLocal;
    TerrainSample contactSample = endSample;
    if (endInside) {
        // Normal projection is iterated because a terrain signed-distance
        // estimate is local: slopes can change over the displacement.
        for (int i = 0; i < 4; ++i) {
            const float penetration = particleRadius - contactSample.signedDistance;
            if (penetration <= 1.0e-5f) break;
            projectedLocal += contactSample.outwardNormal * penetration;
            contactSample = terrain.surface->Sample(projectedLocal);
        }
    } else {
        const TerrainSample startSample = terrain.surface->Sample(startLocal);
        if (startSample.signedDistance < particleRadius) {
            // It was pushed out of the solid during this substep. Its final
            // position is already clear, but contact still supplies the
            // moving wall's point velocity to the momentum update below.
            result.hit = true;
            result.projectedPosition = to;
            result.normal = glm::normalize(terrain.end.rotation * endSample.outwardNormal);
            result.point = terrain.end.position +
                           terrain.end.rotation * endSample.surfacePoint;
            const glm::vec3 previousPoint = terrain.start.position +
                                            terrain.start.rotation * endSample.surfacePoint;
            result.wallVelocity = (result.point - previousPoint) / substepTime;
            return result;
        }
        // Endpoints can both be outside after a fast particle crosses a
        // curved solid. Sample the relative path only when its displacement
        // is substantial at this element's resolution; ordinary resting
        // contacts are handled by the endpoint projection above.
        const glm::vec3 relativeMotion = endLocal - startLocal;
        if (glm::length(relativeMotion) <= 0.5f * particleRadius) return result;
        float previousTime = 0.0f;
        bool crossed = false;
        float hitTime = 0.0f;
        for (int slice = 1; slice <= 8; ++slice) {
            const float time = static_cast<float>(slice) / 8.0f;
            const glm::vec3 point = glm::mix(startLocal, endLocal, time);
            if (glm::dot(point, point) <= kEpsilon * kEpsilon) continue;
            const TerrainSample sample = terrain.surface->Sample(point);
            if (sample.signedDistance < particleRadius) {
                float outsideTime = previousTime;
                float insideTime = time;
                for (int iteration = 0; iteration < 8; ++iteration) {
                    const float middle = 0.5f * (outsideTime + insideTime);
                    const glm::vec3 middlePoint =
                        glm::mix(startLocal, endLocal, middle);
                    if (terrain.surface->Sample(middlePoint).signedDistance < particleRadius)
                        insideTime = middle;
                    else
                        outsideTime = middle;
                }
                hitTime = outsideTime;
                crossed = true;
                break;
            }
            previousTime = time;
        }
        if (!crossed) return result;
        contactSample = terrain.surface->Sample(
            glm::mix(startLocal, endLocal, hitTime));
        projectedLocal = contactSample.surfacePoint +
                         contactSample.outwardNormal * particleRadius;
    }

    result.hit = true;
    result.projectedPosition = terrain.end.position + terrain.end.rotation * projectedLocal;
    result.normal = glm::normalize(terrain.end.rotation * contactSample.outwardNormal);
    result.point = terrain.end.position +
                   terrain.end.rotation * contactSample.surfacePoint;
    const glm::vec3 previousPoint = terrain.start.position +
                                    terrain.start.rotation * contactSample.surfacePoint;
    result.wallVelocity = (result.point - previousPoint) / substepTime;
    return result;
}

void ApplySolidCollisions(std::size_t particleIndex, const glm::vec3& from,
                          std::vector<glm::vec3>& positions,
                          const std::vector<PreparedBox>& boxes,
                          const std::vector<PreparedSphere>& spheres,
                          const std::vector<PreparedTerrain>& terrains,
                          float radius, float substepTime,
                          const std::vector<FluidParticle>& particles,
                          std::vector<FluidContactImpulse>* impulses,
                          std::vector<BoxContact>& lastContacts) {
    // Two passes resolve the common floor/wall corner without introducing a
    // container-specific intersection solver.
    for (int pass = 0; pass < 2; ++pass) {
        for (const PreparedBox& box : boxes) {
            BoxContact contact = CollideBox(from, positions[particleIndex], box,
                                            radius, substepTime);
            if (!contact.hit) continue;
            contact.owner = box.owner;
            const glm::vec3 correction = contact.projectedPosition - positions[particleIndex];
            positions[particleIndex] = contact.projectedPosition;
            lastContacts[particleIndex] = contact;
            if (impulses && box.owner.IsValid() &&
                glm::dot(correction, correction) > 1.0e-12f) {
                // Position projection is an impulse-like velocity change
                // over this substep. The opposite impulse is returned for
                // the owner; its rigid body applies inertia at the point.
                impulses->push_back(FluidContactImpulse{
                    box.owner, contact.point,
                    -(particles[particleIndex].mass / substepTime) * correction});
            }
        }
        for (const PreparedSphere& sphere : spheres) {
            BoxContact contact = CollideSphere(from, positions[particleIndex], sphere,
                                               radius, substepTime);
            if (!contact.hit) continue;
            contact.owner = sphere.owner;
            const glm::vec3 correction = contact.projectedPosition - positions[particleIndex];
            positions[particleIndex] = contact.projectedPosition;
            lastContacts[particleIndex] = contact;
            if (impulses && sphere.owner.IsValid() &&
                glm::dot(correction, correction) > 1.0e-12f) {
                impulses->push_back(FluidContactImpulse{
                    sphere.owner, contact.point,
                    -(particles[particleIndex].mass / substepTime) * correction});
            }
        }
        for (const PreparedTerrain& terrain : terrains) {
            BoxContact contact = CollideTerrain(from, positions[particleIndex], terrain,
                                                radius, substepTime);
            if (!contact.hit) continue;
            contact.owner = terrain.owner;
            const glm::vec3 correction = contact.projectedPosition - positions[particleIndex];
            positions[particleIndex] = contact.projectedPosition;
            lastContacts[particleIndex] = contact;
            if (impulses && terrain.owner.IsValid() &&
                glm::dot(correction, correction) > 1.0e-12f) {
                impulses->push_back(FluidContactImpulse{
                    terrain.owner, contact.point,
                    -(particles[particleIndex].mass / substepTime) * correction});
            }
        }
    }
}

}  // namespace

FluidWorld::FluidWorld() = default;

FluidWorld::FluidWorld(const FluidSettings& settings) : m_settings(settings) {}

void FluidWorld::AddParticle(const glm::vec3& position, const glm::vec3& velocity,
                             float mass) {
    if (mass <= 0.0f) return;
    m_particles.push_back(FluidParticle{position, position, velocity, mass});
    m_lastDensities.push_back(m_settings.restDensity);
}

void FluidWorld::Clear() {
    m_particles.clear();
    m_lastDensities.clear();
}

glm::vec3 FluidWorld::PresentedPosition(std::size_t index, float alpha) const {
    if (index >= m_particles.size()) return glm::vec3(0.0f);
    const FluidParticle& particle = m_particles[index];
    return glm::mix(particle.previousPosition, particle.position,
                    std::clamp(alpha, 0.0f, 1.0f));
}

void FluidWorld::Step(float fixedDeltaTime, const GravityField& gravity,
                      const std::vector<FluidBoxCollider>& boxes,
                      std::vector<FluidContactImpulse>* contactImpulses) {
    Step(fixedDeltaTime, gravity, boxes, {}, {}, contactImpulses);
}

void FluidWorld::Step(float fixedDeltaTime, const GravityField& gravity,
                      const std::vector<FluidBoxCollider>& boxes,
                      const std::vector<FluidSphereCollider>& spheres,
                      std::vector<FluidContactImpulse>* contactImpulses) {
    Step(fixedDeltaTime, gravity, boxes, spheres, {}, contactImpulses);
}

void FluidWorld::Step(float fixedDeltaTime, const GravityField& gravity,
                      const std::vector<FluidBoxCollider>& boxes,
                      const std::vector<FluidSphereCollider>& spheres,
                      const std::vector<FluidTerrainCollider>& terrains,
                      std::vector<FluidContactImpulse>* contactImpulses) {
    if (contactImpulses) contactImpulses->clear();
    if (fixedDeltaTime <= 0.0f || m_particles.empty()) return;
    const int substeps = std::max(m_settings.substeps, 1);
    const int iterations = std::max(m_settings.densityIterations, 0);
    const float substepTime = fixedDeltaTime / static_cast<float>(substeps);
    const float radius = m_settings.smoothingRadius;
    const float restDensity = m_settings.restDensity;
    const std::size_t count = m_particles.size();
    for (FluidParticle& particle : m_particles) {
        particle.previousPosition = particle.position;
    }

    std::vector<glm::vec3> positions(count);
    std::vector<glm::vec3> starts(count);
    std::vector<glm::vec3> unconstrainedPositions(count);
    std::vector<glm::vec3> corrections(count);
    std::vector<float> lambdas(count);
    std::vector<float> densities(count);
    std::vector<BoxContact> lastContacts(count);
    std::vector<PreparedBox> preparedBoxes;
    std::vector<PreparedSphere> preparedSpheres;
    std::vector<PreparedTerrain> preparedTerrains;
    preparedBoxes.reserve(boxes.size());
    preparedSpheres.reserve(spheres.size());
    preparedTerrains.reserve(terrains.size());

    for (int substep = 0; substep < substeps; ++substep) {
        const float alpha0 = static_cast<float>(substep) / static_cast<float>(substeps);
        const float alpha1 = static_cast<float>(substep + 1) / static_cast<float>(substeps);
        preparedBoxes.clear();
        preparedSpheres.clear();
        preparedTerrains.clear();
        for (const FluidBoxCollider& box : boxes) {
            preparedBoxes.push_back(Prepare(box, alpha0, alpha1));
        }
        for (const FluidSphereCollider& sphere : spheres) {
            preparedSpheres.push_back(Prepare(sphere, alpha0, alpha1));
        }
        for (const FluidTerrainCollider& terrain : terrains) {
            if (terrain.surface) preparedTerrains.push_back(Prepare(terrain, alpha0, alpha1));
        }
        std::fill(lastContacts.begin(), lastContacts.end(), BoxContact{});
        for (std::size_t i = 0; i < count; ++i) {
            FluidParticle& particle = m_particles[i];
            starts[i] = particle.position;
            particle.velocity += gravity.Sample(particle.position) * substepTime;
            positions[i] = starts[i] + particle.velocity * substepTime;
            unconstrainedPositions[i] = positions[i];
            ApplySolidCollisions(i, starts[i], positions, preparedBoxes, preparedSpheres,
                                 preparedTerrains,
                                 m_settings.particleRadius, substepTime, m_particles,
                                 contactImpulses, lastContacts);
        }

        for (int iteration = 0; iteration < iterations; ++iteration) {
            const NeighborLists neighbors = BuildNeighbors(positions, radius);
            for (std::size_t i = 0; i < count; ++i) {
                densities[i] = DensityAt(i, positions, m_particles, neighbors, radius);
                const float constraint = std::max(densities[i] / restDensity - 1.0f, 0.0f);
                if (constraint <= 0.0f) {
                    lambdas[i] = 0.0f;
                    continue;
                }
                glm::vec3 ownGradient(0.0f);
                float gradientSum = 0.0f;
                for (std::size_t j : neighbors[i]) {
                    const glm::vec3 gradient = (m_particles[j].mass / restDensity) *
                        SpikyGradient(positions[i] - positions[j], radius);
                    ownGradient += gradient;
                    gradientSum += glm::dot(gradient, gradient);
                }
                gradientSum += glm::dot(ownGradient, ownGradient);
                lambdas[i] = -constraint / (gradientSum + 1.0e-5f);
            }
            for (std::size_t i = 0; i < count; ++i) {
                glm::vec3 correction(0.0f);
                for (std::size_t j : neighbors[i]) {
                    correction += (lambdas[i] + lambdas[j]) *
                        (m_particles[j].mass / restDensity) *
                        SpikyGradient(positions[i] - positions[j], radius);
                }
                const float length = glm::length(correction);
                if (length > m_settings.maxDensityCorrection && length > kEpsilon) {
                    correction *= m_settings.maxDensityCorrection / length;
                }
                corrections[i] = correction;
            }
            for (std::size_t i = 0; i < count; ++i) positions[i] += corrections[i];
            for (std::size_t i = 0; i < count; ++i) {
                ApplySolidCollisions(i, starts[i], positions, preparedBoxes, preparedSpheres,
                                     preparedTerrains,
                                     m_settings.particleRadius, substepTime, m_particles,
                                     contactImpulses, lastContacts);
            }
        }

        for (std::size_t i = 0; i < count; ++i) {
            FluidParticle& particle = m_particles[i];
            // Preserve free-flight velocity exactly. Reconstructing it from
            // two nearby float positions introduces transverse drift even
            // when no pressure/contact correction occurred.
            particle.velocity += (positions[i] - unconstrainedPositions[i]) / substepTime;
            if (lastContacts[i].hit) {
                const float normalSpeed = glm::dot(particle.velocity -
                    lastContacts[i].wallVelocity, lastContacts[i].normal);
                if (normalSpeed < 0.0f) {
                    const glm::vec3 deltaVelocity = -normalSpeed * lastContacts[i].normal;
                    particle.velocity += deltaVelocity;
                    if (contactImpulses && lastContacts[i].owner.IsValid()) {
                        contactImpulses->push_back(FluidContactImpulse{
                            lastContacts[i].owner, lastContacts[i].point,
                            -particle.mass * deltaVelocity});
                    }
                }
            }
            particle.position = positions[i];
        }

        if (m_settings.velocitySmoothing > 0.0f) {
            const NeighborLists neighbors = BuildNeighbors(positions, radius);
            for (std::size_t i = 0; i < count; ++i) {
                densities[i] = DensityAt(i, positions, m_particles, neighbors, radius);
            }
            for (std::size_t i = 0; i < count; ++i) {
                corrections[i] = glm::vec3(0.0f);
                for (std::size_t j : neighbors[i]) {
                    const float neighborDensity = std::max(densities[j], restDensity * 0.3f);
                    const glm::vec3 separation = positions[i] - positions[j];
                    corrections[i] += (m_particles[j].mass / neighborDensity) *
                        (m_particles[j].velocity - m_particles[i].velocity) *
                        Poly6(glm::dot(separation, separation), radius);
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                m_particles[i].velocity += m_settings.velocitySmoothing * corrections[i];
            }
        }
    }

    for (std::size_t i = 0; i < count; ++i) positions[i] = m_particles[i].position;
    const NeighborLists finalNeighbors = BuildNeighbors(positions, radius);
    for (std::size_t i = 0; i < count; ++i) {
        m_lastDensities[i] = DensityAt(i, positions, m_particles, finalNeighbors, radius);
    }
}

FluidDiagnostics FluidWorld::GetDiagnostics() const {
    FluidDiagnostics diagnostics;
    diagnostics.particleCount = m_particles.size();
    for (std::size_t i = 0; i < m_particles.size(); ++i) {
        const FluidParticle& particle = m_particles[i];
        diagnostics.totalMass += particle.mass;
        diagnostics.centerOfMass += particle.mass * particle.position;
        diagnostics.totalMomentum += particle.mass * particle.velocity;
        diagnostics.kineticEnergy += 0.5f * particle.mass *
                                     glm::dot(particle.velocity, particle.velocity);
        diagnostics.meanDensity += m_lastDensities[i];
        const float positiveError = std::max(m_lastDensities[i] / m_settings.restDensity - 1.0f,
                                             0.0f);
        diagnostics.meanPositiveDensityError += positiveError;
        diagnostics.maxPositiveDensityError =
            std::max(diagnostics.maxPositiveDensityError, positiveError);
    }
    if (diagnostics.totalMass > 0.0f) {
        diagnostics.centerOfMass /= diagnostics.totalMass;
    }
    if (diagnostics.particleCount > 0) {
        diagnostics.meanDensity /= static_cast<float>(diagnostics.particleCount);
        diagnostics.meanPositiveDensityError /= static_cast<float>(diagnostics.particleCount);
    }
    return diagnostics;
}
