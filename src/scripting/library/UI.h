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
    quit: () => call("uiQuit"),
    get debugOverlayVisible() {
        return call("uiDiagnostics");
    },
    set debugOverlayVisible(v) {
        call("uiDiagnostics", v);
    }
};
)JS";
} // namespace ScriptBindings::LibraryParts
