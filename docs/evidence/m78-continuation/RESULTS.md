# M78 continuation — final combined candidate results

Starting checkpoint: `9e77b2226edcf217ab789fe4f0fea42a3415a72d`.
Uncommitted and unstaged. Human validation remains pending; this report does
not mark M77 human validated. No new milestone or fluid correction was begun.

## Mechanism isolation

The [isolation report](M77_MECHANISM_ISOLATION.md) records source overlays,
archive identities and both successful and failed variants. The original M77
controller, 60 Hz timestep, 29 N s impulse, authored rig/gains and thresholds
are unchanged. The original Linux and Windows failures remain in
`../m78-followup/REG-001.md` and its original raw receipts.

- Solver warm-start invalidation erased completed stance observations. A
  separate bounded ledger now publishes actual completed start/TOI rows;
  only re-solved primitive groups are replaced. Solver behavior is unchanged
  by this observation correction.
- Angular velocity at a TOI must correspond to that orientation, and the
  physical event state must use the right limit. Existing left-limit geometry
  evaluation remains available.
- The explicit gyroscopic drift plus momentum transport changed free angular
  energy. Implicit body-momentum midpoint with the corresponding Cayley drift
  conserves complete free-horizon momentum/energy within Newton and storage
  error. It has twelve bounded Newton iterations and explicit failure.

The observation correction alone passes the original unit fixture but fails
the actual application. The combined midpoint correction passes both. RK4,
removing gyroscopic transport and changing a planted-foot reference were
diagnostics, not retained fixes. The [independent equations and consumed
segment observations](M77_ANGULAR_MIDPOINT_PROOF.md) distinguish full endpoints
from approximate shortened CCD segments. No exact CCD-energy claim is made.

## Measured before/after

| Original case | First corrective candidate | Combined final Linux | Native Windows VM |
| --- | ---: | ---: | ---: |
| M77 controller | 119 checks / 2 failures | 119 / 0 | 119 / 0 |
| Actual GL/JS corrective step | 15 / 1 | 15 / 0 | 15 / 0 |
| Four original rigs, median | 1.040693 ms | 1.058026 ms | 1.8522 ms |
| Four original rigs, p95 | 18.875046 ms | 5.498837 ms | 10.839 ms |
| Four original rigs, maximum | 33.793832 ms | 26.653463 ms | 56.376 ms |
| Steps above 16.67 ms | 78 / 900 | 11 / 900 | 37 / 900 |

The same performance fixture uses four 13-body articulations, 48 joints,
8,192 triangles and 900 steps at 60 Hz. No mesh/contact/CCD/cadence reduction.
Its original p95 gate passes. Final mesh-4 has 56,576 contact rows, 19,163 CCD
iterations, seven bounded search limits and 26/52 sleeping after 15 seconds.
Mesh-1 remains 0/13 sleeping; no forced sleep or added damping.

The unchanged-workload Boolean-only isolation reaches 14.963051 ms p95 with
all physics counters identical. It avoids full manifold construction only
when an existence consumer has found an accepted witness. Complete response
manifolds remain bitwise equal to the original oracle across 43,980 samples
and 286,091 checks. The final angular correction changes the trajectory, so
its timing difference is not attributed solely to that optimization.

Original expensive-step geometry attribution was approximately 85% of
continuous-impact time (about 81% of the 34.207 ms whole step); continuous
impacts consumed 32.796 ms. On the final trajectory,
solver/CCD/contact/joint work accounts for 90.78% of summed p95-tail time.
The maximum step performs 285 CCD queries and 830 search iterations. Final
late ordinary operation is dominated by cooked contact construction.
See [raw timing and remaining-cost analysis](final-performance/README.md).
Native counters and sleep match Linux. Native p95 passes, but its maximum
exceeds the unchanged 50 ms comparison target; 54.2711 ms of that maximum
is solver time. The VM is a separate environment, without a comparable native
before workload or physical hardware-performance claim.

## Combined Linux execution

[LINUX_FINAL_MATRIX.json](LINUX_FINAL_MATRIX.json): **50/50 cases executed**;
49 expected passes and one intentionally retained failing fluid contract.
Additional final controller, actual corrective step, motion, angular-impact,
free-body, geometry-oracle and original concave-performance probes pass.

Affected checks include M70 physical animation, M77 foundation/variants/step/
recovery/regions/radial/negative controls, ragdolls/joints, contacts/cache/
storage, all 89 impact-timing cases, kinematics, CharacterMotor, gravity
selection and original uniform-gravity fixtures, persistence, separate-process
application saves, mixed lifecycle, touch events, spacecraft angular controls,
prefabs, streaming, scalability and navigation dependency transactions.
Counts include 10,239 motion checks, 1,076 contact-lifetime checks, 345 geometry
checks and 13,327 cache checks. No full production-suite rerun.

