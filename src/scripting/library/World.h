#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char World[] = R"JS(
export const signals = {
    subscribe: (name, phase = "fixed") => call("signalSubscribe", name, phase),
    unsubscribe: token => call("signalUnsubscribe", token),
    emit: (name, payload = null, phase = "fixed") => call("signalEmit", name, payload, phase),
    send: (target, name, payload = null, phase = "fixed") => {
        if (!(target instanceof Entity))
            throw new TypeError("signals.send requires an Entity");
        return call("signalSend", target.id, name, payload, phase);
    },
    get stats() {
        return call("signalStats");
    }
};
export const world = {
    entity,
    queryTags: (required = [], excluded = []) => call("queryTags", required, excluded).map(entity),
    spawnPrefab: (asset, transform = {}, options = {}) => entity(call("spawn", asset, transform, options))
};
export const scenes = {
    get current() {
        return call("sceneCurrent");
    },
    get registered() {
        return call("sceneList");
    },
    load: name => call("sceneLoad", name),
    reload: () => call("sceneReload"),
    get regions() {
        return call("regionList");
    },
    get streamingStats() {
        return call("regionStats");
    },
    requestRegion: (name, options = {}) => call("regionRequest", name, options),
    regionStatus: token => call("regionStatus", token),
    activateRegion: token => call("regionActivate", token),
    releaseRegion: token => call("regionRelease", token),
    unloadRegion: name => call("regionUnload", name),
    owner: entity => call("regionOwner", entity.id),
    resolveRegionEntity: (name, local) => {
        const id = call("regionResolve", name, String(local));
        return id ? new Entity(id) : null;
    },
    pinRegion: (name, reason, pin = true) => call("regionPin", name, reason, pin),
    adopt: (entity, name = "root") => call("regionAdopt", entity.id, name),
    setInterest: (name, position, options) => call("regionInterest", name, position, options),
    removeInterest: name => call("regionRemoveInterest", name)
};
export const localization = {
    get locale() {
        return call("localeInfo").locale;
    },
    get available() {
        return call("localeInfo").available;
    },
    get revision() {
        return call("localeInfo").revision;
    },
    get direction() {
        return call("localeInfo").direction;
    },
    setLocale: locale => call("localeChoose", locale),
    format: (key, args = {}) => call("localeFormat", key, args),
    number: (value, options = {}) => call("localeNumber", value, options),
    reload: () => call("localeReload")
};
export const saves = {
    get busy() {
        return call("lifecycleBusy");
    },
    save: (slot, options = {}) => call("saveRequest", "save", slot, options),
    load: (slot, options = {}) => call("saveRequest", "load", slot, options),
    delete: slot => call("saveRequest", "delete", slot, {}),
    refresh: () => call("saveRequest", "list", "", {}),
    list: () => call("saveList"),
    exists: slot => call("saveList").some(s => s.id === slot),
    status: request => call("saveStatus", request),
    cancel: request => call("saveCancel", request),
    exclude: (entity, excluded = true) => call("saveExclude", entity.id, excluded),
    reference: entity => call("saveReference", entity.id),
    resolve: key => entity(call("saveResolve", key))
};
export const session = {
    get: key => call("sessionGet", key),
    set: (key, value) => call("sessionSet", key, value),
    delete: key => call("sessionDelete", key)
};
export const mods = {
    get active() {
        return call("modCatalog").active;
    },
    contributions: point => call("modCatalog").contributions.filter(c => c.point === point),
    resolve: (owner, name) => call("modCatalog").exports[owner]?.[name] ?? null
};
)JS";
} // namespace ScriptBindings::LibraryParts
