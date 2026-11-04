#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char InputPlayers[] = R"JS(
export class PlayerInput {
    constructor(id) {
        this.id = String(id);
    }
    held(name) {
        return call("playerInput", this.id, "held", name);
    }
    pressed(name) {
        return call("playerInput", this.id, "pressed", name);
    }
    released(name) {
        return call("playerInput", this.id, "released", name);
    }
    axis(name) {
        return call("playerInput", this.id, "axis", name);
    }
    vector(name) {
        return call("playerInput", this.id, "vector", name);
    }
    stick(side) {
        return call("playerInput", this.id, "stick", side);
    }
    get touches() {
        return call("playerInput", this.id, "touches");
    }
    get pointer() {
        return call("playerInput", this.id, "pointer");
    }
}
export class LocalPlayer {
    constructor(id) {
        this.id = String(id);
    }
    get valid() {
        return call("playerExists", this.id);
    }
    get name() {
        return call("playerInfo", this.id).name;
    }
    get devices() {
        return call("playerInfo", this.id).devices;
    }
    get input() {
        return new PlayerInput(this.id);
    }
    get entity() {
        return entity(call("playerEntity", this.id));
    }
    set entity(value) {
        call("playerEntity", this.id, value?.id ?? null);
    }
    get viewport() {
        return call("playerViewport", this.id);
    }
    set viewport(value) {
        call("playerViewport", this.id, value);
    }
    get viewError() {
        return call("playerInfo", this.id).viewError;
    }
    project(point) {
        return call("playerProject", this.id, point);
    }
    ray(point = {
        x: 0.5,
        y: 0.5
    }) {
        return call("playerRay", this.id, point);
    }
    assign(device, options = {}) {
        return call("playerAssign", this.id, device, options);
    }
    unassign(device) {
        return call("playerUnassign", this.id, device);
    }
    setInputMap(map) {
        return call("playerMap", this.id, map);
    }
    setView(pose, fov = 70, range = {}) {
        return call("playerView", this.id, pose, fov, range);
    }
    clearView() {
        return call("playerView", this.id, null);
    }
    leave() {
        return call("playerRemove", this.id);
    }
}
export const players = {
    create: (name = "Player", map = null) => new LocalPlayer(call("playerCreate", name, map)),
    get: id => call("playerExists", String(id)) ? new LocalPlayer(id) : null,
    get list() {
        return call("playerList").map(id => new LocalPlayer(id));
    },
    get devices() {
        return call("playerDevices");
    }
};
export const input = {
    get tilt() {
        return call("tilt");
    },
    setTiltEnabled: enabled => call("setTiltEnabled", enabled),
    get canRumble() {
        return call("inputCanRumble");
    },
    rumble: options => call("inputRumble", options),
    stopRumble: () => call("inputStopRumble"),
    get touches() {
        return call("touches");
    },
    get pointer() {
        return call("pointer");
    },
    get pointerCapture() {
        return call("pointerCapture");
    },
    set pointerCapture(value) {
        call("setPointerCapture", value);
    },
    getBindings: () => call("inputBindings"),
    setBindings: map => call("inputSetBindings", map),
    held: name => call("held", name),
    pressed: name => call("pressed", name),
    released: name => call("released", name),
    axis: name => call("axis", name),
    stick: side => call("stick", side),
    stickDelta: side => call("stickDelta", side),
    vector: name => call("vector", name),
    stickSamples: (side, after = 0) => call("stickSamples", side, after)
};
)JS";
} // namespace ScriptBindings::LibraryParts
