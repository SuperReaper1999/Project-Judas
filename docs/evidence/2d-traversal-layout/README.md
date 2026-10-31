# Traversal layout correction — 31 October 2026

Operator feedback on dca1237f: grid rendering looks wrong, and the player cannot
approach the tall cyan obstacle from its right side. Not human accepted yet.

## Confirmed cause and correction

This is a starter/workshop authoring defect, not a demonstrated MK2 or renderer
fault. The grid template reused the free-movement template's obstacle centered
at (3,2), half-width 0.7 m. A player at x5 requesting the adjacent grid destination
x4 would have its left edge at 3.68, overlapping the obstacle's right edge 3.7
by 0.02 m. The cast correctly rejects the entire 1 m step. That leaves the actor
at x5 and looks like an excessive invisible collision margin. Do not weaken the
cast or shrink the player's physical shape to conceal this layout error.

The grid template now authors that obstacle with 0.45 m half-extents, and its
sprite matches its physical bounds. From the right, native movement now reaches
x4, but still rejects x3. Its boundaries fit the grid, with a north boundary
preventing travel beyond the finite room. The checkerboard now covers the full
walkable room, rather than ending at x8.5 inside the x11.5 wall. The workshop
removes the inherited interior obstacle before placing its own three green cells;
it no longer layers those over a larger cyan obstacle. Its side-view scene omits
the grid north boundary. The HUD has less unused vertical space.

No collision solver, renderer, input or controller response has changed. Grid
movement still stops on an adjacent unoccupied cell; it does not creep toward an
edge by arbitrary sub-cell amounts. The footprint is centered at the character's
feet, rather than filling the top-down character's whole tall sprite.

## Evidence

- source-before.zip: exact prior source project, no saves or executable.
- grid-check.log: 61 passing checks, including an explicit legacy-layout negative
  control (actual native endpoint x5) and corrected starter endpoint x4. The test
  exercises the untouched starter before the older isolated movement fixture.
- cold.log and cold-queue.log: separate processes resume mid-cell and queued-turn
  saves; 7 checks each.
- oneway.log: 23 passing checks, including collision/CCD and persistence.
- editor-workflow.log: 65 passing editor scripting/preset workflow checks.
- sprite-render.log: 22 actual desktop OpenGL pixel checks pass. This is rendering
  correctness, not a performance benchmark; another user demo was left untouched.
- tour.log / tour-proof.json: genuinely relocated export, independent native body
  endpoint/ENTITY comparisons, both platform landings, grid blocking, return and
  save/load. 175 arithmetic assertions across 55 frames, not 175 capabilities.
- grid-blocked.png and upper-platform.png: actual corrected runtime frames.
- layout-measurements.json: causal dimensions and expected native outcomes.

Native Linux/GCC15 Release; actual NVIDIA GTX970 desktop X11 rendering. All listed
commands returned zero. Builds emitted no warnings. Windows/Android are not
qualified by these checks. No release, main promotion or human acceptance claim.

Reproduce with a fresh output directory:

```sh
cmake --build build-2d --target judas_grid_movement_tests judas_2d_traversal_workshop judas judas_export judas_editor_script_workflow_tests judas_one_way_platform_tests judas_sprite_render_tests --parallel 2
build-2d/judas_grid_movement_tests /tmp/grid-layout-fresh
build-2d/judas_grid_movement_tests /tmp/grid-layout-fresh --cold
build-2d/judas_grid_movement_tests /tmp/grid-layout-fresh --cold-queue
build-2d/judas_editor_script_workflow_tests
build-2d/judas_one_way_platform_tests /tmp/oneway-layout-fresh
SDL_VIDEODRIVER=x11 build-2d/judas_sprite_render_tests
build-2d/judas_export projects/2d_traversal_workshop/2D_Traversal_Workshop.judasproj /tmp/traversal-layout-export-fresh build-2d/judas
```

Relocate the complete package. Run the supplied tour with screenshot paths adjusted
and JUDAS_ENGINE_ROOT=/missing/engine plus an isolated writable XDG_DATA_HOME.
Then use tools/validation/check_traversal_tour.py on its runtime log.

## Human retest

Switch to 2. Approach each green cell from all four sides; compare its visible
edge to the last allowed adjacent grid cell. Explore the room's corners and check
the checkerboard reaches every wall. Switch to 1 and retest jump-through/landing.
Use F6/F7, and R to restore the authored layout. These are pending human checks.
