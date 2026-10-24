# Pre-Alpha Linux qualification and MK2 diagnostics

Accepted starting point: `0aaef603b0347a55350a81beff896cff21d914bf`.
Experimental work; no main promotion, M100, release publication or channel change.
Windows and Android qualification are explicitly deferred by the operator.

## Diagnostic correction checkpoint

The production hold branches did not update the old fallback fields. Completed
StepCoupled native statistics summed intervals, but its Latest CCD profiler
counters still described the final inner interval. The corrections instrument
actual branch execution and publish completed macro totals. Restitution retry and
fallback paths also have explicit bounded frame-sum telemetry. Solver decisions,
budgets, collision fidelity and simulation ordering are unchanged.

New ordinary regression: `judas_mk2_diagnostics_tests`, scheduled in focused CI.
106 checks prove real per-body and global limit holds, actual ledger/velocity
consequences, coupled totals, profiler/native parity, free-motion negative controls,
zero-reset behaviour and identical physical endpoints with collection on/off.
The unavailable-manifold and energy-rejection reasons are counted at their actual
return sites; the targeted fixture does not claim to exercise every rare rejection.

The first stationary-ledger oracle was rejected: a legitimately stopped sphere
is not a safety hold. Its failure, fixture search and original probe remain in
[evidence](evidence/prealpha-linux-readiness/diagnostics/COMPARISON.json). All 303
recorded physical endpoint/velocity rows from that probe remain exactly equal to
the accepted binary, SHA-256
`e04b6e646151403c3a2f0f9cf6030c23399b89388f02e0ab2e082579ac0b328f`.
The real box fixtures independently exercise the safety branches without changing
budgets or adding a test-only collision mode.

Fifteen selected combined native Linux cases passed: MK2, balance, motor,
continuation, origin/assemblies, composed lifecycle, streaming, network sessions,
advanced surface, default-off policy, actual settings UI, swimming and separate
basic-water writer/reader processes. Four original mesh/contact cases also passed.
The full production set was not rerun. The supplied raw results identify the dirty
candidate source and tested binaries; this checkpoint is not a clean-release claim.

## Completed Linux SDK qualification

The initial build used a fresh `git clone --shared --single-branch --branch
Experimental` checkout at `ca3de268`, with only tracked source files. Sharing the
read-only Git object store avoids copying history; no previous build output,
source dependency extraction or untracked experiment was reused. Bundled immutable
dependency archives supplied text/render dependencies without downloading them.
The first configure overlapped checkout completion; it was rerun after checkout
finished. The final configured source was clean with zero observed untracked build
inputs (generated Python caches are excluded from source identity).

The fresh `judas_linux_sdk` plus diagnostic-test build took **7 min 54.10 s** at
`--parallel 2`; the recorded largest child RSS was about **1.09 GiB**, not an
aggregate system-RAM measurement. All 17 public tools staged successfully. The SDK
is **903 MiB** on disk, with 232 manifest payload files. No size or speedup over an
older build is claimed. The test package from the later tool workflow was about
56.9 MB including runtime and four assets.

Qualification found two bounded workflow defects, corrected in `8c903dfc`:

- `judas_model_import_cli --create PROJECT ...` previously interpreted a project
  file as a directory, unlike other tools. It now accepts either a `.judasproj`
  file or the existing directory form; a missing project file produces a useful
  project error before creating directories. Both forms were imported, cooked and
  inspected successfully. The directory convention is preserved.
- Combining editor polish/export automation sent the polish test's deliberate
  Escape into the export dialog. Their scripted frames are separated now. This
  was test interference, not an exporter defect; the original cancellation and
  independent successful export are preserved.

The clean checkout was fast-forwarded to that verified correction and the SDK
rebuilt in **29.25 s**. This is an incremental follow-up, not a second claimed clean
build. Final clean build identity is
`0.1.0-prealpha-Experimental-8c903df-clean-434cb9ba4d4a`. Source revision and tree,
compiler, binary hashes and all tool identities are in the evidence. The final
fresh-build diagnostic executable still passes all **106 checks**.

The SDK was copied outside its build directory and executed with `/tmp` as cwd and
`JUDAS_ENGINE_ROOT` unset:

- All 17 tool identities exactly match the SDK manifest. The host loader resolves
  their dependencies. Checksums verify before and after the actual workflows.
- Packaged editor creates an ordinary Visible script starter using its own docs,
  tracks game.js, runs the script, protects unsaved work, tests navigation/picking,
  saves, exports, and restores authored content on Play/Stop.
- **19 actual operations across eight tools** match expected outcomes: scene,
  project and UI authoring/validation; script checking plus syntax-error/recovery
  and missing-file controls; save identity; model import/recipe/cook/inspection;
  visual mesh cooking; volume creation/validation/excavation/cooking; deformable
  baking; and standalone export. Identity checks do not certify every semantic
  operation of all 17 tools.
- Editor and CLI exports were copied again to unrelated `/tmp` directories. Each
  ran 90 fixed steps, executed its project JS and rendered a screenshot. File-call
  traces show **zero accesses to the SDK or development tree** during these runs.
  Host drivers/system libraries remain required; this is not a static Linux game.
- An incomplete/corrupt SDK manifest is correctly rejected. Packaging's 28
  positive/negative controls pass. API/type/reference checking passes for 473
  public symbols, 40 exports and 51 examples; no new JudasJS binding was added.

Reproduction, replacing paths with fresh destinations:

