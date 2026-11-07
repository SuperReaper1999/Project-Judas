# Game-resolution water: first integrated foundation

7 November 2026; Experimental source parent
`7180ad8a7dd12aa1fd0f50ef1da3febe4ff99a52`. Exact tested source is identified by
`source-sha256.json`, build flags, binary hashes in the runner records and raw
results. This is **not a release, an Alpha unlock or human acceptance**.

The operator explicitly authorized replacing microscopic scientific requirements
with believable game water. The implementation changes physical resolution and
the numerical model. Timings are **not matched-fidelity speedups** over the exact
research solver. Its previous failures and negative controls remain preserved.

## What changed

A fixed regular grid owns water quantity. It has bounded sampled boundaries,
ordinary-precision pressure, conservative volume transfers, semi-Lagrangian
velocity transport and coarse finite-air reservoirs. It does not keep extending
the cut-fragment nonlinear solver. Native MK2 boxes, actual M75 heightfield
geometry, Renderer V2 and the existing SaveStorage are integrated in the lab.

| Responsibility | Source |
| --- | --- |
| State, stepping, queries | `src/fluid/GameWater.h`, `GameWater.cpp` |
| Sampled static/moving occupancy and displacement | `GameWaterGeometry.cpp` |
| Connected/coarse air ownership | `GameWaterAir.cpp` |
| Bounded pressure projection and finite vents | `GameWaterPressure.cpp` |
| Paired volume fluxes and mixed-cell phase slip | `GameWaterTransport.cpp` |
| Native MK2 box buoyancy/drag | `GameWaterCoupling.cpp` |
| Engine snapshot persistence | `GameWaterPersistence.cpp` |
| Physical-quantity-driven surface mesh | `GameWaterSurface.cpp` |
| Scene content, real bodies, terrain and cold state | `tools/diagnostics/GameWaterLabScene.cpp` |
| Input, presentation and normal engine profiler | `tools/diagnostics/GameWaterLab.cpp` |

No implementation file in the new water module exceeds 300 lines. The lab's
application coordinates the separate scene and simulation module. There is no
dependency on the old numerical prototype headers or multiprecision arithmetic.

Water quantity and air mass use binary64; velocity and sampled geometry use
binary32; pressure/reductions use binary64. Fixed 4×4×4 occupancy and 4×4 face
samples replace fragment proliferation. Ambiguous static cells are sealed and
reported. Unfittable displacement stays explicitly stored, then returns when
capacity becomes available. It is not silently deleted.

Coarse air uses a polytropic pressure law and finite, dry-aperture orifice flow.
Sub-resolution bubbles retain their mass but do not acquire independent stiff
pressure solves. Numerical voids, foam and small unresolved atmospheric closures
have explicit approximate policies; this is not a dynamic atmospheric solver.

### A sustained defect found and corrected

The first real-time version completed the short runs, but a two-minute air run
revealed residual pressures of several thousand Pa after opening the vent.
Direct cell/owner inspection found a disconnected, numerically stranded layer
of air below partly filled cells. Treating every partial cell as a sealed lower
water film was imposing a microscopic interface the model cannot represent.

The correction is finite, gravity-driven phase slip within mixed cells. Water
settles while an equal gas volume rises; gas mass moves with it. Transfers obey
actual sampled openings, material inventory and available capacity. This is an
explicit dissipative subgrid phase closure, not arbitrary global damping or an
energy/pressure clamp. Exact momentum/angular momentum/energy conservation is
not claimed for this low-order game model.

The corrected two-minute chamber run finishes with at most **4.01 Pa** reservoir
pressure over its last ten seconds, zero recorded water drift and approximately
`1e-9 kg` air-ledger roundoff. The integrated runner now checks the largest
pressure across **all** reservoirs, not merely whichever reservoir is largest.
The original short checks were retained. Applying the new gate to the preserved
pre-correction native rows correctly rejects them (`negative-control.json`).

## Actual qualification

- **24 focused checks pass**, also under AddressSanitizer, UndefinedBehaviorSanitizer
  and leak checking. Includes quiet water, quantities, deterministic grid-state
  continuation, real SaveStorage, failed-load preservation, rotated charts,
  dry-body safeguards, unresolved walls and displaced-water storage/release.
- **64 integrated checks pass** in genuine native processes: quiet pool, float/
  sink, sluice flow, trapped air/vent, actual authored terrain, re-sampled original
  basin in both orientations and separate cold write/read processes.
- Two-minute, 7,200-step pool and rotated-basin runs pass, plus the two-minute
  chamber control. Final long-run rows and results are retained. The chamber's
  late samples overlapped sanitizer compilation, so that control is not used for
  final performance claims.
- Existing production regressions pass: ProjectWater 56 checks, BasicWater
  application 45, BasicWater preview 18, MK2 physics and MK2 rotation cold saves
  (5 write + 3,139 read checks). This does not claim a complete engine-suite run.
- Five real desktop scenes render from a copied package outside the checkout,
  using package-local fonts and saves. Separate relocated write/read processes,
  build identity and SHA-256 receipts are retained. This is not clean-machine
  certification or a normal editor-project export.
- Earlier ASAN/UBSAN-instrumented lab runs cover 600 sluice, 900 chamber and 600
  rotated-basin steps. Final unit coverage instruments the current water module;
  the pre-existing engine library was not rebuilt with sanitizers.

