# Final bounded Pre-Alpha Linux qualification

Engine starting point: human-accepted `c76e92dc20b5765f61d6077bf618a8b94ac22462`.
This Experimental follow-up contains one demonstrated runtime correction and one
corrected physical test oracle. Main is unchanged. M100, publication, versions,
channels, Windows/Android qualification and the operator's game-building trial
are outside this task.

[Support matrix](ALPHA_SUPPORT_MATRIX.md) · [Current findings](PREALPHA_FINDINGS.md)
· [Evidence](evidence/prealpha-final-qualification/README.md)

## Exact-checkpoint build and SDK

A new local checkout of the exact accepted revision supplied only tracked source.
An attempted hard-link clone failed across filesystem boundaries; a non-hard-link
local clone completed. No previous engine object files or untracked experiments
entered this build. The prepared Ubuntu 24.04 root/compiler was reused read-only;
engine/dependency outputs were rebuilt in a new directory.

The rootless bubblewrap backend supplies Ubuntu 24.04.3, GCC 13.3, glibc 2.39,
SDL2 and Mesa llvmpipe. The SDK plus initial selected test targets built in
**610.21 seconds**; configuration/qualification made the whole wrapper **653.47
seconds**. These are build durations, not gameplay performance benchmarks.
There were zero compiler warnings. The clean SDK identity is
`0.1.0-prealpha-Experimental-c76e92d-clean-52df55297d0f`, with zero untracked build
inputs. All 17 tools stage, report the same identity and pass the actual older
loader. Highest required ABI versions: **GLIBC 2.38**, **GLIBCXX 3.4.32**,
**CXXABI 1.3.13**. This qualifies the Ubuntu 24.04 reference userland, not Ubuntu
22.04, every Linux distribution or physical GPU/audio drivers.

SDK checks use a relocated read-only SDK and separate writable work root, with
neither the original source nor build tree mounted. The packaged editor creates a
project using its own bundled content/docs, edits/undoes, protects dirty content,
saves and exports. Separate Play/Stop executes the real JS and proves authored
scene restoration. CLI workflows include valid and invalid scripts, model
import/cooking, UI, saves, terrain/volume editing and cooking, collision cooking,
environment/navigation/liquid baking, mod kit/project creation and dedicated
startup. Each of the **17 public executables** has an actual workflow, not only an
identity probe; these are representative operations, not all subcommands.

Two exports (editor and CLI) run 90 fixed steps, execute their JS and capture a
rendered frame after relocation into **separate package-only namespaces**. Neither
source checkout, build tree nor SDK is mounted there; `/source` is only the moved
game package. System libraries/drivers come from the older root. This is stronger
than changing cwd on the development host, but not a new physical-machine test.

The accepted-source SDK is retained separately because qualification found the
wheel regression below. The corrected candidate was incrementally rebuilt and
requalified with its genuine dirty-candidate identity, then checkpointed as
`f7691198ee299a5752c8a55c93fb42e639d033cf`. A subsequent clean committed build is
`0.1.0-prealpha-Experimental-f769119-clean-252bc26f2546`, with zero untracked build
inputs. All 17 ABI/loader/identity checks, packaged editor/CLI workflows and both
package-only moved exports pass again. The clean rebuild has the same GLIBC 2.38
maximum and no warnings. Its 48.75 s configure +37.27 s incremental build are not
another claimed full clean build.

Five final clean-binary checks pass: rotation/energy, wheels, legacy spacecraft,
restoration of the actual previous cold snapshot, and multi-process networking.
The packaged dedicated executable separately starts its world, executes JS and
advances 59 fixed steps over its one-second bounded run, without a display or audio
device. The 29-case follow-up's physical source hashes exactly match this clean
checkpoint; documentation/build identity changes do not trigger another full run.
[Final clean receipt](evidence/prealpha-final-qualification/clean-final/FINAL_QUALIFICATION.json)
records the actual identities and binary hashes. The accepted-source SDK is not
falsely relabelled as containing the correction.

## Runtime defect found and corrected

### External acceleration timing

The prior rotation correction made every awake body share four chronological
intervals. It also accidentally marked every free body as requiring distribution
of its pre-applied acceleration across those intervals. Wheel suspension supplies
an opposing impulse before `Step`; moving gravity to later times while retaining
that impulse at the front of the step broke their balance.

At rest, the wheel fixture returned a downward endpoint velocity about
`-0.0613125 m/s = -(3/8) g dt`, despite a correct total spring load of 3924 N.
The same source compiled with GCC 15 reproduces it, while the frozen pre-correction
engine passes. This is a runtime regression, not a compiler or hardware diagnosis.
An independent free-body control applies two equal/opposite public kicks: its
velocity stays unchanged but the old candidate moves about a millimetre in one
step. The new test fails four trajectory assertions against the original candidate.

