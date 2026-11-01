#pragma once

#include "FluidSolidSupport.h"
#include "FluidSurfaceReconstruction.h"
#include <limits>

// Included by FluidHydrostaticsTests after its assertion helpers. These are
// mathematical controls for the reconstruction, separate from settling tests.
void SurfaceReconstructionChecks() {
    using namespace FluidHydrostaticDetail;
    std::vector<SurfaceDatum> data;
    for (int z = -2; z <= 2; ++z) for (int x = -2; x <= 2; ++x) {
        const double px = x*.2, pz = z*.2;
        data.push_back({px, pz, .07+.2*px-.3*pz+.5*px*px-.2*px*pz-.4*pz*pz, 1});
    }
    const auto patch = FitCurvedSurface(data, 1, .25);
    Check(patch.has_value(), "quadratic patch has redundant full-rank data");
    if (patch) {
        Near(patch->height, .07, 1e-13, "analytic quadratic evaluated at query origin");
        Near(patch->confidence, 1, 1e-12, "well-conditioned exact patch retains confidence");
        Near(patch->residual, 0, 1e-13, "quadratic residual agrees with analytic data");
    }
    auto scaled = data;
    for (auto& d : scaled) { d.x *= 10; d.z *= 10; }
    const auto large = FitCurvedSurface(scaled, 10, .25);
    Check(large.has_value(), "normalized basis handles coordinate scaling");
    if (patch && large) Near(large->height, patch->height, 1e-14, "scaled patch keeps physical height");
    for (double weight : {0., 1e-20, 1e-12}) {
        auto donor = data;
        donor.push_back({.7, .8, .6, weight});
        const auto weak = FitCurvedSurface(donor, 1, .25, 1e-5);
        Check(weak.has_value(), "vanishing donor does not invalidate a good patch");
        if (weak) Near(weak->height, .07, 1e-9, "vanishing donor cannot move fitted cap finitely");
    }
    auto noisy = data;
    for (std::size_t i = 0; i < noisy.size(); ++i) noisy[i].height += i%2 ? .5 : -.5;
    const auto rejected = FitCurvedSurface(noisy, 1, .25);
    Check(!rejected || rejected->confidence == 0, "unresolved nonquadratic data falls back");
    std::vector<SurfaceDatum> deficient;
    for (int z : {-1, 1}) for (int x = -3; x <= 3; ++x)
        deficient.push_back({x*.2, double(z), .1, 1});
    const auto missing = FitCurvedSurface(deficient, 1, .25, 1e-5);
    Check(!missing || missing->confidence == 0, "missing quadratic mode is not reconstructed");
    for (double weight : {1e-24, 1e-18, 1e-12}) {
        std::vector<SurfaceDatum> weak;
        weak.reserve(deficient.size()+1);
        for (const auto& datum : deficient) weak.push_back(datum);
        weak.push_back({0, 0, 1, weight});
        const auto result = FitCurvedSurface(weak, 1, .25, 1e-5);
        Check(!result || result->confidence == 0,
              "tiny donor completing a missing mode propagates uncertainty");
    }
    auto invalid = data;
    invalid[0].height = std::numeric_limits<double>::quiet_NaN();
    Check(!FitCurvedSurface(invalid, 1, .25), "NaN surface data rejected");
    invalid = data;
    invalid[0].weight = -1;
    Check(!FitCurvedSurface(invalid, 1, .25), "negative fit weight rejected");
    Check(!FitCurvedSurface(data, 1, .25, -1), "negative input uncertainty rejected");
    Check(!FitCurvedSurface(data, std::numeric_limits<double>::infinity(), .25), "infinite fit scale rejected");

    const auto box = MakeBoxFluidVolume(glm::vec3(.3f));
    const FluidSolidQuery solid{box, {}};
    PreparedSolidQuery square({1,0,0}, {0,1,0}, {0,0,1}, 1, 0, solid);
    Near(square.ExteriorFraction({0,0,0}), .64, 5e-8, "box clips square area, not entire column");
    Near(square.ExteriorFraction({.8,0,0}), 1, 5e-8, "separate column remains fully exterior");
    const auto sphere = MakeSphereFluidVolume(1);
    const FluidSolidQuery round{sphere, {}};
    PreparedSolidQuery circle({1,0,0}, {0,1,0}, {0,0,1}, 2, 0, round);
    constexpr double pi = 3.14159265358979323846;
    Near(circle.ExteriorFraction({0,0,0}), 1-pi/4, 1e-12, "circle inside square has analytic area");
    Near(circle.ExteriorFraction({1,0,0}), 1-pi/8, 1e-12, "half circle has analytic clipped area");
    Near(circle.ExteriorFraction({0,1,0}), 1, 0, "sphere tangent plane excludes no area");
    Near(circle.ExteriorFraction({3,0,0}), 1, 1e-12, "separate sphere column stays exterior");
    Near(circle.ExteriorFraction({0,.6,0}), 1-pi*.64/4, 1e-12, "sphere slice uses local circular radius");
    const auto turn = glm::angleAxis(.37f, glm::normalize(glm::vec3(1,2,3)));
    BodyTransform pose; pose.rotation = turn;
    const FluidSolidQuery rotated{sphere, pose};
    PreparedSolidQuery covariant(turn*glm::vec3(1,0,0), turn*glm::vec3(0,1,0),
                                turn*glm::vec3(0,0,1), 2, 0, rotated);
    Near(covariant.ExteriorFraction(turn*glm::vec3(1,0,0)), 1-pi/8, 2e-7,
         "circle clipping follows the supplied rotated frame");
}

void CurvedPhysicalReconstructionChecks() {
    constexpr float half = .3f;
    const auto volume = MakeBoxFluidVolume(glm::vec3(half));
    const auto turn = glm::angleAxis(.63f, glm::normalize(glm::vec3(1,-2,3)));
    for (float sign : {-1.f,1.f}) for (float spacing : {.25f,.125f}) {
        // Exact mean of sign*.5*(x*x+z*z) over this square is sign*half^2/3.
        const float y = sign*half*half/3;
        for (const auto& q : {glm::quat(1,0,0,0), turn}) {
            const glm::vec3 translation(1,-2,3);
            std::vector<FluidParticle> particles;
            const int n = static_cast<int>(std::lround(2/spacing));
            for (int z = 0; z < n; ++z) for (int x = 0; x < n; ++x) for (int row = 0; row < n; ++row) {
                const float px = -1+(x+.5f)*spacing, pz = -1+(z+.5f)*spacing;
                const glm::vec3 local(px, sign*.5f*(px*px+pz*pz)-(row+.5f)*spacing, pz);
                if (glm::all(glm::lessThan(glm::abs(local-glm::vec3(0,y,0)),glm::vec3(half)))) continue;
                const auto position = translation+q*local;
                particles.push_back({position,position,glm::vec3(0),1000*spacing*spacing*spacing,glm::vec3(0)});
            }
            FluidHydrostaticField field;
            field.Build(particles,1000);
            BodyTransform pose; pose.position = translation+q*glm::vec3(0,y,0); pose.rotation = q;
            const Uniform gravity(q*glm::vec3(0,-9.81f,0));
            const auto result = EvaluateFluidHydrostatics(field,volume,pose,gravity);
            Near(result.fraction,.5,.10,"known curved surface recovers half-wet displaced box");
            Near(glm::length(result.fluidVelocity),0,0,"surface fit invents no fluid motion");
        }
    }
}
