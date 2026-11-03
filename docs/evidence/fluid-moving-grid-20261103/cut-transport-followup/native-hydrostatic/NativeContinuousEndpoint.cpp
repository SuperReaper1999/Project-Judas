#define main BasinSharedMain
#include "BasinSharedStep.cpp"
#undef main

#include "CutRegionMomentum.h"

#include <deque>

namespace {

struct Evaluation {
    double y, residual;
    glm::dvec3 force;
    double torque = 0.;
    CutRegionStep step;
};

bool Evaluate(Backend& backend, const Trial& trial, double y, Evaluation& out, std::string& error) {
    const double dt = trial.context.deltaTime;
    const GridVec displacement{0., y - double(trial.body.center.y), 0.};
    out = {};
    if (!backend.Advance(trial.start, displacement, dt, out.step, error))
        return false;
    if (out.step.loads.size() != 1 || out.step.loads[0].owner != Fixture::bodyOwner) {
        error = "continuous solve lost the moving pressure owner";
        return false;
    }
    const auto& load = out.step.loads[0];
    out.force = Vector(load.force);
    const auto center = Vector(backend.fixture.ChartPoint(trial.body.center));
    const auto torque = Vector(load.torque) - glm::cross(center, out.force);
    out.torque = glm::length(torque);
    const double scale = 1. + glm::length(out.force);
    if (std::abs(out.force.x) > 1e-11 * scale || std::abs(out.force.z) > 1e-11 * scale ||
        out.torque > 1e-11 * scale) {
        std::cout << "UNSUPPORTED_REACTION substep=" << trial.context.index
            << " force_x_N=" << out.force.x << " force_y_N=" << out.force.y << " force_z_N=" << out.force.z
            << " torque_N_m=" << out.torque << " arithmetic_bound=" << 1e-11 * scale << '\n';
        error = "level symmetry control has resolved lateral force or pressure torque";
        return false;
    }
    const double velocity = double(trial.body.linearVelocity.y) +
        double(trial.body.inverseMass) * dt * out.force.y;
    out.y = y;
    out.residual = (double(trial.body.center.y) + dt * velocity) - y;
    std::cout << "ROOT substep=" << trial.context.index << " y=" << out.y << " residual_m=" << out.residual
        << " force_N=" << out.force.y << " torque_N_m=" << out.torque
        << " pressure_residual_m3=" << out.step.pressure.maximumBalanceError << '\n';
    return true;
}

double HalfUlp(float x) {
    const auto lower = std::nextafter(x, -std::numeric_limits<float>::infinity());
    const auto upper = std::nextafter(x, std::numeric_limits<float>::infinity());
    return .5 * std::max(double(x) - lower, double(upper) - x);
}

bool RouteWetGeometricRemap(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                            const CutRegionState& physical, CutRegionPhaseInput& input,
                            CutRegionPhaseResult& phase, std::string& error) {
    struct Arc { std::size_t neighbor, face; int sign; };
    const auto count = chart.regions.size();
    const auto faceCount = geometry.faces.size();
    const auto absent = std::numeric_limits<std::size_t>::max();
    std::vector<double> planes;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, 1, physical.water, planes, error))
        return false;
    std::vector<std::vector<Arc>> links(count);
    std::vector<double> faceStock(faceCount), signedFlux(faceCount);
    for (std::size_t i = 0; i < faceCount; ++i) {
        const auto& face = geometry.faces[i];
        if (!(face.startArea > 0. && face.endArea > 0. && face.meanArea > 0.))
            continue;
        const auto phases = MeasureCutRegionFacePhase(face, geometry.motion, 1,
                                                       planes[face.left], planes[face.right]);
        const auto stable = std::min({phases.wet[0].start, phases.wet[1].start,
                                      phases.wet[0].mean, phases.wet[1].mean});
        faceStock[i] = chart.layout.h * stable;
        if (faceStock[i] > 0.) {
            links[face.left].push_back({face.right, i, 1});
            links[face.right].push_back({face.left, i, -1});
        }
    }
    std::vector<double> lose(count), gain(count), used(count);
    for (std::size_t i = 0; i < count; ++i) {
        const double change = geometry.endCapacity[i] - geometry.startCapacity[i];
        lose[i] = std::max(0., -change);
        gain[i] = std::max(0., change);
    }
    std::size_t paths = 0, maxHops = 0;
    double totalMoved = 0.;
    const double h = chart.layout.h;
    const double roundoff = 256. * std::numeric_limits<double>::epsilon() * h * h * h;
    std::vector<std::size_t> previous(count), previousFace(count);
    std::vector<int> sign(count);
    for (std::size_t source = 0; source < count; ++source) {
        while (lose[source] > roundoff) {
            std::fill(previous.begin(), previous.end(), absent);
            previous[source] = source;
            std::deque<std::size_t> queue{source};
            auto sink = absent;
            while (!queue.empty()) {
                const auto cell = queue.front();
                queue.pop_front();
                if (gain[cell] > roundoff) {
                    sink = cell;
                    break;
                }
                if (!(physical.water[cell] > used[cell]))
                    continue;
                for (const auto arc : links[cell]) {
                    if (previous[arc.neighbor] != absent || !(faceStock[arc.face] > 0.))
                        continue;
                    previous[arc.neighbor] = cell;
                    previousFace[arc.neighbor] = arc.face;
                    sign[arc.neighbor] = arc.sign;
                    queue.push_back(arc.neighbor);
                }
            }
            if (sink == absent) {
                error = "geometry-only correction found no admitted wet path from region " + std::to_string(source);
                return false;
            }
            double amount = std::min(lose[source], gain[sink]);
            std::size_t hops = 0;
            for (auto node = sink; node != source; node = previous[node]) {
                const auto donor = previous[node];
                amount = std::min({amount, physical.water[donor] - used[donor],
                                   faceStock[previousFace[node]]});
                ++hops;
            }
            if (!(amount > 0.)) {
                error = "geometry-only correction exhausted a wet path";
                return false;
            }
            for (auto node = sink; node != source; node = previous[node]) {
                const auto face = previousFace[node];
                signedFlux[face] += sign[node] * amount;
                faceStock[face] -= amount;
                used[previous[node]] += amount;
            }
            lose[source] -= amount;
            gain[sink] -= amount;
            totalMoved += amount;
            maxHops = std::max(maxHops, hops);
            ++paths;
        }
    }
    if (*std::max_element(gain.begin(), gain.end()) > roundoff) {
        error = "geometry-only correction did not reach all changed region capacities";
        return false;
    }
    input.startCapacity = geometry.startCapacity;
    input.endCapacity = geometry.endCapacity;
    input.startWater = physical.water;
    input.balanceTolerance = roundoff;
    for (std::size_t i = 0; i < faceCount; ++i) {
        if (signedFlux[i] == 0.)
            continue;
        const auto& face = geometry.faces[i];
        const double wet = std::abs(signedFlux[i]);
        input.faces.push_back({face.left, face.right, signedFlux[i], wet, wet, wet});
    }
    std::cout << "GEOMETRIC_ROUTE paths=" << paths << " ports=" << input.faces.size() << " max_hops=" << maxHops
        << " volume_m3=" << totalMoved << '\n';
    return TransportCutRegionPhases(input, phase, error);
}

