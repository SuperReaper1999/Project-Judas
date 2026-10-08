# M41 authored runtime UI

Current public JavaScript signatures, examples and lifetime rules: [JudasJS reference](JUDASJS.md). This document retains milestone architecture and evidence context.

Judas owns layout/rendering/input. Project `.judasui` assets own the hierarchy;
JavaScript owns menus, HUD meaning and reactions. No browser or gameplay UI classes.

**Accepted M58 update:** [Unicode text / localization](M58.md) and the
[current JS contract](judasjs/localization.md) supersede the original ASCII text
limitations below. Runtime-label source editing now includes a shaped-text preview.

## Authoring

In Edit mode, Asset Browser **Create UI document** makes a tracked asset. Select it
and expand **Edit UI document**: add panels/children, set stable IDs and parents,
choose canvas/panel/text/image/button/slider/toggle, layout, colours, text, font/image
asset IDs and values. **Save UI source** commits the source edit explicitly;
**Reload UI source** discards in-panel edits. Parents must precede children. Delete
removes a subtree. Historical M41 source editing was a basic property panel without undo/preview;
accepted M58 adds a rendered text preview (not a general WYSIWYG editor).

Add **Runtime UI** to a scene entity, select its asset, assign a unique runtime
name and enabled state. Source is copied into the world's runtime instance.
Alternatively scripts can load a registered document dynamically. Scene/prefab
configuration uses normal serialization and property overrides. Stop does not
rewrite source documents or authored scenes. Restart Play after source edits.

Documents are bounded to 2048 elements, one canvas, unique string element IDs.
IDs are stable references, not display-label/array-position identities. Renaming
an ID requires updating scripts that refer to it. Two document instances need
different runtime names; prefab overrides can supply those names.

## Layout / rendering

A reference resolution (default 1280x720) scales uniformly to fit the window;
the reference canvas is centred with unused aspect-ratio space around it.
Layout recomputes against current window size. Anchors address the parent's
padded content rectangle. Size = anchor span + relative size times parent extent
+ fixed size minus margins. Offset and pivot alignment place that rectangle.
Horizontal/vertical containers lay out visible direct children in source order
with margins and spacing; free layout uses anchors directly. Text alignment is
separate from layout pivot alignment.

Renderer owns all GL. UI is a final screen-space alpha-blended pass, in document
load order then authored hierarchy order. Rectangular scissor intersections clip
children of clipped containers; rendering and pointer hits use the same clip.
Images use normal ResourceManager textures with fit/stretch.

**HISTORICAL M41 text path, superseded by accepted M58:** Text used the existing
TrueType loader/ASCII atlas, wrapping at word boundaries and alignment. No Unicode,
rich text, scrollbars or advanced shaping. Font atlases are cached by Renderer
(maximum 64 paths, destroyed at renderer shutdown); image references are released
on document unload/owner destruction and after texture replacement.
Current font/layout resource and cache lifetimes are specified in [M58](M58.md).

## Script API

```js
import {ui,input} from 'judas';
export default class Menu {
  start() { this.hud=ui.get('hud'); }
  update() { this.hud.get('message').text='Hello'; }
  onUI(event) {
    if(event.document==='hud' && event.element==='confirm' && event.type==='click') {
      this.hud.hide();
    }
  }
}
```

- `ui.load(assetID, uniqueName)` / `ui.get(name)` return a document handle (get
  returns null when absent). `get(elementID)` returns a validated element handle.
- Document `visible`, `enabled`, `modal`; `show()`, `hide()`, `unload()`.
- Element `text`, `visible`, `enabled`, `texture`, `value` read/write. Texture must
  be a registered texture; values must be finite and within the authored range.
  Programmatic edits do not synthesize input events.
- `ui.quit()` requests ordinary application shutdown.
- `ui.debugOverlayVisible` controls the existing engine diagnostics independently
  of project HUD content (demo disables diagnostics to keep its menu clear).
- `uiUpdate(dt)` is optional, once per interactive outer frame even while paused.
  `onUI({document,element,type,value})` receives click/change/focus/back events.
  Events dispatch in input order to live behaviours in normal entity/slot order.
  Filter by document/element in the script. Start runs before UI work. Existing
  `update/fixedUpdate` remain gameplay callbacks and do not advance while modal.
  Constructors/callbacks use M40 fault isolation and interrupt guards.

Handles validate document generation and element ID on every call. Unload never
reuses the same document handle in that world; stale accesses throw. Destroying
an owning entity unloads its documents; script-loaded documents are also released
when their creating script slot is removed/disabled. End Play releases UI and the VM. UI state
is presentation state, not automatically persisted; use bounded script state
when game-specific UI values need persistence. Existing save format remains 2.
Authored component fingerprints gain optional `Judas.RuntimeUI.1`; UI-bearing or
scripted worlds with registered UI assets additionally hash all UI IDs/content.
Script/UI-free existing baselines are unchanged. Export packages all registered
assets, validates UI font/image dependencies and resolves normally after moving.

## Input / pause

UI routes before gameplay, using M35 `ui_up/down/left/right/activate/click` and
`pause` actions plus window pointer state. Default gamepad D-pad and South confirm
are supported. Disabled/hidden controls cannot interact. Focus order is authored
order; the top visible modal document owns navigation (otherwise top document's
pointer controls). Sliders use drag or left/right increments; toggles use activation.

A visible enabled modal document pauses the existing accumulator/fixed-step path
and releases mouse capture. Closing it resumes without adding paused time. UI
consumes actions and all actions/axes sharing their physical bindings for the
frame/catch-up steps; pending edges are discarded. The frame that closes a modal
is not handed to gameplay. Main-menu/pause/options meaning is entirely JS.
Legacy scenes without authored UI retain the engine's compatibility pause menu.
The new project demo replaces that flow with authored panels, including nested
Options/Back/Resume/Quit; engine debug HUD remains separate.

## Demo / human checklist

`projects/ui_demo/ui_demo.judasproj`: enter the game, K key / G door, P prefab / J
pulse, Escape authored pause. The JS HUD counts spawned props. Options has a slider
and HUD toggle. Normal M38 export works unchanged.

1. Main-menu mouse buttons work.
2. Arrows/controller focus and confirm work (hardware listening/interaction is operator-owned).
3. Enter game and inspect HUD; P changes its count.
4. Escape pauses/resumes without a gameplay double-action.
5. Options slider/toggle and Back work.
6. Resize and inspect aspect/layout.
7. Export/move/run, including UI/font/image/script content.

**Historical M41 scope:** visual acceptance was pending at the candidate gate;
operator acceptance was subsequently recorded. Localization/Unicode arrived in
M58, pointer capture is script-accessible, and M67 adds transient layout mutation.
See [current UI](judasjs/ui.md), [localization](judasjs/localization.md) and
[input](judasjs/input.md). World-space UI, data binding and a general styling
engine remain outside the current system.
