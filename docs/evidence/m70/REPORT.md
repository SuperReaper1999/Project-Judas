# M70 candidate evidence

Accepted starting checkpoint: `934c5d3f0556c920cc7cae8b80dc4677d8cbf87b`.
Candidate remains uncommitted. Linux execution and numerical/physical checks do
not certify Windows or substitute for operator review of motion quality.
The operator accepted the other lab behaviours but reported a passive-mode
framerate collapse, then a G/floor freeze in the first performance handback.
The [current consumer/frozen-landing follow-up](development/passive-crash-follow-up/REPORT.md)
records the exact core replay and generic numerical correction; human re-test
remains pending. The [preceding passive performance follow-up](development/passive-follow-up/REPORT.md)
and earlier results below remain evidence for their preceding byte cohorts.

Current correction: the identical frozen CCD replay completes in 29.343 ms
(recipe re-check 29.937 ms), versus a retained timeout beyond 35 seconds. All four
600-step project runs finish. Settled passive median/p95/max is
1.245/3.682/5.213 ms; full active wall contacts remain expensive at
16.120/39.001/65.979 ms overall. These are CPU measurements, not a frame-rate
guarantee. Explicit owner-script contact opt-in is proven on unscripted bones
against unscripted ground. Affected numerical/contact/API checks, editor
Play/Stop and corrected Linux moved-package landing/lifetime smokes pass.
The corrected desktop package is reopened for human re-test. Windows M70
moved-package validation remains outstanding; nothing is committed/pushed/tagged.

## Implementation ownership

- Shared-target IK: bounded simultaneous local-pose solve, explicit body-root,
  spine, chains, limits and position/orientation contact frames.
- Partial physical animation: existing mapped rigid bodies, M45 constraints,
  bounded relative joint drives and M47 final pose contributions. No second
  skeleton or physics solver, no world-space root pose servo.
- Ordinary lab: project JavaScript chooses targets, support motion, impacts,
  mode transitions, motor intent, camera, UI and saves through public APIs.
- [Phase/ownership table](../../M70.md) was recorded before physical-animation
  code. Opted-in motor/reference preparation precedes physics; the disabled
  legacy scheduler keeps its accepted phase. Presentation cannot create drive
  history, advance clips or become a physics target.

## Approved integration fixture

[Character Lab](../../../projects/character_lab/README.md) has three registered
scenes and an ordinary prefab. Original CC0 fixtures retain 65 source joints,
71 source hierarchy nodes plus the standard normalized motion root (72 runtime
nodes), six parts, two differently ordered complete skin
palettes (130 entries), helpers/fingers and four clips. The second figure has
different joint names/proportions and its own explicit mapping. Both use the
same import/resource/renderer path. Restricted Skate/M66 content is not shipped.

The wall scene requests simultaneous palm and sole position/orientation frames.
The board scene uses the same solver for translating/tilting support and a
step variant. Its actual fixed collider is distinct from a body-free visual
clone that samples known fixed transforms at presentation alpha. That clone
does not contribute collision geometry or supply targets.
The physical scene combines motor locomotion, a partial upper region, real
impulse, effort disable/restore, full handoff, passive release and explicit
collision-validated animation return. It does not implement balance/get-up.

## Validation commands and result records

The integrated Release build and necessary affected correction builds completed
on Linux with GCC 15.2.0; the final frozen-source build reported zero warnings.
Commands/receipts and source fingerprints accompany this report. No result is
inferred from source existence. The relevant final pass reused affected legacy
coverage; no full production-suite or historical research rerun was performed.

| Check | Normal command / scope |
|---|---|
| Shared target mathematics | `build/judas_multi_target_ik_tests` |
| Articulation/drive/authority mathematics | `build/judas_physical_animation_tests` |
| Imported project + actual VM + examples/lifetime | `build/judas_character_lab_tests <fresh-results>` |
| Fresh-process durable write | `build/judas_character_lab_tests <fresh-cold-results> cold-write` |
| Separate-process durable read | `build/judas_character_lab_tests <same-cold-results> cold-read` |
| M56 same-rig 1/10-instance cases | `build/judas_character_lab_performance <results>` |
| Type/reference/source/live enumeration | `scripts/check_judasjs_api.mjs` with normal TypeScript checker and actual `surface.json` output |
| Ordinary application | `build/judas projects/character_lab/character_lab.judasproj` |
| Packaging | Existing M38 exporter; all three registered scenes, scripts, UI and cooked models |

