# Game-resolution water replacement

**V2 source preview on Experimental; project opt-in in every build channel.** Grid Water V2 now
uses normal Fluid Volume scene components, RuntimeWorld, JudasJS, CharacterMotor,
editor authoring, Renderer V2 and the regular save/export workflow. Main,
published V1 SDKs and Basic Water remain unchanged. The obsolete Alpha channel restriction is removed in V2.

The editable game is [Water V2 Workshop](../projects/water_v2_workshop/README.md).
The [Water API](judasjs/water.md) lists exact scene keys, handles and receipts.

The previous exact/cut-fragment solvers and their failed controls remain research
references. They are not this solver's architecture or its microscopic acceptance
specification. [Previous checkpoint (historical)](https://github.com/SuperReaper1999/Project-Judas/blob/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/FLUID_BULK_STORAGE.md).

## Build and play the editable project

```sh
cmake -S . -B build -DJUDAS_ENABLE_VALIDATION=ON -DJUDAS_BUILD_GAME_WATER_LAB=ON
cmake --build build --target judas judas_editor judas_export judas_game_water_qualification -j2
build/judas_editor projects/water_v2_workshop/water_v2_workshop.judasproj
# Or launch directly:
build/judas projects/water_v2_workshop/water_v2_workshop.judasproj
```

In the editor, add **Fluid volume**, configure cell size, dimensions and fill height,
and explicitly enable Advanced Fluid Dynamics in Project Settings. Grid Water V2
is the sole advanced backend; PBF scenes and backend selectors are retired.
A domain's owner is its bottom centre; positive uniform scale and rotation work.

WASD/mouse walk/look; Space jump/swim; Q dive; G grab/release; H throw; F pump/drain;
B water impulse; 1 ordinary pool; 2 tilted gravity; 3 radial gravity; 4 original
planet basin geometry; F6/F7 save/load; Escape pause; R restart. A held prop is
released on load. All gameplay rules are in the project's JavaScript.

Export with the normal exporter and this Experimental runtime:

```sh
build/judas_export projects/water_v2_workshop/water_v2_workshop.judasproj /tmp/water-workshop-export build/judas
```

The interactive native diagnostic remains available separately:

```sh
cmake --build build --target judas_game_water_lab -j2
bash tools/diagnostics/run_game_water_lab.sh
```

Its controls remain 1 pool, 2 sluice, 3 trapped air, 4 authored terrain, 5 preserved
basin fixture; X rotated gravity; G gate/vent; B throw; R reset; P pause; F5/F9 save/load.
This diagnostic is off by default and is not an Alpha feature unlock.

The observer is a flying camera, not a swimming character. Save slots default to
`.cache/game-water-lab-saves`; `--save-dir` selects another owned directory.
Reset and scene changes intentionally replace the live workshop state. The scene
and gameplay controls live in `tools/diagnostics/GameWaterLabScene.cpp` and
`GameWaterLab.cpp`, rather than being hardcoded into the water solver.

## Chosen model

`src/fluid/GameWater*` separates quantity/state, sampled geometry, air ownership,
pressure, transport, MK2 coupling, presentation and persistence. There are no
growing geometric fragments or per-particle rigid-body queries.

- Fixed regular 3D cells own water volume in binary64. Paired, inventory/capacity-
  limited transfers conserve it. Velocity fields use binary32 and a bounded
  binary64 pressure projection; no exact fractions or multiprecision library.
- Semi-Lagrangian Eulerian velocity transport is deliberately numerically
  diffusive. Exact momentum, angular momentum and energy conservation are **not**
  promised. There is no artificial global damping coefficient, energy clamp or
  arbitrary velocity cap hiding a broken pressure solve.
- Mixed cells permit finite gravity-driven phase slip: water settles while gas
  rises through the unresolved mixture, transferring gas mass with it. This
  avoids treating a diffused partial cell as an impermeable microscopic film.
  It dissipates unresolved phase motion; it is not exact two-phase dynamics.
- Solids use a fixed 4×4×4 occupancy sample set per voxel and 4×4 face samples.
  An ambiguous static cell containing disconnected free spaces is conservatively
  sealed and counted, rather than inventing a passage through a wall. No tiny
  pressure controls are created. Static geometry can be baked from existing
  engine terrain/collision queries through a callback.
- Moving boxes, spheres, capsules and convex compounds change capacity. Displaced water is routed out through permitted
  connections. If it cannot fit, it stays in explicit, measured displaced-volume
  storage and is released when space becomes available. It is never deleted.
- Resolved connected air cavities own mass and compressible storage. A linearized
  polytropic pressure response couples them to water. Measured dry face samples
  distinguish genuinely dry openings from submerged ones; small vents have finite
  orifice flow. Small throat storage is affiliated with an adjacent reservoir,
  retaining finite connections between resolved chambers.
- Sub-resolution breakup stays affiliated with existing coarse owners where
  possible. Unresolved bubbles retain air mass but use a free-surface traction
  approximation, rather than independent stiff pressure calculations. Tiny new
  closures of atmospheric regions use the declared atmospheric/foam approximation
  with an exterior mass receipt. Vacuum/cavitation dynamics are unsupported.
- Supported MK2 primitives receive bounded-volume hydrostatic buoyancy and implicit quadratic
  drag; drag reaction is sent back to nearby water. Displacement and this momentum
  exchange are approximations, not a monolithic added-mass rigid/fluid solve.
  Static geometry blocks buoyancy-field extrapolation through walls.
- Saves reuse `SaveArchive`, `GameSnapshot` and `SaveStorage`, including their
  checksums and atomic publication. The lab saves actual physical body state.
  Grid state reconstructs and continues deterministically in focused controls;
  native rigid contact caches are not claimed to roundtrip bit-for-bit.

The pool/chamber terrain grid is **30×12×20 = 7,200 cells at 0.4 m**. The original
basin preview uses **15×20×15 = 4,500 cells at 0.2 m**. Its walls come from the
preserved fixture; its water seed and geometry are intentionally re-sampled.
The rotated original uses the authored rotation. These are declared-resolution
tests, not identical-geometry comparisons with the microscopic research solver.

## Preserved foundation qualification

These measurements describe the previous native foundation checkpoint, not the
new runtime integration. Current integration evidence is recorded separately in
[evidence/game-water-integration-20261107](evidence/game-water-integration-20261107/README.md).


[Exact source hashes, commands, raw runs and results](evidence/game-water-20261107/README.md).
The integrated runner advances real fluid/MK2 scenes in separate processes:

```sh
./build/judas_game_water_tests
python3 tools/validation/qualify_game_water.py \
  --executable build/judas_game_water_lab --output /tmp/judas-game-water-proof
```

It checks the untouched pool, floating/sinking bodies, real gate flow, sealed-air
compression and finite venting, actual authored terrain, the original basin in
both orientations, global material ledgers and fresh-process continuation.
Performance records distinguish the complete CPU fluid step from water+MK2 scene
cost. Rendering and graphics-driver costs are measured separately, not inferred
from headless simulation. Automated checks and screenshots are not human acceptance.

The measured 1,200-step pool run used 5.77 ms median / 6.34 ms p95 / 7.94 ms
worst for water plus MK2 on the Ryzen 5 5600. The flowing gate and trapped-air
cases had medians of 5.57 and 6.40 ms. These are small game-resolution workloads,
not a matched-fidelity speedup over the previous exact solver. Five actual
relocated rendered scenes maintained 60 Hz simulation at approximately 100
presentations/second on the GTX 970 at 1280×800 (the desktop presentation rate).
Full raw timings, startup, capture hitches and the tested hardware are retained.

Use `--frame-log /tmp/water-frames.csv` for presentation timings. The normal
bounded engine profiler also works with `JUDAS_PROFILE=1` and
`JUDAS_PROFILE_OUTPUT=/tmp/water-profile.json`; the `Game water surface` GPU
interval measures the water draw, not CPU mesh generation or the whole renderer.
The preview retains catch-up backlog instead of silently dropping simulation time.

## Current support and boundaries

- Water owns explicit binary64 quantities and source/drain receipts. The native
  runtime stages an update and publishes only successful steps; pressure/CFL errors
  freeze the affected region, report a diagnostic and prevent saves of that failed
  state. It does not pretend the interval succeeded or silently cut collision coverage.
- Water owners are stationary, resident root-scene domains. Static/moving colliders
  can stream through them, and an origin shift moves all owned coordinates together.
  The static geometry cache is invalidated by actual shape/pose changes, not each
  CharacterMotor step. Water-owner regional suspension and independent-grid transfer
  remain unsupported and give explicit diagnostics. Overlapping grids are rejected.
- Rotated uniform and local planetary gravity charts use the same backend. Spatial
  gravity is sampled per cell and cached; a chart must stay within 26 degrees of
  local down. A chart cannot cover a whole tiny planet or an antipode. Hydrostatic
  sub-cell reconstruction is chart-vertical, so below-resolution radial curvature is
  approximate. No global planetary liquid ocean is claimed.
- MK2 boxes, spheres, capsules and convex compounds displace water. Real CharacterMotor
  capsules participate as moving query actors; project scripts drive swimming using
  the existing `world.fluidSample`. Prescribed motion can inject energy, like moving
  any other collider. Generic deformable coupling, exact added mass and watertight
  moving-container/air-piston loads are not qualified. Buoyancy is displaced-volume
  hydrostatic load, with implicit drag and its reaction; it is not exact traction
  integration of every dynamic pressure on a solid.
- Static boxes/spheres/capsules/convexes, terrain and triangle surfaces feed the
  shared sampled geometry. Triangle surfaces are thin shells at this resolution,
  not automatic watertight volumetric interiors. Moving triangle shells reject;
  use solid primitives/convex compounds. The active budget is 8 regions, 262,144 total
  cells and 512 nearby bodies per region; these are bounds, not a 60 Hz promise.
- Each grid has closed sides/bottom and atmospheric gas at its top. It does not
  automatically spill through its outer bounds. Explicit signed sources/drains
  return actual accepted cubic metres and preserve the unaccepted amount for game
  rules. A sealed/full positive receiver applies backpressure. Sources do not
  represent portable pipes between independent grids.
- Features thinner than the fixed sampling resolution may close/disappear.
  Unresolved bubbles/foam use the documented coarse closure. Air waves, capillarity,
  cavitation and scientific force/torque fidelity remain outside the model.
- Grid Water has a separate [continuous presentation path](WATER_V2_PRESENTATION.md):
  a fine render surface, GPU reconstruction of changing heights/normals, animated
  detail, scene-depth refraction, Fresnel/environment/screen-space reflections,
  shoreline fading and underwater spectral attenuation. Moving-solid exclusions
  do not punch optical holes around an immersed camera. This render-only field
  never supplies buoyancy, pressure, swimming or conserved quantity. Genuine dry
  cells still separate vertically stacked water layers. No rendered particle owns
  mass. Small unresolved voids and sub-2 mm films are presentation approximations;
  detailed spray and a full foam solver are not implemented. Visible cameras share
  field/topology caches; offscreen regions do not reconstruct or upload visual detail.
- Regular saves retain the grid, water, gas and receipt ledgers alongside MK2 and
  script state. Missing/corrupt/incompatible water data reject instead of reseeding.
  Native grid state is versioned; derived geometry/gravity caches rebuild on load.
  Legacy world-delta saves explicitly reject advanced water. Save-size limits still
  apply; oversized saves reject instead of truncating.
- Network replication of advanced water is unsupported. The existing entity
  admission check reports it; a background grid is not implicitly synchronized.
- Linux source-build/relocated-export qualification is separate from release and
  hardware acceptance. No new Windows, Android or browser qualification is claimed.
  The runtime uses portable C++/existing rendering, with no new compute dependency.

## Human check

1. Open the Workshop project in the editor. Select the water owner, inspect grid
   fields and the experimental warning. Play, Stop, then Play again; authoring must
   remain unchanged and initial water should reset deliberately.
2. Walk down the steps, swim and dive. Aim the centre crosshair at a loose prop
   within 5 metres and press G; the HUD must confirm it is held. G releases it;
   H throws it. Watch the green crate float and the grey sphere sink without launching.
3. F toggles circulation (equal pump/drain receipts, rather than a rising pool).
   Aim down at water and press B to splash there; solids block the splash ray.
   Water should flow/settle while quantity
   follows the explicit receipts. Inspect the chamber/roof and ordinary solid contacts.
4. Switch to tilted/radial gravity and the original planet basin geometry. Check
   stable bodies, swimming, surface alignment and camera controls.
5. Save while disturbed, change the pool, reload, then quit/reopen and load again.
   Water, inventory receipts, body poses and script state must survive.
6. Run the normal exported project after moving the package. Keep the Alpha fluid
   lock in place until review; automated checks do not establish human acceptance.
