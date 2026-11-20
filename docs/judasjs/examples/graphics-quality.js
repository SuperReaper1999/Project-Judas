import {settings} from "judas";

/** @param {import("judas").GraphicsPreset} name */
export function choosePreset(name) {
    if (settings.read()?.quality?.presets[name]) settings.applyQuality(name);
}

/** @param {import("judas").GraphicsQuality["shadowFilter"]} filter */
export function chooseShadowFilter(filter) {
    if (settings.read()?.quality) settings.applyQuality({shadowFilter: filter});
}

/** @param {import("judas").GraphicsQuality["renderScale"]} scale */
export function chooseRenderScale(scale) {
    if (settings.read()?.quality?.choices.renderScale.includes(scale)) settings.applyQuality({renderScale: scale});
}

// Call once from update() when a menu request is awaiting completion.
export function finishQualityChange() {
    const state = settings.read();
    if (!state?.quality || state.quality.pending) return null;
    if (state.qualityError) return {error: state.qualityError};
    settings.save();
    return {preset: state.quality.preset};
}
