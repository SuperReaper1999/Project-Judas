#include "CoupledVolumeBox.h"
#include "GridPressureReaction.h"
#include "MovingVolumePressureInternal.h"
#include "PhysicsConstraintBatch.h"
#include "PhysicsWorld.h"

#include <algorithm>
#include <array>
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
#include <vector>

namespace {

using namespace judas::volume_grid;

constexpr float kMacroStep = 1.f / 64;
constexpr glm::vec3 kExtent{.15625f, .171875f, .1875f};
constexpr glm::vec3 kCenter{.75f, .75f, .75f};
constexpr glm::vec3 kGravity{1.25f, -8.f, 2.5f};
constexpr GridLayout kLayout{{8, 7, 7}, .25};
constexpr double kDisplacedMass = 8000. * double(kExtent.x) * kExtent.y * kExtent.z;

GridVec Array(glm::dvec3 v) {
    return {v.x, v.y, v.z};
}

glm::dvec3 Vector(const GridVec& v) {
    return {v[0], v[1], v[2]};
}

double Maximum(glm::dvec3 v) {
    return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)});
}

double Sum(const std::vector<double>& field) {
    return std::accumulate(field.begin(), field.end(), 0.);
}

BoxMotion BoxAt(glm::dvec3 chartCenter) {
    return {27, Array(chartCenter - glm::dvec3(kExtent)),
        Array(chartCenter + glm::dvec3(kExtent)), {}};
}

MovingGridGeometry Build(const BoxMotion& box) {
    MovingGridGeometry geometry;
    std::string error;
    if (!BuildMovingGridGeometry(kLayout, {box}, geometry, error))
        throw std::runtime_error(error);
    return geometry;
}

MovingVolumeState Still(const std::vector<double>& water) {
    MovingVolumeState state;
    state.water = water;
    for (int axis = 0; axis < 3; ++axis) state.velocity[axis].resize(kLayout.FaceCount(axis));
    return state;
}

double HalfUlp(float value) {
    const float previous = std::nextafter(value, -std::numeric_limits<float>::infinity());
    const float next = std::nextafter(value, std::numeric_limits<float>::infinity());
    return .5 * std::max(double(value) - previous, double(next) - value);
}

struct Forecast {
    glm::dvec3 impulse{};
    glm::vec3 velocity{}, endpoint{};
};

Forecast Predict(glm::vec3 start, glm::vec3 velocity, double inverseMass,
                 glm::dvec3 impulse, float dt) {
    const glm::vec3 rounded(glm::dvec3(velocity) + inverseMass * impulse);
    return {impulse, rounded, start + rounded * dt};
}

struct Boundary {
    glm::vec3 origin{}, center{}, velocity{};
    double inverseMass = 0;
    float dt = 0;
};

struct CoarseChart {};

glm::dvec3 Chart(const Boundary& b, glm::vec3 worldPoint) {
    return glm::dvec3(worldPoint) - glm::dvec3(b.origin);
}

CoupledBoxStep Ideal(const Boundary& b, const MovingVolumeState& fluid) {
    const auto box = BoxAt(Chart(b, b.center));
    // MK2 already applied gravity for this interval. Reconstruct the
    // pre-kick input of the isolated solver without double-applying it.
    const glm::dvec3 preGravity = glm::dvec3(b.velocity) - double(b.dt) * glm::dvec3(kGravity);
    const CoupledBoxState body{box.owner, box.lo, box.hi, Array(preGravity), 1. / b.inverseMass};
    CoupledBoxStep step;
    std::string error;
    if (!AdvanceTransportedVolumeBox(kLayout, body, fluid, b.dt, Array(glm::dvec3(kGravity)),
                                     step, error)) {
        throw std::runtime_error("unrepresented coupled solve failed " + error);
    }
    return step;
}

