# Tilemap authoring

Experimental development toward v1.1.0. This is an editor workflow, not a new
JudasJS runtime subsystem. See [sprites](sprites-2d.md), [sprite sheets](sprite-animation.md)
and [planar physics](physics-2d.md) for the underlying public components.

## Make and paint a level

1. Create a 2D project and import a PNG through the Asset Browser. Use the sprite
   sheet editor to slice it into frames and save a `.judassprite` palette.
2. Right-click that sheet in the Asset Browser and choose **Paint tilemap from
   this sheet**, or open **View → 2D authoring → Tilemap painter**. Select the
   palette and create a grid. The new root is selected immediately.
3. Select a tile thumbnail and enable **Paint in Scene view**. Left-drag paints;
   right-drag erases. Rectangle tools fill or erase an inclusive cell rectangle.
   Fast brush strokes interpolate cells. Release commits one undoable action;
   Escape cancels the draft. Middle-mouse panning and wheel zoom remain available.
4. Turn painting off to select scene objects normally. Select a grid root or one
   of its generated children to return to its painter. Save the scene normally.
5. Use the layer controls to add decoration above or below collision tiles.
   Collision, sorting layer and order are per layer; flips are per painted cell.
   Advanced settings contain cell dimensions and explicit palette rebuilding.

The palette uses stable frame keys. Renaming/reordering frames does not retarget
painted cells. Removing a used frame rejects rebuilding. After changing a palette,
use **Rebuild from palette** deliberately; saved runtime sprites do not depend on
the editor being open. Changing the source image dimensions requires re-slicing.

## Collision and ownership

A grid compiles to ordinary sprites and static MK2 planar boxes, merged into
rectangles within 16×16-cell chunks. Erased holes remain holes in real collision.
Use native planar bodies/controllers for gameplay; decorative sprites have no
implied physical thickness. Solid layers may not overlap at the same grid cell.

Move/rotate the grid root as a fixed frame. For physical grids, leave root and
ancestor scale at one and change **Cell size** instead. The root/ancestors must
not be moving physical bodies, motors, ragdolls, deformables, wheels or sockets.
A rotated level needs actors and gravity in that same physical plane.

Generated children belong to the painter. Editing/removing them independently
makes subsequent rebuilds reject rather than overwrite your work. Undo those edits
or use **Detach grid; retain ordinary objects** before customizing them. Detaching
is deliberate: the objects remain, but that grid stops being paintable.

Hierarchy duplication copies an independently editable grid and remaps its
managed identities. Deletion removes its recipe in the same Undo action. Prefabs
retain the compiled sprite/collider subtree; edit the source scene to paint the
original grid. Prefab instances do not carry an editable palette recipe.

## Runtime, persistence and limits

No `Tilemap` JavaScript API is added. Existing entity, sprite, physics, trigger,
scene and save APIs operate on compiled objects. Runtime entity changes do not
rewrite the editor's source recipe. Streaming and modern saves restore ordinary
native body state; standalone exports include the referenced image/palette.

Current bounds are **2,048 occupied cells per grid, eight layers**, coordinates
−512…512, and cell dimensions 0.01…100 metres. Each cell is a normal sprite entity;
this is a practical bounded level editor, not an infinite-world tile renderer.
Frame pivots are centered and art fills the cell. Animated tiles, slopes,
one-way platforms, moving grids and gameplay cell-edit APIs are not provided.
These limits do not change existing 3D or small sprite projects.

Try [Tilemap Workshop](../../projects/tilemap_workshop/Tilemap_Workshop.judasproj).
WASD/arrows move, Space jumps on the trail, 1/2 switch trail/village, R restarts,
F6/F7 save/load. Collect stars through real triggers and push the physical crates.
