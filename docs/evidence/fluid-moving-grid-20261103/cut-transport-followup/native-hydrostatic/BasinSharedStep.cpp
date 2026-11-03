#include "BasinMk2.h"
#include "CutRegionStep.h"

#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <string_view>

namespace {

using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;

double Total(const std::vector<double>& water) {
    return static_cast<double>(std::accumulate(water.begin(), water.end(), 0.L));
}

double MaxAbs(const GridVec& x) {
    return std::max({std::abs(x[0]), std::abs(x[1]), std::abs(x[2])});
}

bool ReadCount(std::string_view text, unsigned& n) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), n);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

enum class SeedMode { Authored, Dry, HydrostaticControl };

struct Backend {
    const Fixture& fixture;
    CutRegionChart chart;
    CutRegionState state;
    CutRegionSettings settings;
    SeedImport seedImport;
    double initialVolume = 0.;
    double maxWaterDrift = 0.;
    unsigned attempted = 0, committed = 0;
    bool traceProposals = false, bracketControl = false;

    explicit Backend(const Fixture& f) : fixture(f) {}

    bool Prepare(SeedMode seedMode, bool conservativeSeed, WallMode wallMode, std::string& error) {
        const auto box = fixture.InitialBox();
        // This bounded probe owns the near-release trajectory only. Grow a
        // future chart by conservative remap before leaving the envelope.
        const GridVec minimum{-.005, -.015, -.005};
        const GridVec maximum{.005, .025, .005};
        CutRegionGeometry geometry;
        if (!PrepareCutRegionChart(fixture.layout, fixture.Walls(wallMode), box, minimum, maximum, chart, error) ||
            !BuildCutRegionGeometry(chart, {}, geometry, error))
            return false;
        settings.upAxis = 1;
        settings.gravity = -double(gravity);
        const double observedSurface = fixture.spacing == .25f ? 1.7094379663467407 : 1.79405;
        settings.referenceHeight = seedMode == SeedMode::Dry ? 0. : observedSurface - fixture.origin[1];
        if (seedMode == SeedMode::Dry) {
            state.water.resize(chart.regions.size());
        } else if (seedMode == SeedMode::HydrostaticControl) {
            state.water.resize(chart.regions.size());
            for (std::size_t i = 0; i < chart.regions.size(); ++i) {
                const auto& region = chart.regions[i];
                if (region.hi[1] <= settings.referenceHeight)
                    state.water[i] = geometry.startCapacity[i];
                else if (region.lo[1] < settings.referenceHeight) {
                    auto upper = region.hi;
                    upper[1] = settings.referenceHeight;
                    state.water[i] = CutRegionClippedVolume(region, geometry.motion, region.lo, upper);
                }
            }
        } else if (!WaterByRegion(fixture, chart, geometry, conservativeSeed, state.water, seedImport, error)) {
            return false;
        }
        state.velocity.resize(chart.regions.size());
        initialVolume = Total(state.water);
        return true;
    }

    bool Advance(const BoxMotion& start, const GridVec& displacement, double dt,
                 CutRegionStep& next, std::string& error) {
        ++attempted;
        GridVec offset{};
        for (int a = 0; a < 3; ++a)
            offset[a] = start.lo[a] - chart.moving->lo[a];
        CutRegionGeometry geometry;
        return BuildCutRegionGeometry(chart, offset, displacement, geometry, error) &&
            AdvanceCutRegionStep(chart, geometry, state, dt, settings, next, error);
    }

    void Commit(CutRegionState next) {
        maxWaterDrift = std::max(maxWaterDrift, std::abs(Total(next.water) - initialVolume));
        state = std::move(next);
        ++committed;
    }

    bool AdvanceWarmup(const Warmup& request, std::string& error) {
        CutRegionStep next;
        if (!Advance(request.body, {}, request.dt, next, error))
            return false;
        Commit(std::move(next.state));
        return true;
    }

