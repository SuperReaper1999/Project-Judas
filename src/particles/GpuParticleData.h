#pragma once
#include "GpuParticles.h"
#include <cstddef>

namespace JudasParticles::GpuDetail {
struct alignas(16) Params {
    std::array<float,4> positionDt{},rotation{0,0,0,1},scale{1,1,1,0};
    std::array<float,4> spreadLife{},velocitySize{},variationEndSize{},accelerationRadius{};
    std::array<float,4> gravityMagnitude{},gravityCenter{},windRestitution{},color{},endColor{};
    std::array<float,4> response{}; // friction, impact weight, cell size, unused
    std::array<float,4> domainMinimum{},domainMaximum{};
    std::array<unsigned,4> control{}; // capacity, spawn count, stable id base, seed
    std::array<unsigned,4> modes{}; // gravity kind, response, max contacts, impact capacity
    std::array<unsigned,4> query{}; // top root, collider count, unused, unused
    std::array<unsigned,4> history{}; // tick low, tick high, discard prior batch, shift only
};
struct alignas(16) ViewParams {
    std::array<float,16> view{},projection{};
    std::array<std::array<float,4>,6> planes{};
    std::array<float,4> sizeLife{}; // start size, end size, lifetime, unused
    std::array<float,4> color{},endColor{};
    std::array<unsigned,4> control{}; // capacity, sort capacity, sorted, unused
};
struct alignas(16) Key {float depth=0;unsigned id=0,slot=0,visible=0;};
struct alignas(16) ImpactRecord {
    std::array<float,4> pointAge{},normalSize{},velocityWeight{};
    std::array<unsigned,4> identity{}; // collider, birth id, feature, reserved
};
struct alignas(16) Collider {
    std::array<float,4> centerRadius{},rotation{},extents{};
    std::array<unsigned,4> meta{}; // kind, asset BVH root, source collider, two sided
};
struct alignas(16) Node {
    std::array<float,4> minimum{},maximum{};
    std::array<unsigned,4> links{}; // first, count, left, right
};
struct alignas(16) Triangle {
    std::array<float,4> a{},b{},c{},normal{};
    std::array<unsigned,4> meta{};
};
static_assert(sizeof(Params)%16==0 && sizeof(ViewParams)%16==0);
static_assert(sizeof(Key)==16 && sizeof(ImpactRecord)==64 && sizeof(Collider)==64);
static_assert(sizeof(Node)==48 && sizeof(Triangle)==80);
} // namespace JudasParticles::GpuDetail
