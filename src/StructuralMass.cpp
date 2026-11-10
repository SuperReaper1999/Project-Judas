#include "StructuralMass.h"
#include "Deformable.h"
#include <cmath>
#include <stdexcept>

StructuralMassProperties IntegrateStructuralMass(const DeformableAsset &asset, const FracturePart &part,
                                                 double density) {
    if (!std::isfinite(density) || density <= 0 || part.tets.empty())
        throw std::invalid_argument("invalid structural volume density/topology");
    StructuralMassProperties result;
    // Integrate relative to a nearby reference to avoid cancellation for translated assets.
    const auto reference = asset.nodes.at(asset.tetrahedra.at(part.tets.front()).x);
    glm::dmat3 second(0);
    glm::dvec3 first(0);
    for (auto index : part.tets) {
        auto tet = asset.tetrahedra.at(index);
        glm::dvec3 r[4];
        for (unsigned k = 0; k < 4; ++k)
            r[k] = asset.nodes.at(tet[k]) - reference;
        const double mass =
            density * std::abs(glm::determinant(glm::dmat3(r[1] - r[0], r[2] - r[0], r[3] - r[0]))) / 6;
        if (!std::isfinite(mass) || mass <= 0)
            throw std::invalid_argument("invalid structural tetrahedron mass");
        auto sum = r[0] + r[1] + r[2] + r[3];
        glm::dmat3 diagonal(0);
        for (auto value : r)
            diagonal += glm::outerProduct(value, value);
        second += (mass / 20) * (diagonal + glm::outerProduct(sum, sum));
        first += mass * .25 * sum;
        result.mass += mass;
    }
    auto offset = first / result.mass;
    result.center = reference + offset;
    second -= result.mass * glm::outerProduct(offset, offset);
    result.inertia = glm::dmat3(second[0][0] + second[1][1] + second[2][2]) - second;
    return result;
}

glm::dvec3 IntegrateStructuralAngularMomentum(const DeformableAsset &asset, const FracturePart &part,
                                              double density, const std::vector<glm::dvec3> &positions,
                                              const std::vector<glm::dvec3> &velocities, glm::dvec3 center,
                                              glm::dvec3 linearVelocity) {
    if (!std::isfinite(density) || density <= 0)
        throw std::invalid_argument("invalid structural momentum density");
    glm::dvec3 total(0);
    for (auto index : part.tets) {
        const auto tet = asset.tetrahedra.at(index);
        glm::dvec3 sumPosition(0), sumVelocity(0), diagonal(0);
        for (unsigned k = 0; k < 4; ++k) {
            const auto r = positions.at(tet[k]) - center;
            const auto v = velocities.at(tet[k]) - linearVelocity;
            sumPosition += r;
            sumVelocity += v;
            diagonal += glm::cross(r, v);
        }
        const auto origin = asset.nodes.at(tet.x);
        const double mass = density *
                            std::abs(glm::determinant(glm::dmat3(asset.nodes.at(tet.y) - origin,
                                                                 asset.nodes.at(tet.z) - origin,
                                                                 asset.nodes.at(tet.w) - origin))) /
                            6;
        // Exact product of the affine position/velocity fields inside a tet.
        total += (mass / 20) * (diagonal + glm::cross(sumPosition, sumVelocity));
    }
    return total;
}
