# Authored runtime UI

[Index](../JUDASJS.md) · [Lifecycle](lifecycle.md) · [Examples](cookbook.md)

Judas owns the UI system. Projects own `.judasui` content. JavaScript owns meaning.
Author canvas/panels/text/images/buttons/sliders/toggles with the editor asset
workflow; script handles manipulate copied runtime state, not authored source.

## ui

`ui.get(name)` returns an existing UIDocument or null. `ui.load(assetID,uniqueName)`
loads a registered document and returns UIDocument or throws TypeError. Loaded
documents belong to the invoking entity/script slot; removal/disable unloads them.
`ui.quit()` requests normal application shutdown, returns undefined.
`ui.debugOverlayVisible` boolean controls engine diagnostics, separate from game UI.

## UIDocument

`handle` is a plain number, opaque in this world. `get(elementID)` validates and
returns UIElement; missing ID throws, never null. `visible`, `enabled`, `modal`
are boolean read/write. `show()`/`hide()` assign visible; `unload()` invalidates all
its elements. These methods return undefined. Repeated unload/stale properties
throw ReferenceError; IDs never recycle in that world. There is no `.valid` API.

Visible+enabled+modal participates in pause/input ownership; hiding a panel is not
necessarily hiding its whole document. UI is screen-space with deterministic
source order, reference-resolution scaling and rectangle clipping. [Authoring/layout](../RUNTIME_UI.md).

## UIElement

`handle`, `id` are plain writable handle fields; do not retarget them casually.
Properties `text`, `visible`, `enabled`, `value`, `texture` read/write through
validation. Text/texture setters require strings <=16384 bytes. Texture accepts
empty string to clear, otherwise registered nonmissing Texture asset ID. Text is
rendered through M58 Unicode shaping, fallback and localization. See [text contracts](localization.md); rich text remains unsupported.
`value` is finite and within authored min/max; use numeric 0/1 for toggle state,
not boolean. The setter does not restrict a toggle to those two values.
Programmatic changes do not synthesize click/change events. Hidden/disabled controls
reject input; setters still edit their stored state.

Text-specific setters include `font`, `textKey`, `direction` and `textAlignment`
(see below). `setLayout` patches the [supported layout fields](#uielementlayout--setlayout-m67);
no arbitrary style/colour setter, manual focus setter/getter, hover event,
per-widget callback registration or DOM exists. Author general layout/style
and initial font selection in the asset.

## onUI and focus

`onUI({document,element,type,value})`: type is `click`, `change`, `focus`, `back`.
Events broadcast in queued input order to live scripts; filter document AND element.
`uiUpdate(dt)` runs while paused and should handle menu input/open/close. For Back,
interpret the document/type rather than assuming a particular element ID.
Keyboard/gamepad focus follows authored control order using project `ui_*` actions;
sliders use left/right, toggles activate. UI consumes overlapping physical bindings
so a confirm mapped to the same button as launch does not also launch gameplay.
Do not implement polling loops or private control event buses.

Unload/owner destruction/Stop/scene changes invalidate handles. UI runtime values
are not automatically saved; store game data in bounded `this.state` and rebuild
presentation in `start` or modern-load `restore`. Script-loaded documents unload
with their slot. No VM/UI
heap is carried through scene replacement. Copyable [UI example](examples/ui.js).

## M58 text / direction / fonts

`textKey`, `font`, `direction`, `textAlignment` and project locale revisions are documented in [localization](localization.md). Runtime labels use the shared Unicode layout path; the former ASCII-only limitation is superseded. Text entry/IME remains outside the current UI API.

## UIElement.layout / setLayout (M67)

`element.layout` returns a copy of `{offset,size,anchorMin,anchorMax,relativeSize,align}`, each `{x,y}`. `element.setLayout(patch)` accepts any subset of those six keys, validates the whole patch, updates atomically and returns a fresh layout snapshot. Unknown keys, nonfinite values or invalid sizing throw `TypeError`; unloaded document/element handles throw `ReferenceError`. No raw layout storage is exposed.

Offsets/sizes use the document reference-resolution units. Anchors, relativeSize and align use the normal RuntimeUI proportions. A flow parent still owns placement on its layout axis: runtime changes do not bypass that rule. Layout invalidation is batched until the next normal input/draw layout pass; drawing, rectangular clipping, focus and pointer tests then use the same computed geometry. A zero/empty patch is legal. Set several fields in one call. These edits are transient presentation state; restore them from the game's existing serialize/restore policy when needed. They do not edit the authored `.judasui` file or automatically become persistent VM state.
