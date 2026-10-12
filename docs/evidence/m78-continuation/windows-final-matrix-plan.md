# Native Windows final-candidate plan

**Preparation only. No native compilation or physics/application test has been started.** Root must first resolve the current actual BalanceLab step failure, freeze the combined source hashes, review the correction and explicitly clear host/guest heavy-work concurrency. Existing native passing/failing results are historical candidate receipts, not final combined proof.

## Disk and ownership

- Latest read-only/staging receipt: 338,067,456 B free (~322.4 MiB); recheck immediately before building. VM remains 4 vCPU, 6 GiB RAM and 40 GiB disk.
- Untouched retained baseline: `Build/Release/judas_engine.lib`, 66,779,688 B, SHA256 `8f6a6e44b439fdfa8c82a9151015022abb5739c802ac854f86372a57d2399092`. All normal DLLs, SDKs, source and working old human-review package remain.
- 19 old generated executables (113,932,800 B) were copied to host, independently SHA/size verified and then removed only from `Build/Release`. Raw approval/archive/cleanup receipts remain in this continuation.
- Conservative added peak budget: 70 MB candidate archive + 45 MB original-source staging/fixture working copies + 16 MB one live case executable/objects + 32 MB small results + 6 MB original concave profiles + 115 MB one new normal package including VM software GL + 32 MiB untouched reserve = 317,554,432 B. Fits the current receipt only with one case executable at a time; remeasure actual peaks.
- Stage immutable inputs, then archive/verify any new generated ZIP/extracted duplicate or completed executable before root-coordinated removal. Never remove baseline libraries/DLLs, original source, old working review package or receipts. Move/rename ONE fresh package to another directory rather than duplicate it.

## Exact source and compiler preparation

Only replace final `RigidBody.cpp`, `PhysicsWorld.cpp`, `CollisionGeometry.cpp` members in a separate candidate archive. Overlay final `RigidBody.h`, `PhysicsWorld.h`, `CollisionGeometry.h`, `RigidMotion.h`. Root confirmed RB/PW layout unchanged versus native baseline; CG header adds a prototype and RigidMotion adds a method, no member-layout change. If that stops being true, this reuse plan must be revised.

MSVC flags are captured exact native recipes (`/O2 /Ob2 /MD /fp:precise /std:c++17 /utf-8 /bigobj` plus original definitions/includes), not guessed flags. Missing previously cleaned engine objects make a normal MSBuild target with references unsafe as an “incremental” rebuild.

After final freeze, run on host:

```bash
python3 docs/evidence/m78-continuation/probes/prepare_windows_final_matrix.py combined-v1
```

This prepares source ZIP + `m78-continuation-direct-combined-v1.json` only. It packages exact selected test files in isolated `source/tests`, preserving original guest test files; original M77 119-check source remains byte-identical. Cases with extra source units check exact host source hashes before compiling. Navigation/main/export use their own exact captured target recipes but all link the SAME new engine archive.

Do not build `matrix-recipe-preparation-only`: it was only a 31-recipe extraction check, has no final freeze claim and no native executable.

Native tools are already staged and parser-verified under `C:/JudasValidation/M78ContinuationSource/Scripts/v4/`. Use this version, not the older v3 case script, because v4 supports a bounded startup harness environment.

## Original fixtures and current gaps

- Original controller source is present at guest SHA `fabaca3fea9087db521c6a43fcb45dd3731dd621c33e328a15ff247366afd772`; original timestep, impulse and thresholds are untouched.
- Guest lacked the imported rider/project/park and FTFT3 original gravity fixtures. Full immutable bundle is prepared: 248 files, 12,226,766 B ZIP, 16,338,823 B unpacked, SHA `17adc7fbf0a0f9eea6b6e28a4e518559ee1d252e45fea29f6bb7a8205c898074`. Contains current unchanged BalanceLab/CharacterLab/Ragdoll inputs, normal bend_bar fixture, FTFT3 and the exact original 12,045,128 B rider. This does not import giant source art or other consumer games.
- To stage it after root clearance:

```powershell
& 'C:\JudasValidation\M78ContinuationSource\Scripts\v4\windows_stage_immutable_inputs.ps1'
```