bool Remap(const Backend& backend, const Trial& trial, const Evaluation& physical,
           const Forecast& forecast, CutRegionState& corrected, CutRegionGeometry& endpointGeometry,
           double& pathWork, int remapMode, std::string& error) {
    const double dt = trial.context.deltaTime;
    const auto& chart = backend.chart;
    const double dy = physical.y - double(trial.body.center.y);
    GridVec origin{};
    for (int axis = 0; axis < 3; ++axis)
        origin[axis] = trial.start.lo[axis] - chart.moving->lo[axis];
    const GridVec idealDisplacement{0., dy, 0.};
    const auto ideal = Array(Vector(origin) + Vector(idealDisplacement));
    const auto native = Array(Vector(origin) + Vector(forecast.translation));
    const auto correction = Array(Vector(native) - Vector(ideal));
    const double arithmeticBudget = 16. * std::numeric_limits<double>::epsilon() * chart.layout.h;
    for (int axis = 0; axis < 3; ++axis) {
        const double carriedAnchorResidual = std::abs(double(forecast.anchor[axis]) - trial.body.center[axis] +
            double(forecast.velocity[axis]) * (double(forecast.duration) - dt));
        const double bound = carriedAnchorResidual + dt * HalfUlp(forecast.velocity[axis]) +
            HalfUlp(forecast.velocity[axis] * forecast.duration) + HalfUlp(forecast.center[axis]) +
            std::abs(physical.residual) + arithmeticBudget;
        if (!(std::abs(correction[axis]) <= bound &&
              bound <= 8. * std::numeric_limits<float>::epsilon() * chart.layout.h)) {
            error = "native correction exceeds representation budget";
            return false;
        }
    }
    CutRegionGeometry physicalGeometry, correctionGeometry, nativeGeometry;
    if (!BuildCutRegionGeometry(chart, origin, idealDisplacement, physicalGeometry, error) ||
        !BuildCutRegionGeometry(chart, ideal, correction, correctionGeometry, error) ||
        !BuildCutRegionGeometry(chart, native, {}, nativeGeometry, error))
        return false;
    if (correctionGeometry.startCapacity != physicalGeometry.endCapacity ||
        correctionGeometry.endCapacity != nativeGeometry.startCapacity) {
        error = "correction geometry did not reproduce canonical endpoint capacities exactly";
        return false;
    }
    double volumeMoved = 0.;
    for (std::size_t i = 0; i < chart.regions.size(); ++i)
        volumeMoved += .5 * std::abs(correctionGeometry.endCapacity[i] - correctionGeometry.startCapacity[i]);
    pathWork = physical.force.y * correction[1];
    std::cout << "REMAP_PREVIEW native_y=" << forecast.center.y << " dy_m=" << correction[1]
        << " swept_m3=" << volumeMoved
        << " force_times_rounding_J=" << pathWork << '\n';

    // The artificial pressure step owns no physical impulse or clock.
    // Retain only its accepted paired face transfers.
    if (remapMode != 0) {
        const auto& motion = *correctionGeometry.motion;
        const auto inCollar = [&](const CutRegion& region) {
            for (int axis = 0; axis < 3; ++axis) {
                const double lo = motion.lo[axis] + std::min(0., correction[axis]) - chart.layout.h;
                const double hi = motion.hi[axis] + std::max(0., correction[axis]) + chart.layout.h;
                if (region.hi[axis] <= lo || region.lo[axis] >= hi)
                    return false;
            }
            return true;
        };
        std::vector<bool> regionIncluded;
        regionIncluded.reserve(chart.regions.size());
        for (std::size_t i = 0; i < chart.regions.size(); ++i) {
            regionIncluded.push_back(inCollar(chart.regions[i]));
            if (!regionIncluded.back() && correctionGeometry.startCapacity[i] != correctionGeometry.endCapacity[i]) {
                error = "local remap excluded a changing volume";
                return false;
            }
        }
        std::size_t active = 0;
        for (auto& face : correctionGeometry.faces) {
            if (!regionIncluded[face.left] || !regionIncluded[face.right]) {
                if (face.sweptVolume != 0.) {
                    error = "local remap sealed an actually swept face";
                    return false;
                }
                face.startArea = face.meanArea = face.endArea = 0.;
            }
            active += face.meanArea > 0.;
        }
        std::cout << "REMAP_COLLAR admitted_faces=" << active << '\n';
    }
    CutRegionPhaseInput input;
    CutRegionPhaseResult phase;
    double correctionPressureResidual = 0.;
    if (remapMode == 2) {
        if (!RouteWetGeometricRemap(chart, correctionGeometry, physical.step.state, input, phase, error))
            return false;
    } else {
        CutRegionState still{physical.step.state.water, std::vector<GridVec>(chart.regions.size())};
        CutRegionStep transport;
        if (!AdvanceCutRegionStep(chart, correctionGeometry, still, 1., {1, 0., 0.}, transport, error)) {
            error = "correction geometric phase solve: " + error;
            return false;
        }
        input.startCapacity = correctionGeometry.startCapacity;
        input.endCapacity = correctionGeometry.endCapacity;
        input.startWater = still.water;
        input.relayAllowed.resize(chart.regions.size());
        std::vector<double> outgoing(chart.regions.size());
        const double volume = chart.layout.h * chart.layout.h * chart.layout.h;
        input.balanceTolerance = 256. * std::numeric_limits<double>::epsilon() * volume;
        for (std::size_t i = 0; i < transport.activeFaces.size(); ++i) {
            const auto& face = correctionGeometry.faces[transport.activeFaces[i]];
            const double flux = transport.pressure.volumeFlux[i];
            const auto donor = flux < 0. ? face.right : face.left;
            const double wet = std::abs(transport.phase.waterFlux[i]);
            input.faces.push_back({face.left, face.right, flux, wet, wet, wet});
            outgoing[donor] += std::abs(flux);
        }
        for (std::size_t i = 0; i < chart.regions.size(); ++i) {
            const double cap = input.startCapacity[i];
            input.relayAllowed[i] = cap == 0. ||
                outgoing[i] - cap > 256. * std::numeric_limits<double>::epsilon() * cap;
        }
        phase = std::move(transport.phase);
        correctionPressureResidual = transport.pressure.maximumBalanceError;
    }
    const std::vector<GridVec> noImpulses(chart.regions.size());
    CutRegionMomentumResult momentum;
    if (!TransportCutRegionMomentum(input, phase, physical.step.state.velocity, noImpulses,
                                    momentum, error)) {
        error = "correction carried physical momentum: " + error;
        return false;
    }
    std::cout << "REMAP_DONE water_before_m3=" << Total(physical.step.state.water)
        << " water_after_m3=" << Total(phase.endWater)
        << " phase_residual_m3=" << phase.maximumWaterResidual
        << " pressure_residual_m3=" << correctionPressureResidual
        << " momentum_residual_kg_m_s=" << MaxAbs(momentum.momentumResidual)
        << " energy_residual_J=" << momentum.kineticResidual
        << " remap_kinetic_delta_J=" << momentum.afterTransportKineticEnergy - momentum.beforeKineticEnergy
        << " accounted_mixing_loss_J=" << momentum.mixingLoss << '\n';
    if (momentum.afterTransportKineticEnergy - momentum.beforeKineticEnergy >
        256. * std::numeric_limits<double>::epsilon() * (1. + momentum.beforeKineticEnergy)) {
        error = "rounding remap created kinetic energy";
        return false;
    }
    corrected = {std::move(phase.endWater), std::move(momentum.velocity)};
    endpointGeometry = std::move(nativeGeometry);
    return true;
}

