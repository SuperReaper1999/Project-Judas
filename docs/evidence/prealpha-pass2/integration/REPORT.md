# Pass2 composed-origin, persistence and first-project evidence

## Scope and status

This directory preserves baseline probes and the actual development sequence.
**Scoped combined Linux validation is complete: 26/26 selected contracts pass.**
[FINAL_RESULTS.json](FINAL_RESULTS.json) ties those results to the frozen 52-file
inventory, exact embedded source identity and per-executable hashes.
[FINAL_RESULTS_PENDING.json](FINAL_RESULTS_PENDING.json) is the unchanged initial
placeholder, superseded by that final receipt. No human acceptance, universal
composition certification or Alpha readiness is claimed.

The original assessment and all 54 reconciled findings are in the
[parent register](../AUDIT_DISPOSITION.md). Physics has its own
[measured report](../physics/REPORT.md); this integration evidence does not replace it.

## Established baseline mechanisms

The two baseline command records name Experimental
`528cfb1b430d710ba5a4d2d13569aad106e3044e` and the exact preserved production
library SHA-256 `fd46ae5b078b32f2be9de4b85bd141d9552bc984595bed9c043279594375c694`.
Compiler, linker, arguments, working directories, exits and timings remain intact.

| Probe | Actual prior result | Preserved files |
|---|---|---|
| Origin admission | Active atmosphere, combustion and conserved liquid rejected by blanket ownership guards | [source](probes/origin-before.cpp), [log](baseline/run-before.log), [command record](baseline/ORIGIN_BASELINE.json) |
| Failed snapshot capture | Unsupported atmosphere rejected after clearing the caller's previous chunk map: `previous_retained=0`, `chunks=0` | [source](probes/capture-before.cpp), [log](baseline/run-capture-before.log), [command record](baseline/CAPTURE_BASELINE.json) |
| Gravity-equilibrium comparison | Plane centre and radial preferred-up differences rejected despite zero coordinate difference | [source](probes/equivalence-before.cpp), [log](baseline/equivalence-before.log) |

The first origin probe establishes the old blanket refusal; its deliberately
unprepared liquid owner is not a candidate-success fixture. Current supported
admission is tested with fully published, real baked data. Pending owners require
separate conservative admission/refusal controls.

## Generic correction boundaries

- Conserved-liquid owners and parcels translate once. Capacity/cavity geometry,
  wave cell volumes, discharge, velocity and previous surface history remain in
  their existing local frame; only derived world-space optical caches are cleared.
- Atmosphere reference origin translates with its celestial owner. Combustion
  retains scalar thermal state and generation-safe physical handles; the physics
  world owns their positions. No thermal/fluid integration equation is replaced.
- Gravity equilibrium compares the physical fields used by its selected model:
  a plane has no meaningful centre; a radial field has no preferred plane-up axis.
- Nonfinite and out-of-chart requests refuse before publishing a new chart.
  Active legacy PBF/emitted-particle ownership remains unsupported.
- Snapshot capture builds a detached map and swaps only after all participants
  succeed. Early unsupported ownership and late participant failures retain the
  caller's prior map. Lazy participant preparation is not a whole-world rollback.
- The shared entity-inclusion predicate retains the original transient/articulation
  exceptions. Unsupported thermal/legacy save checks still inspect all owners;
  marking a global thermal actor transient is not a save-capability bypass.
- Origin support does **not** imply modern slot support for atmosphere/combustion.
  Their explicit save refusal is preserved and tested.

## Genuine development failures and follow-ups

[DIAGNOSTIC_RUNS.json](DIAGNOSTIC_RUNS.json) lists the first nine runs;
their raw logs are under `diagnostic/`.

| Runs | Observation / disposition |
|---|---|
| 1, 3, 4 | Actual spill-equilibrium failure after 26 checks. Run 4 measures up error `3.3501496820591159e-7`, above the unchanged `1e-7` contract. Baking used a raw authored float quaternion while runtime construction normalized it. Failure appears before the origin mechanism is responsible. |
| 2 | Diagnostic executable had not yet been linked; no engine test ran. This is an environment/command failure, preserved separately. |
| 5, 6 | A second non-axis rotation produces an invalid Surface1 bake: `nonmonotonic capacity knots`. Run 6 preserves the complete diagnostic after the earlier expression omitted it. |
| 7 | The fresh-process child rejected physical-state reconstruction. Its exact diagnostic is retained in `run-cold-debug.log`; this run is not counted as passing. |
| 8, 9 | Correctly configured diagnostic cold reconstruction passes: 72 parent checks plus two independent eight-check children. The final combined run adds exclusion, late-failure and pending-owner controls (87 parent checks plus the same two eight-check children). |