double RepresentationBound(const Boundary& b, const CoupledBoxStep& ideal, const Forecast& forecast) {
    const auto center = .5 * (Vector(ideal.body.lo) + Vector(ideal.body.hi));
    const auto represented = Chart(b, forecast.endpoint);
    const auto floatStep = forecast.velocity * b.dt;
    double maximum = 0;
    for (int axis = 0; axis < 3; ++axis) {
        // Velocity representation, the native float multiply, the float
        // position add, and the measured continuous displacement residual.
        const double bound = double(b.dt) * HalfUlp(forecast.velocity[axis]) +
            HalfUlp(floatStep[axis]) + HalfUlp(forecast.endpoint[axis]) +
            ideal.rigidResidual + 16 * std::numeric_limits<double>::epsilon() * kLayout.h;
        if (!std::isfinite(bound) || std::abs(represented[axis] - center[axis]) > bound)
            throw std::runtime_error("native endpoint error is larger than its representation budget");
        maximum = std::max(maximum, bound);
    }
    return maximum;
}

double ValidateChart(const Boundary& b, const CoupledBoxStep& ideal, const Forecast& forecast) {
    const double bound = RepresentationBound(b, ideal, forecast);
    // Admit a geometry repair only at the scale of a few binary32 operations
    // on a cell width. This rejects large absolute coordinate quantization.
    if (bound > 8 * std::numeric_limits<float>::epsilon() * kLayout.h)
        throw CoarseChart{};
    return bound;
}

double SetFilled(const MovingGridGeometry& geometry, MovingVolumeStep& step) {
    const double tolerance = step.residual +
        512 * std::numeric_limits<double>::epsilon() * kLayout.h * kLayout.h * kLayout.h;
    double correction = 0;
    for (std::size_t cell = 0; cell < geometry.endCapacity.size(); ++cell) {
        const double error = std::abs(step.state.water[cell] - geometry.endCapacity[cell]);
        if (error > tolerance)
            throw std::runtime_error("the full-phase transport exceeds roundoff");
        correction += error;
    }
    step.waterRoundoffCorrection += correction;
    step.state.water = geometry.endCapacity;
    return correction;
}

MovingVolumeState Transport(const MovingGridGeometry& geometry, const MovingVolumeState& before,
                            MovingVolumeStep& step, double& momentumError, double& wallImpulse) {
    MovingVolumeMomentumStep advected;
    std::string error;
    if (!AdvectMovingVolumeMomentum(kLayout, geometry, before, step, advected, error))
        throw std::runtime_error("reconciled dual momentum failed " + error);
    momentumError = Maximum(Vector(advected.afterMomentum) - Vector(advected.beforeMomentum));
    MovingVolumeState result{step.state.water, std::move(advected.velocity)};
    for (int axis = 0; axis < 3; ++axis) {
        auto limits = kLayout.cells;
        ++limits[axis];
        double impulse = 0;
        for (int k = 0; k < limits[2]; ++k) {
            for (int j = 0; j < limits[1]; ++j) {
                for (int i = 0; i < limits[0]; ++i) {
                    std::array<int, 3> cell{i, j, k};
                    if (cell[axis] != 0 && cell[axis] != kLayout.cells[axis]) continue;
                    const auto face = kLayout.Face(axis, i, j, k);
                    if (cell[axis] != 0) --cell[axis];
                    const double halfMass = 500 * geometry.endCapacity[kLayout.Cell(cell[0], cell[1], cell[2])];
                    impulse -= halfMass * result.velocity[axis][face];
                    result.velocity[axis][face] = 0;
                }
            }
        }
        wallImpulse = std::max(wallImpulse, std::abs(impulse));
    }
    return result;
}

struct Staged {
    Forecast native;
    MovingVolumeState fluid;
    double representationBound = 0, volumeMoved = 0, roundoff = 0;
    double momentumError = 0, wallImpulse = 0;
    double pressureWorkDifference = 0, workLedgerError = 0;
    unsigned geometryEvaluations = 0;
};

struct RepresentedCycle {};