bool SolveRoot(Backend& backend, const Trial& trial, bool first, Evaluation& root, std::string& error) {
    Evaluation low, high;
    if (first) {
        if (!Evaluate(backend, trial, 0.70007354021072388, low, error) ||
            !Evaluate(backend, trial, 0.70007359981536865, high, error))
            return false;
    } else {
        const double y = double(trial.body.center.y) +
            double(trial.context.deltaTime) * double(trial.body.linearVelocity.y);
        Evaluation p, q;
        if (!Evaluate(backend, trial, y, p, error))
            return false;
        if (std::abs(p.residual) <= 4. * std::numeric_limits<double>::epsilon()) {
            root = std::move(p);
            return true;
        }
        if (!Evaluate(backend, trial, y + p.residual, q, error))
            return false;
        if (p.y < q.y) {
            low = std::move(p);
            high = std::move(q);
        } else {
            low = std::move(q);
            high = std::move(p);
        }
    }
    if (!(low.residual > 0. && high.residual < 0.)) {
        error = "candidate endpoints did not bracket the continuous root";
        return false;
    }
    for (int i = 0; i < 8; ++i) {
        const double y = low.y + (high.y - low.y) * low.residual / (low.residual - high.residual);
        if (!Evaluate(backend, trial, y, root, error))
            return false;
        if (std::abs(root.residual) <= 4. * std::numeric_limits<double>::epsilon())
            return true;
        if (root.residual > 0.)
            low = std::move(root);
        else
            high = std::move(root);
    }
    error = "continuous root did not meet binary64 position residual";
    return false;
}

