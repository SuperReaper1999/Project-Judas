#include "BasinMk2.h"

#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using namespace judas::volume_grid;
using namespace judas::volume_grid::basin_probe;

bool Control(bool rotated, std::string_view name) {
    const auto fixture = Fixture::Authored(rotated);
    const auto parcels = fixture.Parcels();
    const double volume = double(parcels.size()) * fixture.spacing * fixture.spacing * fixture.spacing;
    unsigned commits = 0;
    bool initialBodyCorrect = true;
    const auto stage = [&](const Trial& trial, Proposal& next, std::string& error) {
        initialBodyCorrect &= trial.context.count == 4 &&
            std::abs(1. / trial.body.inverseMass - Fixture::BodyMass()) < 1e-5;
        if (name == "material-reject") {
            error = "deliberately rejected control";
            return false;
        }
        if (name == "floor-sweep")
            next.worldImpulse = glm::dvec3(fixture.rotation * glm::vec3(0, -100000, 0));
        else if (name == "rigid-impulse")
            next.worldImpulse = glm::dvec3(fixture.rotation * glm::vec3(0, 10, 0));
        next.forecast = trial.predict(next.worldImpulse);
        if (name == "bad-forecast")
            next.forecast.center.y += 1.f;
        next.commit = [&] { ++commits; };
        return true;
    };
    const unsigned warmup = name == "zero-impulse" ? 240 : 0;
    const auto result = RunMk2(fixture, warmup, 1, {}, stage);
    const auto localVelocity = glm::conjugate(fixture.rotation) * result.finalVelocity;
    const auto chartPoint = fixture.ChartPoint(result.finalPose.position);
    const bool accepted = name == "zero-impulse" || name == "rigid-impulse";
    bool valid = result.placeholderDestroyed && initialBodyCorrect && result.warmupSteps == warmup &&
        result.chartInitialError < 1e-7 && parcels.size() == (rotated ? 1152 : 592) &&
        (rotated || volume == 9.25);
    if (accepted) {
        const double expected = -double(gravity) * tick + (name == "rigid-impulse" ? 40. / Fixture::BodyMass() : 0.);
        valid &= result.error.empty() && result.proposals == 4 && result.published == 4 &&
            result.completedRigidSteps == 1 && result.accepted == 4 && commits == 4 &&
            std::abs(localVelocity.y - expected) < 2e-7;
    } else {
        valid &= !result.error.empty() && result.proposals == 1 && result.published == 0 &&
            result.accepted == 0 && commits == 0 && result.finalPose.position == fixture.NativeInitialCenter();
        if (name == "floor-sweep")
            valid &= result.error.find("sweep") != std::string::npos;
    }
    std::cout << (valid ? "PASS " : "FAIL ") << (rotated ? "rotated_half " : "half_025 ") << name
        << " parcels=" << parcels.size() << " seed_m3=" << volume << " warmup=" << result.warmupSteps
        << " proposed=" << result.proposals << " published=" << result.published << " accepted=" << result.accepted
        << " chart_y=" << chartPoint[1] << " chart_vy=" << localVelocity.y
        << " initial_chart_error_m=" << result.chartInitialError
        << " impulse_roundoff_Ns=" << result.maxNativeImpulseError << " reason=" << result.error << '\n';
    return valid;
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    unsigned checks = 0, failed = 0;
    for (bool rotated : {false, true}) {
        for (std::string_view name : {"zero-impulse", "rigid-impulse", "floor-sweep", "bad-forecast", "material-reject"}) {
            ++checks;
            failed += !Control(rotated, name);
        }
    }
    std::cout << "SUMMARY " << checks << " native wiring checks, " << failed << " failures\n";
    return failed ? 1 : 0;
}
