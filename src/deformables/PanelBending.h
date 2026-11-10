#pragma once
#include <glm/glm.hpp>
#include <vector>
class SaveArchive;
// Optional finite-mass material kernel. Signed dihedral angles give nonzero
// flat-panel bending stiffness, unlike opposite-vertex distance springs.
class PanelBending {
public:
    struct Hinge {glm::uvec4 nodes;double authored=0,rest=0,lambda=0,plastic=0,work=0;};
    double stiffness=60,yieldMoment=4,hardening=4;
    std::vector<Hinge> hinges;
    void Initialize(const std::vector<glm::dvec3>&,const std::vector<glm::uvec3>&);
    static bool AngleGradient(const glm::dvec3*,double&,glm::dvec3*);
    void Reset();
    void BeginSubstep();
    void Solve(std::vector<glm::dvec3>&,const std::vector<double>& inverseMass,double h);
    void Plastic(const std::vector<glm::dvec3>&,double h);
    void Persist(SaveArchive&);
};
