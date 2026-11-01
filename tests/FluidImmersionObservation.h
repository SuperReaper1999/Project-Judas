#pragma once

#include "FluidWorld.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>
#include <glm/gtc/quaternion.hpp>

namespace FluidTest {
// The ORIGINAL production-test observer, preserved for replay and independent
// known-geometry comparisons. Its remote horizontal plane is not a valid
// general nonplanar-surface oracle. Do not "correct" it here and erase evidence.
inline std::optional<double> ObservedSurface(
    const std::vector<FluidParticle>& particles, const glm::quat& q,
    const glm::vec3& localCenter, const glm::vec3& half, float spacing, int nx) {
    std::vector<double> tops(static_cast<std::size_t>(nx*nx), -1e30);
    for (const auto& particle : particles) {
        const glm::vec3 point = glm::conjugate(q)*particle.position;
        if (std::abs(point.x-localCenter.x)<half.x+spacing ||
            std::abs(point.z-localCenter.z)<half.z+spacing) continue;
        const int x = static_cast<int>(std::floor((point.x+1)/spacing));
        const int z = static_cast<int>(std::floor((point.z+1)/spacing));
        if (x>=0 && x<nx && z>=0 && z<nx) {
            const auto index = static_cast<std::size_t>(z*nx+x);
            tops[index] = std::max(tops[index], static_cast<double>(point.y)+.5*spacing);
        }
    }
    tops.erase(std::remove_if(tops.begin(), tops.end(), [](double v) { return v< -1e20; }), tops.end());
    if (tops.empty()) return {};
    std::sort(tops.begin(), tops.end());
    return tops[tops.size()/2];
}

// Original 32^3 native clipping quadrature. Sharing the measuring tool prevents
// a diagnostic from testing an unrelated approximation of that tool.
inline double BoxBelowObservedPlane(const BodyTransform& pose, const glm::vec3& half,
                                    const glm::quat& frame, double surface) {
    int wet = 0;
    constexpr int n = 32;
    for (int z=0; z<n; ++z) for (int y=0; y<n; ++y) for (int x=0; x<n; ++x) {
        const glm::vec3 offset = half*(glm::vec3(2*x+1,2*y+1,2*z+1)/float(n)-1.0f);
        const glm::vec3 point = glm::conjugate(frame)*(pose.position+pose.rotation*offset);
        if (point.y<surface) ++wet;
    }
    return static_cast<double>(wet)/(n*n*n);
}
} // namespace FluidTest
