#pragma once
namespace ScriptBindings::LibraryParts {
inline constexpr char Water[]=R"JS(
export class GridWater {
    constructor(id) { this.id = String(id); }
    get valid() { return call("gridWaterExists", this.id); }
    get state() { return call("gridWaterState", this.id); }
    impulse(point, momentum, radius = 0) {
        return call("gridWaterImpulse", this.id, point, momentum, radius);
    }
    changeVolume(point, cubicMetres, radius = 0.8) {
        return call("gridWaterChange", this.id, point, cubicMetres, radius);
    }
}
extendEntity({
    get gridWater() {
        return call("gridWaterExists", this.id) ? new GridWater(this.id) : null;
    }
});
extendWorld({
    get gridWaterRegions() { return call("gridWaterOwners").map(entity); }
});
)JS";
}