**Correction:** keep all bodies in chronological CCD intervals, but distribute
acceleration only for actual constraints or a velocity-coupled participant.
Unconstrained bodies retain the already-applied public pre-step kick, alongside
its counter-impulse. No wheel-specific branch, damping, sleeping change, altered
physics cadence or solver-budget change. Contact/joint/coupled gravity treatment
is retained, as are the rotation/arrival corrections.

| Unchanged wheel acceptance | Accepted source | Corrected candidate |
|---|---:|---:|
| Quiet speed after 300 steps | 0.061382 m/s; fails 0.03 bound | 0.002839 m/s; passes |
| Uneven-ground speed, matched GCC15 diagnostic | 0.067607 m/s; fails 0.05 bound | 0.028243 m/s; passes |
| Whole wheel suite | 28/30 pass | 30/30 pass |

All six new external-kick checks pass, including an arbitrarily rotated frame;
all existing rotation-energy checks remain passing (31,404 assertions total).
Free-flight trajectories change where the accepted source incorrectly retimed
acceleration; this restores the pre-correction public-kick behaviour, not identical
trajectories to the defective candidate.

### Legacy spacecraft oracle

The original retained session test fails three first-SAS checks. It assumes
`omega_end = omega_initial * (1 - 10 dt)` for a multi-axis spinning asymmetric box.
The torque kick has that initial angular-velocity effect, but subsequent free
rotation changes world omega as inertia rotates. Spatial angular momentum is the
appropriate impulse observable.

The corrected test independently computes cuboid inertia from authored dimensions
and mass, calculates spatial momentum from the actual endpoint orientation/omega,
and expresses it in initial-inertia angular-velocity units. The original `2e-6`
tolerance, controller inputs, timestep and fixtures are unchanged. All 228 checks
across 21 fixtures now pass, including thrust, coast, rotated/far-origin frames,
SAS convergence and inherited dismount motion. No spacecraft implementation or
controller was retuned. Original test bytes and all three failures are preserved.

## Coordinated regression qualification

The initial run contains **41 invocations**. It found two genuine wheel assertions,
three obsolete spacecraft-oracle assertions and one CharacterMotor output-path
setup abort. CharacterMotor tried to write `/source/build/m49-focused` in the
read-only checkout. Both `.cache` and `build` fixture outputs are now redirected
into owned writable build storage; no tracked source or protected evidence is made
writable. CharacterMotor subsequently passes all 32 checks.

After the bounded correction, **29 affected invocations pass**. This is a narrow
follow-up, not a repeated production-suite marathon. Raw initial results are not
overwritten or presented as 41 passes. Important contracts include:

| Contract | Positive and negative evidence |
|---|---|
| MK2 rotation/contact timing | Independent inertia/RK4 energy and momentum, analytic arrival, absent obstacles, cap/tangent obstruction, reaction observations; original curved-mesh continuation passes. |
| External kick timing | Balanced and unbalanced pre-step kicks in ordinary/rotated frames; original candidate fails the trajectory controls. |
| Vehicles/mechanics | Actual suspension/traction/steering, zero-grip control, load/rotation; missing chassis/invalid authoring; prefab, qualified cross-region references and real mechanical cold saves. |
| Modern and retained characters | CharacterMotor support/filtering, arbitrary gravity selection, curved legacy locomotion, swimming/no-input controls; modern JS project proves legacy adapters inactive. |
| Legacy spacecraft | Real GameSession/StepPlayedWorld with independent torque/momentum, no input-side snap, SAS, inertial coast and release/clearance. |
| Collision/constraints | MK2/joints/balance, completed observations and original finite-feature curved mesh, not only energy. |
| Persistence | Save evolution and excluded-body controls; separate-process elastic continuation compares all 13 physical scalars for 240 subsequent steps and an impulse negative control. |
| M77 | Real corrective action, actual landing and separate cold restoration. |
| Composed world lifecycle | Async terrain collision/navigation publication, origin shifts, failed replacement preserving old data, region streaming and saved exclusion. |
| Basic/advanced water | Default-off policy, independent basic swimming and real cold cycle; explicit enabled-fluid admission/restore controls. Deferred fluid failures are not rerun or counted as passes. |
| Networking | Genuine host/client processes and authority/admission negatives; packaged dedicated startup plus explicit rejection of a composed-world project. Not phone/public-Internet acceptance. |
| Consumer game | Hearthworld felling, mining, construction, gravity gun, excavation, saved inventory/terrain and fresh-process restoration; powertrain game write/read. Owned copies only. |

