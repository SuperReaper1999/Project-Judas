# Deformables V2 — final engineering qualification

**Model-fidelity follow-up:** [current causes, corrections and visual limits](DEFORMABLES_V2_MODEL_FIDELITY.md) supersedes the earlier intact-model material comparison. Full human acceptance remains pending.

This is an Experimental source-preview handover, not a release or human acceptance
of Stages 2–4. Revised Stage 1 was human accepted at `6a1d49b2`. Main, the Advanced
Fluids lock and the RPG project/SDK remain unchanged. All downloaded vehicles and
derivatives remain private and are absent from the repository and public SDK.

## Authoritative implementation

The implementation and native integration checks are checkpointed at
`16a71ec951500362a3189aae5427e0a6fc3fd20c`; the permanent CI registration and
fixture-path correction are `2ce87594692b6457205c472fe11519d90e5ad050`. The local
SDK is `build/linux-sdk`; its exact source/build identity is recorded in
`BUILD_INFO.json` and the final handover receipt.
Its dirty provenance explicitly includes the two pre-existing operator Terrain V2
files; those were preserved. This host build requires glibc 2.43 and is not an
Ubuntu 24.04 distribution certificate. CI separately checks the Ubuntu baseline.
Subsequent documentation-only reconciliation does not change the tested engine.

The earlier [implementation record](DEFORMABLES_V2_IMPLEMENTATION.md) is chronological.
Its early pending-work and rejected-prototype entries describe those checkpoints,
not current outstanding implementation. The current authoring contract is in
[Authoring Deformables V2](DEFORMABLES_V2_AUTHORING.md).

- MK2 owns the actual finite-mass structural sections, connections, wheel mounts
  and drivetrain. There is no cosmetic second chassis or hidden undeformed body.
- Independent coarse local sheets supply signed bending, plastic rest/work and
  finite attachment failure. Detailed render geometry follows physical state.
  Full release retains the same material's mass, momentum, shape and collision.
- Native fixed-interval coupling consumes material forces once and applies actual
  rigid reactions before pose integration. Local quality no longer subdivides
  unrelated rigid simulation. Prescribed bone targets retain the final animation
  phase, explicitly distinct from finite-mass rigid reactions.
- Instance-local contact signatures wake on nearby changed geometry. Quiet dynamic
  mounts can sleep. Plastic structural joints no longer demand their original
  undeformed weld pose to sleep. Ordinary motion/impulse wake rules remain.
- Terrain V2 solid snapshots recover materials buried by actual terrain edits;
  cooked contact and origin rebasing remain authoritative. Capsules use the
  supported static capsule geometry, not invented dynamic capsule support.
- Shared prepared structural skins, immutable detail and retained GPU topology
  avoid rereading model data or recreating materials on each fracture update.
- Ordinary scenes, prefabs, streaming, SaveService and SDK exports own the state.
  No private crash-tool participant or manual post-step panel solver is required.

## Native and SDK evidence

The local focused material suite passes **152/152**, including actual independent
multi-panel prefab instantiation and real streamed eviction/reloading. It verifies
node positions, velocities, plastic hinge rest/work, release, fresh carrier-ID
remapping and independent continuation. Original procedural fixtures supply all
committed coverage; none contains downloaded model data.

All **12 scoped regression executables** pass at 16a71ec9: materials 152, structural
crash/skin 174, bending 39, plastic-section sleep 29, material controls 40,
mechanical connections 37, mechanical scripting 20, shared scripting modules 8,
Inspector workflow 74, vehicle dynamics 97, wheel contact 11 and vehicle authoring
16 checks. The final full engine/SDK run, including the four new suites, is
[CI 42865178610](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42865178610)
at 2ce87594 passed both channels and the Ubuntu SDK checks. The subsequent
presentation-tool correction and final documentation checks are identified
separately in the final handover receipt. Renderer
submission also passes **28/28** on both actual NVIDIA OpenGL 4.6 and forced
portable OpenGL 3.3. The real native deforming Challenger additionally completes
240 fixed steps on GL 3.3 with permanent bending and all bonnet mounts released.
Portable desktop GL is not Android/browser certification.

The 2ce87594 SDK passes its manifest and all **15 current tools**, normal project
creation, editor Play/Stop, import/cook/authoring and relocated export workflows.
The SDK was read-only and the development checkout hidden by an external mount
namespace. The qualifier's `packageOnlyNamespaces` field is false because the
external wrapper supplies that isolation; the command and logs record it.

