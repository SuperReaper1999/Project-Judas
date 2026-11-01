// A deliberately strict, known-failing diagnostic while Advanced Fluids are
// locked. Compare the production sampler AND its original measuring tool with
// declared analytic water geometry. No simulation or equilibrium is assumed.
#include "FluidHydrostatics.h"
#include "FluidImmersionObservation.h"
#include "GravityField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace {
constexpr double fractionTolerance = .10; // Existing production comparison scale.
constexpr double frameTolerance = .001;
constexpr double pi = 3.14159265358979323846;

struct Uniform final : GravityField {
    glm::vec3 acceleration;
    explicit Uniform(glm::vec3 value) : acceleration(value) {}
    glm::vec3 Sample(const glm::vec3&) const override { return acceleration; }
};
struct Frame {
    glm::quat rotation{1,0,0,0};
    glm::vec3 translation{0};
    glm::vec3 Point(glm::vec3 point) const { return translation+rotation*point; }
};
enum class Surface { Flat, Affine, Bowl, Dome, Saddle, Wave };
struct Definition { const char* name; Surface surface; };
constexpr std::array<Definition, 6> definitions{{
    {"flat", Surface::Flat}, {"affine", Surface::Affine},
    {"bowl", Surface::Bowl}, {"dome", Surface::Dome},
    {"saddle", Surface::Saddle}, {"wave", Surface::Wave}
}};

double Height(Surface surface, double x, double z) {
    switch (surface) {
    case Surface::Flat: return 0;
    case Surface::Affine: return .2*x+.1*z;
    case Surface::Bowl: return .5*(x*x+z*z);
    case Surface::Dome: return -.5*(x*x+z*z);
    case Surface::Saddle: return .5*(x*x-z*z);
    case Surface::Wave: return .1*std::sin(pi*x)*std::cos(pi*z);
    }
    throw std::invalid_argument("Unknown analytic surface");
}
// Exact average of h over the centered rectangular footprint, independent of
// particles, the runtime query, the remote observer, mass and density.
double MeanHeight(Surface surface, const glm::vec3& half) {
    const double x2 = double(half.x)*half.x/3;
    const double z2 = double(half.z)*half.z/3;
    switch (surface) {
    case Surface::Bowl: return .5*(x2+z2);
    case Surface::Dome: return -.5*(x2+z2);
    case Surface::Saddle: return .5*(x2-z2);
    default: return 0; // Affine and sine terms integrate to zero by symmetry.
    }
}
// All partial-case surface heights lie strictly between the box bottom/top,
// making this exact volume integration, rather than a density-derived target.
double ExpectedFraction(const Definition& definition, const glm::vec3& half, float y) {
    if (y > .5f) return 0; // Analytic local h is in [-.1,.1].
    if (y < -.5f) return 1; // Local water extends well below this box.
    return .5+(MeanHeight(definition.surface, half)-y)/(2*double(half.y));
}
double IndependentlyIntegratedFraction(const Definition& definition, const glm::vec3& half, float y) {
    // A second geometric route: integrate exact vertical box/heightfield ray
    // intersections. It never reads the particles or the analytic-mean formula.
    constexpr int n=128;
    double fraction=0;
    for (int z=0; z<n; ++z) for (int x=0; x<n; ++x) {
        const double px=-half.x+(x+.5)*2*half.x/n;
        const double pz=-half.z+(z+.5)*2*half.z/n;
        fraction+=std::clamp((Height(definition.surface,px,pz)-y+half.y)/(2*half.y),0.,1.);
    }
    return fraction/(n*n);
}
std::vector<FluidParticle> MakeParticles(const Definition& definition, float spacing,
                                       bool displaced, glm::vec3 localCenter,
                                       const glm::vec3& half, const Frame& frame) {
    const int n = static_cast<int>(std::round(2/spacing));
    std::vector<FluidParticle> particles;
    particles.reserve(static_cast<std::size_t>(n)*n*n);
    for (int z=0; z<n; ++z) for (int x=0; x<n; ++x) for (int y=0; y<n; ++y) {
        const float px = -1+(x+.5f)*spacing;
        const float pz = -1+(z+.5f)*spacing;
        const glm::vec3 local(px, static_cast<float>(Height(definition.surface,px,pz))-(y+.5f)*spacing, pz);
        if (displaced && glm::all(glm::lessThan(glm::abs(local-localCenter), half))) continue;
        FluidParticle particle;
        particle.position = frame.Point(local);
        particle.previousPosition = particle.position;
        particle.mass = 1000*spacing*spacing*spacing;
        particles.push_back(particle);
    }
    return particles;
}
struct Measurement { double engine, observer; };
Measurement Measure(const Definition& definition, float spacing, bool displaced,
                    float y, const Frame& frame, std::size_t& count) {
    const glm::vec3 half(.3f), localCenter(0,y,0);
    const auto particles = MakeParticles(definition,spacing,displaced,localCenter,half,frame);
    count = particles.size();
    FluidHydrostaticField field;
    field.Build(particles,1000);
    BodyTransform pose;
    pose.position = frame.Point(localCenter);
    pose.rotation = frame.rotation;
    const Uniform gravity(frame.rotation*glm::vec3(0,-9.81f,0));
    const auto hydro = EvaluateFluidHydrostatics(field,MakeBoxFluidVolume(half),pose,gravity,
        displaced ? FluidVolumeGeometry::DisplacedSolid : FluidVolumeGeometry::NonDisplacingVolume);
    // The ORIGINAL observer assumes the reservoir origin is world zero. Give
    // it that authored frame; do not mistake an unsupported translated input
    // for a failed measurement. The engine still sees the translated world.
    auto observerParticles=particles;
    for (auto& particle : observerParticles) particle.position-=frame.translation;
    auto observerPose=pose;
    observerPose.position-=frame.translation;
    const auto observed = FluidTest::ObservedSurface(observerParticles,frame.rotation,localCenter,half,
                                                    spacing,static_cast<int>(std::round(2/spacing)));
    return {hydro.fraction, observed ? FluidTest::BoxBelowObservedPlane(observerPose,half,frame.rotation,*observed) : -1};
}
} // namespace