void MeasureWork(const Boundary& b, const glm::dvec3& pressurePath,
                 const MovingVolumeStep& pressure, Staged& stage) {
    const glm::dvec3 predictor(b.velocity), velocity(stage.native.velocity);
    const glm::dvec3 delta = velocity - predictor;
    const glm::dvec3 actualPath = Chart(b, stage.native.endpoint) - Chart(b, b.center);
    const glm::dvec3 gravity(kGravity);
    const double bodyPressureWork = .5 / b.inverseMass *
        (glm::dot(velocity, velocity) - glm::dot(predictor, predictor) + glm::dot(delta, delta));
    const double fluidPressureWork = pressure.pressureKineticWork + pressure.pressureCorrectionEnergy;
    const double potential = kDisplacedMass * glm::dot(gravity, actualPath);
    const double sum = bodyPressureWork + fluidPressureWork + potential;
    // The ideal pressure path and the binary32 rigid velocity/pose each
    // round separately. Carry those differences explicitly in the ledger.
    const double impulseRounding = glm::dot(velocity, delta / b.inverseMass - stage.native.impulse);
    const double pathRounding = glm::dot(stage.native.impulse / double(b.dt),
        double(b.dt) * velocity - pressurePath);
    const double displacedPotentialRounding = kDisplacedMass *
        glm::dot(gravity, actualPath - pressurePath);
    const double accounted = impulseRounding + pathRounding + displacedPotentialRounding;
    stage.pressureWorkDifference = std::abs(sum);
    stage.workLedgerError = std::abs(sum - accounted);
}

Staged ExactPath(const Boundary& b, const MovingVolumeState& fluid, const CoupledBoxStep& ideal) {
    Staged staged;
    const auto start = Chart(b, b.center);
    staged.native = Predict(b.center, b.velocity, b.inverseMass,
        double(b.dt) * Vector(ideal.pressureForce), b.dt);
    staged.representationBound = ValidateChart(b, ideal, staged.native);
    std::vector<glm::vec3> tested;
    for (unsigned iteration = 0; iteration < 12; ++iteration) {
        const auto endpoint = staged.native.endpoint;
        if (std::find(tested.begin(), tested.end(), endpoint) != tested.end())
            throw RepresentedCycle{};
        tested.push_back(endpoint);
        auto box = BoxAt(start);
        box.translation = Array(Chart(b, endpoint) - start);
        const auto geometry = Build(box);
        MovingVolumeStep pressure;
        std::string error;
        if (!detail::AdvanceVolumeWithDualMass(kLayout, geometry, fluid, b.dt, pressure, error))
            throw std::runtime_error("represented pressure solve failed " + error);
        const auto loads = AccumulateGridPressureReaction(geometry.averagePatches, pressure.pressure);
        if (loads.size() != 1 || loads.front().owner != box.owner)
            throw std::runtime_error("represented pressure lost the box owner");
        const auto force = Vector(loads.front().force) - kDisplacedMass * glm::dvec3(kGravity);
        staged.native = Predict(b.center, b.velocity, b.inverseMass, double(b.dt) * force, b.dt);
        if (staged.native.endpoint != endpoint) continue;

        const auto observedCapacity = Build(BoxAt(Chart(b, endpoint))).startCapacity;
        if (geometry.endCapacity != observedCapacity)
            throw std::runtime_error("represented pressure geometry does not equal endpoint reconstruction");
        MeasureWork(b, Chart(b, endpoint) - start, pressure, staged);
        staged.roundoff = SetFilled(geometry, pressure);
        staged.fluid = Transport(geometry, fluid, pressure, staged.momentumError, staged.wallImpulse);
        staged.geometryEvaluations = iteration + 1;
        return staged;
    }
    throw RepresentedCycle{};
}

