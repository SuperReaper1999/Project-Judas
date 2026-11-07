# Game-resolution water replacement

**Experimental source preview, not an unlocked Alpha feature.** This is a working
native Judas scene using a new optional C++ water module, the real MK2 world,
Renderer V2 and normal engine save storage. Main, supported SDKs, Basic Water and
the Alpha fluid lock are unchanged. The new module is not yet selected by ordinary
scene components, JudasJS, the editor or normal game exports.

The previous exact/cut-fragment solvers and their failed controls remain research
references. They are not this solver's architecture or its microscopic acceptance
specification. [Previous checkpoint](FLUID_BULK_STORAGE.md).

## Build and play

From the repository root with the normal Linux source-build prerequisites:

```sh
cmake -S . -B build -DJUDAS_BUILD_GAME_WATER_LAB=ON
cmake --build build --target judas_game_water_lab judas_game_water_tests -j2
bash tools/diagnostics/run_game_water_lab.sh
```

The preview target is off by default, adds no mobile dependencies and is not part
of SDK/release packaging. It does not enable the locked production fluid path.

| Control | Action |
| --- | --- |
| 1 | Filled rectangular pool; light and dense MK2 boxes |
| 2 | Sluice; the gate opens after one second |
| 3 | Air chamber; a small vent opens after ten seconds |
| 4 | Actual M75 authored heightfield, mesh collision and water |
| 5 | Original basin fixture, re-voxelized at game resolution |
| X | Rebuild the scene under rotated uniform gravity |
| R / P | Reset / pause simulation |
| WASD + mouse | Fly the observer camera |
| Space / Ctrl | Fly up / down |
| B | Throw another real, low-density MK2 box; workshop limit 64 |
| G | Operate the sluice gate or chamber vent |
| F5 / F9 | Save / reload water, air, bodies and gate |
| Tab / F12 | Release mouse / capture screenshot |

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
- Moving boxes change capacity. Displaced water is routed out through permitted
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
- MK2 boxes receive bounded-volume hydrostatic buoyancy and implicit quadratic
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

## Qualification

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

## Current boundaries before any unlock

- C++ source-preview module and workshop only. Normal RuntimeWorld ownership,
  editor/JudasJS authoring, automatic streaming, origin shifts and regular exports
  still need integration/qualification. Ordinary project water remains Basic Water.
- Uniform gravity in a rotated chart is tested. A spatially varying radial field,
  cross-grid flow and planetary regional handoff are not implemented here.
- The grid has closed sides/bottom and an atmospheric top. Runtime water sources,
  drains and open liquid boundary ports are not yet APIs. `Fill` is initial authoring
  only. Pressure/CFL budget failures report an error; they do not drop elapsed time
  or silently reduce collision coverage. Extreme failures currently stop the lab.
  A failed physical step is not yet a transactional runtime retry API.
- Box coupling is implemented. Sphere/compound/deformable/character coupling and
  swimming are not yet integrated. This does not change Basic Water swimming.
- Features thinner than the sampling resolution can close or disappear from the
  simulation geometry. Small bubbles, air-wave propagation, surface tension,
  cavitation and scientific force/torque fidelity are outside this model.
- Presentation currently extracts the highest water layer per column. It is
  insufficient for stacked cave layers, detailed splashes, underwater optics or
  final water-material quality. Sub-mobile traces remain in the quantity ledger
  instead of being drawn as a full-cell water sheet. Physical quantity never
  comes from this mesh.
- The grid limit is 262,144 cells, not a real-time performance promise at that
  size. The current save participant has the normal 8 MiB engine string bound;
  oversized snapshots reject rather than truncate. No clean-machine distribution
  or Windows/Android/browser qualification is claimed for the new module.

## Human check

1. Watch the green box float and the grey box sink; throw boxes from different
   heights and move the camera near the surface.
2. Open/close the sluice and watch water physically move between the two sides.
3. Watch chamber pressure build while sealed, then release through the small vent.
4. Inspect the real terrain shoreline and the original basin; use X to rotate.
5. Save, disturb the pool, reload, then close/reopen with `--load` and verify the
   stored state. Keep the normal Alpha fluid lock in place during this review.
