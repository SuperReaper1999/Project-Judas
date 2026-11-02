#include "FluidWorld.h"
#include "FluidDisplacementTracking.h"
#include "ContactSolver.h"
#include "RigidBody.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#include <glm/gtc/quaternion.hpp>

#include "GravityField.h"
#include "RadialTerrain.h"

namespace {

using FluidClock = std::chrono::steady_clock;
double ElapsedFluidMs(FluidClock::time_point t) { return std::chrono::duration<double,std::milli>(FluidClock::now()-t).count(); }

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

// Flat reusable adjacency retains the original cell/particle visitation order.
// Iteration consumes one completed build; the next build occurs only after its
// consumers finish. Capacity growth is real work, not hidden preparation.
struct NeighborLists {
    std::vector<std::size_t> offsets, indices;
    struct Range {
        std::vector<std::size_t>::const_iterator first,last;
        auto begin() const { return first; } auto end() const { return last; }
    };
    Range operator[](std::size_t i) const { return {indices.begin()+offsets[i],indices.begin()+offsets[i+1]}; }
};

const NeighborLists& BuildNeighbors(const std::vector<glm::vec3>& positions, float radius, NeighborLists& neighbors) {
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> grid;
    grid.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        grid[CellFor(positions[i], radius)].push_back(i);
    }

    neighbors.offsets.resize(positions.size()+1);neighbors.indices.clear();
    const float radiusSquared = radius * radius;
    for (std::size_t i = 0; i < positions.size(); ++i) {
        neighbors.offsets[i]=neighbors.indices.size();
        const Cell center = CellFor(positions[i], radius);
        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    const auto found = grid.find(Cell{center.x + x, center.y + y, center.z + z});
                    if (found == grid.end()) continue;
                    for (std::size_t j : found->second) {
                        const glm::vec3 separation = positions[i] - positions[j];
                        if (j != i && glm::dot(separation, separation) < radiusSquared) {
                            neighbors.indices.push_back(j);
                        }
                    }
                }
            }
        }
    }
    neighbors.offsets[positions.size()]=neighbors.indices.size();
    return neighbors;
}

struct FluidKernel {
    float radius,radiusSquared,poly6,spiky;
    explicit FluidKernel(float r):radius(r),radiusSquared(r*r){
        const float radius3=radiusSquared*r, radius9=radius3*radius3*radius3;
        const float radius6=radiusSquared*radiusSquared*radiusSquared;
        poly6=315.0f/(64.0f*kPi*radius9);spiky=-45.0f/(kPi*radius6);
    }
};
float Poly6(float squaredDistance, const FluidKernel& kernel) {
    if (squaredDistance >= kernel.radiusSquared) return 0.0f;
    const float remaining = kernel.radiusSquared - squaredDistance;
    return kernel.poly6 * remaining * remaining * remaining;
}
glm::vec3 SpikyGradient(const glm::vec3& displacement, const FluidKernel& kernel) {
    const float distance = glm::length(displacement);
    if (distance <= kEpsilon || distance >= kernel.radius) return glm::vec3(0.0f);
    const float remaining = kernel.radius - distance;
    return kernel.spiky * remaining * remaining * (displacement / distance);
}

float DensityAt(std::size_t i, const std::vector<glm::vec3>& positions,
                const std::vector<FluidParticle>& particles,
                const NeighborLists& neighbors, const FluidKernel& kernel) {
    float density = particles[i].mass * Poly6(0.0f, kernel);
    for (std::size_t j : neighbors[i]) {
        const glm::vec3 separation = positions[i] - positions[j];
        density += particles[j].mass * Poly6(glm::dot(separation, separation), kernel);
    }
    return density;
}

BodyTransform ResolvedPose(const std::vector<PhysicsWorld::BodyMotionSegment>& motion,
                          double duration, float alpha) {
    const double time=static_cast<double>(alpha)*duration;
    // Prefer the later anchor at a shared endpoint. This also keeps an
    // explicitly discontinuous reset from becoming an interpolated sweep.
    for (auto it=motion.rbegin();it!=motion.rend();++it)
        if (time>=it->begin && time<=it->end)
            return time==it->end ? it->endPose : PhysicsWorld::EvaluateBodyMotionSegment(*it,time);
    return time<motion.front().begin ? motion.front().start : motion.back().endPose;
}

BodyTransform InterpolatePose(const FluidBoxCollider& box, float alpha) {
    if (!box.resolvedMotion.empty()) {
        BodyTransform pose=ResolvedPose(box.resolvedMotion,box.motionDuration,alpha);
        pose.position+=pose.rotation*box.localCenter;
        pose.rotation=glm::normalize(pose.rotation*box.localRotation);
        return pose;
    }
    BodyTransform pose;
    pose.position = glm::mix(box.previousPose.position, box.currentPose.position, alpha);
    pose.rotation = glm::normalize(glm::slerp(box.previousPose.rotation,
                                               box.currentPose.rotation, alpha));
    return pose;
}

BodyTransform InterpolatePose(const FluidSphereCollider& sphere, float alpha) {
    if (!sphere.resolvedMotion.empty()) return ResolvedPose(sphere.resolvedMotion,sphere.motionDuration,alpha);
    BodyTransform pose;
    pose.position = glm::mix(sphere.previousPose.position, sphere.currentPose.position, alpha);
    pose.rotation = glm::normalize(glm::slerp(sphere.previousPose.rotation,
                                               sphere.currentPose.rotation, alpha));
    return pose;
}

BodyTransform InterpolatePose(const FluidTerrainCollider& terrain, float alpha) {
    if (!terrain.resolvedMotion.empty()) return ResolvedPose(terrain.resolvedMotion,terrain.motionDuration,alpha);
    BodyTransform pose;
    pose.position = glm::mix(terrain.previousPose.position,
                             terrain.currentPose.position, alpha);
    pose.rotation = glm::normalize(glm::slerp(terrain.previousPose.rotation,
                                               terrain.currentPose.rotation, alpha));
    return pose;
}