Automated hidden-GL runs use `SDL_VIDEODRIVER=offscreen` and dummy audio, with
isolated output/user-data directories. Desktop-visible operator review is
separate. Application logical-input scripts are retained with results. They
exercise public actions, ordinary scene switching, save/load and reload; no
private JavaScript/native dispatch or privileged project mode is involved.

### Preceding candidate executed results

All following preceding-candidate executables completed successfully. Original
failed or timed-out runs remain preserved and are not counted as successful executions.

| Evidence | Observed result |
|---|---|
| `focused/ik-final.log` | Shared-target core: 38 checks, zero failures. |
| `focused/physical-final.log` | Articulation, drives, transitions, covariance, cache invalidation, actual sleeping/convergence/wake: 64 checks, zero failures. |
| `focused/lab-final.log` | 66 actual-project/VM assertions passed and the process exited zero, including source-normalization, independent contacts, fixed snapshots, layers, hidden head, sockets, prefab, contact attribution and restoration. The earlier teardown error and debugger diagnosis remain preserved below. |
| `focused/cold-write-final.log` | Separate writer: 10 checks, zero failures; real atomic M61 slot committed. |
| `focused/cold-read-final.log` | Separate fresh-process reader: 14 checks, zero failures; targets, physical authority, masked layers and durable JS state restored without initialization replay. |
| `focused/api-types-runtime.log` | TypeScript 6.0.3, 35 examples, 310 public symbols / 27 exports / 213 native operations, live VM enumeration and three negative drift controls all passed. |
| `focused/cookbook.log` | Existing cookbook execution: 278 checks, zero failures. The two new unchanged scripts also executed in the full-rig lab harness. |
| `focused/authoring-desktop-permissions.log` | Unchanged authoring regression: 34 checks, zero failures with desktop graphics permissions. Sandbox-only distant-marker pixel test had blank rendering and is preserved separately; no Renderer correction was retained. |
| `focused/app-wall-corrected.log` | Ordinary wall application exited zero after normal F6/F7 save/load and authored reload. SaveService's new participant header correction is exercised here. |
| `focused/app-board.log` | Ordinary board application exited zero after its public actions, save/load and reload. |
| `focused/app-physical-final.log` | Ordinary partial/full/passive/return application, impacts, prefab, save/load and reload exited zero. |
| `focused/editor-export-final.log` | Editor duplication/undo and Play/Stop both retained IDENTICAL authored content; normal exporter selected eight runtime assets. Exited zero. |
| `focused/package-wall.log`, `package-board.log`, `package-physical.log` | All three moved-package applications exited zero using ordinary controls, durable state and reload. |
| Affected existing regressions | Pose composition 16, legacy ragdoll 36, joints 29, CharacterMotor 32, persistence 27, model runtime 23, presentation 44 and platform-services 14 checks passed. Existing sleep/resistance and authoring coverage also passed. Each accepted result was reused rather than repeated under new labels. |

At the observed steady samples, both rigs' wall contacts were below **0.82 mm /
0.081 degrees**, tilting-board contacts below **3.54 mm / 0.138 degrees**, and the
board-step contacts below **0.113 mm / 0.0058 degrees**. Each was independently
checked against the requested 1 cm / 2 degree envelope, not inferred from aggregate
convergence. The final frozen-source physical check has now passed **64/64**.
Its tensor-aware drive covariance difference was **6.37e-7 m**, compared with the
preserved scalar-drive discrepancy of 16.9 mm; sleeping/convergence/wake checks
also passed without changing the existing sleep policy. Numerical/physical checks
do not claim that source animation quality has improved or that a driven ragdoll
balances itself.

### Preceding candidate HUD-only follow-up

