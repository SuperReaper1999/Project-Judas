#include "CoupledVolumeBox.h"
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
#include <string_view>
#include <utility>

namespace {

using namespace judas::volume_grid;

constexpr float kStep = 1.f / 64;
constexpr unsigned kSubsteps = 4;
constexpr glm::vec3 kHalfExtents{.15625f, .171875f, .1875f};
constexpr glm::vec3 kChartStart{.75f, .75f, .75f};
constexpr float kMass = 64;

struct Checks {
    int total = 0, failures = 0;

    void Expect(bool pass, const std::string& label, double value = 0) {
        ++total;
        failures += !pass;
        std::cout << (pass ? "PASS " : "FAIL ") << label << " value=" << value << '\n';
    }
};

GridVec Array(glm::dvec3 vector) {
    return {vector.x, vector.y, vector.z};
}

glm::dvec3 Vector(const GridVec& vector) {
    return {vector[0], vector[1], vector[2]};
}

double MaxAbs(glm::dvec3 value) {
    return std::max({std::abs(value.x), std::abs(value.y), std::abs(value.z)});
}

double Sum(const std::vector<double>& values) {
    return std::accumulate(values.begin(), values.end(), 0.);
}

BoxMotion BoxAt(glm::dvec3 center) {
    return {19, Array(center - glm::dvec3(kHalfExtents)),
        Array(center + glm::dvec3(kHalfExtents)), {}};
}

MovingGridGeometry Geometry(const GridLayout& layout, const BoxMotion& box) {
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(layout, {box}, geometry, error))
        throw std::runtime_error(error);
    return geometry;
}

MovingVolumeState AtRest(const GridLayout& layout, const MovingGridGeometry& geometry) {
    MovingVolumeState state;
    state.water = geometry.startCapacity;
    for (int axis = 0; axis < 3; ++axis) state.velocity[axis].resize(layout.FaceCount(axis));
    return state;
}

struct Experiment {
    std::string_view name;
    glm::vec3 chartOrigin;
    glm::vec3 initialVelocity;
    glm::vec3 gravity;
};

struct Observation {
    glm::vec3 predictedVelocity{}, observedVelocity{}, observedPosition{};
    glm::dvec3 impulse{};
    unsigned proposals = 0, publications = 0, observations = 0;
    double positionDifference = 0, velocityDifference = 0, impulseError = 0;
    double workDifference = 0, maximumCapacityMismatch = 0, aboveCapacity = 0, waterMassDifference = 0;
};

