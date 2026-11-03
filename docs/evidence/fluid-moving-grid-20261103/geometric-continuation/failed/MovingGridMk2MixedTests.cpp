#include "CoupledVolumeBox.h"
#include "GridCellPhase.h"
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
    glm::dvec3 impulse{};
    glm::vec3 velocity{}, endpoint{};
    MotionAnchor anchor;
    float dt = 0.;
};

struct Result {
    std::vector<glm::vec3> positions, velocities;
    std::vector<glm::dvec3> impulses;
    unsigned published = 0;
    double maxMassDrift = 0., maxImpulseError = 0., maxImpulseBudget = 0.;
    double maxMomentumError = 0., maxPoseError = 0., maxVelocityError = 0.;
    double cavity = 0., expectedCavity = 0., maxInletDrop = 0., maxInletError = 0.;
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
            result.maxInletDrop = std::max(result.maxInletDrop, std::abs(drop));
            result.maxInletError = std::max(result.maxInletError,
                                            std::abs(pressure.flux[1][inlet] - expectedFlux));
            result.allInletsHaveMass = result.allInletsHaveMass &&
                pressure.pressureInertialFace[1][inlet];
            result.upperHalvesStayClosed = result.upperHalvesStayClosed &&
                candidate.water.halfVelocity.velocity[1][2 * cell + 1] == 0.;
        }
    }
    result.expectedCavity = .25 * (double(candidate.endpoint.y) - start.y);
}

Result Run(const Fixture& fixture, bool inlet) {
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
        std::vector<glm::vec3> visited;
        for (int attempt = 0; attempt < 12; ++attempt) {
            const auto endpoint = pending.endpoint;
            if (std::find(visited.begin(), visited.end(), endpoint) != visited.end())
                throw std::runtime_error("native mixed represented endpoint cycles");
            visited.push_back(endpoint);
            auto moving = start;
            moving.translation = Array(glm::dvec3(endpoint) - glm::dvec3(body.center));
            if (!AdvanceGeometricEvolvingVolume(layout, {moving}, water, owned, h, 1, gravity,
                                                 pending.water, error))
                throw std::runtime_error("native mixed represented solve: " + error);
            const auto& loads = pending.water.phase.loads;
            if (loads.size() != 1 || loads.front().owner != owner)
                throw std::runtime_error("native mixed pressure lost its box owner");
            predict(double(h) * Vector(loads.front().force));
            if (pending.endpoint != endpoint)
                continue;

            if (inlet && batch.context.index == 0)
                MeasureFirstInlet(pending, body.center, result);
            proposal.linearImpulses = {pending.impulse};
            proposal.angularImpulses = {glm::dvec3(0)};
            return true;
        }
        throw std::runtime_error("native mixed represented endpoint did not converge");
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
        auto& phase = pending.water.phase.fluid.state;
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
        result.maxMassDrift = std::max(result.maxMassDrift,
            1000. * std::abs(Sum(phase.water) - initialVolume));
        result.positions.push_back(pose.position);
        result.velocities.push_back(velocity);
        result.impulses.push_back(pending.impulse);
        anchor = pending.anchor;
        water = std::move(phase);
        owned = std::move(pending.water.halfVelocity);
    };
    world.ApplyLinearAcceleration(handle, {0.f, gravity, 0.f}, tick);
    world.Step(tick, &coupling);
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
                      "actual native binary32 pose and velocity equal the pressure path", inlet.maxPoseError);
        const double unopposedVelocity = piston.velocity.y + double(tick / 4) * gravity;
        const double coupledVelocity = inlet.velocities.empty() ? 0. : inlet.velocities.front().y;
        checks.Expect(coupledVelocity > 0. && coupledVelocity < unopposedVelocity - .01,
                      "water inertia changes the native ascent relative to gravity alone", coupledVelocity);
        checks.Expect(inlet.expectedCavity > 0. && std::abs(inlet.cavity - inlet.expectedCavity) < 1e-12 &&
                      inlet.upperHalvesStayClosed, "new cavity fills from its actual opening volume", inlet.cavity);
        checks.Expect(inlet.allInletsHaveMass && inlet.maxInletDrop > 1. && inlet.maxInletError < 1e-12,
                      "finite-mass inlets obey independent pressure-drop control", inlet.maxInletError);
        checks.Expect(inlet.maxImpulseError <= inlet.maxImpulseBudget && inlet.maxMassDrift < 1e-8 &&
                      inlet.maxMomentumError < 1e-10,
                      "real body impulse, water mass and momentum remap close", inlet.maxImpulseError);
        std::cout << "inlet expected_cavity=" << inlet.expectedCavity
                  << " max_pressure_drop_pa=" << inlet.maxInletDrop
                  << " mass_drift_kg=" << inlet.maxMassDrift
                  << " max_momentum_error_kg_m_s=" << inlet.maxMomentumError
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
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        ++checks.failures;
    }
    std::cout << "SUMMARY checks=" << checks.count << " failures=" << checks.failures << '\n';
    return checks.failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
