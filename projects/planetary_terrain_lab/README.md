# Planetary Terrain Lab — M85

Open `planetary_terrain_lab.judasproj` in Judas Editor. **Play** starts a real
CharacterMotor on a 6,371 km-radius source with centimetre elevation detail.
Select “Double precision Earth” to edit its **Planet terrain** component:
radius/centre, spectral bands, raster samples, collision resolution and budgets.
Save the normal scene. Export through the ordinary project exporter.

## Controls

- **WASD / mouse:** move/look on the ground; A/D steer orbital free flight.
- **Space:** script-driven jump. **E:** throw an ordinary physical probe.
- **F1:** ascend toward orbit. **F2:** descend and land at the current reference.
- **F3:** visit the boundary between two cube faces, then descend.
- **F4:** automatic orbital descent, landing and a short physical walk.
- **F6 / F7:** save/load a modern save slot, including current origin and terrain interest.
- **R:** reload. **Escape:** pause/resume; the menu has resume/reload buttons.

Ground control is enabled only after the nearby collision tiles are ready.
The HUD distinguishes absolute positions from the local physics chart and reports
independent visual/collision residency. Startup prepares the initial visible cover;
later changes morph without replacing the scene. The orbital camera is project JS,
not a simulated spacecraft. Leaving a ground region retires its physical actors;
landing creates a fresh motor after collision preparation. Physical ground motion
and thrown probes use the normal engine physics/motor path throughout.

This is a procedural Earth-**sized** planet, not an Earth geography dataset. Global
DEM streaming, global rigid-body simulation and planetary volumetric holes are not
claimed. Live rebasing currently supports a checked component subset; see
[full model/authoring/limits](../../docs/M85_PLANETARY_TERRAIN.md) and
[public coordinate API](../../docs/judasjs/planetary-terrain.md).

## Human checklist

1. Move/jump and throw probes; inspect centimetre-scale relief near the local patch.
2. F1: ascend. Watch the sphere and changing tile counts; F2: descend and land.
3. F4: watch a continuous automatic orbit-to-ground journey.
4. F3: visit a cube-face boundary, then walk/jump across it. Check gravity/support.
5. F6/F7 on both the initial ground and face-edge ground. Confirm real supported
   motion and probe contact after load, not merely HUD state.
6. Leave/revisit the local streamed station; confirm marker state/location.
7. Edit one band or raster sample, undo/redo, save, Play/Stop and reload.
8. Launch the moved standalone package and repeat the key traversal/save checks.

M85 Linux human acceptance is [recorded](../../docs/evidence/m85/HUMAN_VALIDATION.md). A later one-sign horizontal-look correction is on Experimental with focused JS checks passing; human confirmation of that correction is pending. Windows is untested for M85 and nonblocking.
DejaVu font licensing is preserved in `Assets/fonts/LICENSE.txt`.
