# M77 uncommitted candidate results

Base HEAD: `ab3dfbab13585680a9e6cc92fa318c8d95012979`.
Human acceptance is pending. Earlier M76/M74/M67 deferred reviews are not
silently promoted. No M78 work, commit, push or tag.

## Architecture and demonstrated envelope

| Capability | Actual evidence / limit |
|---|---|
| Default-off authoring | Scene/prefab/editor configuration; configuration alone does not activate; old fingerprints/save representation preserved. |
| Physics ownership | Fully dynamic free root and feet; normal PhysicsWorld/M45 contacts/joints; paired bounded angular drives only. No root wrench, kinematic foot or solver/cadence workaround. |
| Gravity | Per-mapped-body authoritative sampling, radial and rotated uniform fixtures; owner override is deliberately not inherited by physical bones. Incoherent/zero sampled gravity refuses standing. |
| Standing | Two real accepted foot contacts, bounded upright/height/speed and dwell; moving/rotating, 2 cm uneven, 3° slope and oblique support demonstrated. Not arbitrary terrain/gait certification. |
| Corrective step | Slender rig: passive settle 120 steps, stand 360, actual 29 N s impulse, observe 720. Allowed step lifts then contacts and stands; denied same trial falls. Nearby 28/30 N s and direct animated entry failures retained. This is a narrow demonstrated envelope. |
| Recovery | Two distinct explicitly weighted-foot rigs, face-up/down and nearby rotations; actual physical return to standing. Effort-off remains fallen; low roof fails at deadline. Slender rig is not universal recovery. |
| Actual support | Prior solved impulse/contact observations, contact-face patches, bounded overflow rejection. Ray/box plans never substitute for touchdown. Frictionless/filtered/missing support fail honestly. |
| Pose/ownership | Actual physics contributes through M47; interpolation/skin/socket paths reused. M70/shared IK actuator edits refused while owned; ordinary masked visual layer coexists. |
| Motor return | Separate explicit collision-checked M70 return; blocked placement retains physics, actual velocity retained, existing partial-mode compatibility tested. No automatic stand-to-motor snap. |
| Save/lifetime | Fresh-process step/stand/recovery restore; semantic histories retained, contacts/caches reacquired; missing/newer/truncated records rejected on detached candidate. Normal Application SaveService tested after whitelist fix. |
| Regions | Actual external support/plan pins region; complete articulation adoption retains native task; individual bone refuses; unload/revisit cannot alias old handles. |
| Failure/lifecycle | Cancel releases drives only; invalid/destroyed bodies retire; native intent deliberately survives initiating game-slot callback fault; scene/reload/Stop cleanup verified. |
| Bounds | 128 owners, 32 mapped bodies, two disjoint three-body chains, 8 patches/foot, 32 contact samples/64 candidates per foot; finite attempts/load transfer/landing limits. Not a general full-body planner. |

## Linux validation

- Clean affected Release build passed. Ten emitted warnings originate from three
  inherited sites (two RuntimeWorld indentation sites and a ModContentTests
  temporary-lifetime warning); no zero-warning claim.
- Final foundation 34 checks; controller 119 (final load-tie follow-up); M70 physical-animation 64.
- Affected CharacterMotor, M45/authoring, ragdoll, pose, fingerprint, persistence,
  streaming and M76 regressions pass (individual logs in `final-linux/`).
- Actual GL/AssetDatabase/QuickJS lab: standing 15, denied 15, supports 14,
  recovery 16, radial 18, lifetime 26, regions 24, deliberate fault 15,
  cookbook 15. Corrected content follow-up: supports 14, variants 17,
  blocked recovery 15, recovery 16. Original failed variants are retained.
- Fresh-process step/stand/recovery write and read pass. Save follow-up includes
  world persistence 27, storage 28, foundation 34 and legacy save lab write 13 /
  read 20. Actual Application saves/loads, pauses, reloads and loads four scenes.
- Actual editor Play/Stop restores identical authored content and saves. The
  automated environment reports no relative mouse mode; not operator input proof.
- API: 30 exports / 345 public symbols / 239 native operations / 16 callbacks /
  39 cookbook examples; live enumeration, TypeScript 5.9.3 and three drift
  negative controls pass. The physical-balance example executes in the lab.
