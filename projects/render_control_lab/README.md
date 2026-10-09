# Render Control Lab — M72

An ordinary project using public JudasJS for lighting, render visibility and
instance materials. Open `render_control_lab.judasproj` in the editor and Play,
or pass that project path to the standalone Judas executable.

[JudasJS controls](../../docs/judasjs/materials.md) ·
[Visibility scope](../../docs/judasjs/entities.md#render-visibility-m72) ·
[M72 design and validation status](../../docs/M72.md)

## Controls

| Control | Action |
| --- | --- |
| WASD / mouse | Move/look with the project's inspection camera. |
| Escape | Toggle pointer capture. |
| P | Run/stop the project-owned lighting cycle. |
| Left / Right | Hold to scrub the lighting cycle backward/forward. |
| F | Toggle actual sun enabled state. |
| B | Toggle environment background visibility independently of illumination. |
| V | Toggle exposure independently of sun intensity. |
| H | Toggle entity visibility on multipart instance 40, animated figure 35 and solid probe 60. |
| J | Toggle instance 40's Render component gate. |
| K | Toggle instance 40's first stable imported part when ready. |
| C | Change instance 40's whole material colour/PBR/emission factors. |
| O | Cycle its whole material through cutout, blend and opaque; blend sets partial opacity. |
| T | Replace its base-colour map with registered grid/cutout textures. |
| Y | Submit a missing texture ID and display the rejected batch's error. |
| Backspace | Clear instance 40's whole/stable-part runtime material patches and restore root authored lighting; stop the JS cycle. |
| N | Spawn an ordinary skinned prefab. |
| R | Reload authored scene state. |
| F6 / F7 | Save/load a modern slot, including supported render state and plain JS cycle state. |

The engine does not own this clock, orbit, inspection camera or control mapping.
The script's restore callback reacquires UI/input bindings; restored rendering
intent comes from Judas's normal save participants.

## What to inspect

- **Instance isolation:** instance 41 shares the imported model with 40 and keeps
  its own material/visibility state. Change colour, opacity or textures on 40,
  then compare their appearance.
- **Independent gates:** hide a part with K or the Render component with J,
  toggle H twice, and check the intentionally hidden part/component remains hidden.
  Each entity's gate is local; children and other entities retain their own state.
- **Simulation while hidden:** solid 60 remains a physical ray-query target while
  hidden. The HUD reports its hit identity. H also hides animated figure 35;
  independent orange marker 61 stays visible and follows its resolved joint through
  the ordinary visual-socket API. Joint/socket work continues.
- **Direct light:** change F and scrub the cycle to inspect direct light/shadow
  movement. V adjusts final exposure separately. Ambient/environment light can
  remain after disabling the sun.
- **Real opacity:** O selects alpha mode as well as opacity. Cutout discards
  coverage, blend draws with depth testing and no depth writes/shadows, and opaque
  writes depth. Simple non-intersecting transparency is the review scope.
- **Camera screen:** camera 50 renders a 384×216 texture every rendered frame to
  display 51. It uses the existing shared draw paths and main-focused shadow maps.
  Its pixels/shadow coverage are not a main-view quality oracle.

The HUD reports sun/intensity, exposure, JS time, material mode/resource status,
hidden-solid query, socket readiness and recent errors/save status.

## Replacement and reset boundaries

Y uses a syntactically valid but **unregistered** asset ID. It demonstrates immediate
validation rejection: the grouped material write retains its preceding state.
Asynchronous pending/failed decoding uses the separate registered-malformed-image
render fixture in `tests/RuntimeRenderTests.cpp`; the lab does not ship a corrupt
texture. Its receipt status is recorded in [M72](../../docs/M72.md#validation-status).

Pending/failed texture replacements inherit the selected source map. A newer
selection, clear, destruction or unload cannot publish an obsolete replacement.
Clearing a material target restores its authored assignment/factors/maps, with
other target patches still applied. Backspace handles the lab's whole/part targets;
the runtime inspector can clear any separately selected numeric target. R and
Stop reconstruct the authored scene. Runtime previews do not silently save it.

## Content and review

The lab reuses original Judas procedural textures, environment, multipart fixture,
bend-bar geometry/rig/clips and the licensed font. Content notices remain in
`Assets/LICENSE.txt`, `Assets/models/LICENSE.txt` and `Assets/fonts/LICENSE.txt`.
Shared source assets are not rewritten by the controls.

M72 received operator desktop acceptance on 2026-10-09. Native Windows validation remains outstanding. Current executed scopes, retained
failures/corrections and remaining platform/tooling work are listed in
[M72 validation status](../../docs/M72.md#validation-status). Review the main view,
editor Undo/Redo and Play/Stop, fresh save/load and moved package separately.
