#pragma once
#include "PhysicsWorld.h"

namespace JudasDeformables {
// Instance-local collision publication signature. Immutable geometry ownership,
// actual poses and candidate membership detect edits without a world-wide epoch.
class SleepContacts {
    struct Entry {BodyHandle body;BodyTransform pose;Shape shape;};
    std::vector<BodyHandle> candidates;
    std::vector<Entry> retained,scratch;
    bool valid=false;
public:
    void Invalidate(){valid=false;retained.clear();scratch.clear();}
    bool Refresh(PhysicsWorld&,glm::dvec3 low,glm::dvec3 high,unsigned layer,CategoryMask mask,bool& moving);
};
}