struct PreparedBox;
struct PreparedSphere;
struct PreparedTerrain;
struct BoxContact {
    bool hit = false;
    BodyHandle owner;
    glm::vec3 projectedPosition{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec3 point{0.0f};
    glm::vec3 wallVelocity{0.0f};
    const PreparedBox* box=nullptr; // exact child; prepared substep storage remains stable
    const PreparedSphere* sphere=nullptr;
    const PreparedTerrain* terrain=nullptr;
    bool hardEncounter=false; // immutable initial drift hit, with its original impact response
    bool displacementWitness=false; // belongs to selected geometry, not discarded soft history
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
    glm::vec3 boundMin{0},boundMax{0},skinBoundScale{1}; // outward-rounded endpoint OBB bounds
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

float ResolvedSweepReach(const std::vector<PhysicsWorld::BodyMotionSegment>& motion,
                        double duration,float alpha0,float alpha1,
                        const glm::vec3& center,float endpointReach) {
    float reach=endpointReach;
    const double begin=static_cast<double>(alpha0)*duration,end=static_cast<double>(alpha1)*duration;
    for (const auto& segment:motion) {
        const double lo=std::max(begin,segment.begin),hi=std::min(end,segment.end);
        if (lo>hi) continue;
        // A segment's COM translation is linear; its extrema occur at the
        // interval endpoints. Include its stored repaired endpoint too. This
        // bound preserves out-and-back candidate coverage without increasing
        // fluid substeps or claiming exact collision between temporal samples.
        reach=std::max(reach,glm::distance(center,
            PhysicsWorld::EvaluateBodyMotionSegment(segment,lo).position));
        reach=std::max(reach,glm::distance(center,
            PhysicsWorld::EvaluateBodyMotionSegment(segment,hi).position));
        if (hi==segment.end) reach=std::max(reach,glm::distance(center,segment.endPose.position));
    }
    return reach;
}

PreparedBox Prepare(const FluidBoxCollider& box, float alpha0, float alpha1) {
    PreparedBox prepared;
    prepared.owner = box.owner;
    prepared.start = InterpolatePose(box, alpha0);
    prepared.end = InterpolatePose(box, alpha1);
    prepared.inverseStart = glm::inverse(prepared.start.rotation);
    prepared.inverseEnd = glm::inverse(prepared.end.rotation);
    prepared.halfExtents = box.halfExtents;
    // Cache endpoint bounds once per substep. Bound the represented inverse
    // transform; round outward and retain a conservative floating-point band.
    const glm::dmat3 basis=glm::inverse(glm::dmat3(glm::mat3_cast(prepared.inverseEnd)));
    glm::dvec3 half(0),scale(0);
    for(int i=0;i<3;++i){scale[i]=std::abs(basis[0][i])+std::abs(basis[1][i])+std::abs(basis[2][i]);
        half[i]=glm::dot(glm::dvec3(std::abs(basis[0][i]),std::abs(basis[1][i]),std::abs(basis[2][i])),glm::dvec3(box.halfExtents));}
    const double reach=std::max(half.x,std::max(half.y,half.z));
    const glm::dvec3 rounding=64.0*std::numeric_limits<float>::epsilon()*(2.0*glm::abs(glm::dvec3(prepared.end.position))+glm::dvec3(3*reach+1));
    for(int i=0;i<3;++i){
        prepared.boundMin[i]=std::nextafter(float(double(prepared.end.position[i])-half[i]-rounding[i]),-std::numeric_limits<float>::infinity());
        prepared.boundMax[i]=std::nextafter(float(double(prepared.end.position[i])+half[i]+rounding[i]),std::numeric_limits<float>::infinity());
        prepared.skinBoundScale[i]=std::nextafter(float(scale[i]),std::numeric_limits<float>::infinity());
    }
    prepared.sweptCenter = 0.5f * (prepared.start.position + prepared.end.position);
    float endpointReach=0.5f*glm::distance(prepared.start.position,prepared.end.position);
    if (!box.resolvedMotion.empty()) {
        const auto parentStart=ResolvedPose(box.resolvedMotion,box.motionDuration,alpha0);
        const auto parentEnd=ResolvedPose(box.resolvedMotion,box.motionDuration,alpha1);
        prepared.sweptCenter=0.5f*(parentStart.position+parentEnd.position);
        endpointReach=0.5f*glm::distance(parentStart.position,parentEnd.position);
    }
    prepared.sweptRadius=glm::length(box.halfExtents)+
        (box.resolvedMotion.empty() ? endpointReach : glm::length(box.localCenter)+
            ResolvedSweepReach(box.resolvedMotion,box.motionDuration,alpha0,alpha1,
                               prepared.sweptCenter,endpointReach));
    return prepared;
}

PreparedSphere Prepare(const FluidSphereCollider& sphere, float alpha0, float alpha1) {
    PreparedSphere prepared;
    prepared.owner = sphere.owner;
    prepared.start = InterpolatePose(sphere, alpha0);
    prepared.end = InterpolatePose(sphere, alpha1);
    prepared.radius = sphere.radius;
    prepared.sweptCenter = 0.5f * (prepared.start.position + prepared.end.position);
    prepared.sweptRadius=sphere.radius+ResolvedSweepReach(sphere.resolvedMotion,
        sphere.motionDuration,alpha0,alpha1,prepared.sweptCenter,
        0.5f*glm::distance(prepared.start.position,prepared.end.position));
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
    prepared.sweptRadius=terrain.surface->BoundRadius()+ResolvedSweepReach(terrain.resolvedMotion,
        terrain.motionDuration,alpha0,alpha1,prepared.sweptCenter,
        0.5f*glm::distance(prepared.start.position,prepared.end.position));
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

// Conservative rejection only. The float quaternion/vector predicate has
// fewer than 32 elementary operations per axis; 64 epsilon covers its rounding
// and bound construction. All close/ill-conditioned cases use original tests.
bool BoxBoundMiss(const glm::vec3& from,const glm::vec3& to,const PreparedBox& box,float radius=0) {
    const glm::vec3 margin=radius*box.skinBoundScale;
    return glm::any(glm::greaterThan(glm::min(from,to),box.boundMax+margin)) ||
        glm::any(glm::lessThan(glm::max(from,to),box.boundMin-margin));
}

// A point swept relative to a moving OBB, expanded by the fluid element's
// collision radius. This is geometric contact against the box, regardless
// of whether it is a cup wall, floor, or another ordinary body.
BoxContact CollideBox(const glm::vec3& from, const glm::vec3& to,
                      const PreparedBox& box, float radius, float substepTime) {
    BoxContact result;
    if(box.start.position==box.end.position && (box.start.rotation==box.end.rotation || box.start.rotation==-box.end.rotation) && BoxBoundMiss(from,to,box,radius))return result;
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
    // Exact same relative endpoint segment as the slab test below: if one
    // axis stays wholly outside, no slab contact can exist. No skin/geometry
    // tolerance or collision equation changes.
    if (glm::any(glm::greaterThan(glm::min(startLocal,endLocal),half)) ||
        glm::any(glm::lessThan(glm::max(startLocal,endLocal),-half))) return result;
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
        // An inward trajectory beginning exactly on the expanded face has
        // entry time zero. Retain that face even when it crosses the entire
        // thin wall before the endpoint; outward/tangent paths still depart.
        if (nearTime > entry || (entryAxis < 0 && nearTime == entry)) {
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
    result.box=&box;
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
        // Every represented nonzero radial direction defines a normal, even
        // below the ordinary sweep epsilon. Normalize in double so tiny float
        // components do not lose their squared norm to underflow.
        const glm::dvec3 startDirection(startRelative), endDirection(endRelative);
        const glm::dvec3 reference = glm::dot(startDirection, startDirection) > 0
            ? startDirection : glm::dot(endDirection, endDirection) > 0
            ? endDirection : glm::dvec3(end.rotation * glm::vec3(1, 0, 0));
        // A motionless exact centre has no unique geometric exit. Initial
        // overlap uses the sphere body's local +X as its deterministic policy;
        // this rotates with the body and only selects geometric depenetration.
        outward = glm::vec3(reference / glm::length(reference));
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
    result.sphere = &sphere;
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
            // A hard point exclusion cannot accept the ordinary skin's
            // approximate projection tolerance as actual penetration.
            if (penetration <= (particleRadius == 0.0f ? 0.0f : 1.0e-5f)) break;
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
            result.terrain = &terrain;
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
    result.terrain = &terrain;
    result.projectedPosition = terrain.end.position + terrain.end.rotation * projectedLocal;
    result.normal = glm::normalize(terrain.end.rotation * contactSample.outwardNormal);
    result.point = terrain.end.position +
                   terrain.end.rotation * contactSample.surfacePoint;
    const glm::vec3 previousPoint = terrain.start.position +
                                    terrain.start.rotation * contactSample.surfacePoint;
    result.wallVelocity = (result.point - previousPoint) / substepTime;
    return result;
}

// Physical exclusion is radius zero, with strict represented signed tests.
// No error band reclassifies an interior point as exterior. An unsupported
// hard union must fail explicitly rather than retain a penetrating point.
double BoxSeparation(const glm::vec3& point,const PreparedBox& box,float radius) {
    const glm::vec3 local=box.inverseEnd*(point-box.end.position);
    const glm::vec3 gap=glm::abs(local)-(box.halfExtents+glm::vec3(radius));
    return std::max({double(gap.x),double(gap.y),double(gap.z)});
}
// A coarse endpoint projection must not flip through a stationary physical
// wall just because that wall's opposite face is the nearest exterior point.
// Margins are not barriers: only the unexpanded box interior is.
bool CrossesNonmovingBoxInterior(const glm::vec3& from,const glm::vec3& to,const PreparedBox& box) {
    if(box.start.position!=box.end.position ||
       (box.start.rotation!=box.end.rotation && box.start.rotation!=-box.end.rotation))return false;
    if(BoxBoundMiss(from,to,box))return false;
    const glm::dvec3 start(box.inverseEnd*(from-box.end.position));
    const glm::dvec3 finish(box.inverseEnd*(to-box.end.position));
    bool alreadyInside=true;
    for(int axis=0;axis<3;++axis)alreadyInside=alreadyInside && std::abs(start[axis])<double(box.halfExtents[axis]);
    if(alreadyInside)return false; // no invented side for an already-invalid start
    double entry=0,exit=1;
    for(int axis=0;axis<3;++axis) {
        const double half=double(box.halfExtents[axis]);
        if(!(half>0))return false;
        const double delta=finish[axis]-start[axis];
        if(delta==0) {if(std::abs(start[axis])>=half)return false;continue;}
        const double a=(-half-start[axis])/delta,b=(half-start[axis])/delta;
        entry=std::max(entry,std::min(a,b));exit=std::min(exit,std::max(a,b));
        if(!(entry<exit))return false;
    }
    return exit>0 && entry<1 && entry<exit;
}
bool CrossesAnyNonmovingBox(const glm::vec3& from,const glm::vec3& to,
                           const std::vector<PreparedBox>& boxes) {
    for(const auto& box:boxes)if(CrossesNonmovingBoxInterior(from,to,box))return true;
    return false;
}
bool InsideRealSolid(const glm::vec3& point,const std::vector<PreparedBox>& boxes,
                         const std::vector<PreparedSphere>& spheres,
                         const std::vector<PreparedTerrain>& terrains) {
    for(const auto& box:boxes)if(!BoxBoundMiss(point,point,box) && BoxSeparation(point,box,0)<0)return true;
    for(const auto& sphere:spheres) {
        const glm::dvec3 delta=glm::dvec3(point)-glm::dvec3(sphere.end.position);
        if(glm::dot(delta,delta)<double(sphere.radius)*sphere.radius)return true;
    }
    for(const auto& terrain:terrains) {
        const auto local=terrain.inverseEnd*(point-terrain.end.position);
        if(glm::dot(local,local)<=kEpsilon*kEpsilon)return true;
        if(terrain.surface->Sample(local).signedDistance<0)return true;
    }
    return false;
}
bool OutsideRealSolids(const glm::vec3& point,const std::vector<PreparedBox>& boxes,
                           const std::vector<PreparedSphere>& spheres,
                           const std::vector<PreparedTerrain>& terrains) {
    return !InsideRealSolid(point,boxes,spheres,terrains);
}
bool RepairRealBoxOverlap(const glm::vec3& from,const glm::vec3& reference,glm::vec3& point,
                            const std::vector<BoxContact>& contacts,
                            const std::vector<PreparedBox>& boxes,
                            const std::vector<PreparedSphere>& spheres,
                            const std::vector<PreparedTerrain>& terrains,
                            std::size_t& candidateCount) {
    struct Plane {glm::dvec3 n;double offset;};
    std::vector<const PreparedBox*> encountered;
    for(const auto& contact:contacts)if(contact.box &&
        std::find(encountered.begin(),encountered.end(),contact.box)==encountered.end())encountered.push_back(contact.box);
    std::vector<Plane> planes;planes.reserve(6*encountered.size());
    for(const auto* box:encountered)for(int axis=0;axis<3;++axis)for(int sign:{-1,1}) {
        glm::vec3 localNormal(0);localNormal[axis]=float(sign);
        const auto n=glm::normalize(glm::dvec3(box->end.rotation*localNormal));
        planes.push_back({n,glm::dot(n,glm::dvec3(box->end.position))+box->halfExtents[axis]});
    }
    const glm::dvec3 origin(reference);
    double best=std::numeric_limits<double>::infinity();glm::vec3 selected(0);
    const auto consider=[&](const glm::dvec3& candidate) {
        if(!std::isfinite(candidate.x)||!std::isfinite(candidate.y)||!std::isfinite(candidate.z))return;
        const glm::vec3 nearest(candidate);
        if(!std::isfinite(nearest.x)||!std::isfinite(nearest.y)||!std::isfinite(nearest.z))return;
        // Casting an exact plane intersection can round inward. Search its
        // immediate representable neighbours, retaining strict union/path
        // validation; never enlarge the physical-solid acceptance region.
        glm::vec3 choices[3]={nearest,nearest,nearest};
        for(int axis=0;axis<3;++axis) {
            choices[0][axis]=std::nextafter(nearest[axis],-std::numeric_limits<float>::infinity());
            choices[2][axis]=std::nextafter(nearest[axis],std::numeric_limits<float>::infinity());
        }
        for(int x=0;x<3;++x)for(int y=0;y<3;++y)for(int z=0;z<3;++z) {
            ++candidateCount;
            const glm::vec3 represented(choices[x].x,choices[y].y,choices[z].z);
            if(!OutsideRealSolids(represented,boxes,spheres,terrains))continue;
            bool crosses=false;
            for(const auto& box:boxes)if(CrossesNonmovingBoxInterior(from,represented,box)){crosses=true;break;}
            if(crosses)continue;
            const auto displacement=glm::dvec3(represented)-origin;
            const double distance=glm::dot(displacement,displacement);
            if(distance<best){best=distance;selected=represented;}
        }
    };
    // Closest points in a 3-D polyhedral exterior have at most three
    // linearly independent active planes. Gram rank tolerance is dimensionless
    // double roundoff, never a scene/body/contact-gap threshold.
    constexpr double rankTolerance=64*std::numeric_limits<double>::epsilon();
    for(std::size_t a=0;a<planes.size();++a) {
        const auto& pa=planes[a];const double ra=pa.offset-glm::dot(pa.n,origin);
        consider(origin+pa.n*ra);
        for(std::size_t b=a+1;b<planes.size();++b) {
            const auto& pb=planes[b];const double rb=pb.offset-glm::dot(pb.n,origin);
            const double ab=glm::dot(pa.n,pb.n),determinant=1-ab*ab;
            if(determinant<=rankTolerance)continue;
            consider(origin+pa.n*((ra-ab*rb)/determinant)+pb.n*((rb-ab*ra)/determinant));
            for(std::size_t c=b+1;c<planes.size();++c) {
                const auto& pc=planes[c];const double rc=pc.offset-glm::dot(pc.n,origin);
                const double ac=glm::dot(pa.n,pc.n),bc=glm::dot(pb.n,pc.n);
                const glm::dmat3 gram(glm::dvec3(1,ab,ac),glm::dvec3(ab,1,bc),glm::dvec3(ac,bc,1));
                if(glm::determinant(gram)<=rankTolerance)continue;
                const glm::dvec3 coefficients=glm::inverse(gram)*glm::dvec3(ra,rb,rc);
                consider(origin+pa.n*coefficients.x+pb.n*coefficients.y+pc.n*coefficients.z);
            }
        }
    }
    if(!std::isfinite(best))return false;
    point=selected;return true;
}
struct RealClipLimit {
    double fraction=1;
    int kind=-1; // 0 box, 1 sphere, 2 terrain; index in the prepared substep arrays
    std::size_t index=0;
};
double RealBoxEntry(const glm::vec3& from,const glm::vec3& to,const PreparedBox& box) {
    if(BoxBoundMiss(from,to,box))return 1;
    const glm::dvec3 a(box.inverseEnd*(from-box.end.position));
    const glm::dvec3 b(box.inverseEnd*(to-box.end.position));
    double enter=-std::numeric_limits<double>::infinity(),leave=std::numeric_limits<double>::infinity();
    for(int axis=0;axis<3;++axis) {
        const double d=b[axis]-a[axis],h=box.halfExtents[axis];
        if(d==0) {if(std::abs(a[axis])>=h)return 1;continue;}
        const double lo=(-h-a[axis])/d,hi=(h-a[axis])/d;
        enter=std::max(enter,std::min(lo,hi));leave=std::min(leave,std::max(lo,hi));
        if(!(enter<leave))return 1;
    }
    return leave>0 && enter<1 ? std::max(0.0,enter) : 1;
}
double RealSphereEntry(const glm::vec3& from,const glm::vec3& to,const PreparedSphere& sphere) {
    const glm::dvec3 a=glm::dvec3(from)-glm::dvec3(sphere.end.position),d=glm::dvec3(to)-glm::dvec3(from);
    const double aa=glm::dot(d,d),b=glm::dot(a,d),c=glm::dot(a,a)-double(sphere.radius)*sphere.radius;
    if(!(aa>0))return 1;
    // A roundoff-ambiguous interior start can leave the sphere monotonically.
    // Further inward motion receives no soft clearance permission.
    if(c<0)return b>=0 ? 1 : 0;
    const double discriminant=b*b-aa*c;
    if(!(discriminant>0))return 1; // tangent has no interior interval
    const double root=std::sqrt(discriminant),enter=(-b-root)/aa,leave=(-b+root)/aa;
    return leave>0 && enter<1 ? std::max(0.0,enter) : 1;
}
double RealTerrainEntry(const glm::vec3& from,const glm::vec3& to,const PreparedTerrain& terrain) {
    const glm::vec3 a=terrain.inverseEnd*(from-terrain.end.position),b=terrain.inverseEnd*(to-terrain.end.position);
    const auto inside=[&](const glm::vec3& point) {
        return glm::dot(point,point)<=kEpsilon*kEpsilon || terrain.surface->Sample(point).signedDistance<0;
    };
    // Retain the existing radial-terrain traversal resolution. Unlike boxes
    // and spheres, this sampled path is not an exact continuous intersection.
    float previous=0;
    for(int slice=1;slice<=8;++slice) {
        const float t=float(slice)/8;
        if(inside(glm::mix(a,b,t))) {
            float lo=previous,hi=t;
            for(int iteration=0;iteration<8;++iteration) {
                const float middle=.5f*(lo+hi);
                if(inside(glm::mix(a,b,middle)))hi=middle;else lo=middle;
            }
            return lo;
        }
        previous=t;
    }
    return 1;
}
RealClipLimit ClipRealUnion(const glm::vec3& from,const glm::vec3& to,
                           const std::vector<PreparedBox>& boxes,
                           const std::vector<PreparedSphere>& spheres,
                           const std::vector<PreparedTerrain>& terrains) {
    RealClipLimit result;
    const auto consider=[&](double fraction,int kind,std::size_t index) {
        if(fraction<result.fraction)result={fraction,kind,index};
    };
    for(std::size_t i=0;i<boxes.size();++i)consider(RealBoxEntry(from,to,boxes[i]),0,i);
    for(std::size_t i=0;i<spheres.size();++i)consider(RealSphereEntry(from,to,spheres[i]),1,i);
    for(std::size_t i=0;i<terrains.size();++i)consider(RealTerrainEntry(from,to,terrains[i]),2,i);
    return result;
}

// Final soft-contact classification is independent of the projection history.
// A stale witness cannot harden a clearance which geometry has already yielded.
struct FinalSoftContact {
    BoxContact physical;
    float gap=0;
    bool active=false;
    bool preferredFeasible=false;
};

FinalSoftContact FinalizeSoftContact(const BoxContact& row,const glm::vec3& position,
                                     float radius,float substepTime) {
    FinalSoftContact result;
    result.physical=row;
    auto& contact=result.physical;
    // This band only recognises represented preferred stand-off. It never
    // reclassifies an interior physical point as exterior.
    const float tolerance=64*std::numeric_limits<float>::epsilon()*
        (1+glm::length(position)+glm::length(row.point)+radius);
    if(row.box) {
        const auto& box=*row.box;
        const glm::vec3 local=box.inverseEnd*(position-box.end.position);
        const glm::vec3 oldNormal=box.inverseEnd*row.normal;
        int axis=0;
        for(int i=1;i<3;++i)if(std::abs(oldNormal[i])>std::abs(oldNormal[axis]))axis=i;
        const float sign=oldNormal[axis]>=0 ? 1.f : -1.f;
        const float faceGap=sign*local[axis]-box.halfExtents[axis];
        bool withinFeature=true;
        for(int i=0;i<3;++i)
            withinFeature=withinFeature && std::abs(local[i])<=box.halfExtents[i]+radius+tolerance;
        result.active=withinFeature && faceGap<=radius+tolerance;
        result.preferredFeasible=result.active && std::abs(faceGap-radius)<=tolerance;
        const glm::vec3 closest=glm::clamp(local,-box.halfExtents,box.halfExtents);
        const glm::dvec3 offset=glm::dvec3(local)-glm::dvec3(closest);
        result.gap=static_cast<float>(glm::length(offset));
        if(result.gap>0)contact.normal=glm::normalize(box.end.rotation*glm::vec3(offset/double(result.gap)));
        // At exact physical touch choose the actual nearest exterior face,
        // rather than an old face on the other side of this finite primitive.
        else {
            int face=0;
            for(int i=1;i<3;++i)if(std::abs(local[i])-box.halfExtents[i]>
                std::abs(local[face])-box.halfExtents[face])face=i;
            glm::vec3 normal(0);normal[face]=local[face]>=0 ? 1.f : -1.f;
            contact.normal=box.end.rotation*normal;
        }
        contact.point=box.end.position+box.end.rotation*closest;
        const auto previous=box.start.position+box.start.rotation*closest;
        contact.wallVelocity=(contact.point-previous)/substepTime;
    } else if(row.sphere) {
        const auto& sphere=*row.sphere;
        const glm::dvec3 offset=glm::dvec3(position)-glm::dvec3(sphere.end.position);
        const double distance=glm::length(offset);
        if(!(distance>0))throw std::runtime_error("fluid contact at unresolved sphere centre");
        contact.normal=glm::vec3(offset/distance);
        result.gap=static_cast<float>(std::max(0.0,distance-sphere.radius));
        contact.point=sphere.end.position+contact.normal*sphere.radius;
        const auto previous=sphere.start.position+contact.normal*sphere.radius;
        contact.wallVelocity=(contact.point-previous)/substepTime;
        result.active=result.gap<=radius+tolerance;
        result.preferredFeasible=result.active && std::abs(result.gap-radius)<=tolerance;
    } else if(row.terrain) {
        const auto& terrain=*row.terrain;
        const auto sample=terrain.surface->Sample(terrain.inverseEnd*(position-terrain.end.position));
        contact.normal=glm::normalize(terrain.end.rotation*sample.outwardNormal);
        result.gap=std::max(0.f,sample.signedDistance);
        contact.point=terrain.end.position+terrain.end.rotation*sample.surfacePoint;
        const auto previous=terrain.start.position+terrain.start.rotation*sample.surfacePoint;
        contact.wallVelocity=(contact.point-previous)/substepTime;
        result.active=result.gap<=radius+tolerance;
        result.preferredFeasible=result.active && std::abs(result.gap-radius)<=tolerance;
    }
    return result;
}

const BoxContact& PreferredContact(const BoxContact& original,const FinalSoftContact& final) {
    return original.hardEncounter || original.box ? original : final.physical;
}

struct ContactGroup {
    std::vector<FinalSoftContact> rows;
    bool yielding=false;
};

ContactGroup ClassifyContacts(const std::vector<BoxContact>& contacts,
                             const glm::vec3& position,float radius,float substepTime) {
    ContactGroup group;
    group.rows.reserve(contacts.size());
    for(const auto& contact:contacts) {
        auto final=FinalizeSoftContact(contact,position,radius,substepTime);
        // Retain ownership of a genuine earlier drift hit. Its CURRENT gap is
        // still real: an earlier impact need not remain simultaneous contact.
        if(contact.hardEncounter)final.active=true;
        group.yielding=group.yielding || (final.active && !final.preferredFeasible);
        group.rows.push_back(final);
    }
    const auto preferred=[&](std::size_t k)->const BoxContact& {
        return PreferredContact(contacts[k],group.rows[k]);
    };
    // A feasible boundary-velocity witness bounds the prescribed accumulated-
    // impulse solve in that frame. Preserve ordinary resting/opening behavior;
    // this is sufficient, not a necessary test for all feasible velocities.
    bool compatible=false;
    for(std::size_t a=0;a<contacts.size() && !compatible;++a) {
        if(!group.rows[a].active)continue;
        const glm::dvec3 reference(preferred(a).wallVelocity);
        bool valid=true;
        for(std::size_t b=0;b<contacts.size() && valid;++b)if(group.rows[b].active)
            valid=glm::dot(glm::dvec3(preferred(b).normal),
                           reference-glm::dvec3(preferred(b).wallVelocity))>=0;
        compatible=valid;
    }
    if(compatible)group.yielding=false;
    // Nonnegative normal products cannot make another preferred row close
    // under a positive normal impulse. Competing rows without a bounded
    // reference yield their optional thickness. The sign band covers normal
    // arithmetic rounding, not an angle/impulse cutoff or a physical tolerance.
    for(std::size_t a=0;a<contacts.size() && !group.yielding && !compatible;++a) {
        if(!group.rows[a].active)continue;
        for(std::size_t b=a+1;b<contacts.size();++b)if(group.rows[b].active &&
            glm::dot(preferred(a).normal,preferred(b).normal)<
                -8*std::numeric_limits<float>::epsilon()) {group.yielding=true;break;}
    }
    return group;
}

// Direct callers have no rigid-world callback. Reuse the same contact law
// for all of one particle's prescribed rows; a single projection can undo an
// earlier bound when normals oppose. No rigid state or warm cache is invented.
void ResolvePrescribedContacts(std::vector<FluidParticle>& particles,
                               const std::vector<FluidVelocityContact>& contacts,
                               std::vector<glm::vec3>& impulses,
                               std::size_t& iterationCaps,float& maximumResidual) {
    ContactSolver solver;
    std::vector<RigidBody> walls;
    for(std::size_t first=0;first<contacts.size();) {
        std::size_t end=first+1;
        while(end<contacts.size() && contacts[end].particleIndex==contacts[first].particleIndex)++end;
        auto& particle=particles[contacts[first].particleIndex];
        RigidBody body;
        body.position=particle.position;
        body.linearVelocity=particle.velocity;
        body.inverseMass=1.f/particle.mass;
        solver.Clear();
        walls.clear();walls.resize(end-first);
        float speedScale=std::max(1.f,glm::length(body.linearVelocity));
        for(std::size_t i=first;i<end;++i) {
            const auto& c=contacts[i];auto& wall=walls[i-first];
            wall.position=c.point;wall.linearVelocity=c.wallVelocity;
            speedScale=std::max(speedScale,glm::length(c.wallVelocity));
            Contact geometry;
            geometry.hit=true;geometry.point=c.point;geometry.normal=c.normal;
            geometry.preciseNormal=glm::dvec3(c.normal);
            geometry.hasLocalAnchors=true;
            geometry.localAnchorA=glm::dvec3(c.point)-glm::dvec3(body.position);
            solver.AddContact(body,wall,geometry,0,0);
        }
        solver.Prepare();
        for(std::size_t i=first;i<end;++i)
            solver.SetInelasticVelocityTarget(i-first,contacts[i].minimumNormalVelocity);
        float residual=0;
        int iterations=0;
        do {
            const int count=std::min(10,64-iterations);
            solver.SolveVelocities(count);iterations+=count;residual=0;
            for(const auto& c:solver.Constraints()) {
                const float speed=glm::dot(body.linearVelocity-c.bodyB->linearVelocity,c.normal);
                residual=std::isfinite(speed) ? std::max(residual,c.restitutionBias-speed)
                                             : std::numeric_limits<float>::infinity();
            }
        } while(residual>1e-5f*speedScale && iterations<64);
        maximumResidual=std::max(maximumResidual,residual);
        iterationCaps+=residual>1e-5f*speedScale ? 1 : 0;
        for(int axis=0;axis<3;++axis)if(!std::isfinite(body.linearVelocity[axis]))
            throw std::runtime_error("nonfinite prescribed fluid contact response");
        for(const auto& c:solver.Constraints())if(!std::isfinite(c.normalImpulse))
            throw std::runtime_error("nonfinite prescribed fluid contact impulse");
        particle.velocity=body.linearVelocity;
        for(std::size_t i=first;i<end;++i) {
            const auto& c=solver.Constraints()[i-first];
            impulses[i]=c.normal*c.normalImpulse+c.tangentImpulse;
        }
        // Rows contain pointers to stack/array bodies; clear before destruction
        // or any following resize, including the final particle group.
        solver.Clear();first=end;
    }
}

void ApplySolidCollisions(std::size_t particleIndex, const glm::vec3& from,
                          std::vector<glm::vec3>& positions,
                          const std::vector<PreparedBox>& boxes,
                          const std::vector<PreparedSphere>& spheres,
                          const std::vector<PreparedTerrain>& terrains,
                          float radius, float substepTime, bool initialDrift,
                          std::vector<glm::vec3>& solidCorrections,
                          std::vector<std::vector<BoxContact>>& contacts,
                          std::size_t& hardRepairCount,std::size_t& unresolvedCount,
                          std::size_t& candidateCount,float& maximumRepairDistance,
                          std::size_t& skinClippedCount,std::size_t& skinReversalCount,
                          std::size_t& surfaceRoundingSteps) {
    const glm::vec3 reference=positions[particleIndex];
    const glm::vec3 correctionBefore=solidCorrections[particleIndex];
    const auto contactsBefore=contacts[particleIndex];
    auto& point=positions[particleIndex];
    // Primitive encounter tracking serves only geometric fallback. Keep the
    // legacy owner/normal coalescing for ordinary velocity contacts.
    std::vector<BoxContact> geometryRows=contacts[particleIndex];
    const auto observe=[&](const BoxContact& contact) {
        if(!contact.hit)return;
        for(auto& row:geometryRows)if(row.owner.id==contact.owner.id && row.box==contact.box &&
            glm::dot(row.normal,contact.normal)>=1.0f-8.0f*std::numeric_limits<float>::epsilon()) {
            row=contact;return;
        }
        geometryRows.push_back(contact);
    };
    auto retain = [&](const BoxContact& contact) {
        observe(contact);
        auto& rows = contacts[particleIndex];
        for (auto& row : rows) {
            if (row.owner.id == contact.owner.id && row.box == contact.box &&
                row.sphere == contact.sphere && row.terrain == contact.terrain &&
                glm::dot(row.normal, contact.normal) >= 1.0f - 8.0f * std::numeric_limits<float>::epsilon()) {
                const bool witnessed=row.displacementWitness || contact.displacementWitness;
                if (!row.hardEncounter || contact.hardEncounter) row = contact;
                row.displacementWitness=witnessed;
                return;
            }
        }
        rows.push_back(contact);
    };
    auto commit=[&](BoxContact contact,const auto& insideOwnSolid) {
        if(!contact.hit)return;
        contact.displacementWitness=true;
        // A geometric repair is not automatically an earlier drift impact.
        // Immutable drift witnesses are recorded separately below.
        // A represented surface point can round inward. Advance only to
        // neighbouring float coordinates in its known outward direction;
        // strict whole-union validation follows the hard projection passes.
        // This bounded construction is counted and never alters velocity.
        for(int iteration=0;iteration<32 && insideOwnSolid(contact.projectedPosition);++iteration) {
            for(int axis=0;axis<3;++axis) {
                if(contact.normal[axis]>0)contact.projectedPosition[axis]=std::nextafter(
                    contact.projectedPosition[axis],std::numeric_limits<float>::infinity());
                else if(contact.normal[axis]<0)contact.projectedPosition[axis]=std::nextafter(
                    contact.projectedPosition[axis],-std::numeric_limits<float>::infinity());
            }
            ++surfaceRoundingSteps;
        }
        solidCorrections[particleIndex]+=contact.projectedPosition-positions[particleIndex];
        positions[particleIndex]=contact.projectedPosition;
        retain(contact);
    };
    // Ordinary feasible preferred-skin corrections preserve their original
    // arithmetic and velocity rows. Real point exclusion is established only
    // when those corrections cannot supply a verified physical exterior.
    const auto establishHard=[&]() {
      for(int pass=0;pass<2;++pass) {
        for(const auto& box:boxes) {
            auto c=CollideBox(from,positions[particleIndex],box,0,substepTime);c.owner=box.owner;
            commit(c,[&](const glm::vec3& p){return BoxSeparation(p,box,0)<0;});
        }
        for(const auto& sphere:spheres) {
            auto c=CollideSphere(from,positions[particleIndex],sphere,0,substepTime);c.owner=sphere.owner;
            commit(c,[&](const glm::vec3& p){const glm::dvec3 d=glm::dvec3(p)-glm::dvec3(sphere.end.position);
                return glm::dot(d,d)<double(sphere.radius)*sphere.radius;});
        }
        for(const auto& terrain:terrains) {
            auto c=CollideTerrain(from,positions[particleIndex],terrain,0,substepTime);c.owner=terrain.owner;
            commit(c,[&](const glm::vec3& p){const auto local=terrain.inverseEnd*(p-terrain.end.position);
                return glm::dot(local,local)<=kEpsilon*kEpsilon || terrain.surface->Sample(local).signedDistance<0;});
        }
      }
      if(InsideRealSolid(point,boxes,spheres,terrains) || CrossesAnyNonmovingBox(from,point,boxes)) {
        const glm::vec3 before=point;
        if(!RepairRealBoxOverlap(from,reference,point,geometryRows,boxes,spheres,terrains,candidateCount)) {
            ++unresolvedCount;
            throw std::runtime_error("FluidWorld hard solid union has no verified safe projection for particle "+
                                     std::to_string(particleIndex));
        }
        ++hardRepairCount;maximumRepairDistance=std::max(maximumRepairDistance,glm::distance(point,before));
        solidCorrections[particleIndex]+=point-before;
      }
      if(!OutsideRealSolids(point,boxes,spheres,terrains)) {
        ++unresolvedCount;
        throw std::runtime_error("FluidWorld hard solid union validation failed for particle "+std::to_string(particleIndex));
      }
    };
    struct SoftPreference {glm::dvec3 normal,achieved;int limitingKind;std::size_t limitingIndex;};
    std::vector<SoftPreference> preferences;
    const auto soft=[&](const auto& buildContact) {
        BoxContact contact=buildContact(radius);
        if(!contact.hit)return;
        observe(contact);
        // An exterior prediction on the far side of a crossed wall is not
        // a valid correction anchor. Recover its already verified swept
        // contact before treating clearance adjustment as exterior motion.
        bool startSafe=OutsideRealSolids(point,boxes,spheres,terrains) &&
            !CrossesAnyNonmovingBox(from,point,boxes);
        const bool fullRecoverySafe=OutsideRealSolids(contact.projectedPosition,boxes,spheres,terrains) &&
            !CrossesAnyNonmovingBox(from,contact.projectedPosition,boxes);
        if(!startSafe && !fullRecoverySafe) {
            establishHard();
            contact=buildContact(radius);
            if(!contact.hit)return;
            observe(contact);
            startSafe=true;
        }
        const glm::vec3 before=point,proposal=contact.projectedPosition;
        const glm::dvec3 displacement=glm::dvec3(proposal)-glm::dvec3(before);
        // A safe ordinary projection may recover an overlapping prediction
        // directly. Starting outside, the WHOLE correction segment must also
        // remain outside; safe endpoints alone do not authorize crossing.
        const auto physical=startSafe ? ClipRealUnion(before,proposal,boxes,spheres,terrains) : RealClipLimit{};
        double fraction=physical.fraction;
        for(const auto& preference:preferences) {
            const double change=glm::dot(preference.normal,displacement);
            if(change<0) {
                const double clearance=glm::dot(preference.normal,glm::dvec3(before)-preference.achieved);
                const double allowed=std::max(0.0,clearance)/-change;
                if(allowed<fraction) {fraction=allowed;++skinReversalCount;}
            }
        }
        float representedFraction=static_cast<float>(fraction);
        if(double(representedFraction)>fraction)representedFraction=std::nextafter(representedFraction,0.0f);
        // Full feasible preferred clearance must be bit-for-bit the original
        // projected point, avoiding redundant hard/skin float arithmetic.
        glm::vec3 candidate=representedFraction==1 ? proposal :
            glm::vec3(glm::dvec3(before)+double(representedFraction)*displacement);
        // The exact box/sphere entry is converted to a representable point.
        // Binary retreat handles coordinate rounding without a velocity repair.
        if(!OutsideRealSolids(candidate,boxes,spheres,terrains)) {
            float low=0,high=representedFraction;
            for(int iteration=0;iteration<32;++iteration) {
                const float middle=.5f*(low+high);
                const auto trial=glm::vec3(glm::dvec3(before)+double(middle)*displacement);
                if(OutsideRealSolids(trial,boxes,spheres,terrains))low=middle;else high=middle;
            }
            representedFraction=low;
            candidate=glm::vec3(glm::dvec3(before)+double(low)*displacement);
        }
        if(!OutsideRealSolids(candidate,boxes,spheres,terrains)) {
            ++unresolvedCount;
            throw std::runtime_error("FluidWorld soft skin lost hard exclusion for particle "+std::to_string(particleIndex));
        }
        if(representedFraction<1)++skinClippedCount;
        solidCorrections[particleIndex]+=candidate-before;point=candidate;
        // Track selected geometry independently of retained velocity history.
        contact.displacementWitness=true;
        retain(contact);
        if(physical.fraction<1) {
            // First blocked clearance wins an ambiguous soft-only tie.
            // Later skin projections cannot undo it. Hard exclusion always
            // takes priority; this is geometry ordering, not a force law.
            preferences.push_back({glm::dvec3(contact.normal),glm::dvec3(point),physical.kind,physical.index});
        }
    };
    // Historical sweep ownership must come from the immutable free drift,
    // before ANY preferred correction can push a point into another collider.
    // Density proposals retain physical exclusion but do not invent episodes.
    if(initialDrift) {
        const auto drift=[&](BoxContact c,BodyHandle owner) {
            if(!c.hit)return;
            c.owner=owner;c.hardEncounter=true;retain(c);
        };
        for(const auto& box:boxes)drift(CollideBox(from,reference,box,0,substepTime),box.owner);
        for(const auto& sphere:spheres)drift(CollideSphere(from,reference,sphere,0,substepTime),sphere.owner);
        for(const auto& terrain:terrains)drift(CollideTerrain(from,reference,terrain,0,substepTime),terrain.owner);
    }
    // Preserve the legacy two preferred-clearance passes where feasible.
    // Blocked-clearance preferences live across BOTH passes, so impossible
    // skins cannot repeatedly undo each other in a narrow physical gap.
    for(int pass=0;pass<2;++pass) {
        for(const auto& box:boxes)soft([&](float skin){auto c=CollideBox(from,point,box,skin,substepTime);c.owner=box.owner;return c;});
        for(const auto& sphere:spheres)soft([&](float skin){auto c=CollideSphere(from,point,sphere,skin,substepTime);c.owner=sphere.owner;return c;});
        for(const auto& terrain:terrains)soft([&](float skin){auto c=CollideTerrain(from,point,terrain,skin,substepTime);c.owner=terrain.owner;return c;});
    }
    if(!OutsideRealSolids(point,boxes,spheres,terrains) || CrossesAnyNonmovingBox(from,point,boxes))establishHard();
    if(radius>0 && ClassifyContacts(contacts[particleIndex],point,radius,substepTime).yielding) {
        // A blocked preference must not spend the available real gap to create
        // contact with its opposing wall. Replay only HARD geometry from this
        // stage's original proposal; PBF sees this result in its next iteration.
        // Removing only the velocity constraint left position/velocity clocks
        // inconsistent and preserved the false simultaneous-contact episode.
        const auto encounters=contacts[particleIndex];
        point=reference;solidCorrections[particleIndex]=correctionBefore;
        contacts[particleIndex]=contactsBefore;
        geometryRows=encounters;
        establishHard();
        for(auto c:encounters) {
            c.displacementWitness=false;
            retain(c);
        }
        ++skinClippedCount; // rejected work is counted, not hidden
    }
}

}  // namespace

FluidWorld::FluidWorld() = default;

FluidWorld::FluidWorld(const FluidSettings& settings) : m_settings(settings) {
    if (!std::isfinite(settings.velocitySmoothing) || settings.velocitySmoothing < 0.0f ||
        settings.velocitySmoothing > 1.0f) {
        throw std::invalid_argument("fluid velocitySmoothing must be finite and in [0,1]");
    }
}

void FluidWorld::AddParticle(const glm::vec3& position, const glm::vec3& velocity,
                             float mass) {
    ++m_revision;
    if (mass <= 0.0f) return;
    m_particles.push_back(FluidParticle{position, position, velocity, mass});
    m_lastDensities.push_back(m_settings.restDensity);
    m_boundaryMarkers.push_back({});
}

void FluidWorld::Clear() {
    ++m_revision;
    m_particles.clear();
    m_boundaryMarkers.clear();
    m_lastDensities.clear();
    m_geometryEscapes=m_unresolvedGeometry=m_geometryEscapeCandidates=0;
    m_maximumGeometryEscapeDistance=0;
    m_skinClippedProposals=m_skinPreventedReversals=0;
    m_surfaceRoundingSteps=0;
    m_fallbackContactIterationCaps=0;m_maximumFallbackContactResidual=0;
    m_pressureReconstructionSamples=0;
    m_maximumPressureReconstructionVelocityError=0;
    m_pressureReconstructionVelocityErrorSquared=0;
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
                      std::vector<FluidContactImpulse>* contactImpulses,
                      const FluidVelocityBatchResponse& velocityResponse) {
    ++m_revision;
    if (contactImpulses) contactImpulses->clear();
    if (fixedDeltaTime <= 0.0f || m_particles.empty()) return;
    m_densityMilliseconds=m_boundaryMilliseconds=m_velocityMilliseconds=0;
    const int substeps = std::max(m_settings.substeps, 1);
    const int iterations = std::max(m_settings.densityIterations, 0);
    const float substepTime = fixedDeltaTime / static_cast<float>(substeps);
    const float radius = m_settings.smoothingRadius;
    const FluidKernel kernel(radius);
    const float restDensity = m_settings.restDensity;
    const std::size_t count = m_particles.size();
    const auto boundaryFrame=FluidBoundaryFrame(m_particles);
    for (FluidParticle& particle : m_particles) {
        particle.previousPosition = particle.position;
    }

    m_geometryEscapes=m_unresolvedGeometry=m_geometryEscapeCandidates=0;
    m_maximumGeometryEscapeDistance=0;
    m_skinClippedProposals=m_skinPreventedReversals=0;
    m_surfaceRoundingSteps=0;
    m_fallbackContactIterationCaps=0;m_maximumFallbackContactResidual=0;
    m_pressureReconstructionSamples=0;
    m_maximumPressureReconstructionVelocityError=0;
    m_pressureReconstructionVelocityErrorSquared=0;
    std::vector<glm::vec3> initialVelocities;
    initialVelocities.reserve(count);
    for (const FluidParticle& p : m_particles) initialVelocities.push_back(p.velocity);
    std::vector<glm::vec3> positions(count);
    std::vector<glm::vec3> starts(count);
    std::vector<glm::vec3> unconstrainedPositions(count);
    std::vector<glm::vec3> corrections(count);
    std::vector<glm::vec3> solidCorrections(count);
    // Passive accounting: sum the actual float position changes attributable
    // to PBF in double, so subtraction cancellation can be measured separately
    // from the approximation's real pressure/relaxation velocity.
    std::vector<glm::dvec3> representedPressureCorrections(count);
    std::vector<float> lambdas(count);
    std::vector<float> densities(count);
    NeighborLists neighborWorkspace;neighborWorkspace.indices.reserve(count*64);
    std::vector<std::vector<BoxContact>> contacts(count);
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
        for (auto& rows : contacts) rows.clear();
        std::fill(solidCorrections.begin(), solidCorrections.end(), glm::vec3(0));
        std::fill(representedPressureCorrections.begin(), representedPressureCorrections.end(), glm::dvec3(0));
        auto phaseStart=FluidClock::now();
        for (std::size_t i = 0; i < count; ++i) {
            FluidParticle& particle = m_particles[i];
            starts[i] = particle.position;
            particle.velocity += gravity.Sample(particle.position) * substepTime;
            positions[i] = starts[i] + particle.velocity * substepTime;
            unconstrainedPositions[i] = positions[i];
            ApplySolidCollisions(i, starts[i], positions, preparedBoxes, preparedSpheres,
                                 preparedTerrains,
                                 m_settings.particleRadius, substepTime, true, solidCorrections, contacts,
                                 m_geometryEscapes,m_unresolvedGeometry,m_geometryEscapeCandidates,m_maximumGeometryEscapeDistance,
                                 m_skinClippedProposals,m_skinPreventedReversals,m_surfaceRoundingSteps);
        }

        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);
        for (int iteration = 0; iteration < iterations; ++iteration) {
            phaseStart=FluidClock::now();
            const NeighborLists& neighbors = BuildNeighbors(positions, radius, neighborWorkspace);
            for (std::size_t i = 0; i < count; ++i) {
                densities[i] = DensityAt(i, positions, m_particles, neighbors, kernel);
                const float constraint = std::max(densities[i] / restDensity - 1.0f, 0.0f);
                if (constraint <= 0.0f) {
                    lambdas[i] = 0.0f;
                    continue;
                }
                glm::vec3 ownGradient(0.0f);
                float gradientSum = 0.0f;
                for (std::size_t j : neighbors[i]) {
                    const glm::vec3 gradient = (m_particles[j].mass / restDensity) *
                        SpikyGradient(positions[i] - positions[j], kernel);
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
                        SpikyGradient(positions[i] - positions[j], kernel);
                }
                const float length = glm::length(correction);
                if (length > m_settings.maxDensityCorrection && length > kEpsilon) {
                    correction *= m_settings.maxDensityCorrection / length;
                }
                corrections[i] = correction;
            }
            for (std::size_t i = 0; i < count; ++i) {
                const glm::dvec3 before(positions[i]);
                positions[i] += corrections[i];
                representedPressureCorrections[i] += glm::dvec3(positions[i])-before;
            }
            m_densityMilliseconds+=ElapsedFluidMs(phaseStart);phaseStart=FluidClock::now();
            for (std::size_t i = 0; i < count; ++i) {
                ApplySolidCollisions(i, starts[i], positions, preparedBoxes, preparedSpheres,
                                     preparedTerrains,
                                     m_settings.particleRadius, substepTime, false, solidCorrections, contacts,
                                 m_geometryEscapes,m_unresolvedGeometry,m_geometryEscapeCandidates,m_maximumGeometryEscapeDistance,
                                 m_skinClippedProposals,m_skinPreventedReversals,m_surfaceRoundingSteps);
            }
            m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);
        }

        // This passive geometry state never changes particle positions,
        // velocities, contact impulses or mass. Reuse resolved endpoint poses.
        phaseStart=FluidClock::now();
        std::vector<FluidBoundarySolid> boundarySolids;
        boundarySolids.reserve(preparedBoxes.size()+preparedSpheres.size());
        for(const auto& box : preparedBoxes)
            boundarySolids.push_back({box.owner,box.end,box.halfExtents,0});
        for(const auto& sphere : preparedSpheres)
            boundarySolids.push_back({sphere.owner,sphere.end,{},sphere.radius});
        for (std::size_t i = 0; i < count; ++i) {
            BodyHandle owner{};bool ambiguous=false;
            for(const auto& contact : contacts[i]) {
                if(!contact.displacementWitness)continue;
                if(!owner.IsValid())owner=contact.owner;
                else if(!FluidBoundaryDetail::SameOwner(owner,contact.owner))ambiguous=true;
            }
            if(ambiguous){m_boundaryMarkers[i]={};continue;}
            UpdateFluidDisplacementMarker(m_boundaryMarkers[i], positions[i],
                solidCorrections[i], owner, boundarySolids, m_settings.particleRadius,
                .5f*std::cbrt(m_particles[i].mass/m_settings.restDensity), boundaryFrame);
        }

        m_boundaryMilliseconds+=ElapsedFluidMs(phaseStart);
        phaseStart=FluidClock::now();
        for (std::size_t i = 0; i < count; ++i) {
            FluidParticle& particle = m_particles[i];
            // Preserve free-flight velocity exactly. Reconstructing it from
            // two nearby float positions introduces transverse drift even
            // when no pressure/contact correction occurred.
            // Geometric solid push-out must not become a second pressure/energy
            // kick. Only the PBF correction is reconstructed as velocity;
            // wall-normal collision response below owns the contact impulse.
            const glm::vec3 reconstructedCorrection=positions[i]-unconstrainedPositions[i]-solidCorrections[i];
            const glm::dvec3 reconstructionError=(glm::dvec3(reconstructedCorrection)-
                representedPressureCorrections[i])/double(substepTime);
            const double squaredError=glm::dot(reconstructionError,reconstructionError);
            m_maximumPressureReconstructionVelocityError=std::max(
                m_maximumPressureReconstructionVelocityError,std::sqrt(squaredError));
            m_pressureReconstructionVelocityErrorSquared+=squaredError;
            ++m_pressureReconstructionSamples;
            particle.velocity += (positions[i] - unconstrainedPositions[i] - solidCorrections[i]) / substepTime;
            particle.position = positions[i];
        }
        // Smoothing predicts the velocity BEFORE the boundary projection.
        // Applying it afterwards reintroduced inward velocity at an already
        // resolved wall (independent two-particle witness). Use exactly the
        // same conservative weights; no additional damping or clipping.
        if (m_settings.velocitySmoothing > 0.0f) {
            const NeighborLists& neighbors = BuildNeighbors(positions, radius, neighborWorkspace);
            for (std::size_t i = 0; i < count; ++i) {
                densities[i] = DensityAt(i, positions, m_particles, neighbors, kernel);
            }
            for (std::size_t i = 0; i < count; ++i) {
                corrections[i] = glm::vec3(0.0f);
                for (std::size_t j : neighbors[i]) {
                    // A shared pair denominator makes m_i*dv_i = -m_j*dv_j.
                    // The former neighbour-only density lost momentum whenever
                    // neighbouring densities differed, even with equal masses.
                    const float pairDensity = std::max(densities[i], densities[j]);
                    const glm::vec3 separation = positions[i] - positions[j];
                    corrections[i] += (m_particles[j].mass / pairDensity) *
                        (m_particles[j].velocity - m_particles[i].velocity) *
                        Poly6(glm::dot(separation, separation), kernel);
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                m_particles[i].velocity += m_settings.velocitySmoothing * corrections[i];
            }
        }
        std::vector<FluidVelocityContact> velocityContacts;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& particle=m_particles[i];
            const auto group=ClassifyContacts(contacts[i],positions[i],m_settings.particleRadius,substepTime);
            for(std::size_t k=0;k<contacts[i].size();++k) {
                const auto& final=group.rows[k];
                if(!final.active)continue;
                // Preserve the impulse of a genuine immutable drift hit.
                // Preferred projection history is different: it can only use
                // the actual final feature/gap when the group yields.
                const bool yielding=group.yielding && !contacts[i][k].hardEncounter;
                const auto& c=yielding ? final.physical : PreferredContact(contacts[i][k],final);
                const float target=yielding ? -final.gap/substepTime : 0;
                if(!std::isfinite(target))throw std::runtime_error("nonfinite fluid contact target");
                velocityContacts.push_back({i,c.owner,c.point,c.normal,c.wallVelocity,
                    positions[i],particle.velocity,particle.mass,alpha1,target});
            }
        }
        std::vector<glm::vec3> impulses(velocityContacts.size(),glm::vec3(0));
        if (velocityResponse) velocityResponse(m_particles,velocityContacts,impulses);
        else ResolvePrescribedContacts(m_particles,velocityContacts,impulses,
                                       m_fallbackContactIterationCaps,m_maximumFallbackContactResidual);
        if(impulses.size()!=velocityContacts.size())throw std::runtime_error("fluid contact response returned inconsistent impulse count");
        if(contactImpulses)for(std::size_t row=0;row<velocityContacts.size();++row) {
            const auto& c=velocityContacts[row];
            if(c.owner.IsValid() && glm::dot(impulses[row],impulses[row])>0)
                contactImpulses->push_back({c.owner,c.point,-impulses[row],c.particleIndex});
        }

        m_velocityMilliseconds+=ElapsedFluidMs(phaseStart);
    }

