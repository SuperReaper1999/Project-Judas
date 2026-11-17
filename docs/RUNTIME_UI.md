# Runtime UI authoring and controls

Projects own `.judasui` content; JavaScript owns menu/HUD meaning; the engine owns
layout, rendering, text editing, focus and routed input. These are native controls,
not a browser DOM or game-specific menu framework. Use the matching SDK's
[exact UI API](judasjs/ui.md) and [Unicode/localization](LOCALIZATION.md) contracts.

## Create a document

In Edit, Asset Browser **Create UI document** makes a registered asset. Select it
and open **Edit UI document**. Author a canvas with stable IDs and parent-first
children. Choose Canvas, Panel, Text, Image, Button, Slider, Toggle, Text input,
Scroll container or Virtual list. Configure layout, text, fonts/images, colours,
control limits, accessibility labels and focus styling.

The runtime canvas preview uses the shared UI layout/render path; named source
and visual editing share the same document and validated undo/draft workflow.
**Save UI source** publishes source; **Reload UI source** discards the draft.
Source conflicts remain visible rather than overwriting an external edit.
Add Runtime UI to an entity and select the asset, or use `ui.load(assetId,name)`.
Two live documents need distinct runtime names. Restart Play after source changes.
Stop does not rewrite documents or authored scenes.

Documents are bounded to 2,048 elements and one canvas. IDs are identities, not
labels or array indices: renaming one requires updating its script references.
Dependencies must be registered assets of the correct type. Export uses normal
font/image/camera dependency ownership, including after relocation.

## Layout and responsive design

A reference canvas (default 1280×720) scales uniformly and centres in the viewport.
Anchors address a parent's padded content rectangle. Span, relative size, fixed
size, margins, pivot and offset determine geometry; horizontal/vertical flow owns
its axis for visible direct children. Text alignment is separate from layout alignment.
Rendering, clipping, pointer hits, focus reveal and runtime layout snapshots use
the same geometry. Images support fit/stretch; Unicode shaping is shared with the
runtime text system. UI draws in document/hierarchy order after world rendering.

Use Scroll for overflowing groups and fixed-row Virtual list for large data sets;
do not rely on scaling a fixed desktop layout to make it usable at phone aspects.
`referenceSize` and `setLayout` support explicit reflow. Document scale 0.75–2 changes
reference-unit geometry/text, not physical game scale. Test small landscape and
portrait viewports with real bounds/hit tests, not only a 1280×720 screenshot.
The Willow Town and UI Showcase projects demonstrate these policies.

## Editing, lists and transfers

Text input is single-line valid UTF-8 with an authored byte limit. Selection uses
extended grapheme indices, with clipboard, undo, SDL text/IME commit and read-only
behaviour. Embedded NUL/line breaks reject without replacing the old value.
Native screen-reader adapters, multiline editing and every hardware IME are not
implemented/qualified by the copied accessibility observations.

Scroll/List offsets are clamped reference units. A list holds at most 10,000 rows,
unique stable keys and 2 MiB combined text/IDs; labels are bounded to 512 bytes and
keys to 128 bytes. `setItems` replaces data atomically and preserves a valid enabled
selected key. Offscreen data does not allocate a hidden widget tree. Fixed authored
row height is supported; complex variable-height cells are not.

[Item transfers and live model images](UI_TRANSFERS_AND_PREVIEWS.md) use explicit
drag/drop categories and camera targets. Drop events request a project-owned
inventory mutation; the engine does not decide item rules. Modal/player ownership,
retirement and input reset cancel transient transfers through their normal owners.

## Script lifecycle and pause

```js
import { ui } from 'judas';
export default class Menu {
  start() { this.doc = ui.get('hud'); }
  restore() { this.doc = ui.get('hud'); }
  onUI(event) {
    if (event.document === 'hud' && event.element === 'confirm' && event.type === 'click') {
      this.doc.hide();
    }
  }
}
```

Filter broadcast UI observations by document/element/player. Input types include
click/change/focus/back, text submit and drag/drop lifecycle. Programmatic setters
and list replacement do not manufacture user events. `uiUpdate` runs at the
interactive outer frame even while a modal pauses gameplay; ordinary update/fixed
callbacks do not advance behind a global modal. Local-player documents scope
layout/focus/input; a local modal blocks its player, a global modal pauses the world.

Handles validate document generation and element identity. UI documents have no
`.valid` property: stale access throws. Unload, script/entity retirement, Stop,
scene change and player departure release/invalidate owned documents.

## Save and verify

`captureState`/`restoreState` copy supported control state atomically: text/value,
visibility/enabled, scale, focus, selection, scroll and selected key. Rebuild list
data first. Layout/style, list data, clipboard, undo and IME preedit are not saved
by that snapshot. Its 4 MiB bound does not enlarge the script slot's 64 KiB JSON
limit. Keep game data separate and reconstruct transient UI bindings in `restore`.

Try [UI Showcase](../projects/ui_showcase/README.md),
[Menu Workshop](../projects/menu_workshop/README.md) and
[Willow Town](../projects/willow_town/README.md). Exercise edit/undo/save/reopen,
Play/Stop, keyboard/pointer controls, responsive layouts and cold restoration.
Physical touch/controller/audio/IME/accessibility trials retain their own scopes;
see [development readiness](DEVELOPMENT_READINESS.md).
