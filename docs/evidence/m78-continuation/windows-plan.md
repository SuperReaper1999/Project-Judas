# Native Windows continuation plan

Prepared from actual source, previous native receipts and cleanup manifests on 2026-10-11. This is a plan, not a validation result. The continuation's final source/binary hashes must supersede the earlier candidate hashes without overwriting its genuine failures.

## Existing evidence and gaps

- Native `physical_balance_controller` previously reproduced the unchanged Linux failure: 119 checks, two failures. Preserve `m78-followup/native-final/native-physical_balance_controller.stdout.log` and the original M77 passing results.
- Angular-motion, joints, physical-animation, balance-foundation, CharacterMotor, prefab, streaming and scalability checks passed natively on the earlier combined candidate. Initial collision-fixture path and missing streaming inputs failed honestly; corrected portable paths/unchanged input bytes then passed. They must be rerun after the final physics correction, with fresh binary hashes.
- The late navigation transaction negative proof passed natively: exit 0; source SHA `38aecede76bbd6c2bebed3303bf941b872f116046025ca327288a0aff3489a0b`; executable SHA `b7206a45ee307394a2d9db00f66cbdc458dd9a7178c9321a02fbeca01f88bc00`. Its stdout records 38 checks. This is retained proof of the service, not the new physics candidate.
- Previous `judas_balance_lab_tests.exe` SHA `61e5eafe728d892561e82b82aaa527ed7c2351fbc9a4e299f9c7a48d09312be3` matches the old M77 executable. Do not call an invocation of that executable a current-physics application proof. Explicitly relink it against the final candidate archive.
- No fresh native four-original-rig/8192-triangle performance proof, editor/package smoke or moved-package application proof was produced in the preceding batch. Linux results are not Windows results. The guest uses software OpenGL; native numerical/application checks do not constitute Windows hardware acceptance.

## Disk and build safety

The prior final read-only disk receipt reported 69,431,296 bytes free. The guest's configured disk is 40 GiB, four vCPUs and 6 GiB RAM; no resource/security/configuration change is planned here. A fresh space query is required before each phase.

The preceding cleanup removed 240 completed compiler objects (92,079,037 bytes), including 138 engine objects (63,388,947 bytes), while preserving sources, libraries, executable files, recipes, result directories and the working review package. Therefore a normal MSBuild engine target can regenerate most of the engine even when only a few source files changed. Do not assume a three-file incremental CMake build is safe.

Bounded alternative coordinated by the root agent:

1. Inventory exact current generated `C:\JudasValidation\Build\Release\*.exe`, source/library hashes, free space and running owned processes. Archive each old generated executable directly to the host, verify each SHA and size independently, then remove only the approved matching generated executable. Do not delete DLLs, libraries, source, installed SDK/tools, existing review packages, saves, trust stores or original evidence. Prior 17-executable inventory totalled 109,745,664 bytes; newer spacecraft targets require fresh inventory rather than assumed hashes.
2. Compile only the actually changed translation units using exact existing MSVC flags. Obtain command lines from existing `.vcxproj`/`CL.command*.tlog`; preserve the command receipts. Earlier `RigidBody.obj`, `PhysicsWorld.obj` and `CollisionGeometry.obj` together occupied 1,611,117 bytes. The existing archive is 66,772,256 bytes, so budget a complete second archive, object/compiler temporary footprint, a single output executable, inputs/results and at least 32 MiB untouched reserve before starting. Stop if the projected peak exceeds current free space.
3. Copy the prior engine archive; inspect `lib /list` for exact member identities, remove the old members and add the freshly compiled replacements. Verify the archive contains exactly one member for each replaced translation unit. Preserve old archive SHA/source provenance, final object/archive SHA and link commands. This is an isolated build, not a source-semantic workaround.
4. `RigidMotion.h` is directly included in production by `PhysicsWorld.cpp`; the focused `RigidMotionTests.cpp` also includes it. Any change to `RigidBody.h` layout or inline implementation requires its other dependents to be rebuilt; external-helper-only implementation changes do not justify inventing an ABI change. Do not blindly reuse objects if the final correction changes a struct layout, signature or inline code.
5. Compile/relink one focused target at a time against the final archive, archive its output/receipts, run it, then reclaim only verified owned generated output if required. Do not use `/p:BuildProjectReferences=false` against an old library and label that the combined candidate.

The prior automatic vcpkg manifest install tried to download another Git and filled the disk. Use the existing installed dependencies and `-DVCPKG_MANIFEST_INSTALL=OFF` for any necessary configure; no dependency installation or broad full-build invocation is planned.

### Current continuation headroom receipt

The fresh guest inventory found 19 generated executables / 113,932,800 bytes and 214,675,456 bytes free before archival. All 19 complete binary copies were uploaded to the host and verified independently by SHA256 and exact size. The guest then rechecked every row and verified none was running before removing **only** those exact generated `.exe` files. Free space after removal: **328,589,312 bytes** (313.37 MiB). Libraries, DLLs, sources, SDK/tools, result directories, working review package, saves and trust stores were preserved.

Receipts: `m78-continuation-old-exes.json`, `windows-old-exe-host-verification.json`, `m78-continuation-verified-exe-cleanup.json`. Archived binaries remain under `.cache/windows-vm-transfer/reports/m78-continuation-old-*.exe` on the host. The first archive attempt failed because the owned transfer server was stopped; its failure is retained and the resumed transfer succeeded. No compilation was started by this preparation pass. Requery free space before root-coordinated compilation because Windows may allocate storage between phases.

## Affected native validation matrix