    if(m_unresolvedGeometry)std::fill(m_boundaryMarkers.begin(),m_boundaryMarkers.end(),FluidDisplacementMarker{});
    for (std::size_t i = 0; i < count; ++i) positions[i] = m_particles[i].position;
    const NeighborLists& finalNeighbors = BuildNeighbors(positions, radius, neighborWorkspace);
    for (std::size_t i = 0; i < count; ++i) {
        m_lastDensities[i] = DensityAt(i, positions, m_particles, finalNeighbors, kernel);
        m_particles[i].acceleration = (m_particles[i].velocity - initialVelocities[i]) / fixedDeltaTime;
    }
}

FluidDiagnostics FluidWorld::GetDiagnostics() const {
    FluidDiagnostics diagnostics;
    diagnostics.densityMilliseconds=m_densityMilliseconds;diagnostics.boundaryMilliseconds=m_boundaryMilliseconds;diagnostics.velocityMilliseconds=m_velocityMilliseconds;
    diagnostics.particleCount = m_particles.size();
    diagnostics.geometryEscapeCount=m_geometryEscapes;
    diagnostics.unresolvedGeometryCount=m_unresolvedGeometry;
    diagnostics.geometryEscapeCandidates=m_geometryEscapeCandidates;
    diagnostics.maximumGeometryEscapeDistance=m_maximumGeometryEscapeDistance;
    diagnostics.skinClippedProposals=m_skinClippedProposals;
    diagnostics.skinPreventedReversals=m_skinPreventedReversals;
    diagnostics.surfaceRoundingSteps=m_surfaceRoundingSteps;
    diagnostics.fallbackContactIterationCaps=m_fallbackContactIterationCaps;
    diagnostics.maximumFallbackContactResidual=m_maximumFallbackContactResidual;
    diagnostics.pressureReconstructionSamples=m_pressureReconstructionSamples;
    diagnostics.maximumPressureReconstructionVelocityError=m_maximumPressureReconstructionVelocityError;
    diagnostics.rmsPressureReconstructionVelocityError=m_pressureReconstructionSamples>0 ?
        std::sqrt(m_pressureReconstructionVelocityErrorSquared/double(m_pressureReconstructionSamples)):0;
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