long double Moment(const CutRegion& region, const std::optional<BoxMotion>& motion,
                   double limit) {
    const double high = std::min(region.hi[1], limit);
    if (!(high > region.lo[1]))
        return 0.;
    const auto slab = [](long double dx, long double dz, long double lo, long double hi) {
        return dx * dz * (hi - lo) * .5L * (hi + lo);
    };
    const auto full = slab(region.hi[0] - region.lo[0], region.hi[2] - region.lo[2],
                          region.lo[1], high);
    if (!motion)
        return full;
    const auto cut = [&](int axis, double lo, double hi) {
        return std::max(0., std::min(hi, motion->hi[axis]) - std::max(lo, motion->lo[axis]));
    };
    const auto insideLow = std::max(region.lo[1], motion->lo[1]);
    const auto insideHigh = std::min(high, motion->hi[1]);
    if (!(insideHigh > insideLow))
        return full;
    return full - slab(cut(0, region.lo[0], region.hi[0]), cut(2, region.lo[2], region.hi[2]),
                       insideLow, insideHigh);
}

bool FluidEnergy(const CutRegionChart& chart, const CutRegionGeometry& geometry,
                 const CutRegionState& state, double g, long double& output, std::string& error) {
    std::vector<double> planes;
    if (!ReconstructCutRegionWaterPlanes(chart, geometry, 1, state.water, planes, error))
        return false;
    output = 0.;
    for (std::size_t i = 0; i < chart.regions.size(); ++i) {
        const auto& region = chart.regions[i];
        const long double mass = geometry.startCapacity[i] + 999.L * state.water[i];
        const long double moment = Moment(region, geometry.motion, region.hi[1]) +
            999.L * Moment(region, geometry.motion, planes[i]);
        const auto v = Vector(state.velocity[i]);
        output += .5L * mass * glm::dot(v, v) + g * moment;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    int remapMode = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view option(argv[i]);
        if (option == "--local-remap")
            remapMode = 1;
        else if (option == "--geometry-remap")
            remapMode = 2;
        else if (option == "--geometry-after-first")
            remapMode = 3;
        else
            return 2;
    }
    std::cout << std::setprecision(17);
    const auto fixture = Fixture::Authored(false);
    Backend backend(fixture);
    std::string error;
    if (!backend.Prepare(SeedMode::HydrostaticControl, false, WallMode::Authored, error)) {
        std::cout << "REJECT setup " << error << '\n';
        return 1;
    }
    glm::vec3 lastVelocity(0.);
    double greatestGain = 0.;
    const auto result = RunMk2(fixture, 0, 1, {},
        [&](const Trial& trial, Proposal& pending, std::string& reason) {
            Evaluation root;
            if (!SolveRoot(backend, trial, backend.committed == 0, root, reason))
                return false;
            const auto impulse = double(trial.context.deltaTime) * root.force;
            const auto forecast = trial.predict(impulse);
            const auto staged = std::make_shared<CutRegionState>();
            CutRegionGeometry endpoint;
            double pathWork = 0.;
            const int mode = remapMode == 3 ? (backend.committed == 0 ? 0 : 2) : remapMode;
            if (!Remap(backend, trial, root, forecast, *staged, endpoint, pathWork, mode, reason))
                return false;

            GridVec start{};
            for (int axis = 0; axis < 3; ++axis)
                start[axis] = trial.start.lo[axis] - backend.chart.moving->lo[axis];
            CutRegionGeometry initial;
            long double before, after;
            const double g = -backend.settings.gravity;
            if (!BuildCutRegionGeometry(backend.chart, start, {}, initial, reason) ||
                !FluidEnergy(backend.chart, initial, backend.state, g, before, reason) ||
                !FluidEnergy(backend.chart, endpoint, *staged, g, after, reason))
                return false;
            const double mass = 1. / trial.body.inverseMass;
            const double deltaBody = .5 * mass * (glm::dot(glm::dvec3(forecast.velocity), glm::dvec3(forecast.velocity)) -
                glm::dot(glm::dvec3(lastVelocity), glm::dvec3(lastVelocity))) +
                mass * g * (double(forecast.center.y) - trial.body.center.y);
            const double deltaEnergy = deltaBody + double(after - before);
            const double kineticRoundoff = 256. * std::numeric_limits<double>::epsilon() *
                (1. + std::abs(before) + std::abs(after));
            const double displacedPotentialRoundoff = 1000. * 8. * double(bodyHalf.x) * bodyHalf.y * bodyHalf.z *
                g * std::abs(double(forecast.center.y) - root.y);
            const double energyBound = std::abs(pathWork) + displacedPotentialRoundoff + kineticRoundoff;
            greatestGain = std::max(greatestGain, deltaEnergy);
            std::cout << "ENERGY substep=" << trial.context.index << " body_J=" << deltaBody
                << " fluid_J=" << double(after - before) << " total_J=" << deltaEnergy
                << " native_rounding_bound_J=" << energyBound
                << " pressure_work_residual_J=" << root.step.pressureWorkResidual
                << " carried_kinetic_residual_J=" << root.step.transportKineticResidual << '\n';
            if (deltaEnergy > energyBound) {
                reason = "closed-domain mechanical energy gain exceeds measured representation work";
                return false;
            }
            pending.worldImpulse = impulse;
            pending.forecast = forecast;
            pending.commit = [&backend, staged, &lastVelocity, forecast] {
                backend.Commit(std::move(*staged));
                lastVelocity = forecast.velocity;
                std::cout << "NATIVE_VERIFIED y=" << forecast.center.y << " vy=" << forecast.velocity.y
                    << " water_m3=" << Total(backend.state.water) << '\n';
            };
            return true;
        });
    std::cout << "END remap_mode=" << remapMode << " proposals=" << result.proposals
        << " published=" << result.published << " accepted=" << result.accepted
        << " native_ticks=" << result.completedRigidSteps << " material_commits=" << backend.committed
        << " water_drift_m3=" << backend.maxWaterDrift << " max_energy_gain_J=" << greatestGain
        << " angular_speed=" << glm::length(result.finalAngularVelocity)
        << " max_impulse_error_N_s=" << result.maxNativeImpulseError << " reason=" << result.error << '\n';
    return result.completedRigidSteps == 1 && result.accepted == 4 && backend.committed == 4 ? 0 : 1;
}
