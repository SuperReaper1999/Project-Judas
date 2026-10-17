# M96 — reusable game interfaces

Current API: M96 is human accepted and included in the accepted M99 baseline.
Platform coverage remains scoped to the recorded evidence. The engine owns controls, layout, editing and input routing; project JS
owns screens, inventory data, dialogue and settings policy.

## Authoring

Open a registered `.judasui` asset in **Assets → Edit UI document**. The normal
visual canvas, hierarchy, named source, undo/redo and explicit Save now include
**Text input**, **Scroll container** and **Virtual list**. Existing panel, text,
image, button, slider and toggle assets remain compatible. Extended authoring
emits `JudasUI 3`; old versions 1/2 remain readable. Named JSON uses the existing
schema and optional control fields; no new asset type or object-reference scheme.

Author `placeholder`, `readOnly`, `maxBytes` (1–16384 UTF-8 bytes), `rowHeight`
(12–512 reference pixels), `accessibleLabel`, `description` and `focusColor`.
Text inputs are literal single-line text, not localization-key bindings.
Localize their labels/placeholders through project code. Existing fonts, ICU,
HarfBuzz, bidirectional shaping and fallback remain the rendering authority.

Scroll containers implicitly clip descendants. Normal free/horizontal/vertical
layout computes their extent; offsets are clamped to content. Nesting is supported.
A Virtual list displays fixed-height keyed text rows supplied by JS, up to 10000
rows / 2 MiB combined label/key bytes. Rows are not entities or widget subtrees.
Only the visible row interval is shaped/drawn; no per-frame scan over list data.
Disabled rows cannot select or activate. Replacing data preserves a selected key
when it remains enabled. Variable-height/custom multi-widget rows are not provided.

## Editing and input

The platform adapter consumes SDL text commits and preedit separately. Focusing
an input starts the native text session/Android keyboard and supplies the input
rectangle. Switching fields or player restarts composition ownership. Losing
window focus/backgrounding stops the session and clears pending input. Unload,
player departure and scene replacement invalidate the UI owner.

Editing is UTF-8; selection indices are **ICU extended grapheme indices**, not JS
UTF-16 offsets. Combining marks, emoji ZWJ sequences and flags are not split.
Arrow/Shift+arrow, Home/End, Backspace/Delete, Ctrl+A/C/X/V, Ctrl+Z,
Ctrl+Shift+Z/Ctrl+Y, Enter and Tab are supported. Clipboard uses SDL, not a private
project clipboard. Invalid UTF-8/newlines and over-capacity insertions reject
atomically. Undo/redo is bounded to 32 snapshots/field. Read-only permits selection
and copying. Native repeat is used for editing keys; logical navigation follows
project input edges. Ctrl+arrow currently moves one grapheme, not a word.

IME composition is displayed with an underline, never becomes saved text until
commit, and never emits a change event itself. No IME engine or candidate UI is
reimplemented. OS candidate windows and Android keyboards depend on the installed
input method/backend; automated event injection is not human IME acceptance.

Mouse press/drag selects text; caret hit testing uses shaped glyph clusters and
interpolates internal ligature positions. RTL text is shaped correctly; arrows
move logically. Split visual caret affinity at mixed-bidi boundaries, exact GDEF
ligature caret positions and multiline code-editor behaviour are not implemented.

Projects bind `ui_up/down/left/right`, `ui_activate`, `ui_next`, `ui_click` and
`ui_scroll` (axis). Existing maps are not rewritten; Defaults adds Tab and wheel.
Buttons/toggles activate; sliders use left/right; lists use up/down internally;
Tab/next moves between controls. Mouse wheel and touch/pointer dragging scroll.
A dragged list does not click its release row. Focus reveals controls in scroll
ancestors. Text editing consumes keyboard bindings before ordinary game update /
fixed steps. `uiUpdate` is an earlier UI-policy callback: do not move gameplay
from it or assume it has already received text-consumption suppression.

## Focus, scaling and accessibility

Documents have independent focus. A global modal pauses the world; a local modal
blocks only its player's controls. Existing M89 viewport, device assignment and
captured touch ownership remain authoritative. There is one OS text-composition
session/window; the most recently focused field owns it, and commits carry that
player identity and a generation-like field-session token. Retired native commits
cannot alias a new field after focus or scene replacement. Other players retain
separate focus and selections.

Controls draw explicit configurable focus outlines; focus is not colour-only
background shading. `document.scale` (0.75–2) scales font and control/reference
units, not the camera or game. Anchors/relative sizing still determine available
space. Fixed pixel layouts may overflow at larger scales: author responsive
layouts or reflow through existing `setLayout`. The showcase keeps its screen
regions stable while enlarging labels, rows and text, and uses runtime referenceSize
and setLayout to reflow into portrait orientation. Raster text is bounded to
512 physical pixels regardless of UI scale.

`document.accessibility` exposes copied roles, labels, descriptions, focus and
values. This is a semantic foundation for tooling/adapters, **not OS screen-reader
integration or accessibility certification**. It does not change gameplay based
on disability assumptions. Text must have appropriate authored fonts/contrast.
Only scalable outline fonts are supported; entering emoji/CJK does not install
a font containing those glyphs. Colour-emoji fonts remain unsupported.
There are no mandatory motion effects or flashing focus animations.

## State, lifecycle and export

`captureState` / `restoreState` transfer explicit control presentation state:
text/value/visibility/enabled, scale, focus, grapheme selection, scroll and selected
list key. Restore validates the entire document and rolls back atomically on error.
Rebuild list data first; it is project data and is deliberately not duplicated in
the UI snapshot. Missing/disabled saved keys clear selection rather than aliasing
an unrelated index. Composition, clipboard, undo history, resource handles and
native keyboard state are never serialized.

Snapshots do not automatically register a save participant. Put a bounded snapshot
in existing `this.state` before an ordinary save and restore it after rebuilding
content. The existing 64 KiB script-state limit still applies; do not put a whole
large inventory database in it. Scene/Stop/reload discard UI instances; authored
assets remain unchanged. UI assets/fonts/scripts use normal export dependency
closure; runtime builds do not include editor code.

## Showcase and validation

[Field Notes project and controls](../projects/ui_showcase/README.md).
[Public API](judasjs/ui.md). [Evidence](evidence/m96/REPORT.md).
