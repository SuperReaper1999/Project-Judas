#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Characters[] = R"JS(
export class Character {
    constructor(id) {
        this.id = id;
    }
    get state() {
        const v = call("characterState", this.id);
        return {
            ...v,
            supportEntity: entity(v.supportEntityId)
        };
    }
    get velocity() {
        return this.state.velocity;
    }
    set velocity(v) {
        call("characterVelocity", this.id, v);
    }
    get supported() {
        return this.state.supported;
    }
    get supportNormal() {
        return this.state.supportNormal;
    }
    get supportVelocity() {
        return this.state.supportVelocity;
    }
    get actualDisplacement() {
        return this.state.actualDisplacement;
    }
    get gravity() {
        return this.state.gravity;
    }
    get up() {
        return this.state.up;
    }
    set enabled(v) {
        call("characterEnabled", this.id, v);
    }
    configure(v) {
        return call("characterConfigure", this.id, v);
    }
    accelerate(v) {
        return call("characterAcceleration", this.id, v);
    }
    displace(v) {
        return call("characterDisplacement", this.id, v);
    }
    orient(q) {
        return call("characterOrientation", this.id, q);
    }
    ignore(entities) {
        return call("characterIgnore", this.id, entities.map(e => e.id));
    }
}
extendEntity({
    get character() {
        return call("characterExists", this.id) ? new Character(this.id) : null;
    }
});
)JS";
} // namespace ScriptBindings::LibraryParts
