#pragma once

#include <glm/glm.hpp>

// A pure spatial predicate: "does this position belong to me?" Gives a
// gravity region or a GravityTransition (see GravityContextMap.h) an actual
// boundary, instead of the position-to-a-point falloff distance the old
// GravityResolver used to decide influence — see docs/ARCHITECTURE.md,
// "Gravity context ownership," for why that shape was replaced. A volume
// knows nothing about gravity; it is pure geometry, reusable for anything
// that ever needs "is this point inside this shape."
class GravityVolume {
public:
    virtual ~GravityVolume() = default;

    virtual bool Contains(const glm::vec3& position) const = 0;
};
