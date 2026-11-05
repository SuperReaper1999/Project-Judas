#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Rendering[] = R"JS(
export class Material {
    constructor(entityId, slot = 0) {
        this.entityId = entityId;
        this.slot = slot;
    }
    get state() {
        return call("materialInfo", this.entityId, this.slot);
    }
    assign(asset) {
        return call("materialAssign", this.entityId, this.slot, asset);
    }
    set(parameters) {
        return call("materialOverride", this.entityId, this.slot, parameters);
    }
    clearOverrides() {
        return call("materialClear", this.entityId, this.slot);
    }
}
extendEntity({
    setTile(layer, x, y, frame, options = {}) {
        return call("tileSet", this.id, layer, x, y, frame, options);
    },
    get light() {
        return call("lightInfo", this.id);
    },
    setLight(settings) {
        return call("lightSettings", this.id, settings);
    },
    get camera() {
        return call("cameraInfo", this.id);
    },
    get sprite() {
        return call("spriteInfo", this.id);
    },
    get spriteAnimation() {
        return call("spriteAnimationInfo", this.id);
    },
    setSpriteAnimation(settings) {
        return call("spriteAnimationSet", this.id, settings);
    },
    setSprite(settings) {
        return call("spriteSet", this.id, settings);
    },
    get renderVisible() {
        return call("renderVisible", this.id);
    },
    set renderVisible(value) {
        call("renderVisible", this.id, value);
    },
    get rendererVisible() {
        return call("rendererVisible", this.id);
    },
    set rendererVisible(value) {
        call("rendererVisible", this.id, value);
    },
    material(slot = 0) {
        return new Material(this.id, slot);
    },
    get modelParts() {
        return call("modelParts", this.id);
    },
    setPartVisible(identity, visible) {
        return call("modelPartVisible", this.id, identity, visible);
    },
    burst(count) {
        return call("burst", this.id, count);
    },
    get particles() {
        return call("particleInfo", this.id);
    },
    setParticles(settings) {
        return call("particles", this.id, settings);
    },
    get particleStats() {
        return call("particleStats", this.id);
    },
    takeParticleImpacts() {
        return call("particleImpacts", this.id);
    },
    controlParticles(command) {
        return call("particleControl", this.id, command);
    },
    setCameraEnabled(enabled) {
        return call("camera", this.id, enabled);
    },
    setCameraTargetSize(width, height) {
        return call("cameraTargetSize", this.id, width, height);
    },
    setCameraProjection(range) {
        return call("cameraProjection", this.id, range);
    },
    setCameraPresentation(settings) {
        return call("cameraPresentation", this.id, settings);
    }
});
extendWorld({
    get appearance() {
        return call("appearanceInfo");
    },
    setAppearance: settings => call("appearanceSet", settings),
    resetAppearance: () => call("appearanceReset"),
    setView: (pose, fov = 70, range = {}) => call("setView", pose, fov, range),
    project: point => call("projectViewport", point),
    unproject: point => call("viewportRay", point),
    get viewport() {
        return call("viewportSize");
    },
    clearView: () => call("clearView"),
    get viewRay() {
        return call("viewRay");
    }
});
)JS";
} // namespace ScriptBindings::LibraryParts
