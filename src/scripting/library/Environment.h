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
extendEntity({

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
