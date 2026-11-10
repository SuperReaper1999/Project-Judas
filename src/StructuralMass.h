#pragma once
#include <glm/glm.hpp>
#include <vector>
struct DeformableAsset;
struct FracturePart;

// Uniform-volume tetrahedral integration, independent of simulation node lumping.
struct StructuralMassProperties {
    double mass = 0;
    glm::dvec3 center{0};
    glm::dmat3 inertia{0};
};
StructuralMassProperties IntegrateStructuralMass(const DeformableAsset &, const FracturePart &,
                                                 double density);

glm::dvec3 IntegrateStructuralAngularMomentum(const DeformableAsset &, const FracturePart &, double density,
                                              const std::vector<glm::dvec3> &positions,
                                              const std::vector<glm::dvec3> &velocities, glm::dvec3 center,
                                              glm::dvec3 linearVelocity);