int main(int argc, char** argv) {
    std::optional<std::filesystem::path> output;
    std::string selected;
    for (int i=1; i<argc; ++i) {
        const std::string arg(argv[i]);
        if (arg=="--output" && i+1<argc) output = argv[++i];
        else if (arg=="--case" && i+1<argc) selected = argv[++i];
        else { std::cerr<<"Use --output CSV and/or --case flat|affine|bowl|dome|saddle|wave\n"; return 2; }
    }
    if (!selected.empty() && std::none_of(definitions.begin(),definitions.end(),
        [&](const auto& d) { return selected==d.name; })) {
        std::cerr<<"Unknown analytic surface: "<<selected<<'\n'; return 2;
    }
    std::ofstream file;
    if (output) {
        std::error_code error;
        if (!output->parent_path().empty()) std::filesystem::create_directories(output->parent_path(),error);
        if (error) { std::cerr<<"Cannot create diagnostic output directory: "<<error.message()<<'\n'; return 2; }
        file.open(*output);
        if (!file) { std::cerr<<"Cannot write diagnostic CSV\n"; return 2; }
    }
    std::ostream& csv = output ? file : std::cout;
    csv<<std::setprecision(17)<<"case,spacing,height_case,displaced,frame,expected,engine,observer,particles\n";
    unsigned cases=0, engineFailures=0, observerFailures=0, frameFailures=0, observerFrameFailures=0;
    unsigned referenceChecks=0, referenceFailures=0;
    double engineWorst=0, observerWorst=0, frameWorst=0, observerFrameWorst=0, referenceWorst=0;
    const Frame identity;
    Frame rotated;
    rotated.rotation=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
    rotated.translation={2,-1,3};
    try {
        for (float spacing : {.25f,.125f,.0625f}) for (const auto& definition : definitions) {
            if (!selected.empty() && selected!=definition.name) continue;
            const float mean=static_cast<float>(MeanHeight(definition.surface,glm::vec3(.3f)));
            const std::array<float,5> heights{{mean,mean+.12f,mean-.12f,-.75f,.75f}};
            for (std::size_t h=0; h<heights.size(); ++h) for (bool displaced : {false,true}) {
                const double reference=IndependentlyIntegratedFraction(definition,glm::vec3(.3f),heights[h]);
                const double expected=ExpectedFraction(definition,glm::vec3(.3f),heights[h]);
                const double referenceError=std::abs(reference-expected);
                ++referenceChecks;
                referenceWorst=std::max(referenceWorst,referenceError);
                if (!std::isfinite(reference) || referenceError>1e-5) ++referenceFailures;
                std::array<Measurement,2> results{};
                for (int frame=0; frame<2; ++frame) {
                    std::size_t count=0;
                    const auto result=Measure(definition,spacing,displaced,heights[h],frame ? rotated : identity,count);
                    results[frame]=result;
                    const double engineError=std::abs(result.engine-expected);
                    const double observerError=std::abs(result.observer-expected);
                    ++cases;
                    if (!std::isfinite(result.engine) || engineError>fractionTolerance) ++engineFailures;
                    if (!std::isfinite(result.observer) || observerError>fractionTolerance) ++observerFailures;
                    engineWorst=std::max(engineWorst,engineError);
                    observerWorst=std::max(observerWorst,observerError);
                    csv<<definition.name<<','<<spacing<<','<<h<<','<<displaced<<','<<frame<<','<<expected
                       <<','<<result.engine<<','<<result.observer<<','<<count<<'\n';
                }
                const double difference=std::abs(results[0].engine-results[1].engine);
                frameWorst=std::max(frameWorst,difference);
                if (!std::isfinite(difference) || difference>frameTolerance) ++frameFailures;
                const double observerDifference=std::abs(results[0].observer-results[1].observer);
                observerFrameWorst=std::max(observerFrameWorst,observerDifference);
                if (!std::isfinite(observerDifference) || observerDifference>frameTolerance) ++observerFrameFailures;
            }
        }
    } catch (const std::exception& error) {
        std::cerr<<"Known-geometry diagnostic failed: "<<error.what()<<'\n'; return 2;
    }
    if (output) file.flush();
    if (output && !file) { std::cerr<<"Failed writing diagnostic CSV\n"; return 2; }
    std::cout<<std::setprecision(17)<<"SUMMARY cases="<<cases<<" engine_failures="<<engineFailures
             <<" observer_failures="<<observerFailures<<" frame_failures="<<frameFailures
             <<" observer_frame_failures="<<observerFrameFailures
             <<" reference_checks="<<referenceChecks<<" reference_failures="<<referenceFailures
             <<" engine_worst_error="<<engineWorst<<" observer_worst_error="<<observerWorst
             <<" frame_worst_difference="<<frameWorst<<" observer_frame_worst_difference="<<observerFrameWorst
             <<" reference_worst_error="<<referenceWorst<<'\n';
    // Known failures stay nonzero. Do not wrap the entire comparison in an
    // expected-failure assertion and silently turn absent physics into a pass.
    return engineFailures || observerFailures || frameFailures || observerFrameFailures || referenceFailures ? 1 : 0;
}
