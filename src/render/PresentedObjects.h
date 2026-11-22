#pragma once

#include "../RuntimeWorld.h"

namespace JudasRender {
// The chart and colour draw paths share the exact same presentation sample.
struct PresentedObject {
    SceneTransform pose;
    const std::vector<glm::mat4>* skin = nullptr;
    unsigned layer = 0;
    bool visible = false;
};
struct PresentedObjects {
    std::vector<std::optional<PresentedObject>> statics, dynamics;
};

template<class SamplePose>
PresentedObject SampleObject(const RuntimeWorld& world, SceneObjectId id, float alpha,
                             std::optional<PresentedObject>* slot, bool staticObject,
                             SamplePose samplePose,const SceneObject* knownDefinition=nullptr) {
    if (slot && *slot) return **slot;
    PresentedObject result;
    const auto* definition = knownDefinition?knownDefinition:world.RuntimeDefinition(id);
    if (definition && definition->renderVisible &&
        (!definition->render || definition->render->visible) &&
        (!staticObject || (!definition->deformable && !world.HasRuntimeTerrain(id)))) {
        result = {samplePose(), world.AnimationSkin(id, alpha), world.RenderLayerOf(id), true};
    }
    // The owner and its layer are fixed for this presentation sample. The
    // receiving view still owns distance, frustum, resource and LOD tests.
    if (slot) *slot = result;
    return result;
}

} // namespace JudasRender
