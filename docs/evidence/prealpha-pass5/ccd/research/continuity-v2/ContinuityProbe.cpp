#include "MeshContinuityProbe.h"
#include <iostream>

int main() {
    auto asset = std::make_shared<CollisionAsset>();
    asset->vertices = {{-1,0,-1},{0,0,1},{1,0,-1}};
    CollisionFace face; face.vertices = {0,1,2}; face.normal = {0,1,0};
    asset->faces = {face};
    PrimitivePose mesh, query; mesh.shape = Shape::Cooked(asset, "continuity probe");
    query.shape = Shape::Box({.1,.1,.1});
    RigidMotionSegment segment; segment.inverseMass = 1; segment.end = 1./60;
    segment.position = {0,-.02,0};
    unsigned checks=0, failed=0;
    const auto check = [&](const char *name, episode_probe::Result expected, unsigned budget=1024) {
        const auto result = episode_probe::Connected(query, mesh, 0, segment, 0, segment.end, budget);
        ++checks; if(result.result != expected) ++failed;
        std::cout << name << " result=" << int(result.result) << " intervals=" << result.intervals
                  << " witness_certificates=" << result.pointCertificates
                  << " constant_certificates=" << result.constantCertificates
                  << " separated_at=" << result.separationTime << '\n';
    };
    check("stationary straddler", episode_probe::Result::Connected);
    segment.linearVelocity = {1,0,0};
    check("finite slide", episode_probe::Result::Connected);
    segment.driftAngularVelocity = {0,5,0};
    check("low spin finite slide", episode_probe::Result::Connected);
    segment.linearVelocity = {400,0,0};
    check("actual finite departure", episode_probe::Result::Separated);

    asset->vertices = {{-.02,0,-.02},{0,0,.02},{.02,0,-.02}};
    query.shape = Shape::Box({.3,.005,.05});
    segment.position = {.15,0,0}; segment.linearVelocity = {0,0,0};
    segment.driftAngularVelocity = {0,0,24000};
    const auto planes=episode_probe::Planes(query.shape);
    const bool initial=!episode_probe::Intersection(episode_probe::At(query,segment,0),mesh,0,planes).empty();
    const bool final=!episode_probe::Intersection(episode_probe::At(query,segment,segment.end),mesh,0,planes).empty();
    ++checks; if(!initial||!final)++failed;
    std::cout << "transient control endpoints=" << initial << ',' << final << '\n';
    check("high spin hidden finite exit and reentry", episode_probe::Result::Separated);
    check("uncertified bounded interval is explicit", episode_probe::Result::Uncertain,0);
    std::cout << "checks=" << checks << " failures=" << failed << '\n';
    return failed?1:0;
}
