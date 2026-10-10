# Terrain Lab

Ordinary Judas project for [M75](../../docs/M75.md): editable 64×64 m landscape,
65×65 samples, a hill/depression/flattened pad and spatial grass/soil/sand/stone
painting. There is no invisible helper floor. The second scene is a rigidly
rotated finite patch with independently oriented gravity, not a planet.

Open `terrain_lab.judasproj` in the editor. Select the terrain and use
View → Terrain sculpt / paint → Edit selected terrain. This opens the source and activates the brush. Choose Raise/Lower/Flatten/Smooth/Paint at the top and left-drag on the terrain; Paint also needs a texture layer selected. Save source and
Cook separately before Play/export. The source is `Imports/landscape.judasterrain`.
Source editing deliberately affects shared instances; Fork makes an independent copy.

WASD / mouse / Space: movement, look, script jump. E: drop a physical sphere.
F1/F2: flat/oblique scene. R: reload. F6/F7: modern save/load. Escape: pause.
Crates use ordinary dynamic box contacts. The sphere is an ordinary prefab.
The annex reuses the terrain through a registered additive region.

Ground textures are original small procedural base-color assets authored for this
project, usable without external libraries. DejaVu Sans retains its own license
under `Assets/fonts`. No ignored asset pack, Lastlight or Terrain-ML dependency.
The four texture IDs and output IDs are listed in `Authoring/ids.json`.

A neutral 12×12 m patch beside the landscape has its own editable source and product
IDs (`Imports/neutral.judasterrain`). Painting/sculpting the main landscape cannot
change it. The terrain prefab is `Assets/prefabs/terrain_patch.judasprefab`. Both
main scenes have ordinary offline navigation bakes; rebake after sculpting before
export. Painting weights alone preserves those bakes.