A real private Challenger regression project exported 14 assets and one scene
(110,336,971 bytes) with the matching 2ce87594 SDK. Project-owned JudasJS applied
one coherent physical launch impulse; actual barrier collision produced damage,
without setting damage or forcing release. The moved executable then saved and
cold-loaded in separate processes with the SDK and source project hidden (including the original private model folder), with
network access disabled. Its
maximum permanent bonnet bend was exactly **1.3818208191196202 radians** before
and after process exit, with all three physically failed mounts still released.
Writable user data was retained explicitly between processes. The current native
600-step 30 m/s proof independently cold-verified the exact durable damaged shape:
`7ed692ea5d3e7620fb2cc67903e1be7b517e40ef81cdf4425c0d28bbf37167a0`.
The bonnet had all three mounting patches released and retained permanent bending.

Four dedicated V2 material/structure/bending/sleep suites are now registered in the
ongoing CI manifest. Registering them also exposed a test-only working-directory
assumption; the structural fixture now uses the supplied engine root instead of
requiring the caller's current directory to be the checkout. Its assertions and
original fixture are unchanged.

A normal asynchronous desktop editor launch, eight seconds of preparation, ten
seconds of Play, Stop and clean window close showed the actual 24-section car
(26 total physics bodies), rather than accepting an early loading screenshot as
proof. A separate editor automation pass verified duplicate/undo, identical authored
scene after Play/Stop and saving. No human review is inferred from these runs.

Matching Windows MinGW compilation, PE transitive dependencies and build provenance
pass at 2ce87594. Native Windows/MSVC execution was **not performed**. The inspected
VM remained shut down: its 6 GiB allocation was unsuitable with roughly 5 GiB
available on this 8 GiB host. Android/browser execution was not performed. No
hardware/controller/audio or clean-machine certification is claimed.

## Physics and rendering cost

900 fixed steps per CPU row; same native Challenger settings and detailed source.
These headless rows exclude rendering, with no local compiler or GPU qualification
job running during measurement. Peak whole-process RSS was 642,176–642,804 KiB,
including detailed model decoding, preparation and evidence buffers; this is not
per-panel solver memory. No physical fidelity or constraints were reduced to obtain
these results.

| Workload | p50 ms | p95 ms | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| One idle car | 1.339 | 1.350 | 1.376 | 1.523 |
| Four idle cars | 9.570 | 9.677 | 10.305 | 11.021 |
| One driving over course | 1.462 | 1.522 | 1.544 | 1.755 |
| Frontal, 30 m/s | 1.060 | 2.403 | 3.641 | 195.526 |
| Offset pole, 20 m/s | 1.485 | 1.723 | 3.318 | 36.388 |
| Side, 15 m/s | 1.302 | 1.714 | 1.954 | 4.128 |
| Rear, 15 m/s | 1.437 | 1.545 | 1.951 | 3.774 |
| Four-car collision, 20 m/s | 12.205 | 15.165 | 21.431 | 325.473 |
| Repeated frontal impacts | 1.620 | 5.722 | 46.888 | 196.840 |

The matched same-root contact-preparation A/B retained an identical durable physical
hash: p50 6.576 → 3.824 ms, p95 8.173 → 4.197, p99 12.086 → 5.420. Its worst
198.582 → 194.921 ms remains effectively a severe-contact spike. Later native clock
changes deliberately change integration, so their absolute timings above are not
claimed as matched-trajectory speedups against Stage 1.

GTX 970 / NVIDIA 580.178.04, modern GL 4.6, 300 fixed-step 20 m/s desktop runs using the corrected source-material/glass
setup and authored lighting:

| Rendering | One car p50/p95/p99/worst ms | Four cars p50/p95/p99/worst ms |
|---|---|---|
| CPU submission | 0.890 / 0.941 / 1.104 / 6.621 | 2.635 / 2.701 / 2.810 / 7.672 |
| GPU query | 1.064 / 1.118 / 1.188 / 7.006 | 1.886 / 1.951 / 2.563 / 8.378 |

Cold first-resource preparation is reported separately: **126.49 ms** one car,
**421.75 ms** four cars, including initial native mesh/material submission and an
explicit completion wait. It is not silently removed from startup cost. Physics
running beside rendering differs in host contention from the headless rows.

## Real-model scope and private launchers

Private handover root:
`/home/conner/Projects/Judas_Deformables_V2_Crash_Proof/Stage234-Acceptance/`.
The accepted Stage 1 binaries and launchers remain separately intact.

