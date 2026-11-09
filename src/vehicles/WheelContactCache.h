#pragma once
#include "PhysicsWorld.h"

namespace JudasVehicles {
struct SupportCollider {
    BodyHandle body;
    BodyTransform pose;
    ShapeType type=ShapeType::Box;
    glm::vec3 half{0};
    float radius=0,halfHeight=0;
    std::shared_ptr<const CollisionAsset> asset;
    std::shared_ptr<const RadialTerrain> terrain;
};
struct WheelContactCache {
    bool valid=false;
    BodyHandle chassis;
    glm::vec3 anchor{0},down{0},axle{0};
    float radius=0,width=0,travel=0;
    std::vector<SupportCollider> colliders,scratch;
    PhysicsCastHit hit;
};
}