    bool Propose(const Trial& trial, Proposal& proposal, std::string& error) {
        auto forecast = trial.predict(glm::dvec3(0));
        const auto rotation = glm::normalize(glm::dquat(fixture.rotation));
        // Search the actual representable endpoint. The probe deliberately
        // publishes neither a mismatched pressure path nor an approximate
        // fallback. A repeated endpoint needs a separate conservative remap.
        std::vector<glm::vec3> seen;
        std::optional<float> lower, upper;
        for (unsigned iteration = 0; iteration < 16; ++iteration) {
            if (std::find(seen.begin(), seen.end(), forecast.center) != seen.end()) {
                error = "native pressure forecast repeated a binary32 endpoint";
                return false;
            }
            seen.push_back(forecast.center);
            auto next = std::make_shared<CutRegionStep>();
            if (!Advance(trial.start, forecast.translation, trial.context.deltaTime, *next, error))
                return false;
            if (next->loads.size() != 1 || next->loads.front().owner != Fixture::bodyOwner) {
                error = "cut-region basin did not recover the moving pressure owner";
                return false;
            }
            const auto& load = next->loads.front();
            const GridVec chartCenter = fixture.ChartPoint(trial.body.center);
            const glm::dvec3 aboutCenter = Vector(load.torque) - glm::cross(Vector(chartCenter), Vector(load.force));
            // The shared cut-region geometry models only linear box motion.
            // Any resolved angular impulse needs an oriented geometry solve.
            const double forceScale = 1. + MaxAbs(load.force);
            if (glm::length(aboutCenter) > 1e-11 * forceScale) {
                error = "native basin proposal requires resolved pressure torque (" +
                    std::to_string(glm::length(aboutCenter)) + " N m)";
                return false;
            }
            const auto impulse = (rotation * Vector(load.force)) * double(trial.context.deltaTime);
            const auto newForecast = trial.predict(impulse);
            if (traceProposals) {
                const auto p = forecast.center;
                const auto q = newForecast.center;
                std::cout << "PROPOSAL substep=" << trial.context.index << " iteration=" << iteration
                    << " p=(" << p.x << "," << p.y << "," << p.z << ")"
                    << " q=(" << q.x << "," << q.y << "," << q.z << ")"
                    << " force=(" << load.force[0] << "," << load.force[1] << "," << load.force[2] << ")"
                    << " balance_m3=" << next->pressure.maximumBalanceError << '\n';
            }
            if (newForecast.center == forecast.center ||
                (bracketControl && newForecast.translation == forecast.translation)) {
                proposal.worldImpulse = impulse;
                proposal.forecast = newForecast;
                proposal.commit = [this, next] { Commit(std::move(next->state)); };
                return true;
            }
            if (bracketControl) {
                if (newForecast.translation[0] != forecast.translation[0] ||
                    newForecast.translation[2] != forecast.translation[2]) {
                    error = "one-axis control found a resolved lateral displacement";
                    return false;
                }
                if (newForecast.center.y > forecast.center.y)
                    lower = forecast.center.y;
                else
                    upper = forecast.center.y;
                if (lower && upper) {
                    const float y = static_cast<float>(.5 * (double(*lower) + double(*upper)));
                    if (!(y > *lower && y < *upper)) {
                        error = "opposite signs on adjacent native y endpoints with no exact chart fixed point";
                        return false;
                    }
                    forecast = newForecast;
                    forecast.center.y = y;
                    forecast.translation = Array(Vector(fixture.ChartPoint(forecast.center)) - Vector(chartCenter));
                    continue;
                }
            }
            forecast = newForecast;
        }
        error = "native basin pressure forecast exhausted its 16 endpoint trials";
        return false;
    }
};