Fresh Balance Lab export: four registered scenes, seven assets, 54,536,980
bytes. The shipping runtime moved to `/tmp/judas-m78-continuation-balance-midpoint`
passes 400-frame startup, scene switching, save/load, pause/resume and reload
from an unrelated cwd with an unavailable engine-root override. Editor
duplicate/undo/Edit/Play/Stop passes and leaves its owned scene **IDENTICAL**.
Offscreen execution does not establish interactive pointer behavior.

## Native Windows execution

The [executed native matrix](WINDOWS_FINAL_MATRIX.json) and
[native report](WINDOWS_FINAL_REPORT.md) record final MSVC tests and package
identities. Prepared recipes are not counted as passes.
The matrix has 49 focused executions: 48 program passes and one in-progress
cold-read failure; the maximum-time target failure is reported separately.
Native builds reuse the verified baseline archive with three affected
translation units replaced and fresh test/application links. No new whole-engine
clean Windows build or zero-warning result is claimed.
The final MSVC archive is
`791edc06d69e9a0c750c2bf23f91b20936ed5c0d530f20a936eed0c544c9ba48`.
The unchanged controller passes 119/0 and the actual application step passes
15/0. The first application startup failed under the launcher's default VM
graphics-driver selection; the identical executable passes with the existing
`GALLIUM_DRIVER=llvmpipe` VM configuration. The failure and correction are
preserved in [windows-vm-gl-environment.json](windows-vm-gl-environment.json).
No runtime change was made for that environment correction.
The absent historical contact-cache fixture was copied byte-for-byte from
HEAD, and the identical contact-cache executable then passed all 13,327
checks. The initial failure is preserved separately from the corrected run.
M70/M77 variants, physics, contacts, CCD, gravity, retained prefab/streaming/
navigation/scalability corrections and relevant lifecycle checks pass.
Standing and recovery cold saves pass; in-progress cold save does not.
The new native package passes four-scene startup/switch/save/load/pause/reload
after moving and launching from unrelated `C:\Windows\Temp` with an unavailable
engine-root override. Software Mesa execution is not physical GPU/input/audio
or human acceptance. No full production suite was repeated.

## Retained failures and limits

- **FLU-001 stays failed:** 731 checks / one failure, 0.558701396 m/s against
  the unchanged 0.5 m/s-at-four-seconds Linux check. The proposed future
  settling criterion remains in `../m78-followup/FLU-001.md`. No fluid source,
  fixture, threshold or protected research was changed.
- The exact unchanged native fluid check passes at 0.419123590 m/s (731/0).
  The platform discrepancy is recorded; the Linux failure still leaves FLU-001
  failed, with no fluid changes or favourable overall claim.
- **In-progress cold save remains failed:** 13/1 natively and 13/1 when the exact
  native snapshot is loaded on Linux. The same captured native state continued
  uninterrupted stands. All dynamic physical state, mapped joint settings and
  controller history match exactly. Missing drive replay alone and waiting
  bypass alone fail; one-step warm-seed isolation demonstrates joint numerical
  history sensitivity. No sufficient generic correction was established, so
  none of those experimental overlays was retained. The exact binary save and
  commands are preserved in [the diagnosis/reproduction](M77_COLD_CONTINUATION_DIAGNOSIS.md).
- Shortened CCD drift retains energy interpolation error; force/torque kicks,
  impacts and constraints remain approximate. Midpoint nonconvergence throws
  explicitly; singular/locked-axis inertia retains its existing path.
- Original performance gates pass on Linux. Native median/p95 pass; native
  maximum 56.376 ms exceeds the unchanged 50 ms target. Individual collision
  spikes and incomplete mesh-rig sleeping remain measured limits.
- Human current-candidate/M77 acceptance and Windows physical GPU/controller/
  audio validation remain outstanding. A software-rendered VM is process proof.

## Identities and preservation

The original audit's 600-file and first follow-up's 439-file manifests remain
immutable. [PRESERVATION_SOURCE_FINAL.json](PRESERVATION_SOURCE_FINAL.json)
checks all 38,386 original tracked paths, with only 22 authorized changes and
no missing/unexpected changes. Independent protected verification checks
35,323 files / 1,910,079,101 bytes with no changed/missing historical files.
Seven local-only original inputs also match. Existing failures are retained.

Exact candidate paths: [CHANGED_FILES.txt](CHANGED_FILES.txt).
Final source/evidence hashes: [FINAL_FINGERPRINTS.json](FINAL_FINGERPRINTS.json).
Binary/package identities are recorded separately; executables, archives,
temporary projects and build outputs are excluded from repository deliverables.
The unrelated `POST_M73_REPORT.txt` remains untouched and excluded.