The two rotated-bake failures are deferred authoring findings. Exact generated
cavities, baked data and sidecars from runs 1/3/4/5/6 are preserved under
`failed-bake-fixtures/`, with hashes in [COPY_MANIFEST.json](COPY_MANIFEST.json).
Small `.judasbasin` files are reproduction data, not executable binaries.
No accepted bake, original failure, numerical threshold or independent research
has been rewritten to make these cases pass.

The positive origin fixture instead uses the exactly represented non-Y quaternion
`(.5, .5, .5, .5)` and a separate radial case. That isolates translation from the
known bake defects. It proves that declared orientation subset; it is not a
claim that the failed general rotations now bake correctly.

## What the positive test actually observes

`tests/OriginEnvironmentTests.cpp` uses ordinary asset metadata, capacity/surface
baking, resource publication, RuntimeWorld and WorldPersistence. It checks actual:

- occupied liquid samples and independent old-chart miss controls;
- complete persisted dynamic-surface bytes, not a cached supported flag;
- conserved quantities, finite parcel positions and unchanged parcel velocities;
- rebuilt optical planes and continuing paired flow after translation;
- uniform non-Y and radial gravity, forward and return shifts;
- cold-process scene/native reconstruction, wave hash and physical query endpoint;
- atmosphere/gas/carrier state and continuing thermal exchange;
- explicit unsupported thermal/PBF persistence/origin boundaries;
- caller-output preservation on failed snapshot capture and repaired success.

The final combined source passes **87 parent checks and two independent eight-check
cold children**. It includes pending-owner/nonzero refusal versus no-op/publication
controls, late-capture atomic failure versus repaired success, and independently
eligible dynamic exclusion controls. These are additional observations, not
retroactive changes to the original 72-check records.

The first-project diagnostic runs the exact public example through a newly
created normal project: **20 checks** cover saved/reopened content, actual entity
rotation and project camera, failed source preflight, rejected reload retaining
live state, repaired reload and Stop cleanup. The composed lifecycle diagnostic
passes **34 checks**, including real surrounding-solid and navigation positives
to avoid an empty-collider false green. Neither headless case certifies editor
clicks or appearance; those remain in the human checklist.

## Reproduction

Build the focused targets using the current configured Release tree:

```sh
cmake --build build --target judas_origin_environment_tests judas_first_project_tests -j2
build/judas_origin_environment_tests /absolute/new-origin-output
build/judas_first_project_tests /absolute/new-first-project-output
```

The origin parent launches fresh `--cold` children itself. Separate preserved
failing contracts use new directories and must continue returning nonzero:

```sh
build/judas_origin_environment_tests --bake-regression normalization /absolute/new-normalization-failure
build/judas_origin_environment_tests --bake-regression knots /absolute/new-knot-failure
```

Do not reuse/delete the archived failure directories or weaken their thresholds.
The fixture creates all needed liquid data; no ignored source art, old milestone
cache or operator save is required. Original baseline probes must instead be
built against the recorded baseline source/library, not the corrected headers.

For actual SaveService build diagnostics, the existing Save Lab uses tracked
models/audio/fonts/cavities/bakes. Copy its tracked project into an owned directory,
use a fresh shared `XDG_DATA_HOME`, `SDL_AUDIODRIVER=dummy` and
`JUDAS_WORLD_STATE=none`, then run `judas_save_application_tests` in `write` and
fresh-process `read` modes. A working GL context is required for that application
test; offscreen/llvmpipe is functional evidence, not GPU qualification. Its new
assertion compares the durable diagnostic with the actual shared build ID.

## Preservation and finalization

No executable, object, static/shared library or unrelated game asset is packaged
here. [COPY_MANIFEST.json](COPY_MANIFEST.json) maps every copied file back to its
cache location with exact sizes/hashes. Empty compiler/linker logs are retained
rather than replaced with invented output.

The original diagnostics remain unchanged. Final receipts, source inventories,
known-failing data and raw logs are under `final/`, mapped by
[FINAL_COPY_MANIFEST.json](FINAL_COPY_MANIFEST.json). The current whole-directory
[MANIFEST.json](MANIFEST.json) hashes all evidence except itself.


## Final combined Linux results

