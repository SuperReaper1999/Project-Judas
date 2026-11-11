#include "LightingWorld.h"
#include "../RuntimeWorld.h"
#include "../Scene.h"

std::vector<DynamicLight> BuildWorldLights(const RuntimeWorld& world, float alpha,unsigned maximumSources,bool simulation) {
    std::vector<DynamicLight> lights;



    unsigned examined=0;
    for (const RuntimeWorld::StaticLight& s : world.StaticLights()) {
        if(examined++>=maximumSources)break;
        if(!world.IsPublished(s.id))continue;
        DynamicLight light;
        light.kind = s.light.kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
        const auto* definition=world.RuntimeDefinition(s.id);if(!definition)continue;
        const auto t=simulation?world.SimulationTransform(s.id,definition->transform):world.PresentedTransform(s.id,definition->transform,alpha);
        light.position = t.position;
        light.direction = t.rotation*glm::vec3(0,0,-1);
        if(!s.light.enabled)continue;
        light.identity=s.id;light.castsShadow=s.light.castsShadow;light.shadowPriority=s.light.shadowPriority;
        light.color = s.light.color*s.light.intensity;
        light.range = s.light.range;
        light.innerConeDegrees = s.light.innerConeDegrees;
        light.outerConeDegrees = s.light.outerConeDegrees;
        lights.push_back(light);
    }
    return lights;
}

std::vector<DynamicLight> BuildAuthoredLights(const Scene& scene) {
    std::vector<DynamicLight> lights;
    for (const SceneObject& o : scene.Objects()) {
        if (!o.light||!o.light->enabled) continue;
        DynamicLight light;
        light.kind = o.light->kind == SceneLightKind::Spot ? LightKind::Spot : LightKind::Point;
        light.position = o.transform.position;
        light.direction = glm::normalize(glm::normalize(o.transform.rotation) * glm::vec3(0.0f, 0.0f, -1.0f));
        light.identity=o.id;light.castsShadow=o.light->castsShadow;light.shadowPriority=o.light->shadowPriority;
        light.color = o.light->color*o.light->intensity;
        light.range = o.light->range;
        light.innerConeDegrees = o.light->innerConeDegrees;
        light.outerConeDegrees = o.light->outerConeDegrees;
        lights.push_back(light);
    }
    return lights;
}
