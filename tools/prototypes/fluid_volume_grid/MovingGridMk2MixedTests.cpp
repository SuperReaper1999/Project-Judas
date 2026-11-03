#include "CoupledVolumeBox.h"
#include "GridCellPhase.h"
#include "GridGeometricMomentum.h"
#include "OwnedEvolvingVolume.h"
#include "PhysicsConstraintBatch.h"
#include "PhysicsWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace judas::volume_grid;
constexpr GridLayout layout{{8, 8, 8}, .125};
constexpr float tick = 1.f / 60.f;
constexpr float gravity = -9.81f;
constexpr std::uint32_t owner = 4;

struct Checks {
    int count = 0, failures = 0;

    void Expect(bool passed, const char* label, double value = 0.) {
        ++count;
        failures += !passed;
        std::cout << (passed ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

struct Fixture {
    glm::vec3 center, half, velocity;
    float mass;
    double waterline;
};

GridVec Array(glm::dvec3 v) { return {v.x, v.y, v.z}; }
glm::dvec3 Vector(const GridVec& v) { return {v[0], v[1], v[2]}; }

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.);
}

double MaxAbs(glm::dvec3 value) {
    return std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
}

double HalfUlp(float value) {
    const auto lo = std::nextafter(value, -std::numeric_limits<float>::infinity());
    const auto hi = std::nextafter(value, std::numeric_limits<float>::infinity());
    return .5 * std::max(double(value) - lo, hi - double(value));
}

BoxMotion BoxAt(glm::vec3 center, glm::vec3 half) {
    return {owner, Array(glm::dvec3(center) - glm::dvec3(half)),
        Array(glm::dvec3(center) + glm::dvec3(half)), {}};
}

struct MotionAnchor {
    glm::vec3 position{}, velocity{};
    double begin = 0.;
    bool valid = false;
};

struct Candidate {
    OwnedEvolvingVolumeStep water;
    MovingVolumeState nativeWater;
    HalfDualVolumeState nativeOwnership;
    glm::dvec3 impulse{};
    glm::vec3 velocity{}, endpoint{};
    MotionAnchor anchor;
    float dt = 0.;
    double pressurePathRise = 0.;
    double remapDistance = 0., remapMomentumError = 0.;
    double endpointRoundoff = 0.;
};

struct Result {
    std::vector<glm::vec3> positions, velocities;
    std::vector<glm::dvec3> impulses;
    unsigned published = 0;
    double maxMassDrift = 0., maxImpulseError = 0., maxImpulseBudget = 0.;
    double maxMomentumError = 0., maxPoseError = 0., maxVelocityError = 0.;
    double maxRemapDistance = 0., maxRemapMomentumError = 0.;
    double endpointRoundoff = 0.;
    double cavity = 0., expectedCavity = 0., nativeCavityError = 0.;
    double maxInletDrop = 0., maxInletError = 0.;
    bool allInletsHaveMass = true, upperHalvesStayClosed = true;
};

void MeasureFirstInlet(const Candidate& candidate, glm::vec3 start, Result& result) {
    const auto& pressure = candidate.water.phase.fluid;
    const double area = layout.h * layout.h;
    const double inletMass = .5 * area * layout.h * 1000.;
    const double scale = double(candidate.dt) * area;
    for (int z = 2; z < 6; ++z) {
        for (int x = 2; x < 6; ++x) {
            const auto cell = layout.Cell(x, 4, z);
            const auto donor = layout.Cell(x, 3, z);
            const auto inlet = layout.Face(1, x, 4, z);
            const double drop = pressure.pressure[donor] - pressure.pressure[cell];
            // The donor's wet half owns all the start inertia of this face.
            // There is no starting velocity, and the exposed cell starts solid.
            const double expectedFlux = scale * (pressure.gravityVelocityIncrement[1][inlet] +
                                                 scale * drop / inletMass);
            result.cavity += pressure.state.water[cell];
            result.nativeCavityError += candidate.nativeWater.water[cell];
            result.maxInletDrop = std::max(result.maxInletDrop, std::abs(drop));
            result.maxInletError = std::max(result.maxInletError,
                                            std::abs(pressure.flux[1][inlet] - expectedFlux));
            result.allInletsHaveMass = result.allInletsHaveMass &&
                pressure.pressureInertialFace[1][inlet];
            result.upperHalvesStayClosed = result.upperHalvesStayClosed &&
                candidate.water.halfVelocity.velocity[1][2 * cell + 1] == 0.;
        }
    }
    result.expectedCavity = .25 * candidate.pressurePathRise;
    result.nativeCavityError = std::abs(result.nativeCavityError -
        .25 * (double(candidate.endpoint.y) - start.y));
}

void ReconcileGeometry(const CoupledBoxStep& ideal, glm::vec3 half, Candidate& candidate) {
    const auto endpoint = glm::dvec3(candidate.endpoint);
    const auto idealCenter = .5 * (Vector(ideal.body.lo) + Vector(ideal.body.hi));
    const auto correction = endpoint - idealCenter;
    candidate.remapDistance = MaxAbs(correction);
    for (int axis = 0; axis < 3; ++axis) {
        const double bound = HalfUlp(candidate.endpoint[axis]) +
            HalfUlp(candidate.velocity[axis] * candidate.dt) +
            double(candidate.dt) * HalfUlp(candidate.velocity[axis]) +
            ideal.rigidResidual + 16 * std::numeric_limits<double>::epsilon() * layout.h;
        if (std::abs(correction[axis]) > bound ||
            bound > 8 * std::numeric_limits<float>::epsilon() * layout.h)
            throw std::runtime_error("native mixed reconciliation exceeds its binary32 bound");
    }
    std::string error;
    const BoxMotion start{owner, ideal.body.lo, ideal.body.hi, {}};
    auto motion = start;
    motion.translation = Array(correction);
    MovingGridGeometry geometry;
    GridCellPhase phase;
    const auto& physical = candidate.water.phase.fluid.state;
    if (!BuildMovingGridGeometry(layout, {motion}, geometry, error) ||
        !ReconstructGridCellPhase(layout, {start}, 1, physical.water, phase, error))
        throw std::runtime_error("mixed rounding remap geometry: " + error);
    auto still = physical;
    auto zeroOwnership = candidate.water.halfVelocity;
    for (int axis = 0; axis < 3; ++axis) {
        std::fill(still.velocity[axis].begin(), still.velocity[axis].end(), 0.);
        std::fill(zeroOwnership.velocity[axis].begin(), zeroOwnership.velocity[axis].end(), 0.);
    }
    // Unit bookkeeping clock. The artificial pressure and velocity are not
    // applied to either physical system. Only its conservative phase fluxes
    // carry the actual owned momentum to the native endpoint.
    OwnedEvolvingVolumeStep remap;
    if (!AdvanceGeometricEvolvingVolume(layout, {motion}, still, zeroOwnership, 1., 1, 0.,
                                         remap, error))
        throw std::runtime_error("mixed rounding phase remap: " + error);
    GridGeometricMomentumStart geometryStart;
    GridGeometricTransport transport;
    if (!BuildGridGeometricMomentumStart(layout, geometry, {start}, 1, phase.waterPlane,
                                         physical, candidate.water.halfVelocity, geometryStart, error) ||
        !AdvectGridGeometricMomentum(layout, geometry, {motion}, 1, phase.waterPlane, geometryStart,
                                     remap.phase.fluid, candidate.water.halfVelocity, transport, error))
        throw std::runtime_error("mixed rounding momentum remap: " + error);
    candidate.nativeWater = std::move(remap.phase.fluid.state);
    // The continuous box translated to the represented center and the
    // box rebuilt from that center can round their bounds differently.
    // Verify their cut capacities before mapping the phase representation.
    GridCellPhase publishedPhase;
    GridCellPhaseReconciliation endpointRepresentation{geometry.endCapacity, {}};
    if (!ReconstructGridCellPhase(layout, {BoxAt(candidate.endpoint, half)}, 1,
                                  candidate.nativeWater.water, publishedPhase, error,
                                  &endpointRepresentation))
        throw std::runtime_error("mixed native endpoint representation: " + error);
    candidate.endpointRoundoff = endpointRepresentation.volumeRoundoff;
    candidate.nativeWater.water = std::move(endpointRepresentation.water);
    candidate.nativeWater.velocity = std::move(transport.merged.faceVelocity);
    candidate.nativeOwnership = std::move(transport.merged.state);
    candidate.remapMomentumError = MaxAbs(Vector(transport.momentumResidual));
}

Result Run(const Fixture& fixture, bool inlet, unsigned ticks = 1) {
    PhysicsWorld world;
    if (!world.Init())
        throw std::runtime_error("MK2 init failed");
    const auto handle = world.CreateDynamicBox(fixture.center, fixture.half, fixture.mass, 0, 0);
    if (!handle.IsValid())
        throw std::runtime_error("MK2 dynamic box creation failed");
    world.SetLinearVelocity(handle, fixture.velocity);
    const auto initialBox = BoxAt(fixture.center, fixture.half);
    GridPhaseGeometry seed;
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildGridPhaseGeometry(layout, {initialBox}, 1, fixture.waterline, seed, error) ||
        !BuildMovingGridGeometry(layout, {initialBox}, geometry, error))
        throw std::runtime_error("native mixed seed: " + error);
    MovingVolumeState water;
    water.water = std::move(seed.water);
    for (std::size_t cell = 0; cell < water.water.size(); ++cell)
        water.water[cell] = std::min(water.water[cell], geometry.startCapacity[cell]);
    HalfDualVolumeState owned;
    for (int axis = 0; axis < 3; ++axis) {
        water.velocity[axis].resize(layout.FaceCount(axis));
        owned.velocity[axis].resize(2 * layout.CellCount());
    }
    const double initialVolume = Sum(water.water);

