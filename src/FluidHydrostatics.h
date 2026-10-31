#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include "CollisionShapes.h"
#include "FluidWorld.h"

class GravityField;

// Deterministic shape-local volume quadrature, constructed once per shape.
// Boxes use equal Cartesian subvolumes; spheres use antipodal equal-volume
// radial/angular samples. Compound boxes are partitioned into a nonoverlapping
// union before sampling, so overlapping child boxes do not double their volume.
struct FluidVolumeSample {
    glm::vec3 localPosition{0};
    glm::vec3 localHalfExtents{0};
    float localRadius = 0; // isotropic sample footprint (sphere quadrature)
    float volume = 0;
    glm::quat localRotation{1,0,0,0};
};
struct FluidVolumeQuadrature {
    std::vector<FluidVolumeSample> samples;
    float totalVolume = 0;
    float boundRadius = 0;
    std::vector<CompoundBox> solidBoxes; // exact union pieces used for geometric column occlusion
    float solidSphereRadius = 0;
};
struct FluidSolidQuery {
    const FluidVolumeQuadrature& volume;
    BodyTransform pose;
};
FluidVolumeQuadrature MakeBoxFluidVolume(const glm::vec3& halfExtents, unsigned samplesPerAxis = 4);
FluidVolumeQuadrature MakeSphereFluidVolume(float radius, unsigned radialLayers = 3, unsigned antipodalPairs = 24);
FluidVolumeQuadrature MakeCompoundFluidVolume(const std::vector<CompoundBox>& boxes, unsigned samplesPerAxis = 4);

struct FluidFieldSample {
    float fraction = 0;
    float density = 0; // rest density of liquid, not multiplied by fraction
    glm::vec3 velocity{0};
    glm::vec3 acceleration{0};
};

// Approximate liquid columns at max particle spacing, in an explicit tangent/
// up basis. Vertical particle intervals form a locally filled envelope WITHIN
// each column (internal air pockets below this resolution are unresolved); column
// occupancy fractions are averaged by positive area/spatial-kernel weights.
// Nearby lower liquid columns vote dry at an elevated sample. A supplied solid
// excludes geometrically occluded column footprints so excluded interior
// particles do not masquerade as a lower free surface. There is no SPH reaction.
// The lattice is anchored at the first retained particle, so its reference
// follows universe rotations/translations; changing particle order can change
// the finite-resolution approximation. Wetness is restricted to the convex
// lateral envelope of observed wet column footprints; the donor reach does
// not make a dry point outside that envelope wet. Sub-resolution shorelines,
// concavities and air pockets inside this local envelope remain unresolved.
// Resolved bulk motion retains the local spherical Vp*(1-distance²/reach²)²
// kernel. If that kernel has no donors for a wet envelope, motion is extended
// from the admitted unoccluded columns using Vp * admitted interval fraction *
// (1-lateral²/reach²)². This preserves uniform motion in underresolved envelopes;
// continuity for nonuniform data at this missing-data seam is not guaranteed.
// Displaced-volume/contact shadows do not define bulk motion.
// There is no velocity classifier or temporal filter.
// Direction/tangent are supplied by the caller; zero-gravity callers use a
// body-local reference, never a privileged world-up gravity direction.
// The low-level default retains interpolation across observed columns for
// reconstruction controls. Physical loads, swimming and script samples request
// connectedWetSupport, so separate wet patches do not fill the gap between them.
class FluidHydrostaticField {
public:
    void Build(const std::vector<FluidParticle>& particles, float restDensity,
               const std::vector<bool>* excludedParticles = nullptr);
    FluidFieldSample Query(const glm::vec3& point, const glm::vec3& up,
                           float sampleHalfHeight, float extrapolationRadius,
                           const glm::vec3& tangentAxis,
                           const FluidSolidQuery* solid = nullptr,
                           bool connectedWetSupport = false) const;
    float RestDensity() const { return m_density; }
    float MaximumParticleSpacing() const { return m_maxSpacing; }
    std::size_t ParticleCount() const { return m_particles.size(); }
private:
    struct Particle { glm::vec3 position, velocity, acceleration; float spacing, volume; };
    struct Key {
        std::int64_t x, y, z;
        bool operator==(const Key& b) const { return x == b.x && y == b.y && z == b.z; }
    };
    struct Hash { std::size_t operator()(const Key& k) const; };
    Key Cell(const glm::vec3& point) const;
    float m_density = 0, m_maxSpacing = 0, m_cellSize = 1;
    glm::vec3 m_min{0}, m_max{0};
    std::vector<Particle> m_particles;
    std::unordered_map<Key, std::vector<std::size_t>, Hash> m_cells;
    std::vector<Key> m_occupiedCells; // deterministic z/y/x traversal; no retained pointers
};

struct FluidHydrostaticResult {
    float totalVolume = 0, submergedVolume = 0, fraction = 0, density = 0;
    glm::vec3 submergedCentroid{0};
    glm::vec3 fluidVelocity{0}, fluidAcceleration{0};
    glm::vec3 buoyancyForce{0}, buoyancyTorque{0}; // torque about pose.position
};
// A rigid solid excludes liquid from its interior; a sampling volume such as
// the query-only player does not. This is an explicit caller geometry contract,
// not a classification inferred from particle motion or body properties.
enum class FluidVolumeGeometry { DisplacedSolid, NonDisplacingVolume };
// No body mass enters this calculation. Apply the returned force at COM and
// returned torque once, or (for uniform effective gravity) force at centroid.
// Exterior particle collision must not add a second hydrostatic reaction.
FluidHydrostaticResult EvaluateFluidHydrostatics(const FluidHydrostaticField& field,
                                                const FluidVolumeQuadrature& volume,
                                                const BodyTransform& pose,
                                                const GravityField& gravity,
                                                FluidVolumeGeometry geometry = FluidVolumeGeometry::DisplacedSolid);
