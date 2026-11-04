#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Environment[] = R"JS(
export class Weather {
    constructor(id) {
        this.id = String(id);
    }
    get valid() {
        return call("weatherExists", this.id);
    }
    get enabled() {
        return call("weatherEnabled", this.id);
    }
    set enabled(value) {
        call("weatherEnabled", this.id, value);
    }
    get state() {
        return call("weatherState", this.id);
    }
    transition(profile, duration = 1) {
        return call("weatherTransition", this.id, profile, duration);
    }
    strike(position) {
        return call("weatherStrike", this.id, position);
    }
}
export const weather = {
    sample: point => call("weatherSample", point),
    get quality() {
        return call("weatherQuality");
    },
    set quality(value) {
        call("weatherQuality", value);
    }
};
export class LiquidVolume {
    constructor(handle) {
        this.handle = handle;
    }
    get valid() {
        return call("liquidValid", this.handle);
    }
    get state() {
        const v = call("liquidState", this.handle);
        return {
            ...v,
            entity: entity(v.entityId)
        };
    }
    set enabled(value) {
        call("liquidEnabled", this.handle, value);
    }
    transferTo(destination, volume, options = {}) {
        return call("liquidTransfer", this.handle, destination.handle, volume, options);
    }
    applyImpulse(point, impulse) {
        return call("liquidImpulse", this.handle, point, impulse);
    }
    set surfaceEnabled(value) {
        call("liquidSurfaceEnabled", this.handle, value);
    }
}
export const liquid = {
    sample: point => {
        const v = call("liquidSample", point);
        return v ? {
            ...v,
            entity: entity(v.entityId)
        } : null;
    },
    samplePresented: point => {
        const v = call("liquidPresentedSample", point);
        return v ? {
            ...v,
            entity: entity(v.entityId)
        } : null;
    },
    accounting: (material = "water") => call("liquidAccounting", material),
    get errors() {
        return call("liquidErrors");
    },
    get connections() {
        return call("liquidConnections");
    },
    submerged: target => call("liquidSubmerged", target.id)
};
extendEntity({
    get liquid() {
        const h = call("liquidOwner", this.id);
        return h ? new LiquidVolume(h) : null;
    },
    get weather() {
        return call("weatherExists", this.id) ? new Weather(this.id) : null;
    },
    get basicWater() {
        return call("basicWater", this.id);
    },
    setBasicWater(settings) {
        return call("basicWaterSet", this.id, settings);
    }
});
extendWorld({
    fluidSample: (point, up, halfHeight, radius, tangent) => call("fluidSample", point, up, halfHeight, radius, tangent)
});
)JS";
} // namespace ScriptBindings::LibraryParts
