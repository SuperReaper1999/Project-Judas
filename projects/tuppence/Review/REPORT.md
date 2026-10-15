# Tuppence review — 15 October 2026

**Gameplay follow-up:** the initial shelf cleared the whole bed after one drop. See `pusher-followup/REPORT.md` for the reproduced failure, corrected short-stroke content and updated measurements. Original receipts below are retained.

## Release boundary

M84 human acceptance and Planetfall release authorization were recorded in `59f8b70ac372cd68be4c587f8f28111a132555ff`, promoted normally to main. This new arcade project and its collision corrections are Experimental, awaiting human review. No Windows or human acceptance is claimed for Tuppence.

## Game and ownership

Ordinary registered project, three editable scenes, public JudasJS game script, runtime UI, original synthesized audio, prefab spawning, native save/load and scene switching. The two selectable games are a 112-ball pusher and a compact 24-token 2p pusher. Coins use real 12-sided convex geometry, an embossed original Blender/glTF mesh, a finite-force native slider motor, contact-driven shelf transfer, a lower tray and physical payout. All scoring, credits and automatic insertion are JS. Neither physics nor rendering contains a game/scene-name branch. No collision freezing, cadence reduction or fake background coins. The stress scene contains 208 actual balls.

The 2p cabinet is a stylized simulation of the familiar insert/push/collect mechanism, not a dimensionally exact commercial cabinet. It deliberately has a bounded population of 32. Increasing that limit or adding densely packed layers can cause expensive CCD work.

## Demonstrated defects and generic corrections

1. **Support spin:** thin flat bodies rotating about a support normal triggered the all-direction rotational CCD bound despite not moving through that support plane. The red upright fixture performed 5,665 samples (~76 ms). Bound rotation projected onto a valid convex separating plane using the existing anchored geometric drift; retain the conservative original path for concave terrain/mesh and unavailable planes. Corrected fixtures perform zero extra rotational samples (~0.1 ms), including arbitrarily rotated support; actual tumbling still hits. Sphere-spin invariance already passed before the change: it was investigated, not falsely credited as a fix.
2. **Repeated deep-contact queries:** the penetration predicate built complete cooked manifolds for every search point. Use the same accepted witness predicate with an early face-axis rejection, conservative threshold guard and original fallback. Differential tests exercise both orders, threshold boundaries, rotation and large coordinates.
3. **Duplicate SAT directions:** exact parallel source-local edges generated redundant cross axes. Cache exact parallel direction representatives at cook/load time; retain all physical topology/witness edges. No approximate angle merging or asset format change. Symmetric 12-sided coin: 36 edges, seven distinct directions.
4. **Asymmetric face manifold:** when SAT selected A's face, clipping only supported a B-reference face and fell back to edge witnesses. A tilted coin on a large flat box produced four contacts forward, one reverse, with a reverse witness outside its footprint. Clip the incident B polygon against A's reference face too. The fixture now produces four/four contacts. Concave-triangle response is untouched.

No solver iteration budgets, physical cadence, contact quality or acceptance thresholds were reduced. Fixes are generic C++ physics mechanisms, with no new public JS API.

## Failed designs and measurements

Raw failures are retained here. Intermediate measurements have different evolving content; do not treat them as an apples-to-apples engine speedup benchmark.

- Original 71-coin kinematic cabinet: partial run, fixed p95 4,906 ms, max 6,535 ms. Increasing motor realism and generic corrections was necessary; the initial population was impractical.
- 71 coins after finite-force motor and SAT direction cache: completed gameplay/save checks, p95 276.10 ms, max 447.66 ms. After symmetric manifold correction: p95 95.94 ms, max 201.58 ms. Still unsuitable as the default cabinet.
- Rejected retaining-ridge content: 112 balls p95 35.69 ms/max 222.79 ms; 208 balls p95 685.00 ms/max 3,447.64 ms. Trapping the bed is a genuine remaining pressure/CCD scaling limit, not solved by this batch. Default payout edge is open.
- Initial compact coin authoring left a gap between shelf reach and the lower bed; payout/save-score checks correctly failed. Final content fills that reach with contiguous rows.
- Two new harness assumptions were corrected: credits can increase from genuine early payouts, and switching into a machine may score from initial settling. Verify the actual credit ledger and reset drop/autoplay state rather than asserting an arbitrary unchanged bank/zero score after many simulated steps. Original failures are preserved.
- Native gdb attachment was unavailable (ptrace denied); no security changes were made. Only the owned test process was stopped.

Final measured results are in `final-results.txt` and per-scenario CSV files. They include full rendering work, but use deterministic 60 Hz supplied frame intervals; these are work timings, not a measured wall-clock FPS certification. Maxima and dense-bed limits remain visible. Human responsiveness review is still required.

## Focused validation and reproduction

Build `judas`, `judas_export`, `judas_ball_pusher_application_tests`, `judas_sphere_impact_bounds_tests`, `judas_cooked_penetration_tests` in Release using the normal CMake build. Native graphical checks require a desktop/OpenGL session.

```
build/judas_ball_pusher_application_tests projects/tuppence/Scenes/cabinet.judas /tmp/tuppence-balls play
build/judas_ball_pusher_application_tests projects/tuppence/Scenes/coins.judas /tmp/tuppence-coins coins
build/judas_ball_pusher_application_tests projects/tuppence/Scenes/crowded.judas /tmp/tuppence-crowded stress
build/judas_export projects/tuppence/tuppence.judasproj .cache/Tuppence-Package build/judas
```

Application checks exercise real M35 input, UI clicks, normal pause, actual physical payout, piece/credit accounting, native save/load and cross-mode scene transition. Moved export is exercised from `/tmp` against copied assets. Runtime hashes verify the packaged executable matches the built runtime.

Final physics-source regression receipts under `focused/`: cooked penetration 3,849; spin bounds 16; collision geometry 1,303; impact solver 267; impact timing 3,197; impact robustness 4,412; kinematic 96; contact lifecycle 1,076; contact storage 921; impact mesh-cost 311; ragdoll 36; physics continuation 112; CharacterMotor 32. All pass. Mesh-cost dense-trough contact digest remains `c007331aa912b5d9`, deepest 21.48 mm; the full mesh-cost worst step was 52.92 ms. These are focused regressions, not a full production-suite pass.

Release compilation succeeded. The full header rebuild exposed two existing misleading-indentation warnings in unchanged `RuntimeWorld.cpp` (1570, 1587); the final modified-source incremental build had no warnings. No unrelated warning cleanup was attempted.

## Limits and preservation

Dense thin convex stacks and constrained/trapped ball beds can still hitch. No claim of unlimited pieces or universal 60 FPS. Coins are oversized authored game geometry with a real coin mass, not exact SI-scale commercial hardware. One shared save slot intentionally restores whichever machine was saved. Windows, controller hardware and human arcade feel remain unvalidated. No protected FTFT/P1 history, independent fluid work, accepted fluid implementation or existing games were changed by these collision/game additions. Source and binary fingerprints and exact intended inventory accompany the report.
