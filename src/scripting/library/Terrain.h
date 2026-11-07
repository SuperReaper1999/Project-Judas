#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Terrain[] = R"JS(
export class Terrain {
    constructor(id) {
        this.id = id;
    }
    get state() {
        return call("terrainState", this.id);
    }
    create(options) {
        return call("terrainCreate", this.id, options);
    }
    createLandscape(source) {
        return call("terrainLandscape", this.id, source);
    }
    brush(point, options) {
        return call("terrainBrush", this.id, point, options);
    }
}
extendEntity({
    get terrain() {
        return new Terrain(this.id);
    },
    get planetState() {
        return call("planetInfo", this.id);
    },
    planetSample(absolute) {
        return call("planetSample", this.id, absolute);
    },
    preparePlanetCollision(absolute) {
        return call("planetCollision", this.id, absolute);
    },
    clearPlanetCollision() {
        return call("planetClear", this.id);
    }
});
extendWorld({
    get origin() {
        return call("worldOrigin");
    },
    get originState() {
        return call("worldOriginState");
    },
    toAbsolute: point => call("worldAbsolute", point),
    toLocal: point => call("worldLocal", point),
    requestOrigin: point => call("worldOriginRequest", point)
});
)JS";
} // namespace ScriptBindings::LibraryParts