int SweepInitialTimestep(Backend& backend) {
    const auto initial = backend.state.water;
    for (int exponent = 0; exponent <= 6; ++exponent) {
        const auto begin = std::chrono::steady_clock::now();
        const double dt = std::ldexp(double(tick), -exponent);
        CutRegionStep proposed;
        std::string error;
        const bool accepted = backend.Advance(backend.fixture.InitialBox(), {}, dt, proposed, error);
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        std::cout << "TIMESTEP reciprocal=" << (60 << exponent) << " dt=" << dt
            << " accepted=" << accepted << " ms=" << elapsed;
        if (accepted) {
            double maximumSpeed = 0.;
            for (const auto& velocity : proposed.state.velocity)
                maximumSpeed = std::max(maximumSpeed, glm::length(Vector(velocity)));
            std::cout << " endpoint_m3=" << Total(proposed.state.water)
                << " max_speed_m_s=" << maximumSpeed
                << " max_pressure_residual_m3=" << proposed.pressure.maximumBalanceError;
        } else {
            std::cout << " reason=" << error;
        }
        std::cout << '\n';
    }
    const bool isolated = backend.state.water == initial && backend.committed == 0;
    std::cout << "TIMESTEP isolated=" << isolated << " material_commits=" << backend.committed << '\n';
    return isolated ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    bool rotated = false, conservativeSeed = false, timestepSweep = false;
    bool traceProposals = false, bracketControl = false;
    SeedMode seedMode = SeedMode::Authored;
    WallMode wallMode = WallMode::Authored;
    unsigned warmup = 240, steps = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--rotated")
            rotated = true;
        else if (arg == "--dry-control")
            seedMode = SeedMode::Dry;
        else if (arg == "--equilibrium-control")
            seedMode = SeedMode::HydrostaticControl;
        else if (arg == "--rounded-boundary-control")
            wallMode = WallMode::RoundedControl;
        else if (arg == "--native-wall-pose-control")
            wallMode = WallMode::NativePoseControl;
        else if (arg == "--conservative-seed")
            conservativeSeed = true;
        else if (arg == "--timestep-sweep")
            timestepSweep = true;
        else if (arg == "--trace-proposals")
            traceProposals = true;
        else if (arg == "--bracket-control")
            bracketControl = true;
        else if (arg == "--warmup" && i + 1 < argc) {
            if (!ReadCount(argv[++i], warmup))
                return 2;
        } else if (arg == "--steps" && i + 1 < argc) {
            if (!ReadCount(argv[++i], steps))
                return 2;
        } else {
            return 2;
        }
    }
    if (bracketControl && (rotated || seedMode != SeedMode::HydrostaticControl))
        return 2;
    const auto fixture = Fixture::Authored(rotated);
    Backend backend(fixture);
    backend.traceProposals = traceProposals;
    backend.bracketControl = bracketControl;
    std::string error;
    std::cout << std::setprecision(17);
    if (!backend.Prepare(seedMode, conservativeSeed, wallMode, error)) {
        std::cout << "REJECT seed " << error << '\n';
        return 1;
    }
    std::cout << "BEGIN " << (rotated ? "rotated_half" : "half_025") << " seed_mode=" << int(seedMode) << " wall_mode=" << int(wallMode) << " regions=" << backend.chart.regions.size()
        << " faces=" << backend.chart.faces.size() << " water_m3=" << backend.initialVolume
        << " seed_shifted_m3=" << backend.seedImport.shiftedVolume
        << " seed_shifted_parcels=" << backend.seedImport.affectedParcels
        << " seed_maximum_hops=" << backend.seedImport.maximumHops
        << " warmup_requested=" << warmup << " native_steps_requested=" << steps << '\n';
    if (timestepSweep)
        return SweepInitialTimestep(backend);
    const auto result = RunMk2(fixture, warmup, steps,
        [&](const Warmup& request, std::string& why) { return backend.AdvanceWarmup(request, why); },
        [&](const Trial& request, Proposal& proposal, std::string& why) {
            return backend.Propose(request, proposal, why);
        });
    const auto finalLocalVelocity = glm::conjugate(fixture.rotation) * result.finalVelocity;
    std::cout << "END final_vy=" << finalLocalVelocity.y << " warmup_steps=" << result.warmupSteps << " material_attempts=" << backend.attempted
        << " material_commits=" << backend.committed << " proposals=" << result.proposals
        << " impulses_published=" << result.published << " native_accepted=" << result.accepted
        << " native_steps=" << result.completedRigidSteps << " water_drift_m3=" << backend.maxWaterDrift
        << " impulse_roundoff_Ns=" << result.maxNativeImpulseError << " reason=" << result.error << '\n';
    return result.error.empty() && result.completedRigidSteps == steps ? 0 : 1;
}