Run from `C:\JudasValidation\Source`, with fresh owned output roots and final-source/library/executable hashes in every receipt. Preserve the unchanged 60 Hz step fixture, 29 N s input, body/shape data, actuator limits and thresholds. Do not retune M77 to make the physics correction pass.

| Scope | Current target / arguments | Why |
|---|---|---|
| Angular transport / drift | `judas_rigid_motion_tests` | Original four cadences, independent world angular momentum, rotation/impulse/torque checks. |
| M77 controller | `judas_physical_balance_controller_tests` | The unchanged blocking corrective-step case plus support/recovery controls. |
| M77 mechanics | `judas_physical_balance_foundation_tests` | Contact/actuation/support foundations affected by shared body motion. |
| M70 | `judas_physical_animation_tests`; `judas_character_lab_tests <fresh-output>` | Shared angular motion, passive/partial physics, owner contacts and actual public-JS application. |
| M77 actual application | `judas_balance_lab_tests <fresh-output> step-trace`, `variants`, `recovery`, `lifecycle`, `regions`, `radial`, `denied`, `blocked-recovery` as warranted | Real source project, assets, script lifecycle, physical step, moving/arbitrary gravity and explicit negative controls. Each mode needs its own fresh output directory. |
| Cold physical save | `judas_balance_lab_tests <one-new-output> cold-stand-write` then `cold-stand-read`; separately `cold-recovery-write` then `cold-recovery-read` | Fresh process snapshot and first resumed physics steps; read reuses only its corresponding new write directory. |
| M71 prescribed motion | `judas_kinematic_tests`; `judas_kinematic_lab_tests` | Corrected dynamic angular drift must not change prescribed target trajectories or save state. |
| Contact geometry | `judas_collision_geometry_tests`; `judas_rigid_contact_tests`; `judas_contact_lifecycle_tests`; `judas_contact_cache_tests` | Exact clipped-face witnesses, normal contact response, cache/lifetime and source handle changes. |
| CCD / impacts | `judas_impact_timing_tests thin`, `grazing`, `fast`, `persistent`, `cap`; `judas_impact_solver_tests` | Thin barriers, grazing, first impact timing, persistent contact and bounded event policy. Preserve existing requirements; no contact/CCD disabling. |
| Constraint and articulated lifecycle | `judas_joint_tests`; `judas_ragdoll_tests`; `judas_touch_event_tests --output <fresh-output>` | Real hinge/constraints, mapping/lifecycle and collision attribution remain correct. |
| Gravity | `judas_gravity_selection_tests <fresh-output>`; `judas_uniform_gravity_scene_tests --fixtures docs/evidence/ftft3/fixtures --output <fresh-output>` | Authored uniform/radial/arbitrary selection and rigid consumer consistency. Existing historical fixture source is read-only. |
| Saved physics / lifecycle | `judas_world_persistence_tests`; `judas_lifecycle_integration_tests --output <fresh-output>` | Modern participant state and normal reload/entity/body lifecycle. |
| Angular consumer | `judas_spacecraft_control_tests`; `judas_spacecraft_flight_tests` | Correct world angular momentum contract, unchanged control semantics. |
| Retained corrections | `judas_prefab_tests`; `judas_streaming_tests`; `judas_scalability_tests <fresh-output>`; `judas_navigation_dependency_tests <fresh-owned-project> Imports/landscape.judasterrain` | Retain the already-passing independent fixes on the combined final candidate; do not mutate preceding fixtures. |

Run relevant exact performance observer separately while the host/guest are not compiling. The native VM's performance is a distinct environment; do not manufacture a Windows-vs-Linux equivalence claim. The unchanged original four-rig/900-step/60 Hz mesh observer must report median, p95, maximum, contacts, CCD, sleeping and profiler stage costs.

## Input / application / package requirements

- Original mesh-rig performance observer needs immutable `projects/post_m65_consumers/post_m65_consumers.judasproj`, `Scenes/skate/park.judas`, `Assets/skate/models/rider.glb` (12,045,128 bytes) and `.judasmeta` (`97c3acdb99c5ffefb002c03ecec7a8ed`). Floor geometry is generated by the exact observer: 64x64 cells, 8192 triangles. The 142 MiB model authoring source is not required by this runtime workload.
- Full `projects/balance_lab` is 46 files / 1,450,890 bytes; `projects/character_lab` is 44 files / 1,753,926 bytes; full ragdoll demo is 16 files / 798,606 bytes. Verify all staged bytes, not merely the scene. Earlier Windows missing-input failures were fixed by staging original ragdoll scene and animation mesh; do not assume every new application's inputs already exist.
- `UniformGravitySceneTests` needs `docs/evidence/ftft3/fixtures` (137 files / 228,437 bytes); copy exact fixtures to owned guest source if absent. Do not write into protected host fixture directories.
- Actual GL applications need the retained OpenGL software DLLs, font assets, current project resources and an owned TEMP directory. Use the same genuine EngineHost/resource path; a failed context is an environment gap, not a fake pass.
- For a fresh moved-package smoke, export using the final `judas_export.exe`/`judas.exe`, verify `game.judasproj`, resource closure and packaged DLLs, move it to an unrelated directory, then launch there. Keep old working human-review package intact. If disk cannot fit another full DLL copy, use an explicitly owned bounded one-at-a-time output/archive plan rather than silently substituting the old package.

## Evidence rules

No old receipt is overwritten. Capture stdout/stderr, exit code, source/library/executable SHA, input hashes, cadence and exact arguments. Preserve all earlier Linux/Windows failures and failed transfer attempts. The FLU-001 failed check and deferred contract proposal remain untouched. Final candidate status must distinguish numerical passes, native application/package coverage and outstanding hardware/human acceptance.