The gate ran on Linux x86-64 / glibc 2.43 from the dirty Experimental candidate
based on `528cfb1b430d710ba5a4d2d13569aad106e3044e`. The final embedded source
fingerprint is `1c37839dac7066db466418f983819d6c86fe3663bfce38b91434c42d7c312f24`.
At execution, all 52 recorded intended source bytes matched their frozen hashes.
Later tooling-only edits have their own 53-file current inventory below. The gate verifies
source identity before and after execution and each executable's unchanged bytes.
Recorded unrelated untracked observations remain separately identified; this is
not an exact clean-commit/release artifact or a hermetic-build claim.

| Receipt | Actual result / scope |
|---|---|
| [Original combined manifest](final/original-manifest/RESULTS.json) | **25/26 pass.** Only the new origin test fails when it incorrectly calls `SetTransient` on a static metadata-only actor with no EntityRecord. Original failure/log preserved. |
| [Corrected combined manifest](final/followup-manifest/RESULTS.json) | **26/26 pass**, 2026-10-17 23:44 UTC. The follow-up changes only `tests/OriginEnvironmentTests.cpp` to use independently eligible dynamic thermal/nonthermal controls and clarifies starter HUD prose in `docs/FIRST_PROJECT.md`. No engine mechanism, physics threshold, controller or quality change. |
| [Origin environment](final/followup-manifest/origin-environment.log) | **87 parent + 2×8 fresh-process checks**, actual disturbed surface bytes/ledger, submersion/buoyancy, optical reconstruction, flow, parcels, physical queries, atmosphere/combustion continuation and explicit unsupported boundaries. |
| [First project](final/followup-manifest/first-project.log) | **20 checks**; exact public example, ordinary project/serializers, transform/camera endpoints, invalid-source refusal and repaired deliberate reload. |
| [Composed lifecycle](final/followup-manifest/composed-lifecycle.log) | **34 checks**; positive collision/navigation endpoints, async preparation through origin change, rejected replacement, cold restore and owner destruction. |
| [Supplemental commands](final/supplemental/RESULTS.json) | **32 ordinary positive commands pass**. The original manifest failure and correctly nonzero known-failures command are two additional records, not correctness passes. Includes affected rigid/ragdoll/CCD, M70/M77 cold stand/recovery, animation cold save, liquid/refill/combustion, editor Play/Stop/debug tools, M98 migrations/mod negatives, export and SDK inspection. |
| [Save storage](final/supplemental/save_storage.log) | **41 checks**; explicit diagnostic roundtrip and independent old-header missing diagnostic restore `unknown`. |
| [Actual SaveService write](final/supplemental/save_application_write.log) / [fresh read](final/supplemental/save_application_read.log) | **14 / 20 checks**; durable diagnostic equals actual shared engine build ID. No native save format or content compatibility identity change. |
| [Live API/type drift](final/supplemental/api_drift_live.log) | **473 public symbols, 40 exports**, runtime enumeration, TypeScript 5.9.3, 51 examples and four negative drift controls pass. |
| [Relocated standalone](final/supplemental/relocated_standalone.log) | The real first-project package launches from unrelated `/tmp` working directory; editor Edit/Play/Stop and screenshot evidence remain separate automated observations. |
| [Actual host SDK](final/supplemental/sdk_verify.log) / [ABI receipt](final/supplemental/sdk-qualification.json) | **17 tools / 212 files** pass current-host integrity and dependency inspection. This is the host glibc-2.43 artifact, not an older-distribution qualification. |

The supplemental commands ran before the two-file test/prose follow-up. Their exact
older binary hashes/embedded identities are retained. No runtime source changed
in that follow-up; affected final origin and all 26 gate cases were rerun together.
Those earlier receipts are not relabelled as later binary identities.

On the small paired origin fixture, measured shift costs were **0.00869 / 0.008265 ms**
for non-Y uniform/radial cases; displaced-volume error was **4.56e-16 / 2.38e-15 m³**,
buoyancy error zero and optical-path differences **7.79e-5 / 8.13e-5 m**. Full local
wave history and quantity ownership remain byte-identical through translation.
These are scoped continuity measurements, not large-pool performance claims.

## Preserved failures and release boundaries

The [known-failures receipt](final/known-failures/RESULTS.json) has **three failed
contracts**, all returning nonzero; none is counted as acceptance:

1. [Full curved-mesh continuation](final/known-failures/impact-mesh-continuation.log):
   four unchanged assertions fail after 90 steps. Initial arrival is corrected;
   subsequent one-sided glancing contact continuation is not.
2. [Rotated bake/runtime normalization](final/known-failures/liquid-bake-orientation.log):
   original `1e-7` equivalence contract fails before any origin shift.
3. [Rotated surface knots](final/known-failures/liquid-bake-knots.log):
   unchanged normal decoder rejects generated nonmonotonic Surface1 knots.

