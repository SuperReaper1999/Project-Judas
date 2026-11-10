#pragma once
#include "MeshData.h"
#include <array>
#include <map>
#include <glm/gtc/quaternion.hpp>
struct CrashWheelMount {
    glm::vec3 center{0};
    glm::quat axleFrame{1,0,0,0};
    float radius=.35f,width=.25f;
};
// Fits a wheel's area-weighted short principal axis. Input is baked vehicle-rest
// space; output vertices use the engine wheel convention (+X axle, +Y up).
// Stationary brake hardware shares the fitted center/frame but must not spin.
CrashWheelMount PrepareCrashWheel(MeshData& rotating, MeshData& brakeHardware);

// Source-part identity, not material-slot order or wheel-name guessing, can
// explicitly own the private proof's assembly. An authored map is exhaustive.
enum class CrashPartRole { Body, Wheel, Caliper };
using CrashPartRoles = std::map<std::string, CrashPartRole>;
bool ParseCrashPartRoles(const std::string&, CrashPartRoles&, std::string& error);
CrashPartRole ResolveCrashPartRole(const std::string& part, const CrashPartRoles&);
