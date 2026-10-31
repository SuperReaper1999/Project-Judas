# Advanced-fluid unlock qualification — 2026-10-31

Follow-up: [body-aware wet support](FLUID_BODY_SUPPORT_REPAIR.md) addresses the
confirmed lateral boundary failures below and records its own final-source tests.
This page preserves the earlier qualification at `82e324b2`; the Alpha lock and
the unsupported particle persistence/streaming/origin boundaries remain in place.

**Decision: keep the Alpha lock.** The original launch reproduction is repaired
and human-tested, but this pass does not qualify a general unlock. Experimental
project opt-in remains available; the default-off setting and warning are unchanged.
Main, published packages, conserved-water implementations and independent research
were not changed.

Baseline: `66534a25cd9d893a2966bbba9b9cb4766f0c75bb`. The containing commit adds
save admission protection, a lifecycle regression and this qualification evidence;
it does not change fluid forces, wet reconstruction, integration or solver budgets.

## What passed

Evidence is under [fluid-unlock-qualification-20261031](evidence/fluid-unlock-qualification-20261031/).
Raw failures are preserved alongside successful runs. Source/binary hashes and
environment are in `PROVENANCE.json`; the evidence archive has its own hash manifest.

| Exercise | Observed result and boundary |
|---|---|
| Wet classification | Existing 120-check suite passes, including positive immersion, dry gaps, separated pools and rotation. |
| Production buoyancy | 8,021 total checks, zero mandatory acceptance failures; **two existing diagnostic failures**, detailed below. Includes flotation, neutral/sinking bodies, rotated gravity, common freefall and light bodies. |
| Actual particle-field player | Full run: 5,661 total checks, zero mandatory acceptance failures; **four existing micro-rest diagnostic failures**. Includes density variants, rotated gravity, common freefall, zero gravity and swimming through water. The separate smoke and fall-only runs also pass. |
| Supported containers | Four supported stationary-container cases, 4,340 checks, zero failures. These do not certify general free-moving container momentum. |
| Contact mechanics | Seven cavity-impact cases pass independent momentum/energy controls; smoothing, boundary, rigid-coupling and particle-world suites pass. |
| Actual fluid-demo interaction | Flat pool 24/24 and planet 25/25: walk/swim, pick up a container, dip, acquire real particles, lift, carry, pour, destroy and reconstruct. Particle count/mass remain exact; no unresolved physical geometry is reported. Scripts/UI are deliberately removed by this existing headless test; native export below retains normal application content. |
| Original launch scenes | Current runtime runs 1,200 frames at 1/60 s each in flat and radial scenes. Bodies remain finite and settle. Ship peak speeds remain ordinary falling values: 6.7035 m/s flat and 2.0950 m/s radial. |
| Human observation | Operator explicitly observed no launching in the current original radial scene. This does not imply human acceptance of all cases in this table. |
| Feature policy | Project Water and Project Fluid Policy pass. Advanced dynamics remain off by default; dormant authoring does not activate a particle field. Basic Water and swimming policy remain independent. |
| Lifecycle restrictions | New 21-check regression verifies real solver advancement, opt-in persistence, exact authored-lattice reconstruction, and transactional rejection of unsupported modern snapshots, streamed particle owners and live origin shifts. |
| Ordinary save regressions | Legacy world-state 307/307; modern world persistence 27/27. |
| Relocated Experimental export | Exports 10 assets/16 scenes, 59,859,997 bytes; moves to a new directory containing spaces outside the source project, then runs 240 application frames without `JUDAS_ENGINE_ROOT`. Original radial gameplay is finite, with normal falling/settling. This is Linux host relocation, not clean-machine certification. |

The first policy run failed its export assertion without an engine-resource root.
It passed all 26 checks with the required `JUDAS_ENGINE_ROOT` supplied. Both logs
are retained; the initial failure is not represented as a production fluid defect.

## Genuine save defect corrected

Modern slots already reject active legacy PBF owners because no particle snapshot
participant exists. The legacy delta-save preflight, however, returned success
despite omitting the evolved particle state. A new regression failed against that
implementation (`lifecycle-before-legacy-guard.log`).

`WorldState.cpp` now rejects active particle fluids both in preflight and direct
capture, explaining that particle continuation is not recorded. Rejection does
not alter the live particles or existing modern snapshot destination. Default-off
particle authoring still saves normally. Existing files are not deleted or migrated.
This is **honest rejection, not implementation of fluid cold saves**. Old delta
saves cannot be assumed to restore water; this patch does not invent absent state.

## Remaining boundary failure

`BoundaryProbe.cpp` exercises independently authored particle occupancy, with a
positive fully wet control and a small dry body inside a hole in one connected
ring of water. It sweeps hole sizes in ordinary and rotated gravity.

The larger holes correctly report dry. Two smaller holes incorrectly report
**fraction 1 and 78.48 N**, against zero expected occupancy/force, in both frames:
four failed checks. In the clearer case the nearest water column is 0.5 m from
the centre, its half-width is 0.125 m, and the body's half-width is 0.1 m: a
0.275 m lateral dry gap remains. This is a frozen particle-field counterexample,
not a new dynamic launch or a static equilibrium claim for an unsupported hollow
pool without confining walls.