Exact final failing bake data and metadata are retained under
`final/known-failures/bake-orientation/Assets` and `bake-knots/Assets` as well as
the original development fixtures. Deferred FLU-001 remains unchanged and was
not reclassified by unrelated tests. The original `a106…` snapshot cannot acquire
historical continuation data it never recorded.

Dense-contact results remain mixed: **27.070→25.937 ms median,
413.328→416.695 ms p95, 2123.409→1970.799 ms worst**, with matched physical digests.
No predictable-real-time closure is claimed. See the separate physics report for
raw measurements and preserved rejected alternatives.

Native Windows **15 Python distribution controls** pass; current C++/SDK functional
validation is incomplete under severe guest disk pressure. VM timing is not a
performance gate. A new actual Android artifact and post-commit Ubuntu-24.04 CI
remain separate pending receipts. No new hardware, controller, Wi-Fi/Internet,
security or human gameplay acceptance is invented.

## Consolidated human acceptance checklist

1. Confirm editor/runtime/tool identity names Experimental and the same intended build.
2. Create the documented first project, Play/Stop, edit `rotationSpeed`, reject an
   intentional script error, repair/reload, save and launch its exported package.
3. Exercise supported vehicle/character/world-origin travel; verify physical
   interaction before and after shifting, then save/reload and revisit a region.
4. Inspect conserved liquid under non-Y/radial gravity through an origin change:
   existing disturbance, parcels and solid interaction must continue. General
   rotated baking failures remain separately reproducible and unaccepted.
5. Exercise cold M70/M77 recovery and an ordinary dense-world close. The stress
   workload still has documented long steps; known full curved continuation fails.
6. Inspect platform receipts before distributing: current-host Linux is proved;
   portable older Linux, full current Windows SDK and new Android hardware are not.
7. Keep these scoped automated results distinct from approval to promote main or
   publish Alpha. This candidate is not a universal engine capability certification.


## Late tooling control: original failure and corrected follow-up

The [late tooling receipt](final/tooling-original/RESULTS.json) records **6/6 CMake
controls pass and 5/6 gate unit controls pass**. Its [original failing assertion](final/tooling-original/validation.log)
assumes all known findings remain unscheduled, even though corrected first-arrival
CCD is intentionally now a normal scheduled regression. Unresolved/manual cases
must remain isolated; the resolved arrival case may be scheduled. The narrow [test policy correction and rerun](final/tooling-followup/VALIDATION_FOLLOWUP.json)
now pass **6/6**; the before-source and original 5/6 failure remain untouched.
The [separate source delta](final/tooling-followup/SOURCE_DELTA.json) records the
53rd intended file (`tests/test_prealpha_validation.py`), not a runtime or physical
condition change. The native gate retains its exact earlier 52-file receipt.
This does not invalidate the 26 native
case results or convert the three genuine failed physical/authoring contracts to
passes, and there is no global all-tooling-passed claim.


## Final source and actual Android tool follow-ups

The [final 53-file inventory](final/FINAL_SOURCE_FILES.json) is separately verified
with zero mismatches. Relative to the native 26 receipt it adds only the scheduling
unit test and changes the distribution control/independent APK verifier. No C++
engine, physical fixture, solver budget or threshold changed. The 52-file native
receipt remains intact rather than being rewritten to pretend it tested another
source identity. Final runtime/exporter/SDK/APK provenance refresh and post-commit
clean CI have their own qualification receipts.

Actual arm64 compilation and signing reached APK verification and revealed a
[canonical ABI-name mismatch](final/android-abi-followup/CAUSE.json): CMake emits
`arm64-v8a`, while the verifier accepted only `aarch64`/`arm64`. The original signed
APK bytes were preserved; the [corrected independent verifier](final/android-abi-followup/corrected-verifier.json)
accepts those same bytes (SHA-256 `03de6fa092b5fb23017b6fce3d651c6db4218735fa30f9d87e7779f22ab5bc3e`).
A real positive fixture and mismatched-ABI negative now bring Linux distribution
controls to **16/16 pass**. This is a packaging/tool correction, not an engine
architecture or signature/device proof. The original failure remains nonzero in
its command receipt. Final 53-artifact rebuild/qualification remains separate.


**Final platform status is maintained separately in
[PACKAGING_STATUS.json](../PACKAGING_STATUS.json).** The pending-artifact wording
above records the state when these native/tooling receipts were consolidated;
that status file records subsequent final-source APK/SDK results. It does not
supply new hardware acceptance or older-Linux CI results unless explicitly stated.