Staged Reconcile(const Boundary& b, const CoupledBoxStep& ideal) {
    Staged staged;
    staged.native = Predict(b.center, b.velocity, b.inverseMass,
        double(b.dt) * Vector(ideal.pressureForce), b.dt);
    staged.representationBound = ValidateChart(b, ideal, staged.native);
    const auto idealCenter = .5 * (Vector(ideal.body.lo) + Vector(ideal.body.hi));
    const auto represented = Chart(b, staged.native.endpoint);
    MeasureWork(b, idealCenter - Chart(b, b.center), ideal.fluid, staged);
    const BoxMotion box{ideal.body.owner, ideal.body.lo, ideal.body.hi, Array(represented - idealCenter)};
    const auto geometry = Build(box);
    const auto canonicalEndpoint = Build(BoxAt(represented));
    const double tolerance = 512 * std::numeric_limits<double>::epsilon() *
        kLayout.h * kLayout.h * kLayout.h;
    auto before = Still(geometry.startCapacity);
    for (std::size_t cell = 0; cell < before.water.size(); ++cell) {
        const double inputRoundoff = std::abs(ideal.fluid.state.water[cell] - before.water[cell]);
        const double outputRoundoff = std::abs(geometry.endCapacity[cell] - canonicalEndpoint.startCapacity[cell]);
        if (inputRoundoff > tolerance || outputRoundoff > tolerance)
            throw std::runtime_error("binary64 remap bounds did not match canonical endpoint");
        staged.roundoff += inputRoundoff + outputRoundoff;
        staged.volumeMoved += .5 * std::abs(geometry.endCapacity[cell] - before.water[cell]);
    }
    MovingVolumeStep remap;
    std::string error;
    // The correction has no time interval or applied force. The unit
    // bookkeeping clock constructs a shared geometrically conservative flux;
    // its artificial pressure and velocities are discarded.
    if (!AdvanceMovingVolume(kLayout, geometry, before, 1., {}, remap, error))
        throw std::runtime_error("rounding-only volume remap failed " + error);
    staged.roundoff += SetFilled(geometry, remap);
    remap.state.velocity = ideal.fluid.state.velocity;
    staged.fluid = Transport(geometry, before, remap, staged.momentumError, staged.wallImpulse);
    staged.fluid.water = canonicalEndpoint.startCapacity;
    staged.geometryEvaluations = 1;
    return staged;
}

struct Result {
    unsigned accepted = 0, observed = 0, maximumEvaluations = 0;
    bool cycle = false, rejectedCoarse = false;
    double massError = 0, volumeMoved = 0, bound = 0, roundoff = 0;
    double momentumError = 0, wallImpulse = 0;
    double pressureWorkDifference = 0, workLedgerError = 0;
};

Result Run(std::string_view label, bool exact, glm::vec3 origin) {
    PhysicsWorld world;
    if (!world.Init()) throw std::runtime_error("MK2 init failed");
    const auto handle = world.CreateDynamicBox(origin + kCenter, kExtent, 64, 0, 0);
    world.SetLinearVelocity(handle, {.5f, -.125f, .25f});
    auto fluid = Still(Build(BoxAt(glm::dvec3(kCenter))).startCapacity);
    const double initial = Sum(fluid.water);
    Staged pending;
    Result result;
    PhysicsConstraintParticipant participant;
    participant.bodies = {handle};
    participant.maximumBodies = 1;
    participant.maximumContacts = 1;
    participant.maximumJointRows = 1;
    participant.propose = [&](const PhysicsConstraintBatch& batch, PhysicsConstraintProposal& proposal) {
        if (batch.bodies.size() != 1 || !batch.contacts.empty() || !batch.joints.empty())
            throw std::runtime_error("unexpected native rows in sealed free-box probe");
        const auto& body = batch.bodies.front();
        if (body.angularVelocity != glm::vec3(0) || body.orientation != glm::quat(1, 0, 0, 0))
            throw std::runtime_error("this bridge admits chart-aligned, nonspinning boxes only");
        const Boundary b{origin, body.center, body.linearVelocity, body.inverseMass, batch.context.deltaTime};
        if (fluid.water != Build(BoxAt(Chart(b, b.center))).startCapacity)
            throw std::runtime_error("material state is not the actual starting geometry");
        const auto ideal = Ideal(b, fluid);
        pending = exact ? ExactPath(b, fluid, ideal) : Reconcile(b, ideal);
        proposal.linearImpulses = {pending.native.impulse};
        proposal.angularImpulses = {glm::dvec3(0)};
        return true;
    };
    participant.published = [&](const auto&) { ++result.accepted; };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    coupling.afterSubstep = [&](const PhysicsWorld::VelocityCoupling::Substep&) {
        const auto transform = world.GetTransform(handle);
        if (transform.position != pending.native.endpoint ||
            world.GetLinearVelocity(handle) != pending.native.velocity)
            throw std::runtime_error("authoritative native pose/velocity differed from staged representation");
        const auto actual = Build(BoxAt(glm::dvec3(transform.position) - glm::dvec3(origin)));
        if (pending.fluid.water != actual.startCapacity)
            throw std::runtime_error("prepared fluid does not match actual native occupied geometry");
        std::swap(fluid, pending.fluid);
        ++result.observed;
        result.massError = std::max(result.massError, 1000 * std::abs(Sum(fluid.water) - initial));
        result.maximumEvaluations = std::max(result.maximumEvaluations, pending.geometryEvaluations);
        result.bound = std::max(result.bound, pending.representationBound);
        result.volumeMoved = std::max(result.volumeMoved, pending.volumeMoved);
        result.roundoff = std::max(result.roundoff, pending.roundoff);
        result.momentumError = std::max(result.momentumError, pending.momentumError);
        result.wallImpulse = std::max(result.wallImpulse, pending.wallImpulse);
        result.pressureWorkDifference = std::max(result.pressureWorkDifference, pending.pressureWorkDifference);
        result.workLedgerError = std::max(result.workLedgerError, pending.workLedgerError);
    };

    try {
        for (int step = 0; step < 4; ++step) {
            world.ApplyLinearAcceleration(handle, kGravity, kMacroStep);
            world.Step(kMacroStep, &coupling);
        }
    } catch (const RepresentedCycle&) {
        result.cycle = true;
    } catch (const CoarseChart&) {
        result.rejectedCoarse = true;
    }
    std::cout << "NATIVE " << label << " accepted=" << result.accepted << " verified=" << result.observed
              << " cycle=" << result.cycle << " coarse_rejected=" << result.rejectedCoarse
              << " max_geometry_evaluations=" << result.maximumEvaluations
              << " representation_bound_m=" << result.bound << " max_remapped_volume_m3=" << result.volumeMoved
              << " max_mass_error_kg=" << result.massError << " max_accounted_roundoff_m3=" << result.roundoff
              << " max_momentum_error_ns=" << result.momentumError << " max_wall_impulse_ns=" << result.wallImpulse
              << " max_native_pressure_work_difference_j=" << result.pressureWorkDifference
              << " max_work_error_after_rounding_ledger_j=" << result.workLedgerError
              << '\n';
    return result;
}

