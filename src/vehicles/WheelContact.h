#pragma once

#include "Wheel.h"
#include "WheelContactCache.h"

namespace JudasVehicles {
// Bounded, finite-width tread approximation. The physical query geometry is
// independent of the render mesh and uses the current MK2 collision epoch.
PhysicsCastHit FindSupport(PhysicsWorld&, BodyHandle chassis, const WheelSettings&,
                          glm::vec3 anchor, glm::vec3 down, glm::vec3 axle,
                          std::vector<BodyHandle>& scratch,
                          WheelContactCache* cache=nullptr,bool sleeping=false);
}