- **Challenger:** primary drivable native structural/panel proof, correct wheel
  assembly, permanent bonnet bends, progressive physical mounts and debris.
- **Shelby:** actual geometry, normalized wheel/caliper fitting and a native
  structural driving/crash project. Its private texture derivative fits the
  current decoder bound; the original is unchanged. Not individually calibrated
  or a second complete bonnet implementation.
- **Blazer:** actual geometry, wheels and native structural driving/crash project;
  a private rest-pose/yaw preparation corrects its opposite source-forward axis
  before wheel fitting. Working Vehicles V2 physics is unchanged. The same
  calibration and full-panel scope limitation applies.
- **Ural:** static editable four-panel authoring gallery, not a six-wheel crash rig.
  Hood/roof/left/right door are independent physical sheets. Paired source doors
  were split privately: 392 triangles per side, zero crossing triangles, all
  678,111 original vertices and 226,037 triangles retained in the complete model.
  Rest mapping error is zero. Gallery boundaries are prescribed world mounts;
  explicit release demonstrates ownership, not finite hardware load failure.

`Play-Challenger.sh` opens the ordinary game; `Edit-Challenger.sh` opens its normal
editor project. W/S drive/reverse, A/D steer, Space brake, K/L save/load, R reset,
Esc pause. `Controlled-Crash.sh frontal|offset|side|rear|cars` supplies controlled
impacts: Enter launch, P pause, Q/E orbit, C bonnet follow, F6 save. The other model
launchers are named consistently. Ural: 1–4 select, B impulse, X explicit release,
K/L save/load; close camera views follow the selected panel.

## Material presentation correction

The crash preparation tool previously repainted named paintwork blue and forced
wheel opacity. Those overrides are removed; imported factors, maps, metallic,
roughness and opacity are retained. Its diagnostic lighting now uses the authored
surface-to-light direction consistently with the normal project renderer.

The three legacy FBX cars also needed explicit private material authoring: their
windows imported with zero opacity, while several opaque body/rim materials were
marked Blend even with unit opacity and fully opaque texture alpha. Private
recipes retain RGB/UV data, author the truly opaque surfaces Opaque, and provide
standard translucent PBR glass (RGB .12/.17/.22, alpha .26, roughness .12,
metallic 0, double-sided). Genuine transparent decals and lights retain their
source settings. This is an authored approximation of unsupported legacy shader
graphs, not automatic recreation of another game's materials. The original files
are unchanged; recipes, derivatives and pre-correction projects remain private.

A same-root 600-step 20 m/s A/B, with a separate cold read for each variant,
produced the identical complete durable physical hash before/after the tool
correction: `11855fd35dc402aadad7414efff4c86e4d42bbb208f52034ff97e1b75b9cc54d`.
The render timings above were remeasured after correcting materials/lighting;
the earlier visual/timing evidence is retained with its earlier scope.

## Remaining limitations and review

- **Known performance limitation:** severe structural MK2 collision/CCD spikes remain,
  especially interacting cars and repeated impacts. This is not a steady 60 Hz
  guarantee for four heavily colliding cars. No damping or artificial sleep masks it.
- The real multibody car can retain small structural oscillations while the bonnet
  sleeps; a completely quiet fleet is not asserted from an idle input command.
- Physical panels approximate sensible single-surface regions. Split closed,
  overhanging or paired geometry; arbitrary automatic semantic panel discovery,
  per-render-polygon collision and arbitrary internal sheet cutting are not supplied.
- Normal attachment authoring uses a common source-local frame and explicit mount
  offsets. Arbitrary independent relative attachment rotations are not inferred.
- Soft contact is sampled, not general mesh CCD. Dynamic capsules, arbitrary
  soft-sheet character blocking, soft-material buoyancy and per-polygon particle
  deposition are not implied. Structural MK2 proxies retain current integrations;
  the Advanced Fluids lock is unchanged.
- Terrain edits can legitimately impart contact impulses when solid is sculpted
  through material. Coarse proxies and presets require game-specific calibration.
- Retired incomplete material saves are intentionally rejected. No V1 migration
  or compatibility layer is introduced.

Human review: drive/reverse/brake/steer; inspect intact wheels and panels; perform
frontal, offset, side, rear and repeated crashes; inspect partial/full separation;
save, close, reopen and load damage; author a fresh panel/frame with live diagnostics;
inspect both split Ural doors. Full milestone acceptance remains the operator's.
