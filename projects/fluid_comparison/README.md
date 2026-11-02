# Experimental fluid comparison

Two editable scenes for comparing the current particle-water implementation:

- **F1: Large rectangular pool.** A 12 × 8 m basin, nominal 2 m initial water stock, clear sides, an observation deck and exit steps. Three pick-up cubes provide light (500 kg/m³), water-density (1,000 kg/m³) and heavy (2,000 kg/m³) controls.
- **F2: Original awkward basin.** The original Terrain planet scene, with its radial gravity, terrain, spacecraft, props and small lake. Its eight original objects and fluid settings are preserved exactly; only the comparison HUD is added.

The scenes are separate deliberately. Gravity regions use first-match ownership, and water volumes share a scene-level solver and particle budget. Putting both basins in the original scene would change the comparison.

## Play and edit

Open `fluid_comparison.judasproj` in the current Experimental editor, then press Play. The rectangular pool is the startup scene. Both scene files are editable normally.

From a repository build:

```sh
JUDAS_WORLD_STATE=none ./build/judas projects/fluid_comparison/fluid_comparison.judasproj
```

Use the path to your actual runtime build if different. `JUDAS_WORLD_STATE=none` starts from authored state without loading or writing a world save.

| Control | Action |
| --- | --- |
| WASD / arrows, mouse | Walk and look |
| Space | Jump / swim upward |
| G / H | Grab or drop / throw a targeted pick-up prop |
| V | First / third person |
| F1 / F2 | Fresh rectangular pool / original awkward planet |
| R | Reset the current scene |
| Escape | Pause; switch scene, resume or quit |
| B, in the planet scene | Add water through the original emitter |
| F, in the planet scene | Original spacecraft control interaction |

Walk forward down the central steps to enter the pool; return up those same steps. The cubes start on the dry deck so their entry is under your control. Their colours describe density controls, not a claim that neutral buoyancy or settling is fully qualified.

## Water stock and regeneration

The pool uses the same fluid scale (10), spacing (0.5 m), update cadence (30 Hz) and hydrostatic drag rate (2) as the original scene. A 24 × 4 × 16 initial lattice is clipped against the actual stairs, with particle-radius clearance. The remaining **1,440 particles / 180,000 kg** are grouped into editable stock volumes without changing their positions. There is no pool emitter or automatic refill.

`Source/original_terrain.judas` is the frozen original scene. Its hash and pool parameters are in `Source/authoring.json`. Regenerate only this project's scene files with:

```sh
python3 projects/fluid_comparison/Tools/build_scenes.py
```

Regeneration replaces edits to these two generated scenes. Copy a scene first if you want to preserve hand edits.

## Scope and limitations

This is a diagnostic comparison project, not a fluid unlock or a physics correction. It opts into Advanced Fluid Dynamics on Experimental. The published Alpha lock remains in place, and a locked Alpha runtime cannot run this comparison's advanced water. The current experimental surface, immersion, settling and fluid/solid geometry limitations still apply. No Basic Water overlay conceals particle behaviour.

The runtime intentionally uses the same built-in player/control path as the original Terrain planet scene. The scenes do not share live state; switching reloads the selected authored scene. Native Linux checks are recorded in `Verification/RESULTS.txt`; Windows, Android and human acceptance of this project are pending.

## Asset provenance

The spacecraft is the existing Judas-authored `assets/models/plane.obj`, with its original CC0-equivalent provenance header. The font and redistribution notice are copied from the existing fluid demo: `Assets/fonts/LICENSE.txt`. The comparison UI reuses that demo's project-owned menu layout. No external downloads or independent research are included.