    Result result;
    Candidate pending;
    MotionAnchor anchor;
    glm::vec3 nativePredictor{};
    double nativeInverseMass = 0.;
    PhysicsConstraintParticipant participant;
    participant.bodies = {handle};
    participant.maximumBodies = 1;
    participant.maximumContacts = 1;
    participant.maximumJointRows = 1;
    participant.propose = [&](const PhysicsConstraintBatch& batch, PhysicsConstraintProposal& proposal) {
        if (batch.bodies.size() != 1 || batch.bodies.front().handle.id != handle.id ||
            !batch.contacts.empty() || !batch.joints.empty())
            throw std::runtime_error("unexpected native rows in contact-free mixed probe");
        const auto& body = batch.bodies.front();
        if (body.angularVelocity != glm::vec3(0) || body.orientation != glm::quat(1, 0, 0, 0))
            throw std::runtime_error("the native mixed probe admits translation only");
        nativePredictor = body.linearVelocity;
        nativeInverseMass = body.inverseMass;
        const auto start = BoxAt(body.center, fixture.half);
        const float h = batch.context.deltaTime;
        const double begin = double(batch.context.stepDeltaTime) * batch.context.index /
            batch.context.count;
        const glm::dvec3 preGravity = glm::dvec3(body.linearVelocity) - glm::dvec3(0., double(h) * gravity, 0.);
        const CoupledBoxState rigid{owner, start.lo, start.hi, Array(preGravity), 1. / body.inverseMass};
        OwnedCoupledBoxStep ideal;
        if (!AdvanceGeometricEvolvingCoupledVolumeBox(layout, rigid, water, owned, h, 1, gravity,
                                                       ideal, error))
            throw std::runtime_error("native mixed free forecast: " + error);

        pending = {};
        pending.dt = h;
        const auto predict = [&](glm::dvec3 impulse) {
            pending.impulse = impulse;
            pending.velocity = glm::vec3(glm::dvec3(nativePredictor) + nativeInverseMass * impulse);
            pending.anchor = anchor;
            if (!anchor.valid || anchor.velocity != pending.velocity ||
                anchor.position + anchor.velocity * float(begin - anchor.begin) != body.center) {
                pending.anchor = {body.center, pending.velocity, begin, true};
            }
            pending.endpoint = pending.anchor.position + pending.anchor.velocity *
                float(double(h) - (pending.anchor.begin - begin));
        };
        predict(double(h) * Vector(ideal.box.pressureForce));
        const auto continuousImpulse = pending.impulse;
        const auto represented = pending.endpoint;
        const auto testRepresented = [&](glm::vec3 endpoint) {
            auto moving = start;
            moving.translation = Array(glm::dvec3(endpoint) - glm::dvec3(body.center));
            if (!AdvanceGeometricEvolvingVolume(layout, {moving}, water, owned, h, 1, gravity,
                                                 pending.water, error))
                throw std::runtime_error("native mixed represented solve: " + error);
            const auto& loads = pending.water.phase.loads;
            if (loads.size() != 1 || loads.front().owner != owner)
                throw std::runtime_error("native mixed pressure lost its box owner");
            predict(double(h) * Vector(loads.front().force));
            pending.pressurePathRise = moving.translation[1];
            return pending.endpoint == endpoint;
        };
        if (testRepresented(represented)) {
            pending.nativeWater = pending.water.phase.fluid.state;
            pending.nativeOwnership = pending.water.halfVelocity;
        } else {
            const auto firstTarget = pending.endpoint;
            auto adjacent = represented;
            adjacent.y = std::nextafter(represented.y, firstTarget.y);
            if (testRepresented(adjacent)) {
                pending.nativeWater = pending.water.phase.fluid.state;
                pending.nativeOwnership = pending.water.halfVelocity;
            } else {
                std::cout << "REMAP boundary=" << batch.context.index
                          << " native_y=" << represented.y << " mapped_y=" << firstTarget.y
                          << " adjacent_y=" << adjacent.y << " adjacent_mapped_y="
                          << pending.endpoint.y << '\n';
                predict(continuousImpulse);
                pending.pressurePathRise = .5 * (ideal.box.body.lo[1] + ideal.box.body.hi[1]) - body.center.y;
                pending.water.phase.fluid = std::move(ideal.box.fluid);
                pending.water.halfVelocity = std::move(ideal.halfVelocity);
                pending.water.transportMomentumError = ideal.transportMomentumError;
                ReconcileGeometry(ideal.box, fixture.half, pending);
            }
        }
        if (inlet && result.published == 0 && batch.context.index == 0)
            MeasureFirstInlet(pending, body.center, result);
        proposal.linearImpulses = {pending.impulse};
        proposal.angularImpulses = {glm::dvec3(0)};
        return true;
    };
    participant.published = [&](const auto&) { ++result.published; };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const auto&) {
        const auto pose = world.GetTransform(handle);
        const auto velocity = world.GetLinearVelocity(handle);
        result.maxPoseError = std::max(result.maxPoseError,
            MaxAbs(glm::dvec3(pose.position) - glm::dvec3(pending.endpoint)));
        result.maxVelocityError = std::max(result.maxVelocityError,
            MaxAbs(glm::dvec3(velocity) - glm::dvec3(pending.velocity)));
        if (pose.position != pending.endpoint || velocity != pending.velocity)
            throw std::runtime_error("actual MK2 mixed pose or velocity differed from forecast");
        if (world.GetAngularVelocity(handle) != glm::vec3(0) || pose.rotation != glm::quat(1, 0, 0, 0))
            throw std::runtime_error("native mixed probe drifted outside its translation-only scope");
        GridCellPhase actual;
        auto& phase = pending.nativeWater;
        if (!ReconstructGridCellPhase(layout, {BoxAt(pose.position, fixture.half)}, 1,
                                      phase.water, actual, error))
            throw std::runtime_error("mixed material does not fit actual native geometry: " + error);
        const glm::dvec3 measuredImpulse =
            (glm::dvec3(velocity) - glm::dvec3(nativePredictor)) / nativeInverseMass;
        const auto impulseError = glm::abs(measuredImpulse - pending.impulse);
        for (int axis = 0; axis < 3; ++axis) {
            const double budget = HalfUlp(velocity[axis]) / nativeInverseMass +
                8 * std::numeric_limits<double>::epsilon() * (1 + std::abs(pending.impulse[axis]));
            if (impulseError[axis] > budget)
                throw std::runtime_error("native mixed impulse rounding exceeds its ULP budget");
            result.maxImpulseBudget = std::max(result.maxImpulseBudget, budget);
        }
        result.maxImpulseError = std::max(result.maxImpulseError, MaxAbs(impulseError));
        result.maxMomentumError = std::max(result.maxMomentumError,
            MaxAbs(Vector(pending.water.transportMomentumError)));
        result.maxRemapDistance = std::max(result.maxRemapDistance, pending.remapDistance);
        result.maxRemapMomentumError = std::max(result.maxRemapMomentumError, pending.remapMomentumError);
        result.endpointRoundoff += pending.endpointRoundoff;
        result.maxMassDrift = std::max(result.maxMassDrift,
            1000. * std::abs(Sum(phase.water) - initialVolume));
        result.positions.push_back(pose.position);
        result.velocities.push_back(velocity);
        result.impulses.push_back(pending.impulse);
        anchor = pending.anchor;
        water = std::move(phase);
        owned = std::move(pending.nativeOwnership);
    };
    for (unsigned step = 0; step < ticks; ++step) {
        world.ApplyLinearAcceleration(handle, {0.f, gravity, 0.f}, tick);
        world.Step(tick, &coupling);
    }
    return result;
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    try {
        const Fixture piston{{.5f, .6875f, .5f}, {.25f, .1875f, .25f}, {0.f, .75f, 0.f}, 192.f, .94};
        const auto inlet = Run(piston, true);
        checks.Expect(inlet.published == 4 && inlet.positions.size() == 4,
                      "mixed pressure and real MK2 advance four boundaries", inlet.published);
        checks.Expect(inlet.maxPoseError == 0. && inlet.maxVelocityError == 0.,
                      "actual native binary32 pose and velocity equal the staged endpoint", inlet.maxPoseError);
        const double unopposedVelocity = piston.velocity.y + double(tick / 4) * gravity;
        const double coupledVelocity = inlet.velocities.empty() ? 0. : inlet.velocities.front().y;
        checks.Expect(coupledVelocity > 0. && coupledVelocity < unopposedVelocity - .01,
                      "water inertia changes the native ascent relative to gravity alone", coupledVelocity);
        checks.Expect(inlet.expectedCavity > 0. && std::abs(inlet.cavity - inlet.expectedCavity) < 1e-12 &&
                      inlet.nativeCavityError < 1e-12 && inlet.upperHalvesStayClosed,
                      "new cavity fills from its actual opening volume", inlet.cavity);
        checks.Expect(inlet.allInletsHaveMass && inlet.maxInletDrop > 1. && inlet.maxInletError < 1e-12,
                      "finite-mass inlets obey independent pressure-drop control", inlet.maxInletError);
        checks.Expect(inlet.maxImpulseError <= inlet.maxImpulseBudget && inlet.maxMassDrift < 1e-8 &&
                      inlet.maxMomentumError < 1e-10 && inlet.maxRemapMomentumError < 1e-10,
                      "real body impulse, water mass and momentum remap close", inlet.maxImpulseError);
        std::cout << "inlet expected_cavity=" << inlet.expectedCavity
                  << " max_pressure_drop_pa=" << inlet.maxInletDrop
                  << " mass_drift_kg=" << inlet.maxMassDrift
                  << " max_momentum_error_kg_m_s=" << inlet.maxMomentumError
                  << " max_rounding_remap_m=" << inlet.maxRemapDistance
                  << " max_remap_momentum_error_kg_m_s=" << inlet.maxRemapMomentumError
                  << " impulse_budget_ns=" << inlet.maxImpulseBudget << '\n';

        // The 1 kg/m³ body is neutral in air, making this a static wet-versus-
        // air pressure control. Its bottom and the waterline share a cell.
        Fixture dry{{.5f, .53125f, .5f}, {.25f, .125f, .25f}, {}, .0625f, .40225};
        const auto wet = Run(dry, false);
        dry.waterline = 0.;
        const auto air = Run(dry, false);
        const bool above = std::all_of(wet.positions.begin(), wet.positions.end(),
            [&](glm::vec3 position) { return double(position.y - dry.half.y) >= .40225 + .0039; });
        checks.Expect(wet.positions.size() == 4 && air.positions.size() == 4 && above &&
                      wet.positions == air.positions && wet.velocities == air.velocities,
                      "4 mm dry gap gives the same actual native trajectory as air only");
        double dryImpulseError = 0.;
        for (std::size_t i = 0; i < std::min(wet.impulses.size(), air.impulses.size()); ++i)
            dryImpulseError = std::max(dryImpulseError, MaxAbs(wet.impulses[i] - air.impulses[i]));
        checks.Expect(dryImpulseError < 1e-12 && wet.maxMassDrift < 1e-8,
                      "nearby water transfers no pressure impulse across the dry gap", dryImpulseError);

        const auto continuing = Run(piston, true, 24);
        checks.Expect(continuing.published == 96 && continuing.positions.size() == 96 &&
                      continuing.maxPoseError == 0. && continuing.maxVelocityError == 0. &&
                      continuing.velocities.front().y > 0. && continuing.velocities.back().y < 0.,
                      "repeated native pressure coupling survives the upward and return stroke",
                      continuing.published);
        checks.Expect(continuing.endpointRoundoff > 0. && continuing.endpointRoundoff < 1e-13 &&
                      continuing.maxMassDrift < 1e-8 && continuing.maxMomentumError < 1e-10 &&
                      continuing.maxRemapMomentumError < 1e-10,
                      "actual repeated binary32 endpoints preserve both phase and momentum budgets",
                      continuing.endpointRoundoff);
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        ++checks.failures;
    }
    std::cout << "SUMMARY checks=" << checks.count << " failures=" << checks.failures << '\n';
    return checks.failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
