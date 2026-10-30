# Sprite Workshop — native 2D Stage 1

Open `Sprite_Workshop.judasproj` in the current Experimental editor, then Play.
WASD/arrows or a supported gamepad's left stick move the explorer in XY. This is
a visual authoring sample: **no planar collision, jumping, tilemap or sprite
animation system yet**. Stage 2 adds physical 2D movement.

The explorer and checker ground are ordinary native sprites. Edit `Assets/game.js`
for movement/camera policy. The camera is script-owned and orthographic, with a
320×180 integer-scaled canvas at 32 pixels per world unit. `pixelPerfect` and
`speed` are inspector script properties. Smaller windows use fractional downscale.

In Edit mode, middle-drag pans, wheel zooms, F frames selection, and the toolbar
switches 2D/3D view. Import/drop PNGs to create proportionate selected sprites.
Select the explorer to edit its image, pivot, flips, tint and sorting. Alt-click
cycles overlapping selectable objects; transparent pixels reveal objects behind.

Source artwork is original and reproducible with `tools/CreateSpriteStarterArt.py`.
No downloaded assets or external attribution obligations. Native sprite and lens
settings use normal scene/prefab serialization and modern save slots. The older
`.judasstate` delta format is not a complete runtime-presentation snapshot.

[API/workflow](../../docs/judasjs/sprites-2d.md) · [Stage report](../../docs/2D_STAGE1.md).