The installer verifies every SHA before copying and preserves any differing prior guest input in its own `prior/` directory. This only stages exact existing authored input into the guest; it does not edit the host repository.

- Navigation test needs a NEW owned copy of the original `.cache/windows-vm-transfer/m78-dependent-fixture.zip` (799,680 B, 2,575,450 B unpacked), SHA `571d2eeb6d467055d2d00eca46a7b81026e1a226228c336b9bc1f9d73fbbb946`. Expand to `C:/JudasValidation/M78ContinuationNav-combined-v1/`; do not reuse its previously mutated passing fixture. Retained native authoring/nav-baker library hashes are in the input receipt.

## Core invocation

Use encoded PowerShell through `.cache/m78-followup/ga-nocapture.py` (libvirt escalation required), collect completion with `.cache/windows-vm-transfer/ga.py status <pid>`. Start no second heavy process during a build or performance run.

```powershell
& 'C:\JudasValidation\M78ContinuationSource\Scripts\v4\windows_direct_library.ps1' -ManifestName 'm78-continuation-direct-combined-v1.json'
& 'C:\JudasValidation\M78ContinuationSource\Scripts\v4\windows_direct_case.ps1' -ManifestName 'm78-continuation-direct-combined-v1.json' -CaseId 'controller'
```

Change `CaseId` sequentially to each exact matrix ID below. Library and each case receipt identify source/object/archive/executable hashes, flags, arguments, exit status and free space. Archive and independently SHA/size verify each new executable before deleting its owned build copy after all modes have run. This prevents 31 executables accumulating in the tiny guest disk.

| ID | Actual unchanged/current test source | Purpose |
|---|---|---|
| controller | `tests/PhysicalBalanceControllerTests.cpp` | unchanged M77 119 checks; same inputs/timestep/thresholds |
| motion | `tests/RigidMotionTests.cpp` | combined rigid motion / world-angular-momentum |
| event | `docs/evidence/m78-continuation/probes/event_angular_transport.cpp` | actual PhysicsWorld impact event angular-transport observer |
| foundation | `tests/PhysicalBalanceFoundationTests.cpp` | M77 force/contact foundations |
| physical | `tests/PhysicalAnimationTests.cpp` | M70 physical animation |
| ragdoll | `tests/RagdollTests.cpp` | actual mapped rig/lifetime/orientation |
| joints | `tests/JointTests.cpp` | M45 articulations |
| rigid_contact | `tests/RigidContactTests.cpp` | actual rigid contacts |
| geometry | `tests/CollisionGeometryTests.cpp` | portable geometry regression and exact parity |
| contact_lifecycle | `tests/ContactLifecycleTests.cpp` | first-step/persistent/end contact observation and saves |
| contact_cache | `tests/ContactCacheTests.cpp` | prepared contact cache lifetime |
| impact_solver | `tests/ImpactSolverTests.cpp` | impact solver |
| impact_timing | `tests/ImpactTimingTests.cpp` | all unchanged impact/CCD/ledger fixtures |
| gravity | `tests/GravitySelectionTests.cpp` | generation-safe selected gravity and authoring |
| uniform_gravity | `tests/UniformGravitySceneTests.cpp` | FTFT3 original gravity/serialization fixtures |
| world_save | `tests/WorldPersistenceTests.cpp` | normal save capture/restore/first resumed step |
| kinematic | `tests/KinematicTests.cpp` | moving support and prescribed-body timeline |
| kinematic_lab | `tests/KinematicLabTests.cpp` | real JS timeline/save/stream integration |
| motor | `tests/CharacterMotorTests.cpp` | CharacterMotor regressions |
| lifecycle | `tests/LifecycleIntegrationTests.cpp` | actual mixed lifetime/persistence |
| balance_lab | `tests/BalanceLabTests.cpp` | actual public JS M77 step and lifecycle/cold saves |
| character_lab | `tests/CharacterLabTests.cpp` | M70 actual GL/JS and cold saves |
| spacecraft_control | `tests/SpacecraftControlTests.cpp` | retained world-L spacecraft actuation contract |
| spacecraft_flight | `tests/SpacecraftFlightTests.cpp` | retained world-L flight contract |
| prefab | `tests/PrefabTests.cpp` | retained independent template correction |
| streaming | `tests/WorldStreamingTests.cpp` | retained independent streaming correction |
| scalability | `tests/ScalabilityTests.cpp` | streaming residency/lifetime load |
| nav_dependency | `tests/NavigationDependencyTests.cpp` | retained multi-scene stale-navigation correction |
| concave | `docs/evidence/m78-followup/probes/concave_final.cpp` | unchanged original 900 x four cases, 8192 mesh triangles |
| runtime | `src/main.cpp` | fresh standalone used to package ordinary project |
| export | `src/ExportMain.cpp` | normal exported package with fresh runtime |

