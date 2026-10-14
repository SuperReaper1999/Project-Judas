# Dig Lab — ordinary editable volumetric terrain

Open `volume_terrain_lab.judasproj` in Judas Editor. Select the rock, then
**View → Volumetric terrain - dig / build → Edit selected volume**.
Add/remove/smooth, Save source, Cook, and rebuild stale navigation before Play/export.
Full workflow and limitations: [M84 guide](../../docs/M84_VOLUMETRIC_TERRAIN.md).

WASD/mouse; Space jump; E throw a physical crate; F1/F2 flat/rotated;
F3 request/release annex; F6/F7 save/load; R reload; Escape pause.

The through-tunnel, chamber, roof and sealed pocket are actual mesh/collision.
Edits are authoring operations; no live gameplay digging API is claimed.
Starting crates stay clear of the walk lane. The existing motor can temporarily
lose support reporting when an overlapping crate side masks its floor sweep;
that preserved limitation is described in the guide/evidence.

`Imports/cave.judasvolume` is the editable source. `Assets/Volumes/cave` contains
normal cooked model/collision and source hashes. Prefab/UI/font/script/world/nav
assets use existing pipelines. Dynamic crate prefab is explicitly declared in
project runtimeAssets so E works in standalone exports too.

The volume and tools are original Judas assets/code. DejaVu font licensing is
preserved in Assets/fonts/LICENSE.txt. No additional third-party dependency.