bool VerifyNativeAnchor() {
    PhysicsWorld world;
    if (!world.Init()) throw std::runtime_error("MK2 init failed");
    const auto body = world.CreateDynamicBox(kCenter, kExtent, 64, 0, 0);
    constexpr float speed = .001f;
    constexpr float step = kMacroStep / 4.f;
    world.SetLinearVelocity(body, {speed, 0, 0});
    PhysicsConstraintParticipant participant;
    participant.bodies = {body};
    participant.propose = [](const auto& batch, auto& proposal) {
        proposal.linearImpulses.assign(batch.bodies.size(), glm::dvec3(0));
        proposal.angularImpulses.assign(batch.bodies.size(), glm::dvec3(0));
        return true;
    };
    PhysicsWorld::VelocityCoupling coupling;
    coupling.physicalBatch = &participant;
    std::array<float, 2> observed{};
    coupling.afterSubstep = [&](const auto& context) {
        if (context.index < observed.size()) observed[context.index] = world.GetTransform(body).position.x;
    };
    world.Step(kMacroStep, &coupling);
    const float naive = observed[0] + speed * step;
    const float anchored = kCenter.x + speed * (2 * step);
    std::cout << "NATIVE unchanged-velocity-anchor actual_second=" << observed[1]
              << " naive_stepwise=" << naive << " native_anchor=" << anchored
              << " discrepancy_m=" << double(naive) - anchored << '\n';
    return observed[1] == anchored && observed[1] != naive;
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    try {
        const auto exact = Run("represented-fixed-point", true, {});
        const auto remap = Run("rounding-only-remap", false, {});
        const auto far = Run("coarse-chart", false, {65536.f, -65536.f, 131072.f});
        const std::array checks{
            exact.accepted == 16 && exact.observed == 16 && !exact.cycle,
            exact.massError < 1e-8 && exact.momentumError < 2e-9 && exact.workLedgerError < 2e-9,
            remap.accepted == 16 && remap.observed == 16 && remap.volumeMoved > 0,
            remap.massError < 1e-8 && remap.momentumError < 2e-9 && remap.workLedgerError < 2e-9,
            far.rejectedCoarse && far.accepted == 0 && far.observed == 0,
            VerifyNativeAnchor(),
        };
        const auto failures = std::count(checks.begin(), checks.end(), false);
        std::cout << "SUMMARY checks=" << checks.size() << " failures=" << failures << '\n';
        return failures ? EXIT_FAILURE : EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
