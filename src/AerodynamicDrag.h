#pragma once

#include <glm/glm.hpp>

#include "PhysicsWorld.h"
#include "ReferenceFrame.h"

class AtmosphereField;

// Measurements from one current-pose aerodynamic evaluation. Relative air
// velocity is body-point velocity minus the gas velocity at that point.
struct AerodynamicDragResult {
    float density = 0.0f;           // kg/m^3
    float pressure = 0.0f;          // Pa
    glm::vec3 relativeAirVelocity{0.0f};  // m/s
    float relativeAirspeed = 0.0f;  // m/s
    float projectedArea = 0.0f;     // m^2, along relative air velocity
    float dynamicPressure = 0.0f;   // Pa
    glm::vec3 force{0.0f};         // N, applied to PhysicsWorld
};

// Exact orthographic projected area of a rectangular box. The input
// direction is in world space; zero direction has no exposed flow area.
float BoxProjectedArea(const BodyBox& box, const glm::vec3& worldFlowDirection);

// Minimal centred-box aerodynamic model for the existing spacecraft. Gas is
// sampled at the box centre; its own point velocity includes omega x r.
// Symmetric pressure loading acts through the centre of mass, so this model
// intentionally generates no torque or lift. The result is diagnostic data;
// the force itself enters the ordinary rigid-body accumulator before Step().
// Bodies without exactly one centred box receive no aerodynamic force.
AerodynamicDragResult ApplyAerodynamicDrag(PhysicsWorld& physics, BodyHandle body,
                                           const AtmosphereField& atmosphere,
                                           const ReferenceFrame& planetFrame,
                                           float dragCoefficient = 1.0f);