The lab now displays each actor's worst drive angle error in degrees and mapped
joint key, peak torque in N m, and saturated/total drive count through the existing
`physicalState.drives` snapshot. This final project-JavaScript change affects
presentation only; motion, physics, targets, runtime APIs and the executable are
unchanged by that HUD addition. Earlier numerical and M56 performance results
remained applicable to that candidate; the subsequent passive correction has
its own linked follow-up. `focused/lab-hud-final.log` passed all 66 checks and
exited zero;
`focused/app-physical-hud-final.log` exited zero with readable diagnostics;
`focused/editor-export-hud-final.log` exited zero with IDENTICAL undo/Play-Stop
content and eight exported assets. The refreshed package's
`focused/package-wall-hud-final.log`, `package-board-hud-final.log` and
`package-physical-hud-final.log` all exited zero from the unrelated working
directory (1.62, 1.07 and 2.12 seconds respectively). No C++ rebuild or broader
validation rerun was needed.

### Preceding candidate moved package

Playable package: `/tmp/judas-m70-character-lab-20261008/`; launch its `judas`
executable with no scene/project arguments. The preceding HUD-refreshed package
cohort contained 31 files / 53,029,658
bytes (50.57 MiB). The test launched the absolute executable from the unrelated
`/tmp/judas-m70-unrelated-working-directory`, after renaming the exported owned
source copy's `Assets` and `Sources` out of reach. All three registered scenes
loaded from the package itself. This source-hiding operation affected only a
task-owned export copy, not the repository project or consumer workspaces.
Application action scripts are under `application/`; moved-package screenshots
were written in the unrelated working directory. The package remains temporary
local output and is not a committed executable/build artifact.
This retained size/result cohort predates the passive correction. Current
package refresh/checks belong to the linked passive follow-up and its receipts;
the old byte count is not asserted for the refreshed executable.

## Preserved development failures and corrections

| Record | Cause and correction |
|---|---|
| `development/lab-asset-identity-first.log` | Generator tried to remint already imported asset metadata; corrected to reuse importer-owned stable identity/recipe. |
| `development/lab-socket-fields-first.log` | A named socket fixture omitted complete offset TRS; corrected authored rotation/scale fields. |
| `development/lab-named-json-first.log` | Named format canonicalized the ordinary script-properties JSON wrapper into its string representation; generator now reads either representation and writes through named patches. |
| `development/lab-generator-corrected.log` | Corrected original fixture generation. |
| `development/lab-authored-m70-settings.log` | Complete named regeneration using the rebuilt CLI: authored IK/physical settings, explicit physical subset, interpolated visual boards and normal prefab. |
| `development/lab-first-runtime.log` | X11 display unavailable in sandbox; no engine assertion or project execution occurred. Automated run uses hidden offscreen GL; human desktop validation remains separate. |
| `development/lab-offscreen-first.log` | Hidden GL succeeded; first world build returned normal asynchronous `loading`. Focused harness now performs bounded resource wait/retry only for that status. No runtime/demo policy workaround. |
| `development/lab-integrated-first.log` | Initial 53-check run reported nine harness failures. Frame input was submitted before `BeginFrame`, which clears pressed edges; public control pulses now enter after that boundary. Each copied example now has its own tracked source path. The expected hierarchy count now includes the normal importer-added motion root (71 source / 72 runtime nodes). No runtime change was made for these harness corrections. |
| `development/lab-cold-read-first.log` | The separate-process reader expected JS instance state before ordinary script synchronization had constructed the instance. Restore correctly queues durable records. The check now synchronizes through the existing instance-construction path, compares state before callbacks, then verifies resumed fixed steps without replaying initialization. No runtime change was made. |
| `development/lab-teardown-interactive.log` | Debugger reproduced all 66 assertions passing, followed by a harness teardown hang: explicit host shutdown destroyed jobs before stack-held worlds destroyed their scene/save services. `SaveService::~SaveService` subsequently called `JobSystem::Cancel` through its expired host pointer. The harness now uses normal reverse-scope destruction (worlds/services first, host last). No deadline extension or runtime change. |

Additional genuine numerical/physical failures, original warning output and
corrected follow-ups are retained alongside the final results.
Source inspection also found that clearing disposable animation cache during
same-asset save/stream restore lost the prepared IK mapping: restoration now
rebuilds the mapping from durable ordinary component settings, leaving sampled
status and drive-motion history disposable. Cold and suspension checks cover it.
The ordinary application additionally exposed that SaveService's participant
header allow-list had not included the new physical-animation chunk. The normal
M61 container now permits that versioned participant; application save/load
follow-up is recorded separately from direct SaveStorage restoration checks.

