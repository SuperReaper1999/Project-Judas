#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Core[] = R"JS(
const call = (op, ...args) => globalThis.__judas(op, ...args);
export const engine = {
    get buildInfo() {
        return Object.freeze(call("engineBuildInfo"));
    }
};
export const scripts = {
    faults: () => call("scriptFaults")
};
export const profiler = {
    scope: (label, callback) => call("profileScope", label, callback),
    counter: (label, value, mode = "sum") => call("profileCounter", label, value, mode)
};
export const time = {
    get elapsed() {
        return call("elapsed");
    },
    get delta() {
        return call("delta");
    },
    get fixed() {
        return call("fixed");
    }
};
export const console = {
    log: (...args) => call("log", args.map(String).join(" "))
};
// Subsystems own their Entity members. Preserve the non-enumerable descriptors
// that class declarations provide, including getters, setters and method names.
const extendEntity = members => {
    const descriptors = Object.getOwnPropertyDescriptors(members);
    for (const descriptor of Object.values(descriptors))
        descriptor.enumerable = false;
    Object.defineProperties(Entity.prototype, descriptors);
};
const extendWorld = members => Object.defineProperties(world, Object.getOwnPropertyDescriptors(members));
)JS";
} // namespace ScriptBindings::LibraryParts