`impact_timing` runs `all`, preserving all original thin/grazing/fast/persistent/cap/ledger tests and threshold witnesses. If any fail, preserve/report them; do not silently narrow the command after seeing a failure.

`balance_lab` first runs `step-trace`, the actual public JS impulse-to-step path. Compile once, then reuse the verified same executable with unique receipts for lifecycle/regions/radial/variants/recovery/denied/blocked-recovery/low-grip/fault/cookbook and three cold save pairs (standing, recovery, in-progress step). Exact additional run IDs/arguments live in the matrix manifest. `character_lab` adds its actual cold-write/cold-read pair.

```powershell
& 'C:\JudasValidation\M78ContinuationSource\Scripts\v4\windows_direct_run.ps1' -ManifestName 'm78-continuation-direct-combined-v1.json' -CaseId 'balance_lab' -RunId 'balance-lifecycle' -Arguments @('C:\JudasValidation\M78ContinuationResults\combined-v1\balance-lifecycle','lifecycle')
```

Cold write/read share their explicitly owned output directory; all other outputs are fresh. The runner verifies executable, final archive and compiled source identities again; it does not recompile for each mode.

`concave` is the exact original observer (900 steps at 60 Hz for box-1, box-4, mesh-1, mesh-4; 13 bodies/rig; 8192 mesh triangles; unchanged mesh/contact fidelity/CCD). Collect CSV and profiler JSON files (~5.6 MB), not just aggregate p95. Native VM timing is its own measurement, not a fabricated like-for-like Linux speedup.

## Fresh runtime/export/moved-package proof

Before the `runtime` case, create its owned `M78ContinuationResults/combined-v1/startup.harness` using ordinary current input (no scene mutation):

```text
FRAMES 120
LOG_EVERY 30
FRAME_SECONDS 0.016666667
```

The runtime case manifest points `JUDAS_TEST_SCRIPT` there and passes the normal registered BalanceLab project. Retain the new runtime executable for the export case. `export` invokes normal `ExportMain` with project path, unique package destination, and the freshly compiled runtime. Verify adjacent `Build/Release/windows-runtime` manifest + `windows-licenses` exist before export; old exported packages are not final evidence.

For VM-only GL, copy the unchanged retained `opengl32.dll` (139,264 B) and `libgallium_wgl.dll` (61,868,032 B) next to the new package runtime, record their SHA/source, and distinguish this harness adapter from the ordinary packaged renderer requirement. Normal package without these is ~52.3 MB; VM package ~114.3 MB.

Move the one newly exported package to another owned directory. Launch its `judas.exe` from unrelated `C:/Windows/Temp` with no arguments, `JUDAS_ENGINE_ROOT` pointing to an unavailable directory and the same explicit owned startup harness. Capture exit/log/screenshot or normal callback status and a package manifest tying `judas.exe` back to the freshly linked runtime SHA. Preserve exported authored scene/asset hashes. Do not overwrite or claim the old human-review package as fresh.

## Outstanding and reporting boundaries

- Actual BalanceLab step is still blocking source freeze according to root; unit 119 pass alone is insufficient. No final native tests are claimed by this preparation.
- Windows software-GL VM is available for native process tests. Physical GPU/controller/audio and human Windows interactive acceptance remain unverified.
- FLU-001 remains FAILED under its original 0.5 m/s at 4 s contract; no fluid code, research or threshold is changed by this native plan.
- Every old Linux/native failure and prior passing receipt stays intact. A fresh combined receipt must name the exact final archive and tests used.
