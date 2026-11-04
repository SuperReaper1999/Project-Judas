#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Mechanics[] = R"JS(
export class Fracture {
    constructor(id, epoch) {
        this.id = String(id);
        this.epoch = String(epoch);
    }
    get valid() {
        return call("fractureValid", this.id, this.epoch);
    }
    get state() {
        const state = call("fractureState", this.id, this.epoch);
        state.parts = state.parts.map(p => ({
            ...p,
            entity: p.entity ? new Entity(p.entity) : null
        }));
        return state;
    }
    release(interfaceKey, revision) {
        return call("fractureRelease", this.id, this.epoch, interfaceKey, revision);
    }
    remove(part, revision) {
        return call("fractureRemove", this.id, this.epoch, part, revision);
    }
    force(part, value) {
        return call("fractureForce", this.id, this.epoch, part, value);
    }
    impulse(part, value) {
        return call("fractureImpulse", this.id, this.epoch, part, value);
    }
}
export class Deformable {
    constructor(id, epoch) {
        this.id = id;
        this.epoch = epoch;
    }
    get valid() {
        return call("deformableValid", this.id, this.epoch);
    }
    get state() {
        return call("deformableState", this.id, this.epoch);
    }
    get enabled() {
        return this.state.enabled;
    }
    set enabled(value) {
        call("deformableEnabled", this.id, this.epoch, value);
    }
    reset() {
        return call("deformableReset", this.id, this.epoch);
    }
    force(value, group = "") {
        return call("deformableForce", this.id, this.epoch, value, group);
    }
    impulse(value, group = "") {
        return call("deformableImpulse", this.id, this.epoch, value, group);
    }
    impulseAt(location, value) {
        return call("deformableHitImpulse", this.id, this.epoch, location, value);
    }
    release(group) {
        return call("deformableRelease", this.id, this.epoch, group);
    }
    attach(group, options) {
        return call("deformableAttach", this.id, this.epoch, group, {
            ...options,
            targetId: options.target?.id
        });
    }
    setMaterial(options) {
        return call("deformableMaterial", this.id, this.epoch, options);
    }
    raycast(origin, direction, maximum) {
        return call("deformableRaycast", this.id, this.epoch, origin, direction, maximum);
    }
}
export class Wheel {
    constructor(id) {
        this.id = String(id);
    }
    get valid() {
        return call("wheelValid", this.id);
    }
    get enabled() {
        return this.settings.enabled;
    }
    set enabled(value) {
        call("wheelEnabled", this.id, value);
    }
    get settings() {
        const v = call("wheelSettings", this.id);
        return {
            ...v,
            chassis: entity(v.chassisId)
        };
    }
    get state() {
        const v = call("wheelState", this.id);
        return {
            ...v,
            contactEntity: entity(v.contactEntityId)
        };
    }
    set(commands) {
        return call("wheelSet", this.id, commands);
    }
    configure(settings) {
        return call("wheelConfigure", this.id, settings);
    }
}
export class Powertrain {
    constructor(id) {
        this.id = String(id);
    }
    get valid() {
        return call("powertrainValid", this.id);
    }
    get enabled() {
        return this.settings.enabled;
    }
    set enabled(value) {
        call("powertrainEnabled", this.id, value);
    }
    get settings() {
        return call("powertrainSettings", this.id);
    }
    get state() {
        return call("powertrainState", this.id);
    }
    set(commands) {
        return call("powertrainSet", this.id, commands);
    }
}
extendEntity({
    get fracture() {
        const epoch = call("fractureExists", this.id);
        return epoch ? new Fracture(this.id, epoch) : null;
    },
    get deformable() {
        const epoch = call("deformableExists", this.id);
        return epoch ? new Deformable(this.id, epoch) : null;
    },
    get wheel() {
        return call("wheelExists", this.id) ? new Wheel(this.id) : null;
    },
    get powertrain() {
        return call("powertrainExists", this.id) ? new Powertrain(this.id) : null;
    }
});
)JS";
} // namespace ScriptBindings::LibraryParts
