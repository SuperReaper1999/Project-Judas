#include "FluidHydrostatics.h"
#include "FluidColumnLayers.h"
#include "GravityField.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void Near(double actual, double expected, double tolerance, const std::string& name) {
    ++checks;
    if (std::isfinite(actual) && std::abs(actual - expected) <= tolerance) return;
    ++failures;
    std::cout << "FAIL " << name << " actual=" << actual << " expected=" << expected << '\n';
}
struct Uniform final : GravityField {
    glm::vec3 gravity;
    explicit Uniform(glm::vec3 value) : gravity(value) {}
    glm::vec3 Sample(const glm::vec3&) const override { return gravity; }
};
struct Frame {
    glm::quat rotation{1, 0, 0, 0};
    glm::vec3 offset{0};
    glm::vec3 Point(glm::vec3 p) const { return offset + rotation * p; }
    glm::vec3 Vector(glm::vec3 p) const { return rotation * p; }
};
std::vector<FluidParticle> Layers(const Frame& frame, bool enclosed = false) {
    std::vector<FluidParticle> particles;
    for (int x = -8; x <= 8; ++x) for (int z = -8; z <= 8; ++z) {
        for (int y = -4; y <= 3; ++y) {
            const float height = .25f * (y + .5f);
            if (std::abs(height) < .5f && !(enclosed && (std::abs(x) >= 3 || std::abs(z) >= 3))) continue;
            const auto position = frame.Point({.25f * x, height, .25f * z});
            const auto velocity = frame.Vector({height < 0 ? .4f : -.6f, 0, 0});
            particles.push_back({position, position, velocity, 15.625f, glm::vec3(0)});
        }
    }
    return particles;
}
void LayerQueries(const Frame& frame) {
    FluidHydrostaticField field;
    field.Build(Layers(frame), 1000);
    const auto up = frame.Vector({0, 1, 0}), tangent = frame.Vector({1, 0, 0});
    // Deliberately large donor radius: both strata enter the search. A search
    // radius must not become occupied water between their actual surfaces.
    for (bool connected : {false, true}) {
        for (float height : {-.25f, 0.f, .25f}) {
            const auto query = field.Query(frame.Point({0, height, 0}), up, .1f, 1, tangent, nullptr, connected);
            Near(query.fraction, 0, 0, "resolved vertical air stays dry");
            Near(glm::length(query.velocity), 0, 0, "dry query publishes no borrowed layer velocity");
        }
        const auto partial = field.Query(frame.Point({0, 0, 0}), up, .75f, 1, tangent, nullptr, connected);
        // [-.75,.75] intersects [-1,-.5] and [.5,1]: .5 / 1.5.
        Near(partial.fraction, 1. / 3., .002, "two layers add occupied lengths without filling gap");
        const auto bottom = field.Query(frame.Point({0, -.75f, 0}), up, .1f, .1f, tangent, nullptr, connected);
        Near(bottom.fraction, 1, .002, "lower layer remains wet");
        Near(glm::length(bottom.velocity - frame.Vector({.4f, 0, 0})), 0, .0001,
             "resolved layer keeps its own motion");
    }
    for (int step = -24; step <= 24; ++step) {
        const double y = step * .05;
        double expected = 0;
        for (const auto& span : {std::pair<double, double>{-1, -.5}, {.5, 1}})
            expected += std::max(0.0, std::min(y + .1, span.second) - std::max(y - .1, span.first)) / .2;
        const auto query = field.Query(frame.Point({0, float(y), 0}), up, .1f, 1, tangent, nullptr, true);
        Near(query.fraction, expected, .003, "vertical sweep follows both actual free surfaces");
    }
    const Uniform gravity(frame.Vector({0, -9.81f, 0}));
    BodyTransform pose; pose.position = frame.offset; pose.rotation = frame.rotation;
    for (const auto& volume : {MakeBoxFluidVolume(glm::vec3(.1f)), MakeSphereFluidVolume(.1f),
                              MakeCompoundFluidVolume({{{-.2f, 0, 0}, {.1f, .1f, .1f}},
                                                       {{.2f, 0, 0}, {.1f, .1f, .1f}}})}) {
        for (const auto geometry : {FluidVolumeGeometry::DisplacedSolid, FluidVolumeGeometry::NonDisplacingVolume}) {
            const auto result = EvaluateFluidHydrostatics(field, volume, pose, gravity, geometry);
            Near(result.fraction, 0, 0, "body/player in vertical gap stays dry");
            Near(glm::length(result.buoyancyForce), 0, 0, "dry body receives no buoyancy");
        }
    }
    const auto tall = MakeBoxFluidVolume({.1f, .75f, .1f}, 12);
    const auto result = EvaluateFluidHydrostatics(field, tall, pose, gravity);
    Near(result.fraction, 1. / 3., .003, "solid spanning both layers retains partial volume");
    Near(glm::length(result.buoyancyForce), 1000 * 9.81 * tall.totalVolume / 3., 2,
         "partial displaced weight agrees independently");
    // Connecting the strata around a cavity must not make its centre wet.
    field.Build(Layers(frame, true), 1000);
    const auto cavity = EvaluateFluidHydrostatics(field, MakeBoxFluidVolume(glm::vec3(.1f)), pose, gravity);
    Near(cavity.fraction, 0, 0, "enclosed 3D pocket is not filled by connected outer water");
    const auto player = field.Query(frame.offset, up, .1f, 1, tangent, nullptr, true);
    Near(player.fraction, 0, 0, "enclosed pocket does not trigger swimming");
    const auto tallCavity=EvaluateFluidHydrostatics(field,tall,pose,gravity);
    Near(tallCavity.fraction,1./3.,.003,"solid crossing connected cavity does not displace its air");

}
void DisplacedSupportContinuity() {
    std::vector<FluidParticle> particles;
    for(int z=-4;z<=4;++z)for(int x=-4;x<=4;++x)for(int y=-4;y<=3;++y) {
        const bool remote=x==3 && z==0;
        const float height=(y+.5f)*.25f;
        const float surface=x<0 ? .25f : 0.f;
        if(remote ? std::abs(height)<.5f : height>=surface)continue;
        FluidParticle particle;
        particle.position={x*.25f,height,z*.25f};
        particle.mass=15.625f;
        particles.push_back(particle);
    }
    FluidHydrostaticField field;field.Build(particles,1000);
    const auto volume=MakeBoxFluidVolume(glm::vec3(.3f));
    // The remote split column enters the search at reach=.75. Its kernel
    // weight tends to zero there. It must not switch ownership of all other
    // columns and produce a finite jump. Compare two shrinking radius brackets;
    // this is a continuity control, not a new physical settling tolerance.
    for(int angle=1;angle<=8;++angle)for(float halfHeight:{.10f,.15f,.20f,.25f}) {
        BodyTransform pose;pose.rotation=glm::angleAxis(angle*.1f,glm::vec3(0,0,1));
        const FluidSolidQuery solid{volume,pose};
        const auto bracket=[&](float epsilon) {
            const auto a=field.Query({}, {0,1,0},halfHeight,.25f-epsilon,{1,0,0},&solid,true);
            const auto b=field.Query({}, {0,1,0},halfHeight,.25f+epsilon,{1,0,0},&solid,true);
            return std::abs(double(a.fraction)-b.fraction);
        };
        const double coarse=bracket(.001f),fine=bracket(.00001f);
        const double rounding=64*std::numeric_limits<float>::epsilon();
        Near(std::max(0.,fine-.02*coarse),0,rounding,
             "vanishing remote donor cannot switch displaced-column ownership");
    }
}
void EquivalentSolidFrames() {
    // The same physical box can be represented by its parent rotation or by a
    // rotated compound child. That authoring choice must not rotate the water
    // lattice or change its load. This is a physical representation equivalence,
    // not an assertion about a preferred numerical result.
    for(int angle=1;angle<=8;++angle) {
        const auto turn=glm::angleAxis(angle*.13f,glm::normalize(glm::vec3(1,2,3)));
        const glm::vec3 half(.3f);
        std::vector<FluidParticle> particles;
        for(int z=-4;z<=4;++z)for(int x=-4;x<=4;++x)for(int y=-4;y<=1;++y) {
            const glm::vec3 position(x*.25f,(y+.5f)*.25f,z*.25f);
            if(position.y> (x<0?.25f:0.f))continue;
            if(glm::all(glm::lessThan(glm::abs(glm::inverse(turn)*position),half)))continue;
            FluidParticle p;p.position=position;p.mass=15.625f;particles.push_back(p);
        }
        FluidHydrostaticField field;field.Build(particles,1000);
        auto box=MakeBoxFluidVolume(half);
        CompoundBox child;child.halfExtents=half;child.rotation=turn;
        auto compound=MakeCompoundFluidVolume({child});
        BodyTransform rotated;rotated.rotation=turn;
        const Uniform gravity({0,-9.81f,0});
        const auto a=EvaluateFluidHydrostatics(field,box,rotated,gravity);
        const auto b=EvaluateFluidHydrostatics(field,compound,BodyTransform{},gravity);
        Near(a.fraction,b.fraction,2e-6,"parent/child frame preserves identical displaced volume");
        Near(glm::length(a.buoyancyForce-b.buoyancyForce),0,.01,"parent/child frame preserves identical buoyancy");
        Near(glm::length(a.buoyancyTorque-b.buoyancyTorque),0,.005,"parent/child frame preserves identical torque");
    }
}
void IntervalControls() {
    using Span = std::pair<double, double>;
    const auto measure = [](std::vector<Span> spans, double height) {
        return FluidHydrostaticDetail::ColumnOccupiedHeight(spans, height, .25, 0);
    };
    Near(measure({}, 1), 0, 0, "empty column");
    Near(measure({{-.5, -.25}, {.25, .5}}, 1), .5, 0, "resolved gap remains empty");
    Near(measure({{.25, .5}, {-.5, -.25}}, 1), .5, 0, "interval ordering independent");
    Near(measure({{-.5, .1}, {-.1, .5}, {-.5, .1}}, 1), 1, 0, "overlapping particles never double volume");
    Near(measure({{-.5, -.1}, {.1, .5}}, 1), 1, 0, "sub-resolution disorder remains reconstructed");
    Near(measure({{-.5, -.25}, {.25, .5}}, .1), 0, 0, "query wholly inside resolved gap");
    Near(measure({{-.5, -.25}, {.25, .5}}, 0), 0, 0, "zero-height query inside gap");
    Near(measure({{-.5, .5}}, 0), 1, 0, "zero-height query inside water");
    Near(measure({{-.5, -.25}, {0, .25}}, 1), .75, 0, "one-column resolution closes internal gaps only");
}
void ConnectedSlabContinuity() {
    // The root column ends at y=0; all neighboring columns end at y=.25.
    // A physical sample slab just touching the root surface cannot borrow an
    // entire slab of taller neighbor water through an infinitesimal overlap.
    const auto rotation=glm::angleAxis(.83f,glm::normalize(glm::vec3(1,-2,3)));
    constexpr float spacing=.25f, halfHeight=.1f, radius=.3f;
    double kernelSum=0;
    const double reach=double(radius)+2*spacing;
    for (int z=-4; z<=4; ++z) for (int x=-4; x<=4; ++x) {
        const double distance2=spacing*spacing*(x*x+z*z);
        if (distance2>=reach*reach) continue;
        const double kernel=1-distance2/(reach*reach);
        kernelSum+=kernel*kernel;
    }
    const double rounding=64*std::numeric_limits<float>::epsilon();
    for (const Frame& frame : {Frame{},Frame{rotation,{2,-1,3}}}) {
        std::vector<FluidParticle> particles;
        for (int z=-4; z<=4; ++z) for (int x=-4; x<=4; ++x) {
            for (int y=-4; y<(x||z ? 1 : 0); ++y) {
                FluidParticle particle;
                particle.position=frame.Point({x*spacing,(y+.5f)*spacing,z*spacing});
                particle.mass=15.625f;
                particles.push_back(particle);
            }
        }
        FluidHydrostaticField field;
        field.Build(particles,1000);
        double previousJump=0;
        for (float epsilon : {.001f,.00001f}) {
            const auto sample=[&](float y,bool connected) {
                return field.Query(frame.Point({0,y,0}),frame.Vector({0,1,0}),
                    halfHeight,radius,frame.Vector({1,0,0}),nullptr,connected).fraction;
            };
            const double below=sample(halfHeight-epsilon,true);
            const double above=sample(halfHeight+epsilon,true);
            Near(below,epsilon/(2*double(halfHeight)),rounding,
                 "connected sample integrates only its admitted wet-height interval");
            Near(above,0,0,"connected slab above its local water surface stays dry");
            const double jump=std::abs(below-above);
            if (previousJump>0) Near(std::max(0.,jump-.02*previousJump),0,rounding,
                "vanishing root overlap cannot admit a finite neighbor slab");
            previousJump=jump;
            // The default low-level unconnected reconstruction deliberately
            // retains its published spatial column mean.
            const double rootWeight=1/kernelSum;
            Near(sample(halfHeight+epsilon,false),1-rootWeight,rounding,
                 "unconnected low-level column mean remains unchanged");
            Near(sample(halfHeight-epsilon,false),1-rootWeight+rootWeight*epsilon/(2*halfHeight),rounding,
                 "unconnected partial-column reconstruction remains unchanged");
        }
    }
}
} // namespace
int main() {
    IntervalControls();
    DisplacedSupportContinuity();
    EquivalentSolidFrames();
    ConnectedSlabContinuity();
    const auto rotation = glm::angleAxis(.83f, glm::normalize(glm::vec3(1, -2, 3)));
    for (const Frame& frame : {Frame{}, Frame{rotation, {3, -2, 4}}, Frame{rotation, {4096, 0, 0}}}) LayerQueries(frame);
    std::cout << "Fluid vertical support: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
