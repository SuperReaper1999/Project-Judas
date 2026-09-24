#include <cmath>
#include <cstdio>
#include <vector>

#include "FluidSurface.h"

int main() {
    const std::vector<glm::vec3> particles{
        {0.0f, 0.0f, 0.0f}, {0.06f, 0.0f, 0.0f},
        {0.0f, 0.06f, 0.0f}, {0.06f, 0.06f, 0.0f},
        {0.0f, 0.0f, 0.06f}, {0.06f, 0.0f, 0.06f},
        {0.0f, 0.06f, 0.06f}, {0.06f, 0.06f, 0.06f}};
    const auto mesh = BuildFluidSurface(particles, 0.12f, 0.04f, 0.5f);
    if (mesh.empty() || mesh.size() % 3 != 0) {
        std::fprintf(stderr, "fluid surface was empty or not triangulated\n");
        return 1;
    }
    for (const MeshVertex& vertex : mesh) {
        if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
            !std::isfinite(vertex.position.z) || !std::isfinite(vertex.normal.x) ||
            !std::isfinite(vertex.normal.y) || !std::isfinite(vertex.normal.z)) {
            std::fprintf(stderr, "fluid surface contained a non-finite vertex\n");
            return 1;
        }
    }
    const std::vector<glm::vec3> translated{{3.0f, -2.0f, 1.0f}};
    const auto shifted = BuildFluidSurface(translated, 0.12f, 0.04f, 0.5f);
    if (shifted.empty()) {
        std::fprintf(stderr, "isolated simulated droplet had no visible surface\n");
        return 1;
    }
    std::printf("fluid surface: %zu triangle vertices for eight nearby particles; %zu for a droplet\n",
                mesh.size(), shifted.size());
    return 0;
}
