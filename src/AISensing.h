#pragma once
#include "PhysicsWorld.h"
#include "Scene.h"
#include <memory>
#include <optional>
#include <string>

class RuntimeWorld;
struct AISensingSettings {
    double interval = .2;
    unsigned maxResults = 16, maxRaycasts = 8, maxCandidates = 128;
};
struct AISensingRequest {
    glm::vec3 origin{0};
    float radius = 1;
    PhysicsQueryFilter targets;
    CategoryMask occluderLayers = kAllCategories;
    bool visibility = false;
};
struct AISensingTarget {
    SceneObjectId entityId = 0;
    BodyHandle body{};
    glm::vec3 position{0};
    double distance = 0;
    std::optional<bool> visible;
};
struct AISensingResult {
    std::vector<AISensingTarget> targets;
    bool cached = false, stale = false, truncated = false;
    double age = 0, sampledAt = 0;
    unsigned candidatesConsidered = 0, raycasts = 0;
    std::size_t queryCandidates = 0;
    std::string Json() const;
};

// Timed, explicitly stale snapshots. Result/raycast/post-query processing bounds
// do NOT cap the underlying shared broadphase's traversal or materialized matches.
class AISensing {
public:
    explicit AISensing(AISensingSettings settings = {});
    ~AISensing();
    AISensingResult Sample(RuntimeWorld&, SceneObjectId owner, const AISensingRequest&);
    void Invalidate();
    const AISensingSettings& Settings() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
