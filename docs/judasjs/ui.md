# UI — current M96 controls

[Index](../JUDASJS.md) · [Input](input.md) · [Local players](local-multiplayer.md)
· [Save lifecycle](saves.md) · [Authoring and limitations](../M96_UI_TOOLKIT.md)

Judas owns the UI system; projects own content; JavaScript owns meaning.
Runtime edits copy authored data and never change the source asset.

## ui

`ui.get(name): UIDocument|null`; `ui.load(asset,name): UIDocument` (unique name,
registered UI asset). Invalid load throws TypeError. Loaded documents belong to
calling script/entity slot. `ui.quit()` requests ordinary shutdown.
`ui.debugOverlayVisible` toggles engine diagnostics independently of game UI.

## UIDocument

`handle` is opaque in this world; `get(id)` returns UIElement or throws
ReferenceError. Missing/unloaded/stale properties throw; no `.valid` API.
`visible`, `enabled`, `modal` are booleans. `show/hide/unload()` return undefined.
Unload, owner removal, Stop, scene change and player departure invalidate handles.

- `player: LocalPlayer|null`: scope layout/input/focus to its viewport. Null is
  global. A local modal blocks that player; a global modal pauses the world.
- `focused: string`: selected control ID, or empty.
- `focus(id: string|null): void`: explicit focus/clear. Hidden, disabled or
  non-control targets throw TypeError. Scroll ancestors reveal the focused item.
  Focus changes queue a `focus` event; ordinary value setters do not queue changes.
- `referenceSize: {x,y}`: live canvas reference dimensions, finite 64–16384.
  Use with setLayout for portrait/window reflow. It is presentation geometry, not
  game scale, and is deliberately not restored from another device's UI snapshot.
- `scale: number`: 0.75–2, scales reference-unit control geometry and text.
  Author anchored/relative layouts or reflow; this does not magically resize a
  fixed source layout to fit every font size.
- `captureState(): UIState`: detached JSON-safe control snapshot.
- `restoreState(state): void`: validated atomic restore into the same element
  schema. Rebuild list data first. Text/value, visibility/enabled, scale, focus,
  selection, scroll and selected key restore; missing keys clear selection.
  Does not persist layout/style changes, list data, undo, clipboard or IME preedit.
- `accessibility: UIAccessibleElement[]`: detached visible/enabled semantic roles,
  labels, descriptions, focus, numeric values and relevant text/selected index.
  This is **not native screen-reader integration**.

Snapshot bounds are 4 MiB; existing saved script state is limited to 64 KiB.
Keep game data separate and rebuild presentation in `start`/modern `restore`.

## UIElement

`handle`, `id` are plain fields: do not retarget them casually.
`text`, `font`, `textKey`, `direction`, `textAlignment`, `texture`, `visible`,
`enabled`, `value` retain their existing validated setters. `text` clears textKey.
Fonts/textures accept empty or a registered, nonmissing asset of the right type.
`value` must be finite and in authored min/max; toggle uses numeric 0/1.
`direction`: auto/ltr/rtl. `textAlignment`: left/right/center/start/end; legacy
numeric alignment reads empty. See [Unicode/localization](localization.md).

### layout / setLayout(patch)

Copy of `{offset,size,anchorMin,anchorMax,relativeSize,align}`, each `{x,y}`.
A patch accepts any subset, rejects unknown/nonfinite/invalid fields atomically
and returns a fresh snapshot. Reference-resolution units; flow parent retains
axis ownership. Layout, clipping, pointer hit tests and focus use the same geometry.

### configure(options)

Returns undefined. Validated atomic subset of: `placeholder`, `accessibleLabel`,
`description`, `readOnly`, `maxBytes`, `fontSize`, `rowHeight`, `background`, `color`,
`focusColor`. Colours are `{r,g,b,a}`. Font size >0..512; rowHeight 12..512 reference
pixels; maxBytes integer 1..16384. Unknown keys throw. No DOM or arbitrary CSS.

### Text input

`selection: {anchor,caret}` reads/writes **extended grapheme indices** in the
committed text. Bounds reject. `text` is single-line valid UTF-8 within maxBytes;
programmatic replacement places selection at the end. Placeholder is presentation,
not the value. Read-only blocks user editing, not programmatic updates.
The native SDL text/IME path supports clipboard, selection, undo and commit;
[precise editing/platform limits](../M96_UI_TOOLKIT.md#editing-and-input).

### Scroll container / virtual list

`scroll: {x,y}` is a clamped content offset in reference units (not screen pixels).
Only Scroll/List kinds accept it. Scroll containers implicitly clip children.
`setItems([{id,text,enabled?},…])` replaces list rows atomically; unique nonempty
IDs ≤128 bytes, label ≤512 bytes, ≤10000 rows and ≤2 MiB combined text/IDs.
`selectedIndex` reads/writes zero-based index or -1. Disabled/out-of-range throws.
Replacements preserve an enabled selected key; removed keys clear it. Selection
reveals the row; offscreen data does not create hidden widget trees.
Rows have one fixed authored rowHeight. Complex cells/variable-height rows are
outside this implementation. `setItems` and setters do not synthesize change events.

## onUI / frame timing

`onUI({document,element,type,value,playerId,text,item})` broadcasts queued input
observations to live scripts; filter document/element/player. Types:
`click`, `change`, `focus`, `back`, **`submit`**.

- Text input change/submit: `text` is committed text, `item` empty.
- List change/click: `value` is row index, `text` label, `item` stable row key.
- Other controls retain existing numeric `value`; added strings are empty.
- IME preedit does not emit changes. Enter submits, not another click.

Use `uiUpdate` for menu policy while paused; normal game update/fixed steps run
after the UI input boundary. Text consumes keyboard-sharing gameplay bindings.
UIUpdate itself precedes that boundary; it must not serve as authoritative movement.
Bind logical `ui_*` controls in the project map; keyboard/gamepad follow source
order, list arrows select internally, Tab/next moves between controls. Touch drag
scrolls and suppresses click after dragging. The OS text session has one explicit
player owner while documents retain independent focus.

```js
import {ui} from 'judas';
export default class Menu {
  start() {
    this.doc=ui.get('menu'); // authored ordinary scene UI component
    this.doc.get('inventory').setItems([
      {id:'rope',text:'Climbing rope'}, {id:'lamp',text:'Survey lamp'}
    ]);
    this.doc.get('name').configure({placeholder:'Your callsign',maxBytes:128});
    this.doc.focus('name');
  }
  onUI(e) {
    if(e.document==='menu' && e.type==='submit')
      this.doc.get('welcome').text=`Hello, ${e.text}`;
  }
}
```
