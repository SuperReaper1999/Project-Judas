#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Navigation[] = R"JS(
export class NavigationAgent {
    constructor(id) {
        this.id = id;
    }
    get state() {
        const s = call("navAgentState", this.id);
        return {
            ...s,
            link: entity(s.linkId)
        };
    }
    set enabled(value) {
        call("navEnabled", this.id, "agent", value);
    }
    setDestination(point) {
        return call("navDestination", this.id, point);
    }
    clear() {
        return call("navClear", this.id);
    }
    set stopped(value) {
        call("navStopped", this.id, value);
    }
    get stopped() {
        return this.state.stopped;
    }
    get steering() {
        return this.state.steering;
    }
    get remainingDistance() {
        return this.state.remainingDistance;
    }
    completeLink() {
        return call("navCompleteLink", this.id);
    }
    configure(settings) {
        return call("navConfigure", this.id, settings);
    }
}
const navResult = r => r ? {
    ...r,
    surface: entity(r.surfaceId)
} : null;
const navPath = p => ({
    ...p,
    corners: p.corners.map(c => ({
        ...c,
        link: entity(c.linkId)
    }))
});
export const navigation = {
    sample: (point, range = 2, filter = {}) => navResult(call("navSample", point, range, filter)),
    path: (start, end, filter = {}) => navPath(call("navPath", start, end, filter)),
    raycast: (start, end, filter = {}) => navResult(call("navRaycast", start, end, filter)),
    get areas() {
        return call("navAreas");
    },
    get profiles() {
        return call("navProfiles");
    },
    get errors() {
        return call("navErrors");
    }
};
extendEntity({
    get navigationObstacle() {
        return call("navObstacleInfo", this.id);
    },
    get navigationLink() {
        return call("navLinkInfo", this.id);
    },
    setNavigationObstacle(settings) {
        return call("navObstacleSet", this.id, settings);
    },
    setNavigationLink(settings) {
        return call("navLinkSet", this.id, settings);
    },
    setNavigationEnabled(component, enabled) {
        return call("navEnabled", this.id, component, enabled);
    },
    get navigation() {
        return call("navAgentExists", this.id) ? new NavigationAgent(this.id) : null;
    }
});
)JS";
} // namespace ScriptBindings::LibraryParts