Current API/type checking passes for 473 symbols, 40 exports and 51 examples;
this is structural/type coverage, not execution of every public member. Packaging's
28 and distribution's 21 positive/negative controls pass. Current-doc link checking
passes; semantic status corrections were reviewed separately.

## Setup failures, boundaries and preservation

The initial SDK-work layout was correctly rejected because its writable output
contained the read-only SDK mount. Moving SDK/output to sibling directories fixed
the runner. An environment-bake probe used unsupported 8/8 arguments and was
correctly rejected; the documented minimum 16/32 succeeds. These are preserved
runner/setup mistakes, not engine defects or fabricated numerical failures.

The source remains unchanged outside the three implementation/test paths and
current documentation/inventory. Historical reports, protected research, existing
games, shaders/assets, licensing, versions and release channels are unchanged.
No new gameplay/API feature was added. The corrected runtime has automated Linux
validation; it has not acquired human acceptance from these tests.

## Coverage decisions and readiness

- Retained C++ controllers have targeted positive/negative current-engine coverage;
  the former completely unverified CAP-01 is improved, not every old game certified.
- Universal replication, arbitrary physical save-topology migration, arbitrary
  planetary excavation, all active-region suspension combinations and multiple
  simultaneous physics charts remain unsupported subsets, not new work promises.
- Resource-residency scaling and every public API/editor interaction are not
  exhaustively rebenchmarked. Prior measured evidence retains its own source scope.
- FLU-001 and half-turn disturbed-liquid sampling remain failed/deferred behind
  default-off advanced fluids. Historical snapshots missing continuation remain
  incompatible. No fluid or independent research changes.
- Dense physics, extreme-spin phase error and long-run elastic dissipation retain
  documented bounds; no new FPS or performance improvement claim.
- Windows/Android qualification and the operator-built game trial remain separate.

No demonstrated major redesign is required by this pass. The bounded correction
and support decisions are ready for human review with the clean SDK receipt
attached. This is not a declaration of Alpha readiness.

## Reproduction and human checklist

Use the [older-userland builder](LINUX_BASELINE_BUILD.md), at the receipt's exact
revision, in a new build directory; configure Release/Experimental and two jobs.
Evidence includes all command arrays, source/build identities, selected targets,
root package versions, tool operations, binaries' hashes and runner scripts.
Known relative-output tests need writable disposable `build`/`.cache` destinations;
they must not run directly against protected evidence. SDK editor/tool runs mount
only SDK plus output; moved games mount only package plus the test driver.

1. Launch the qualified SDK editor; create a Visible script project and Play/Stop.
2. Change/check a script, save/reopen and export; move the package and launch it.
3. Optional wheel check: open `projects/wheel_lab/wheel_lab.judasproj` with the
   qualified SDK editor. W/S drive, A/D steer, Space brakes, V runs the tour;
   stationary suspension should settle and uneven ground should remain usable.
   No source quality or physics-budget switch is needed.
4. Optional existing-game check: Hearthworld save/quit/reopen, then inspect carried
   objects, buildings and excavation. Your actual game-building trial stays deferred.
5. Review the support matrix before choosing advanced fluids, migration or online
   combinations. Windows/Android stay deferred; no controller/hardware claim is
   inferred from synthetic Linux input.

## Local deliverables

All are local qualification artifacts, not published releases:

- SDK: `.cache/prealpha-final-qualification/sdk-final/`.
- Archive: `.cache/prealpha-final-qualification/Judas-PreAlpha-Linux-Ubuntu24-f7691198.tar.gz`
  (416,770,175 bytes; SHA-256
  `ede3f65b4211136e54c745ed619e60f583f04f12ad047ea3027e25bf47cb7c4c`).
- Payload: 237 manifest files, 934,406,243 logical bytes; 17 executables.
- Created editable project: `.cache/prealpha-final-qualification/sdk-work-final/game/SDK_First_Game.judasproj`.
- Moved games: `.cache/prealpha-final-qualification/final-moved-editor-export/`
  and `final-moved-cli-export/` beside it.
- Source/build/ABI/artifact details: [artifact manifest](evidence/prealpha-final-qualification/clean-final/FINAL_ARTIFACTS.json).

The engine and bundled SDK docs identify their exact clean runtime checkpoint
f7691198. This later qualification/support supplement records the finished tests;
it does not pretend that a binary contains subsequent documentation commits.
The initial c76 SDK remains under `.cache/prealpha-final-qualification/sdk/` for
comparison. Use the corrected **sdk-final**, not that preserved failing baseline.
