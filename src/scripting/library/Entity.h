#pragma once

namespace ScriptBindings::LibraryParts {
inline constexpr char Entity[] = R"JS(
export class Entity {
    constructor(id) {
        this.id = String(id);
    }
    get valid() {
        return call("valid", this.id);
    }
    get transform() {
        return call("transform", this.id);
    }
    get presentedTransform() {
        return call("presentedTransform", this.id);
    }
    set transform(value) {
        call("setTransform", this.id, value);
    }
    get parent() {
        return entity(call("parent", this.id));
    }
    get children() {
        return call("children", this.id).map(entity);
    }
    destroy() {
        return call("destroy", this.id);
    }
    hasTag(tag) {
        return call("hasTag", this.id, tag);
    }
    addTag(tag) {
        return call("addTag", this.id, tag);
    }
    removeTag(tag) {
        return call("removeTag", this.id, tag);
    }
    get classification() {
        return call("classification", this.id);
    }
    scriptState(slot) {
        return call("scriptState", this.id, slot);
    }
}
export const entity = id => id && id !== "0" ? new Entity(id) : null;
)JS";
} // namespace ScriptBindings::LibraryParts