```sh
git clone --shared --single-branch --branch Experimental /path/to/Project-Judas clean-source
cmake -S clean-source -B clean-source/build -DCMAKE_BUILD_TYPE=Release -DJUDAS_BUILD_CHANNEL=Experimental -DJUDAS_ENABLE_VALIDATION=ON
cmake --build clean-source/build --parallel 2 --target judas_linux_sdk judas_mk2_diagnostics_tests
python3 clean-source/build/linux-sdk/tools/package_sdk.py verify clean-source/build/linux-sdk
python3 clean-source/build/linux-sdk/tools/inspect_sdk.py clean-source/build/linux-sdk --host-check --output sdk-host.json
```

Run editor automation from an unrelated cwd with `JUDAS_ENGINE_ROOT` unset,
`JUDAS_EDITOR_AUTOTEST=<fresh screenshot prefix>`,
`JUDAS_EDITOR_AUTOTEST_NEW_PROJECT=<fresh project directory>`,
`JUDAS_EDITOR_AUTOTEST_POLISH=1`, and
`JUDAS_EDITOR_AUTOTEST_EXPORT=<fresh package directory>`. The hook uses the real
production project-creation and dialog paths, operates Save all, compares actual
package content and dirty state, then performs Play/Stop and exits. The SDK tool
workflow command arrays, native results, identities, provenance, package
file traces and build timings are preserved in
[evidence](evidence/prealpha-linux-readiness/sdk/SDK_MANIFEST.json).

## Consumer game and cross-system coverage

Hearthworld's actual gameplay observer passes **66 checks** for tools/resources,
physical tree felling, mining, gravity-gun work, construction, excavation, collision
and a durable game save. A **separate process** then passes **41 checks**, comparing
saved script inventory, built/physical entities and terrain to the real capture
boundary, and stepping/rendering the restored world. The project and user-save
roots were disposable copies; the existing consumer project was not modified.
The basic-water cold-cycle also uses separate writer/reader processes and verifies
unchanged source-save bytes. Networking tests use genuine host/client processes,
not a phone or Internet acceptance claim.

The 15+4 cross-system cases were run on the diagnostic checkpoint's engine code;
the later correction changes only the model-import CLI and editor automation,
not the physics, fluid, streaming, network or persistence implementation. Those
unchanged mechanisms were not redundantly rerun after the tool-only correction.
The final fresh checkout rebuild and diagnostics independently qualify the
combined engine headers/implementation.

## Remaining findings and Alpha decision

| Finding | Current disposition |
| --- | --- |
| Misleading MK2 fallback/CCD telemetry | Corrected and behaviourally checked; human review pending |
| Fresh Linux SDK/onboarding/exports | Pass on this Ubuntu 26.04.1 x86-64 / GCC 15.2 / GTX 970 host |
| Ubuntu 24.04 binary baseline | Not newly qualified here; this SDK genuinely requires glibc 2.43, GLIBCXX 3.4.32 and CXXABI 1.3.15. The Ubuntu24/glibc2.39 policy correctly rejects it. Build/qualify that baseline in its matching CI/host before advertising these binaries for it. |
| Windows / Android | Explicitly deferred by operator; no builds, VM or device qualification in this pass |
| Human-built first game | Still the operator's intended next acceptance exercise |
| MK2 extreme elastic confinement | **Newly preserved open numerical defect**, not solved or accepted; see below |
| FLU-001 and half-turn disturbed-liquid sampling | Deferred and unchanged; advanced fluids remain default-off experimental |
| Old saves / universal composition / online security | Explicit existing support limits remain; no invented continuation or universal compatibility |
| Dense MK2 performance | Workload-dependent; no new optimization or universal frame-rate claim |
| Remaining broad API/editor coverage and registry debt | Partial qualification, not every-feature certification or a wholesale cleanup |

### Numerical concern needing attention

[MK2-ELASTIC-CONFINEMENT](evidence/prealpha-linux-readiness/elastic-limit/REPRODUCE.md)
is confirmed in a direct native no-force test: a 100 m/s, 100 rad/s elastic box
confined between walls gains energy from 5077.33 to 125043.48 in one 1/60 step.
The native probe retains its 16 controls. A corresponding ordinary runtime scene
has an exactly identical endpoint on the retained accepted-candidate binary and
this candidate, establishing that diagnostic reporting did not introduce that
behaviour. The two entry points are not claimed to initialize identically.

The root numerical mechanism has **not** been established. A safety hold measures
truncated travel; it does not itself certify energy correctness. This deserves a
separate, bounded mechanics triage before promising high-rate elastic correctness.
No numerical retune, hidden clamp, reduced cadence or speculative solver rewrite
was included in this qualification task. It is not silently counted as a pass.

### Readiness recommendation

The requested Linux qualification is complete and supports proceeding with the
operator's game-building trial. It does **not** establish unrestricted Alpha
readiness. The high-rate numerical finding needs an explicit triage/disposition,
and platform/ABI claims must match their receipts. Deferred experimental-fluid
and universal-composition limits need not trigger another broad rewrite.

## Human review / next trial

1. Use the relocated SDK to create your own project; import a model and attach JS.
2. Check/debug/fix your script, Play/Stop, save and reopen, then export and move it.
3. If desired, inspect the physics profiler's new CCD hold/response and restitution
   counters; ordinary scenes can legitimately report zero. The automated cap
   fixtures are the positive proof, not a claim that every stress scene hits a cap.
4. Report any workflow blocker from your game. Decide separately how far to pursue
   the preserved high-rate elastic numerical issue before M100.

Main remains at accepted 0aaef603. Verified checkpoints are Experimental only.
No M100, Alpha publication, tags or build-channel change. Protected history,
independent research, existing games and unrelated local experiments are preserved.