Observation Run(Checks& checks, const Experiment& experiment, bool applyPressure) {
    const std::string label = std::string(experiment.name) + (applyPressure ? " coupled" : " declined");
    const GridLayout layout{{8, 7, 7}, .25};
    PhysicsWorld world;
    if (!world.Init()) throw std::runtime_error("cannot initialize native physics");
    const auto handle = world.CreateDynamicBox(experiment.chartOrigin + kChartStart,
        kHalfExtents, kMass, 0, 0);
    if (!handle.IsValid()) throw std::runtime_error("native dynamic box creation failed");
    world.SetLinearVelocity(handle, experiment.initialVelocity);
    world.ApplyLinearAcceleration(handle, experiment.gravity, kStep);
    const auto initial = BoxAt(glm::dvec3(kChartStart));
    const auto originalFluid = AtRest(layout, Geometry(layout, initial));

    Observation observed;
    CoupledBoxStep candidate;
    glm::vec3 start{}, predictor{};
    double inverseMass = 0;
    PhysicsConstraintParticipant participant;
    participant.bodies = {handle};
    participant.maximumBodies = 1;
    participant.maximumContacts = 1;
    participant.maximumJointRows = 1;
    participant.propose = [&](const PhysicsConstraintBatch& batch, PhysicsConstraintProposal& proposal) {
        ++observed.proposals;
        // This is a one-boundary probe. Remaining native substeps are allowed
        // to finish normally without presenting un-reconciled fluid state.
        if (batch.context.index != 0) return false;
        checks.Expect(batch.context.count == kSubsteps && batch.context.deltaTime == kStep / kSubsteps &&
            batch.bodies.size() == 1 && batch.contacts.empty() && batch.joints.empty(),
            label + " receives one free body before the native pose advance");
        if (batch.bodies.size() != 1 || !batch.contacts.empty() || !batch.joints.empty())
            throw std::runtime_error("unexpected constraint rows in free-box probe");
        const auto& body = batch.bodies.front();
        start = body.center;
        predictor = body.linearVelocity;
        inverseMass = body.inverseMass;
        checks.Expect(body.handle.id == handle.id && body.handle.world == handle.world &&
            body.angularVelocity == glm::vec3(0) && body.orientation == glm::quat(1, 0, 0, 0),
            label + " sees the expected translation-only native participant");
        const glm::vec3 accelerationShare = experiment.gravity * (kStep / kSubsteps);
        checks.Expect(predictor == experiment.initialVelocity + accelerationShare,
            label + " native predictor already contains the chronological gravity share");
        if (!applyPressure) {
            observed.predictedVelocity = predictor;
            return false;
        }

        const double dt = batch.context.deltaTime;
        const glm::dvec3 nativeGravity = glm::dvec3(accelerationShare) / dt;
        const auto box = BoxAt(glm::dvec3(body.center) - glm::dvec3(experiment.chartOrigin));
        // The isolated pressure solver includes gravity in its own predictor.
        // Feed it the pre-gravity body velocity, then propose only its pressure
        // impulse to MK2, which already owns the real gravity integration.
        const CoupledBoxState state{box.owner, box.lo, box.hi,
            Array(glm::dvec3(predictor) - glm::dvec3(accelerationShare)), 1. / inverseMass};
        std::string error;
        if (!AdvanceTransportedVolumeBox(layout, state, originalFluid, dt, Array(nativeGravity),
                                          candidate, error)) {
            throw std::runtime_error("live material proposal could not solve: " + error);
        }
        observed.impulse = dt * Vector(candidate.pressureForce);
        const glm::dvec3 exactVelocity = glm::dvec3(predictor) + inverseMass * observed.impulse;
        observed.predictedVelocity = glm::vec3(exactVelocity);
        const double solveDifference = MaxAbs(exactVelocity - Vector(candidate.body.velocity));
        checks.Expect(solveDifference < 1e-7, label + " real pressure impulse reconstructs coupled velocity",
            solveDifference);
        checks.Expect(std::abs(candidate.pressureEnergyResidual) < 2e-7,
            label + " staged pressure projection passes work identity", candidate.pressureEnergyResidual);
        proposal.linearImpulses = {observed.impulse};
        proposal.angularImpulses = {glm::dvec3(0)};
        return true;
    };
    participant.published = [&](const PhysicsConstraintBatch::Context&) {
        // Acknowledge native acceptance only. The staged ideal geometry is
        // never published as actual material geometry without reconciliation.
        ++observed.publications;
    };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const PhysicsWorld::VelocityCoupling::Substep& context) {
        if (context.index != 0) return;
        ++observed.observations;
        observed.observedVelocity = world.GetLinearVelocity(handle);
        const auto transform = world.GetTransform(handle);
        observed.observedPosition = transform.position;
        const glm::vec3 expectedPosition = start + observed.predictedVelocity * context.deltaTime;
        checks.Expect(observed.observedVelocity == observed.predictedVelocity,
            label + " native accepts the pressure-only binary32 velocity");
        checks.Expect(observed.observedPosition == expectedPosition &&
            transform.rotation == glm::quat(1, 0, 0, 0) && world.GetAngularVelocity(handle) == glm::vec3(0),
            label + " native alone produces the represented pose");
        if (!applyPressure) return;

        const glm::dvec3 observedVelocity(observed.observedVelocity);
        const glm::dvec3 exactVelocity = glm::dvec3(predictor) + inverseMass * observed.impulse;
        const glm::dvec3 appliedImpulse = (observedVelocity - glm::dvec3(predictor)) / inverseMass;
        observed.impulseError = MaxAbs(appliedImpulse - observed.impulse);
        const double roundedImpulseError = MaxAbs(observedVelocity - exactVelocity) / inverseMass;
        checks.Expect(std::abs(observed.impulseError - roundedImpulseError) < 1e-12,
            label + " applied impulse differs only by native velocity representation", observed.impulseError);
        observed.velocityDifference = MaxAbs(observedVelocity - Vector(candidate.body.velocity));
        const auto actualCenter = glm::dvec3(transform.position) - glm::dvec3(experiment.chartOrigin);
        const auto predictedCenter = .5 * (Vector(candidate.body.lo) + Vector(candidate.body.hi));
        observed.positionDifference = MaxAbs(actualCenter - predictedCenter);
        observed.workDifference = .5 / inverseMass *
            (glm::dot(observedVelocity, observedVelocity) -
             glm::dot(Vector(candidate.body.velocity), Vector(candidate.body.velocity)));
        const auto actualGeometry = Geometry(layout, BoxAt(actualCenter));
        for (std::size_t cell = 0; cell < actualGeometry.startCapacity.size(); ++cell) {
            const double excess = candidate.fluid.state.water[cell] - actualGeometry.startCapacity[cell];
            observed.maximumCapacityMismatch = std::max(observed.maximumCapacityMismatch, std::abs(excess));
            observed.aboveCapacity += std::max(excess, 0.);
        }
        observed.waterMassDifference = 1000 * (Sum(candidate.fluid.state.water) - Sum(originalFluid.water));
    };
    world.Step(kStep, &coupling);
    checks.Expect(observed.proposals == kSubsteps && observed.observations == 1 &&
        observed.publications == (applyPressure ? 1u : 0u),
        label + " one material boundary and ordinary remaining native cadence");
    if (applyPressure) {
        std::cout << "NATIVE " << experiment.name
                  << " pressure_impulse_ns=" << MaxAbs(observed.impulse)
                  << " represented_impulse_error_ns=" << observed.impulseError
                  << " coupled_velocity_error_mps=" << observed.velocityDifference
                  << " coupled_pose_error_m=" << observed.positionDifference
                  << " native_vs_ideal_kinetic_difference_j=" << observed.workDifference
                  << " max_cell_capacity_mismatch_m3=" << observed.maximumCapacityMismatch
                  << " unreconciled_overfill_m3=" << observed.aboveCapacity
                  << " staged_total_water_mass_difference_kg=" << observed.waterMassDifference << '\n';
    }
    return observed;
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    Checks checks;
    try {
        for (const auto& experiment : {
                 Experiment{"local-moving", {}, {.75f, -.2f, .25f}, {}},
                 Experiment{"local-oblique-gravity", {}, {}, {1.25f, -8.f, 2.5f}},
                 Experiment{"coarse-native-coordinate", {65536.f, -65536.f, 131072.f},
                     {.75f, -.2f, .25f}, {}},
             }) {
            const auto coupled = Run(checks, experiment, true);
            const auto declined = Run(checks, experiment, false);
            checks.Expect(MaxAbs(glm::dvec3(coupled.observedVelocity) -
                glm::dvec3(declined.observedVelocity)) > 1e-4,
                std::string(experiment.name) + " real native motion distinguishes pressure from decline");
            checks.Expect(coupled.positionDifference > 0 && coupled.maximumCapacityMismatch > 0 &&
                coupled.aboveCapacity > 0 && std::abs(coupled.waterMassDifference) < 1e-8,
                std::string(experiment.name) + " exposes the staged-vs-actual volume mismatch without losing water",
                coupled.maximumCapacityMismatch);
        }
    } catch (const std::exception& error) {
        checks.Expect(false, std::string("native probe failure: ") + error.what());
    }
    std::cout << "SUMMARY checks=" << checks.total << " failures=" << checks.failures << '\n';
    return checks.failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