`development/performance-preliminary/` preserves the first same-rig summary and
ten-instance partial M56 capture. This run overlapped a final editor build, so its
absolute timings are not an isolated acceptance measurement. It nevertheless
exposed approximately 22,100 ordinary allocations per articulation/step and an
expensive drive scope even in passive mode. Source tracing found repeated region
mapping: each region key repeatedly searched every mapped bone through the
canonical skeleton-key finder, twice per step. The correction caches the already
validated mapping with explicit configuration/asset/restore invalidation.
Preceding-candidate same-binary measurements were taken after that correction
with the build idle;
no target quality, cadence or physical effort is reduced to conceal the cost.

## Preceding candidate performance interpretation

The retained comparison uses the same preceding-candidate binary and full figure,
60 warm steps then 120 measured 60 Hz steps, in disabled / shared IK / partial / passive modes,
with one and ten instances. M56 captures contain individual scopes, median,
p95/max, solver iterations and dynamic/awake counts. Ordinary C++ `new/new[]`
calls are observed inside the step only; this does not count C malloc/QuickJS,
aligned allocations, GPU memory, startup or profiler collection. These are CPU
pose/physics measurements, not a renderer/GPU or interactive FPS certification.

Preceding-candidate build-idle measurements are in [`performance/summary.json`](performance/summary.json)
and its eight ordinary M56 captures. Inclusive scope medians are shown separately
from total fixed-step median/p95/max; the scopes should not be summed as an
independent end-to-end timing model.

| Mode | Instances | Fixed median / p95 / max ms | Pose median ms | Drive median ms | Rigid physics median ms | Physics-to-pose median ms |
|---|---:|---:|---:|---:|---:|---:|
| Disabled | 1 | 0.061 / 0.074 / 0.090 | 0.011 | 0.008 | 0.001 | <0.001 |
| Shared IK | 1 | 0.109 / 0.129 / 0.166 | 0.060 | 0.007 | 0.001 | <0.001 |
| Partial | 1 | 0.141 / 0.206 / 0.484 | 0.026 | 0.013 | 0.043 | 0.023 |
| Passive | 1 | 0.467 / 3.350 / 14.226 | 0.029 | 0.013 | 0.391 | 0.029 |
| Disabled | 10 | 0.536 / 0.588 / 0.621 | 0.078 | 0.069 | 0.002 | <0.001 |
| Shared IK | 10 | 1.095 / 1.183 / 1.366 | 0.612 | 0.070 | 0.003 | <0.001 |
| Partial | 10 | 1.743 / 2.778 / 5.297 | 0.281 | 0.174 | 0.600 | 0.273 |
| Passive | 10 | 15.417 / 38.952 / 75.039 | 0.290 | 0.150 | 14.449 | 0.368 |

| Mode / instances | IK solve median / p95 / max µs (sum) | Iterations median / max (sum) | Dynamic / awake bodies | Ordinary new calls median / p95 / max |
|---|---:|---:|---|---:|
| Disabled / 1 | 0 / 0 / 0 | 0 / 0 | 0 / 0 | 18 / 18 / 18 |
| Shared IK / 1 | 44.198 / 54.554 / 65.299 | 1 / 1 | 0 / 0 | 119 / 119 / 119 |
| Partial / 1 | 0 / 0 / 0 | 0 / 0 | 8 / 8 | 83 / 85 / 137 |
| Passive / 1 | 0 / 0 / 0 | 0 / 0 | 16 / 0–16 | 101 / 342 / 796 |
| Disabled / 10 | 0 / 0 / 0 | 0 / 0 | 0 / 0 | 153 / 153 / 153 |
| Shared IK / 10 | 447.739 / 510.685 / 548.830 | 10 / 10 | 0 / 0 | 1163 / 1163 / 1163 |
| Partial / 10 | 0 / 0 / 0 | 0 / 0 | 80 / 80 | 771 / 782 / 1301 |
| Passive / 10 | 0 / 0 / 0 | 0 / 0 | 160 / 96–160 | 1777 / 3225 / 4998 |

