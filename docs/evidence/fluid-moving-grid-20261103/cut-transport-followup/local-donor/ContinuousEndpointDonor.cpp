#define main BasinSharedMain
#include "BasinSharedStep.cpp"
#undef main

#include "CutRegionMomentum.h"

namespace judas::volume_grid {
bool StageOnlyCutRegionRemap(const CutRegionChart&, const CutRegionGeometry&, const CutRegionState&,
                            CutRegionStep&, CutRegionPhaseInput&, std::string&);
}

namespace {

struct Evaluation {
    double y, residual;
    glm::dvec3 force;
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
    const double scale = 1. + glm::length(out.force);
    if (std::abs(out.force.x) > 1e-11 * scale || std::abs(out.force.z) > 1e-11 * scale ||
        glm::length(torque) > 1e-11 * scale) {
        error = "level symmetry control has resolved lateral force or pressure torque";
        return false;
    }
    const double velocity = double(trial.body.linearVelocity.y) +
        double(trial.body.inverseMass) * dt * out.force.y;
    out.y = y;
    out.residual = (double(trial.body.center.y) + dt * velocity) - y;
    std::cout << "ROOT y=" << out.y << " residual_m=" << out.residual
        << " force_N=" << out.force.y << " pressure_residual_m3=" << out.step.pressure.maximumBalanceError << '\n';
    return true;
}

double HalfUlp(float x) {
    const auto lower = std::nextafter(x, -std::numeric_limits<float>::infinity());
    const auto upper = std::nextafter(x, std::numeric_limits<float>::infinity());
    return .5 * std::max(double(x) - lower, double(upper) - x);
}

bool RemapPreview(const Backend& backend, const Trial& trial, const Evaluation& physical,
                  const Forecast& forecast, std::string& error) {
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
        // This diagnostic inspects the first unanchored MK2 interval only.
        const double bound = dt * HalfUlp(forecast.velocity[axis]) +
            HalfUlp(forecast.velocity[axis] * float(dt)) + HalfUlp(forecast.center[axis]) +
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
    std::cout << "REMAP_PREVIEW native_y=" << forecast.center.y << " dy_m=" << correction[1]
        << " swept_m3=" << volumeMoved
        << " force_times_rounding_J=" << physical.force.y * correction[1] << '\n';

    // The artificial pressure step owns no physical impulse or clock.
    // Retain only its accepted paired face transfers.
    CutRegionState still{physical.step.state.water, std::vector<GridVec>(chart.regions.size())};
    CutRegionStep transport;
    CutRegionPhaseInput input;
    if (!StageOnlyCutRegionRemap(chart, correctionGeometry, still, transport, input, error)) {
        error = "correction geometric phase solve: " + error;
        return false;
    }
    std::cout << "REMAP_PHASE water_before_m3=" << Total(physical.step.state.water)
        << " water_after_m3=" << Total(transport.state.water)
        << " phase_residual_m3=" << transport.phase.maximumWaterResidual
        << " mixture_residual_m3=" << transport.pressure.maximumBalanceError
        << " tolerance_m3=" << input.balanceTolerance << '\n';
    struct Outflow { double wet = 0., dry = 0., total = 0.; };
    std::vector<Outflow> flow(chart.regions.size());
    for (std::size_t f = 0; f < input.faces.size(); ++f) {
        const auto& edge = input.faces[f];
        auto& from = flow[edge.signedVolume < 0. ? edge.right : edge.left];
        from.wet += std::abs(transport.phase.waterFlux[f]);
        from.dry += std::abs(transport.phase.airFlux[f]);
        from.total += std::abs(edge.signedVolume);
    }
    for (std::size_t i = 0; i < flow.size(); ++i) {
        if (input.relayAllowed[i])
            continue;
        const double wet = input.startWater[i], cap = input.startCapacity[i], dry = cap - wet;
        const double threshold = 256. * std::numeric_limits<double>::epsilon() * cap;
        if (i == 2524) {
            std::cout << "REMAP_DONOR region=" << i << " cap_m3=" << cap << " water_m3=" << wet
                << " dry_m3=" << dry << " water_out_m3=" << flow[i].wet
                << " air_out_m3=" << flow[i].dry << " total_out_m3=" << flow[i].total
                << " ulp_bound_m3=" << threshold << '\n';
            break;
        }
    }
    const std::vector<GridVec> noImpulses(chart.regions.size());
    CutRegionMomentumResult momentum;
    if (!TransportCutRegionMomentum(input, transport.phase, physical.step.state.velocity, noImpulses,
                                    momentum, error)) {
        error = "correction carried physical momentum: " + error;
        return false;
    }
    std::cout << "REMAP_DONE water_before_m3=" << Total(physical.step.state.water)
        << " water_after_m3=" << Total(transport.state.water)
        << " phase_residual_m3=" << transport.phase.maximumWaterResidual
        << " pressure_residual_m3=" << transport.pressure.maximumBalanceError
        << " momentum_residual_kg_m_s=" << MaxAbs(momentum.momentumResidual)
        << " energy_residual_J=" << momentum.kineticResidual << '\n';
    return true;
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    const auto fixture = Fixture::Authored(false);
    Backend backend(fixture);
    std::string error;
    if (!backend.Prepare(SeedMode::HydrostaticControl, false, WallMode::Authored, error)) {
        std::cout << "REJECT setup " << error << '\n';
        return 1;
    }
    bool reachedRemap = false, remapPassed = false;
    const auto result = RunMk2(fixture, 0, 1, {},
        [&](const Trial& trial, Proposal&, std::string& reason) {
            Evaluation low, high;
            if (!Evaluate(backend, trial, 0.70007354021072388, low, reason) ||
                !Evaluate(backend, trial, 0.70007359981536865, high, reason))
                return false;
            if (!(low.residual > 0. && high.residual < 0.)) {
                reason = "retained adjacent endpoints no longer bracket the continuous root";
                return false;
            }
            Evaluation root;
            for (int i = 0; i < 8; ++i) {
                const double y = low.y + (high.y - low.y) * low.residual / (low.residual - high.residual);
                if (!Evaluate(backend, trial, y, root, reason))
                    return false;
                if (std::abs(root.residual) <= 4. * std::numeric_limits<double>::epsilon())
                    break;
                if (root.residual > 0.)
                    low = std::move(root);
                else
                    high = std::move(root);
            }
            if (std::abs(root.residual) > 4. * std::numeric_limits<double>::epsilon()) {
                reason = "continuous root did not meet binary64 position residual";
                return false;
            }
            const auto impulse = double(trial.context.deltaTime) * root.force;
            const auto forecast = trial.predict(impulse);
            std::cout << "NATIVE_FORECAST velocity_y=" << forecast.velocity.y << " y=" << forecast.center.y
                << " impulse_N_s=" << impulse.y << '\n';
            reachedRemap = true;
            remapPassed = RemapPreview(backend, trial, root, forecast, reason);
            if (remapPassed)
                reason = "isolated diagnostic completed without impulse publication";
            return false;
        });
    std::cout << "END remap_reached=" << reachedRemap << " remap_passed=" << remapPassed
        << " proposals=" << result.proposals << " published=" << result.published
        << " material_commits=" << backend.committed << " reason=" << result.error << '\n';
    return remapPassed && result.published == 0 && backend.committed == 0 ? 0 : 1;
}