The connected convex envelope intentionally bridges particle holes displaced by
immersed solids. It cannot currently distinguish those holes from all transient
dry holes/air pockets. This confirms a documented modelling limitation rather
than contradicting the fixed external-gap reproduction. Do not count the failing
probe as a green acceptance suite or describe wet ownership as generally solved.

The next useful correction must distinguish body-displaced holes from genuinely
dry space while preserving the already-passing submerged-body controls. Simply
removing interpolation would repeat a previously failed approach. No speculative
replacement is integrated in this pass.

## Other retained limits

- Production diagnostics: the zero-gravity reference pool is not quiescent at
  release; the 0.864 kg sub-particle-scale body does not settle at desired immersion.
  The existing test explicitly treats that scale as observational. No thresholds,
  classifications or controller settings were changed here.
- Full player diagnostics: four fixed-warmup pools fail the stricter microscopic
  rest condition. Macroscopic motion/swimming checks pass. This is not proof that
  every pool rests quietly.
- Container interaction runs observe maximum transient particle speeds of about
  25.87 m/s (pool) and 49.74 m/s (planet). Existing gates pass, but do not impose a
  speed bound. No new safety certificate is inferred from their pass counts.
- Active PBF **modern and legacy cold continuation is unsupported**, as are
  ownership by streamed regions and live origin shifts. Existing rejection is
  preferable to quietly losing or mistranslating state. Supported conserved-liquid
  persistence is a separate representation and does not qualify particle saves.
- Exterior momentum exchange remains approximate/one-way in the documented path.
  Historical FLU-001 and research failures are untouched.
- No new native Windows result. The VM is shut off and configured for 6 GiB on
  this 8 GiB host; it was not started under current memory pressure. Windows
  compilation/runtime qualification and Android fluid budgets remain pending.
- No full advanced-fluid editor UI automation, hours-long soak or arbitrary
  networked-fluid replication qualification is claimed.

## Runtime cost

Linux Release, GCC 15.2, Ryzen 5 5600. The existing whole-step performance fixture
uses 125 particles and 360 fixed steps per scene. The first run overlapped other
qualification work and is diagnostic only. A subsequent run without our concurrent
compiler/player workloads produced:

| Scene | Whole-step median | p95 | Worst | Mean |
|---|---:|---:|---:|---:|
| Classic | 1.373 ms | 3.471 ms | 5.446 ms | 1.947 ms |
| Terrain planet | 1.513 ms | 6.749 ms | 7.019 ms | 3.259 ms |

Both unchanged fixture budgets pass. Peak process RSS was 30,556 KiB, excluding
editor/renderer memory. This is CPU fluid/physics timing, not GPU or full-game FPS.
Nearest-rank p95 and raw rows are preserved in `MEASUREMENTS.json` and
`performance-isolated/*.csv.gz`. These are current costs, **not a claimed speedup**.
Larger production cases still cost roughly 11 ms amortized coupling per step for
the 1,152-particle light-body fixture, and have little frame budget to spare.

## Reproduce

```sh
cmake --build build --target judas_fluid_lifecycle_qualification_tests judas_project_water_tests judas_project_fluid_policy_tests judas_production_fluid_tests judas_player_fluid_integration_tests judas_fluid_demo_integration_tests judas_production_fluid_performance -j2
JUDAS_ENGINE_ROOT="$PWD" ./build/judas_fluid_lifecycle_qualification_tests
JUDAS_ENGINE_ROOT="$PWD" ./build/judas_project_fluid_policy_tests
./build/judas_production_fluid_tests --output /tmp/fluid-production-fresh
./build/judas_player_fluid_integration_tests --output /tmp/fluid-player-fresh
./build/judas_fluid_demo_integration_tests --scene pool --output /tmp/fluid-pool-fresh
./build/judas_fluid_demo_integration_tests --scene planet --output /tmp/fluid-planet-fresh
./build/judas_production_fluid_performance --output /tmp/fluid-cost-fresh
```

Use fresh output directories. Diagnostic failures are reported separately by the
existing suites; `--strict-diagnostics` promotes them to failure without changing
their thresholds. The boundary probe is a standalone C++17 diagnostic linked to
the matching engine libraries; its failing exit is intentional evidence, not an
expected pass. The retained native scripts record exact scene/frame commands.

## Human acceptance checklist before any unlock proposal

1. Inspect actual flotation/sinking and swimming, not only the now-calm original scene.
2. Inspect container pickup/filling/carry/pour in both flat and radial gravity.
3. Resolve or explicitly accept a narrowly stated dry-hole/air-pocket support boundary.
4. Accept explicit save/stream/origin restrictions; do not promise particle persistence.
5. Complete native Windows qualification and choose practical platform/workload budgets.
6. Review quiet-water jitter and the recorded high transient particle speeds.
7. Only then consider removing the channel-wide lock. Retain default-off project
   opt-in and its warning. Publication/promotion still require operator approval.