- Export and moved space/Unicode package pass from unrelated cwd with repository
  hidden by a process-only namespace. Initial sandbox refusal and corrected run
  are retained. No host/global configuration was altered.
- Narrow profiler constructor follow-up: 27 profiler, 34 foundation, 118 controller
  checks pass. The portable report-path follow-up also passes; final native Windows result is
  recorded below.

No unrelated full production/research suite was rerun. Validation stays focused
on changed joint/contact/world/save/region/API paths and actual applications.

## Preserved failures and corrections

`../DEVELOPMENT.md` and `../development/` retain failed root-torque posture,
light-foot recovery, wrong drive rate units, unsuccessful universal foot
reference policies, failed strict global angular conservation, and narrow
step/recovery limits. Final failed content variants, actual SaveService omission,
Windows Update interruption and MSVC compiler failures are also retained.

The SaveService fix admits the already-implemented seventeenth participant and
rejects an eighteenth. It changes no save version/identity. Compiler probes
isolate MSVC C1060 to deeply nested aggregate initialization of profiler lanes;
a user-provided Lane constructor keeps existing values and capacity and permits
standard Release compilation. No optimization flags are accepted as the fix.

## Reproduction

From repository root, configure ordinary Release using existing build instructions.
Build `judas`, `judas_editor`, `judas_export`,
`judas_physical_balance_foundation_tests`,
`judas_physical_balance_controller_tests`, `judas_balance_lab_tests` and affected
existing regression targets. Native executables take no arguments. Lab executable:

```sh
build/judas_balance_lab_tests /tmp/m77-standing standing
build/judas_balance_lab_tests /tmp/m77-denied denied
build/judas_balance_lab_tests /tmp/m77-recovery recovery
build/judas_balance_lab_tests /tmp/m77-radial radial
build/judas_balance_lab_tests /tmp/m77-save cold-write
build/judas_balance_lab_tests /tmp/m77-save cold-read
build/judas projects/balance_lab/balance_lab.judasproj
```

Other lab modes: supports, variants, blocked-recovery, low-grip, lifecycle,
regions, fault, cookbook; cold-stand-write/read and cold-recovery-write/read.
Use fresh output directories and preserve original failure results.
Normal generator `scripts/create_m77_lab.py` uses existing authoring/import tools;
it does not create a hidden runtime mode. Do not overwrite authored content
merely to rerun validation. Windows proof uses native x64 executables, normal
project paths, least-privilege interactive Conner, and app-local disposable Mesa
copies only. Hardware/input acceptance remains separate.

## Final Windows follow-up

Native x64 stock Release build passes after the narrow profiler constructor fix;
no compiler optimization override is retained. All 27 final cases pass:
profiler 27, balance foundation 34, controller 119, existing M70/motor, all twelve
public-API lab modes, six fresh-process save phases, normal Application services,
editor Play/Stop, export and moved-package launch. Original lab step and cold-step
failures remain beside the corrected follow-up. Measured initial pre-plan load
noise (.0107% of total impulse on Windows) selected the opposite symmetric foot;
the generic .1% relative tie band now uses stable authored order and the actual
public-JS comparison passes on both platforms.

The VM remains 4 vCPU / 6 GB / 60 GB. Tests run in interactive session 1 as
least-privilege Conner (`admin:false`). Final source verification matches all
853 inputs (852 original inputs plus the existing engine LICENSE required by the
export staging fixture), and confirms the temporarily hidden owned source copy
was restored. No source, tools, SDK, libraries, user data or accepted evidence were
removed. After validation, only 267 disposable `.obj` files under the owned
`C:\JudasValidation\Build` were reclaimed (115,829,677 bytes); final binaries and
packages remain. No VM configuration/disk/mount/network/privilege changes.

Mesa/llvmpipe exists only in disposable runtime/editor test copies and the moved
VM-only package copy. The pristine exported package inventory proves it contains
no Mesa DLLs. Hardware OpenGL, physical input feel and operator acceptance remain
outstanding. This is native Windows execution proof, not hardware certification.
Final executable and pristine package hashes, build logs, per-case commands,
editor captures, source verification and cleanup receipts are in `windows/`.

Final Linux corrective-step follow-up reruns profiler/foundation/controller,
public JS step/denied/lifetime, all three cold-save families and seven measured
scenarios. All pass. The current final metrics are in `PERFORMANCE.md` and
`step-choice-followup/`; prior measurements remain historical evidence.