Automated input, screenshots and builds do not constitute operator acceptance.

### CPU cost

Ryzen 5 5600, 12 logical CPUs, 8 GB RAM, GCC 15.2.0, Release/O3, Linux. Full
hardware and exact configuration are in `hardware.json` and `build-flags/`.
The following are whole water-plus-MK2 CPU steps over 1,200 ticks (20 seconds).
Compilation did not overlap this final coordinated measurement.

| Scene | Median ms | p95 ms | Worst ms |
| --- | ---: | ---: | ---: |
| Quiet, no bodies | 6.629 | 6.823 | 7.660 |
| Filled pool, two bodies | 5.771 | 6.339 | 7.936 |
| Sluice flow | 5.574 | 6.925 | 9.433 |
| Air chamber, no bodies | 6.401 | 6.873 | 10.299 |
| Real authored terrain | 6.161 | 6.544 | 8.271 |
| Re-sampled original basin | 4.320 | 6.119 | 10.151 |
| Authored rotated basin | 4.293 | 5.855 | 8.188 |

The 7,200-step pool measures 5.930 / 6.284 / 9.430 ms; the rotated basin measures
4.259 / 4.543 / 8.167 ms. Its physical trajectories differ from the previous
microscopic model by design. The pool's final ten-second mean floating-body
centre is 1.987 m at a nominal 2 m surface; the dense box rests on the floor.

Pool/chamber/terrain: **7,200 cells at 0.4 m**, a 12×4.8×8 m domain.
Original: **4,500 cells at 0.2 m**. Re-sampling gives about 7.972 m³ in that fixture,
not the old exact reference's 9.25 m³; there is no claim of identical initial
geometry or material. Conservation checks use the new authored initial quantity.

The persistent pool-array estimate is about 1.74 MiB. Headless whole-process RSS
is about 28–30 MiB; real rendered processes are about 130–160 MiB. These figures
are not VRAM measurements. Raw resource receipts include startup and allocator
effects; the array estimate excludes temporary solver workspaces.

### Rendering

NVIDIA GTX 970, driver 580.178.04, OpenGL 4.6, 1280×800. Five relocated native
scenes sustain 60 Hz simulation at approximately the desktop's 100 Hz presentation
rate. This is not an uncapped FPS claim. `rendered-final/` contains every frame's
CPU submission, swap/capture and backlog times, screenshots and bounded engine
profiler snapshots. PNG capture hitches remain in the records.
The rotated-basin profiler exceeded its bounded event capacity, so its per-pass
GPU data is incomplete and is not used for a GPU timing claim. Actual CPU CSVs,
simulation progression and rendered captures remain available.

`Game water surface` GPU timestamps cover the water draw only; CPU mesh generation,
other rendering, UI and swap are separate. Presentation uses a basic transparent
mesh, not final water optics. No graphics reset was reported in these bounded
runs; absence of a report is not proof that every driver failure is impossible.

## Reproduce

Use the normal Linux source-build prerequisites, then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_BUILD_GAME_WATER_LAB=ON
cmake --build build --target judas_game_water_lab judas_game_water_tests -j2
ctest --test-dir build -R '^experimental.game-water$' --output-on-failure
python3 tools/validation/qualify_game_water.py \
  --executable build/judas_game_water_lab --output /tmp/judas-water-proof
bash tools/diagnostics/run_game_water_lab.sh
```

For a local relocated preview:

```sh
python3 tools/diagnostics/package_game_water_lab.py \
  --executable build/judas_game_water_lab --output /tmp/judas-water-package
cd /tmp/judas-water-package
sha256sum -c SHA256SUMS
./play.sh
```

Existing output directories are refused. This is a desktop source-preview
package using the current host's runtime dependencies, not a published SDK.

## Remaining integration and product boundaries

See [the full usage/limitations guide](../../GAME_WATER_PREVIEW.md). Not yet
qualified: ordinary scene/JudasJS/editor ownership, streaming and origin shifts,
spatially varying radial gravity, general body shapes, player swimming, runtime
sources/drains, stacked-cave presentation or polished water materials. Only
uniform chart-aligned gravity and box coupling are currently supported.
Unsupported chart/gravity combinations diagnose instead of silently misbehaving.
Extreme pressure/CFL budget failures stop the lab; physical-step rollback is not
yet a transactional retry contract. The snapshot participant retains the normal
8 MiB bound. Windows/Android/browser qualification is not claimed.

The new foundation is usable for the requested early native pool/flow/float
demonstration. It is not yet a developer-facing replacement for the locked scene
backend. The next integration should use this measured foundation, not return to
tiny pressure fragments or exact arithmetic as runtime requirements.

## Evidence notes

`visual-qualified/`, `long-final/`, `rendered-final/`, `visual-final/` and the final unit logs
are the current qualification. `exploratory-records.tar.gz` retains unsuccessful
and superseded attempts, including a pinned source snapshot before phase-slip
correction. Not every earlier exploratory binary has a captured source snapshot;
those diagnostic logs are not presented as independently reproducible candidates.
The one MK2 cold-save usage-only invocation and the earlier package identity
wrapper mistake are recorded separately from passing validation.

Raw compiler flags and selected logs are stored as gzip files, preserving their
original bytes (including trailing whitespace) rather than normalizing evidence.

No old test was deleted or weakened. No main promotion, release, SDK selection
change, Alpha-lock change or independent-research modification was performed.
