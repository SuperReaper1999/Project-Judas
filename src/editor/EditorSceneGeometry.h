#pragma once
#include "DebugDraw.h"
#include "Prefab.h"
#include "Scene.h"
#include <algorithm>
#include <optional>
#include <glm/gtc/quaternion.hpp>
namespace EditorSceneGeometry {
// Framing and non-geometric selection outlines use a generous size hint.
// Actual viewport selection is handled by EditorScenePicker's geometry tests.
inline float PickRadius(const SceneObject &o) {
    float radius = 0.75f;
    if (o.render) {
        if (o.render->shape == SceneShape::Sphere)
            radius = std::max(radius, o.render->radius);
        else if (o.render->shape == SceneShape::Sprite)
            radius = glm::length(o.render->sprite.size * glm::vec2(o.transform.scale));
        else if (o.render->shape == SceneShape::Box)
            radius = std::max(radius, glm::length(o.render->halfExtents));
        else if (o.render->shape == SceneShape::Mesh)
            radius = std::max(radius, 2.0f * std::max(o.transform.scale.x,
                                                      std::max(o.transform.scale.y, o.transform.scale.z)));
    }
    if (o.body) {
        if (o.body->shape == SceneShape::Sphere)
            radius = std::max(radius, o.body->radius);
        else if (o.body->shape == SceneShape::Box)
            radius = std::max(radius, glm::length(o.body->halfExtents));
        else if (o.body->shape == SceneShape::Terrain)
            radius = std::max(radius, 90.0f);
    }
    if (o.gravity) {
        radius = std::max(radius, o.gravity->regionShape == SceneRegionShape::Sphere
                                      ? o.gravity->regionRadius * 0.1f
                                      : glm::length(o.gravity->regionHalfExtents) * 0.1f);
    }
    return radius;
}

struct InitialFrameTarget {
    SceneObjectId id = kInvalidSceneObjectId;
    glm::vec3 point{0};
    float radius = .75f;
};

// Opening a scene should show its playable area rather than whichever settings
// owner happened to serialize first. Flatten into a copy: initial camera framing
// neither changes authored local transforms nor constructs a physical world.
inline std::optional<InitialFrameTarget> InitialFraming(const Scene &authored) {
    Scene world;
    std::string error;
    if (!FlattenHierarchy(authored, world, error))
        return std::nullopt;
    const auto target = [](const SceneObject &object) {
        return InitialFrameTarget{object.id, object.transform.position, PickRadius(object)};
    };
    for (const auto &object : world.Objects())
        if (object.playerStart || object.characterMotor)
            return target(object);
    const auto visible = [&](const SceneObject &object) {
        return object.renderVisible && object.render && object.render->visible &&
               (world.Settings().mainCameraRenderMask & CategoryBit(object.renderLayer));
    };
    for (const auto &object : world.Objects())
        if (visible(object) && !object.ui && !object.planet && !object.atmosphere && !object.celestial &&
            !object.basicWater && !object.weather)
            return target(object);
    // Unrendered bodies, lights/cameras and plain transform groups are useful
    // authoring fallbacks; pure settings/UI/environment owners are not.
    for (const auto &object : world.Objects())
        if (object.body || object.renderCamera || object.light ||
            (!object.ui && !object.gravity && !object.planet && !object.atmosphere && !object.celestial &&
             !object.basicWater && !object.weather && !object.fluidVolume && !object.liquidBasin &&
             !object.liquidConnection))
            return target(object);
    // An environment-only scene still has a meaningful overview when it has
    // actual visible geometry. A metadata-only/empty scene keeps the camera.
    for (const auto &object : world.Objects())
        if (visible(object))
            return target(object);
    return std::nullopt;
}

// The selection outline: the object's own shape where it has one.
inline void BuildSelectionLines(const SceneObject &o, DebugLineList &out) {
    const glm::vec3 color(1.0f, 0.85f, 0.2f);
    const glm::vec3 position = o.transform.position;
    const glm::quat rotation = glm::normalize(o.transform.rotation);
    bool drawn = false;
    if (o.render) {
        if (o.render->shape == SceneShape::Box) {
            out.Box(position, rotation, o.render->halfExtents * 1.02f, color);
            drawn = true;
        } else if (o.render->shape == SceneShape::Sphere) {
            out.Sphere(position, o.render->radius * 1.02f, color);
            drawn = true;
        }
    }
    if (!drawn && o.body) {
        if (o.body->shape == SceneShape::Box) {
            out.Box(position, rotation, o.body->halfExtents * 1.02f, color);
            drawn = true;
        } else if (o.body->shape == SceneShape::Sphere) {
            out.Sphere(position, o.body->radius * 1.02f, color);
            drawn = true;
        } else if (o.body->shape == SceneShape::Compound) {
            for (const CompoundBox &box : o.body->compoundBoxes)
                out.Box(position + rotation * box.localCenter, rotation, box.halfExtents * 1.02f, color);
            drawn = true;
        }
    }
    if (!drawn)
        out.Sphere(position, PickRadius(o), color, 16);
}

} // namespace EditorSceneGeometry