The steady IK workload requests four reachable rest-relative perturbations
(+0.025 m Y, −0.015 m Z), and converged in one iteration per instance. These
numbers are not the worst cost of a 64/96-iteration conflicting-contact solve;
the ordinary wall demonstration separately used seven/eight iterations.
The control retains the same full imported asset and explicit disabled physical
policy, with no articulation or shared solve allocated.

The build-idle uncached follow-up is retained in
`development/performance-uncached/`. On the same fixture/cadence, ten-instance
partial drive median fell from 10.998 to 0.174 ms and ordinary allocation median
from 220,821 to 771 after validated mapping caching. Passive drive median fell
from 10.807 to 0.150 ms; no cadence/effort/quality reduction was used.

**Preceding candidate measured limit:** ten passive full articulations (160 bodies,
96–160 awake during this collapse/contact window) still have a 38.952 ms fixed-step
p95 and 75.039 ms maximum. The existing rigid contact scope accounts for
37.997 / 74.029 ms of those p95/max values. This is an honest contact-heavy limit,
not a crowd-size/60 FPS guarantee and not hidden behind an average. Broad contact
or sleeping optimization was outside that candidate's planned work. The later
human-reproduced two-rig partial-to-passive failure is a different workload;
its [current follow-up](development/passive-follow-up/REPORT.md) supersedes these
passive timings for the repaired actual-project scenario. Historical one/ten-rig
results are not silently relabelled as measurements of the corrected binary.

## Remaining review boundaries

- Linux/GCC Release numerical, physical, hidden-GL, editor and package checks
  executed; Windows runtime/graphics/controller hardware was not tested here.
- Other lab behaviours have operator acceptance. Passive performance re-test
  remains pending after the measured correction. Offscreen SDL's relative-capture
  warnings and CPU measurements do not certify desktop FPS or final acceptance.
- No automatic contact/gait/recovery/balance or source-animation repair is
  provided. IK is bounded and may report limited targets; physical joints may
  lag or saturate. Positive uniform participating scale is supported; singular,
  sheared/reflected active paths are diagnosed.
- Restricted original art and Claude's live game are untouched. Later actual
  consumer adoption and M67's deferred human checklist are not claimed complete.
- Source overlaps/adoption guidance are in [M70.md](../../M70.md). Final changed
  file/fingerprint/Git receipts accompany this report; candidate is uncommitted.

## Human review

1. F1: inspect four wall contacts on both differently proportioned rigs. T changes
   targets, I asks an impossible contact; examine each distance/angular residual.
2. C/L/H: inspect crossfade, masked Carry and hidden visual head continuity.
3. F2: inspect both feet as the ordinary support translates/tilts; T adds the
   stepping variant. Check joints, imported mesh parts and attached socket props.
4. F3: use WASD/Space with the motor and J to hit the actual mapped arm.
5. K: remove/restore physical effort while the visible physical region remains
   the actual dynamic result. Inspect limits, contacts and finite response.
6. G/P: full physical handoff and passive release; limbs stay constrained and
   existing velocity is retained. No balancing/standing behaviour is promised.
7. N: explicit animation return; verify blend, no bind flash and a clear refusal
   when placement/path is blocked. The chosen safe demo placement settles under
   ordinary gravity, rather than performing an automatic get-up.
8. Z in F3: independent ordinary prefab instance. F6/F7 save/load, R reload,
   Escape pause/resume; no stale bodies, constraints, targets or pointer state.
9. Repeat in moved M38 package, including all three scenes and durable state.

Claude's subsequent real Skate adoption remains a separate consumer review.

## Current consumer and landing-freeze follow-up

The subsequent human G/floor freeze rejected the preceding passive performance
handback. The [consumer and numerical landing follow-up](development/passive-crash-follow-up/REPORT.md) records
the exact core reproduction, rotation-sampling correction, explicit owner-contact
subscription, API conventions and affected results. Windows M70 moved-package
validation remains outstanding. The corrected landing candidate requires human
re-test; prior successful measurements retain their preceding-candidate scope.
