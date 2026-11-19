#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char UI[] = R"JS(
export class UIElement {
    constructor(handle, id) {
        this.handle = handle;
        this.id = id;
    }
    get scroll() {
        return call("uiControl", this.handle, this.id, "scroll");
    }
    set scroll(value) {
        call("uiControl", this.handle, this.id, "scroll", JSON.stringify(value));
    }
    get selection() {
        return call("uiControl", this.handle, this.id, "selection");
    }
    set selection(value) {
        call("uiControl", this.handle, this.id, "selection", JSON.stringify(value));
    }
    get selectedIndex() {
        return call("uiControl", this.handle, this.id, "selected");
    }
    set selectedIndex(value) {
        call("uiControl", this.handle, this.id, "selected", JSON.stringify(value));
    }
    setItems(items) {
        call("uiControl", this.handle, this.id, "items", JSON.stringify(items));
    }
    configure(options) {
        call("uiControl", this.handle, this.id, "configure", JSON.stringify(options));
    }
    setPreviewCamera(camera) {
        if (camera !== null && !(camera instanceof Entity))
            throw new TypeError("preview camera requires an Entity or null");
        call("uiControl", this.handle, this.id, "previewCamera", JSON.stringify(camera === null ? null : camera.id));
    }
    get fontSize() {
        return call("uiGet", this.handle, this.id, "fontSize");
    }
    get layout() {
        return call("uiLayout", this.handle, this.id);
    }
    setLayout(patch) {
        return call("uiLayout", this.handle, this.id, patch);
    }
    get text() {
        return call("uiGet", this.handle, this.id, "text");
    }
    set text(v) {
        call("uiSet", this.handle, this.id, "text", v);
    }
    get visible() {
        return call("uiGet", this.handle, this.id, "visible");
    }
    set visible(v) {
        call("uiSet", this.handle, this.id, "visible", v);
    }
    get enabled() {
        return call("uiGet", this.handle, this.id, "enabled");
    }
    set enabled(v) {
        call("uiSet", this.handle, this.id, "enabled", v);
    }
    get value() {
        return call("uiGet", this.handle, this.id, "value");
    }
    set value(v) {
        call("uiSet", this.handle, this.id, "value", v);
    }
    get font() {
        return call("uiGet", this.handle, this.id, "font");
    }
    set font(v) {
        call("uiSet", this.handle, this.id, "font", v);
    }
    get textKey() {
        return call("uiGet", this.handle, this.id, "textKey");
    }
    set textKey(v) {
        call("uiSet", this.handle, this.id, "textKey", v);
    }
    get direction() {
        return call("uiGet", this.handle, this.id, "direction");
    }
    set direction(v) {
        call("uiSet", this.handle, this.id, "direction", v);
    }
    get textAlignment() {
        return call("uiGet", this.handle, this.id, "textAlignment");
    }
    set textAlignment(v) {
        call("uiSet", this.handle, this.id, "textAlignment", v);
    }
    get texture() {
        return call("uiGet", this.handle, this.id, "texture");
    }
    set texture(v) {
        call("uiSet", this.handle, this.id, "texture", v);
    }
}
// Region coordinates use X/Z on the horizontal world plane, with north at -Z.
export class WorldMap {
    #region;
    #view;
    #image;
    #sampling;
    constructor(image, {min, max, orientation = 0}) {
        if (!(image instanceof UIElement) || !Number.isInteger(orientation) || orientation < 0 || orientation > 3 ||
            ![min?.x, min?.z, max?.x, max?.z].every(Number.isFinite) || max.x <= min.x || max.z <= min.z)
            throw new TypeError("WorldMap requires an image and finite, ordered X/Z bounds with orientation 0–3");
        const dx = max.x - min.x, dz = max.z - min.z;
        if (!Number.isFinite(dx) || !Number.isFinite(dz)) throw new TypeError("WorldMap bounds exceed the coordinate range");
        const right = [[1, 0], [0, 1], [-1, 0], [0, -1]][orientation];
        const down = [-right[1], right[0]];
        this.#image = image;
        this.#region = {x: min.x + dx / 2, z: min.z + dz / 2, right, down,
                        width: orientation % 2 ? dz : dx, height: orientation % 2 ? dx : dz, orientation};
        this.setView();
    }
    setView({center, width, height, zoom = 1, heading, circular = false} = {}) {
        const r = this.#region;
        center ??= r;
        width ??= r.width;
        height ??= r.height;
        heading ??= r.orientation * Math.PI / 2;
        if (![center.x, center.z, width, height, zoom, heading].every(Number.isFinite) ||
            width <= 0 || height <= 0 || zoom <= 0 || typeof circular !== "boolean")
            throw new TypeError("Invalid WorldMap view");
        const c = Math.cos(heading), s = Math.sin(heading);
        const right = [c, s], down = [-s, c];
        const w = width / zoom, h = height / zoom;
        if (!Number.isFinite(w) || !Number.isFinite(h) || w <= 0 || h <= 0)
            throw new TypeError("Invalid WorldMap view scale");
        const imageDelta = (x, z) => [(x * r.right[0] + z * r.right[1]) / r.width,
                                      (x * r.down[0] + z * r.down[1]) / r.height];
        const uvRight = imageDelta(w * right[0], w * right[1]);
        const uvDown = imageDelta(h * down[0], h * down[1]);
        const centerUV = imageDelta(center.x - r.x, center.z - r.z);
        const origin = centerUV.map((v, i) => .5 + v - (uvRight[i] + uvDown[i]) / 2);
        const sampling = JSON.stringify({origin, right: uvRight, down: uvDown, circular});
        if (sampling !== this.#sampling) {
            call("uiControl", this.#image.handle, this.#image.id, "mapSampling", sampling);
            this.#sampling = sampling;
        }
        this.#view = {x: center.x, z: center.z, right, down, width: w, height: h, circular};
    }
    worldToMap({x, z}) {
        if (![x, z].every(Number.isFinite)) throw new TypeError("WorldMap requires a finite X/Z position");
        const v = this.#view, r = this.#region;
        const dx = x - v.x, dz = z - v.z;
        const u = .5 + (dx * v.right[0] + dz * v.right[1]) / v.width;
        const t = .5 + (dx * v.down[0] + dz * v.down[1]) / v.height;
        const rx = x - r.x, rz = z - r.z;
        const inRegion = Math.abs(rx * r.right[0] + rz * r.right[1]) <= r.width / 2 &&
                         Math.abs(rx * r.down[0] + rz * r.down[1]) <= r.height / 2;
        const inView = v.circular ? (u - .5) ** 2 + (t - .5) ** 2 <= .25
                                  : u >= 0 && u <= 1 && t >= 0 && t <= 1;
        return {x: u, y: t, visible: inRegion && inView};
    }
    mapToWorld({x, y}) {
        if (![x, y].every(Number.isFinite)) throw new TypeError("WorldMap requires a finite map position");
        const v = this.#view, dx = (x - .5) * v.width, dy = (y - .5) * v.height;
        return {x: v.x + dx * v.right[0] + dy * v.down[0],
                z: v.z + dx * v.right[1] + dy * v.down[1]};
    }
    detach() {
        call("uiControl", this.#image.handle, this.#image.id, "mapSampling", "null");
        this.#sampling = undefined;
    }
    placeMarker(marker, position) {
        const p = this.worldToMap(position);
        if (p.visible) {
            const anchor = {x: p.x, y: p.y};
            marker.setLayout({anchorMin: anchor, anchorMax: anchor, align: {x: .5, y: .5}});
        }
        marker.visible = p.visible;
        return p.visible;
    }
}
export class UIDocument {
    constructor(handle) {
        this.handle = handle;
    }
    get referenceSize() {
        return call("uiControl", this.handle, "", "reference");
    }
    set referenceSize(value) {
        call("uiControl", this.handle, "", "reference", JSON.stringify(value));
    }
    get focused() {
        return call("uiControl", this.handle, "", "focus");
    }
    focus(id) {
        call("uiControl", this.handle, "", "focus", JSON.stringify(id ?? ""));
    }
    get scale() {
        return call("uiControl", this.handle, "", "scale");
    }
    set scale(value) {
        call("uiControl", this.handle, "", "scale", JSON.stringify(value));
    }
    captureState() {
        return call("uiControl", this.handle, "", "capture");
    }
    restoreState(state) {
        call("uiControl", this.handle, "", "restore", JSON.stringify(state));
    }
    get accessibility() {
        return call("uiControl", this.handle, "", "accessibility");
    }
    get drag() {
        return call("uiControl", this.handle, "", "drag");
    }
    beginDrag(id) {
        call("uiControl", this.handle, "", "beginDrag", JSON.stringify(id));
    }
    cancelDrag() {
        call("uiControl", this.handle, "", "cancelDrag");
    }
    get player() {
        const id = call("uiPlayer", this.handle);
        return id ? new LocalPlayer(id) : null;
    }
    set player(value) {
        call("uiPlayer", this.handle, value?.id ?? null);
    }
    get(id) {
        call("uiGet", this.handle, id, "visible");
        return new UIElement(this.handle, id);
    }
    get visible() {
        return call("uiGet", this.handle, "", "visible");
    }
    set visible(v) {
        call("uiSet", this.handle, "", "visible", v);
    }
    get enabled() {
        return call("uiGet", this.handle, "", "enabled");
    }
    set enabled(v) {
        call("uiSet", this.handle, "", "enabled", v);
    }
    get modal() {
        return call("uiGet", this.handle, "", "modal");
    }
    set modal(v) {
        call("uiSet", this.handle, "", "modal", v);
    }
    show() {
        this.visible = true;
    }
    hide() {
        this.visible = false;
    }
    unload() {
        call("uiUnload", this.handle);
    }
}
export const ui = {
    get: name => {
        const id = call("uiFind", name);
        return id ? new UIDocument(id) : null;
    },
    load: (asset, name) => new UIDocument(call("uiLoad", asset, name)),
    quit: () => call("uiQuit")
};
)JS";
} // namespace ScriptBindings::LibraryParts
