#pragma once
#include "GpuParticleData.h"
#include "CollisionSnapshot.h"
#include <string>
#include <vector>
namespace JudasParticles::GpuDetail {
struct PackedGeometry {
    std::vector<Collider> colliders;
    std::vector<Node> nodes;
    std::vector<Triangle> triangles;
    unsigned root=0;
    std::uint64_t fingerprint=0;
};
std::uint64_t GeometryFingerprint(const CollisionSnapshot&,std::string& error);
bool PackGeometry(const CollisionSnapshot&, PackedGeometry&,std::string& error);
std::string CollisionShaderSource();
}
