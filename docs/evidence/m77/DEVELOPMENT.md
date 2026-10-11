# M77 development observations

## 2026-10-11 — free-root actuation preflight

Disposable mechanics probe under `.cache/m77/experiments/`: original seven-body,
two-legged box articulation, all dynamic, 34 kg total, ordinary 9.81 m/s² gravity,
0.8 material friction on rig and ground, 60 Hz; no capsule, root pin, transform
repair, extra gravity or external stabilizing force. This is a development probe,
not the final lab, final accepted envelope or a controller-success test.

1. Independent implicit M70 relative-posture torques plus paired balance effort
   traversing each leg did not establish standing. Root fell from 1.2 m to about
   0.31 m in one second. With corrective effort omitted the root settled near
   0.15 m. A compile attempt first used an unnecessary experimental GLM include;
   removing that include corrected the tooling error.
2. Root-orientation feedback held the torso up (up dot around 0.98) while root
   height remained around 0.29 m: legs had collapsed. This is explicitly a failed
   standing result, despite visually upright torso orientation.
3. Opt-in bounded quaternion drive rows solved within the existing joint/contact
   iterations retained root height near 1.2 m after a 5 N s lateral perturbation,
   but continued motion/drift remained. This establishes an actuation direction
   to investigate, not stable balance or a complete M77 controller. The initial
   runtime-field placement broke a JointState aggregate initializer; moving the
   new default-off field after existing members corrected that compile failure.

Raw output and original probe variants remain in the ignored experiment directory.
The next bounded experiment adds contact-aware ankle reference feedback, then
physical lift/touchdown and supported recovery. No existing solver cadence,
friction policy, CCD or protected research code has been weakened.

## Contact and actuator foundation checks

`tests/PhysicalBalanceFoundationTests.cpp` currently passes 32 checks, using the
modified `PhysicsWorld.cpp` and `JointSolver.cpp`, unchanged dependency archives
and the existing compiler/link configuration. This is focused native development
proof, not an application smoke, clean full-engine build or M77 acceptance.

Checks exercise paired angular impulse, no invented linear impulse, magnitude
effort caps, quaternion antipodes, finite/atomic validation, relative target-rate
tracking, actual hinge limits, opt-in disable, invalid/stale handles, true solver
contacts, filtering/sensors, teleport invalidation and fail-closed contact overflow.
Observation revisions invalidate only the new native observation view; they do
not clear or change the existing physics warm-start policy.

The first target-rate row omitted `dt` in its damping reference term, requesting
roughly fixed-frequency-scaled angular velocity. The independent 1 rad/s test
measured 36.0843925 rad/s and failed. Correcting the dimensional factor yielded
the passing result. Original failed output is retained in `development/`.
The row bias is now
`gamma * (dt * stiffness * angularError - dt * damping * targetRate)`, with
`gamma = 1 / (dt * damping + dt² * stiffness)`. All impulses remain paired and
capped. Default-off existing joint behaviour is not replaced.

## Recovery experiments — not the completed controller

Staged relative references, added arms, hinge/ball variants and activation ramps
did not establish supported standing on the slender-foot fixture. Some aggressive
references launched it or produced persistent free-foot motion; torso orientation
alone was again insufficient. No failed attempt is counted as successful recovery.
The probes used ordinary physics with bounded drives, without root forces,
transform corrections, kinematic feet or an invisible supporting capsule.

A narrower seven-body, 28 kg robot experiment uses a 4 kg root, four 1 kg leg
segments and two explicitly authored 10 kg cubic feet (0.6 m sides). This has a
different, intentionally disclosed support/mass envelope. It is not evidence
that a normal slender-foot humanoid can recover. Distal references use actual
load-bearing contacts, paired effort and target-rate compensation; other joints
receive relative posture references. Cubic feet may contact on their real side
faces; the final controller must represent accepted contact patches explicitly.

| Start | Effort | Root height at step 1140 | Root-up/gravity-up dot | COM speed (rounded m/s) |
|---|---|---:|---:|---:|
| +90° settled fallen | enabled | 1.2453 | 0.9998 | 0.0000 |
| -90° settled fallen | enabled | 1.2493 | 0.9990 | 0.0000 |
| +90° settled fallen | disabled | 0.1500 | 0.0004 | 0.0001 |
| -90° settled fallen | disabled | 0.1483 | -0.0005 | 0.0000 |
| +1.4 rad start variation | enabled | 1.2452 | 0.9998 | 0.0000 |
| -1.4 rad start variation | enabled | 1.2493 | 0.9991 | 0.0000 |

60 Hz, normal uniform gravity, initial 120-step passive settling, then a bounded
reference ramp. Ground is ordinary finite static geometry; whole articulation is
dynamic. The test omits self-collision explicitly to isolate actuation, and has no
runtime mode/motor, asset, save or script integration. Raw body/contact/effort
logs, source variants and checksums are retained in `development/`.
World Y in these sources is the chosen flat test fixture, not an implemented
generic planning frame. Rigidly rotated/radial/support equivalence is not proved.

## Remaining work before a candidate

- Implement the native task/planner with authored mappings and contact patches,
  gravity-coherent frames, observed settling/failure gates and real corrective steps.
- Demonstrate two compatible, differently named/proportioned rigs, bounded losses,
  stepping denied/allowed, terrain and moving/rotated/radial supports.
- Integrate explicit M70 authority and reference ownership, full physical pose
  publication, public JS, editor/scene/prefab and atomic configuration validation.
- Persist semantic task/planner state with fresh-process mid-task restoration;
  test lifetime, faults, regions, mod admission and safe motor return/refusal.
- Build the ordinary Balance Lab, profile representative cases, run focused
  application/export/native Windows VM proof and prepare the operator checklist.

Starting HEAD remains unchanged. Nothing is staged, committed, pushed or tagged.
M78 and unrelated game/fluid work have not been started.

## Normal build follow-up

The normal incremental Release build completed for
`judas_physical_balance_foundation_tests` and `judas_physical_animation_tests`.
Both executables passed: 32 and 64 checks respectively. The first build exposed
a new incomplete `JointState` aggregate initializer warning; explicit state
construction corrected it, followed by the affected rebuild/tests. Two existing
misleading-indentation warnings in unchanged `RuntimeWorld.cpp` were observed;
that unrelated source was not edited. No clean candidate build, production-suite
run, application/export smoke, Windows proof or human acceptance is claimed.

## Controller/integration follow-up — still development

Native control now passes 118 focused development checks. `controller-negative-
matrix-first.log` preserves an invalid moving-support assertion: a finite-mass
plate resting on a high-friction static bed need not visibly move under a small
impulse. The corrected fixture uses an explicitly frictionless bed to distinguish
finite contact reaction from an anchor; plate/feet retain normal friction.

Curved support first failed the standing dwell. Always forcing upright foot
orientation fixed that case but broke cube-side get-up and stepping; capturing
a fixed planted frame also broke stepping. Those real failures are retained in
`controller-supports-first.log`, `controller-fixed-foot-experiment.log` and
`controller-captured-foot-experiment.log`. The explicit authored planted-reference
weight preserves both contact models through the same controller/drives.

Normal asset/project generation first attempted to remint an existing importer
identity; the generator now reuses registered metadata/import recipes. The first
application build rejected balance+motor without a M70-specific authority marker;
the canonical validator now also recognizes an explicit M77 balance definition.
The first application test then queried an already-destroyed raw PhysicsWorld and
crashed its driver; that post-destruction query was removed. No live application
crash or completed application proof is claimed from that driver result.

The seven-body momentum audit records inherited angular numerical drift, including
effort-off controls. Final native/API/application/save/platform evidence remains
outstanding. Nothing committed, pushed or tagged.

## Application and durable-task follow-up — still development

Normal ResourceManager/QuickJS/GL application checks now pass: standing/step 15,
moving/rotating support 14, and both distinct fallen rigs 16. The first lab JS
used a nonexistent `Entity.name` property when choosing the authored root key.
It now uses explicit project profile mappings; no runtime API was invented.
The initial animated-rest -> B -> J case did not establish a successful step.
The declared comparison uses P (passive), two seconds of physical settling, B
and six seconds standing, then J. That same 29 N s impulse succeeds with
stepping enabled; the native denied comparison falls. This narrow initial-state
and perturbation envelope is retained honestly, not claimed as universal gait.

Separate processes now restore in-progress step, standing and in-progress recovery
native history, preserving clocks and real bodies without replaying an impulse or
start callback. Saved observations are unavailable until actual contact sampling.
Missing/newer/truncated balance chunks are rejected on detached candidates; live
physical state remains unchanged. The balance record bound matches 128 runtime
owners and duplicate required-owner declarations are rejected. No final platform,
performance, export or human acceptance claim is made from these checks.

## Native platform build follow-up — not an engine crash

The planned clean affected Linux Release build was paused while the native Windows
build ran, to serialize heavy host/guest work. Windows Update interrupted its first
build with an OS/service-pack restart (System event 1074, svchost/SYSTEM, planned;
6006/6005 confirm stop/start). The VM returned normally; the original partial build
log and event receipt are retained. No Judas process was running at that boundary.

The resumed Windows build then failed compiling unchanged PerformanceProfiler.cpp:
MSVC C1060 compiler heap exhaustion and MSBuild memory diagnostics. One-worker retry
reproduced C1060 with over 4 GB guest physical memory available. A bounded retry
selects the already-installed x64 compiler host via MSBuild's
PreferredToolArchitecture=x64; no VM configuration, disk expansion, global setting
or runtime semantics were changed. Final result remains to be recorded.

The current lab now authors low-grip and obstructed recovery variants, modest slope,
2 cm uneven ground, a rotated finite platform with an ordinary precedence-ordered
uniform gravity region, a visual skeleton socket, and explicit partial/layer controls.
These are normal project content; final application assertions are being rerun.

## Clean Linux candidate pass and authored-variant corrections

The planned clean affected Release build completed. The final controller's 118
checks pass with ordinary sleeping; an experimental slender-fixture sleep-disable
was removed before that build. Shared-axis/contact foundation remains 32 checks.
Affected M70, CharacterMotor, joints/authoring, ragdoll, pose, canonical fingerprints,
persistence, streaming and M76 checks pass. No full unrelated production run occurred.

The expanded application matrix exposed two content mistakes, retained in final-linux
logs: the unselected slope actor's active motor moved it off its finite platform
before requesting physics; the blocked actor escaped a short roof and legitimately
stood outside it. Support/fallen fixtures now keep motors inactive until explicitly
returned, and the visible low roof covers the bounded recovery trial. No native
controller, gravity, friction, cadence or collision policy changed for these fixes.
The narrow rerun passes supports (14), uneven/slope/oblique variants (17), blocked
recovery (15) and both clear fallen recoveries (16). The blocked root remained
fallen (up .092, height .540 m) and reported its finite standing deadline.

## Candidate follow-up — Linux, authoring and application save

The clean affected Linux Release build completed. Ten warning emissions came
from three inherited locations: two RuntimeWorld indentation sites and a
ModContentTests temporary-lifetime diagnostic. No zero-warning claim is made.
Final native controller 118 and M70 64 checks pass with ordinary sleeping; the
experimental sleeping-off fixture was removed before this clean build.

Expanded application variants initially failed: an enabled motor moved the
slope actor off its finite support before physics activation, and a fallen
actor escaped a short roof and legitimately stood outside it. Normal content
authoring now leaves motors inactive in support/fallen trials and places a
visible broader roof over the obstructed-recovery trial. These are content
corrections, not controller or friction/cadence changes. The original failures
and affected supports/variants/blocked/recovery follow-ups are preserved.

Normal Application F6/F7 exposed a real M77 integration omission: SaveService
rejected the new physical-balance participant as an unexpected chunk. The
participant whitelist/count and SaveStorage metadata bound now allow seventeen
instead of sixteen entries, without a format version change. Foundation checks
prove seventeen accepted/eighteen rejected; they now total 34. Existing world
persistence 27, storage 28 and legacy save-lab fresh-process write 13/read 20
pass. The corrected normal application executes save/load, pause, reload and
all four registered scenes. Original failure and corrected logs remain.

The actual editor autotest restores identical authored state after Play/Stop
and saves normally. Automated capture reports no relative mouse mode in its
execution environment; this is not human pointer/usability acceptance.

The moved Linux package also passes the application services script from an
unrelated space/Unicode path with the source checkout hidden in a temporary
process-only filesystem namespace. The first restricted-sandbox namespace
attempt was refused before launch; an authorized process-only run succeeded.
No host setting/source mutation was required.

## Windows continuation — compiler boundary

The VM was shut down when the session resumed; its original 4-vCPU/6-GB
configuration was booted unchanged. Source transfer verified all 852 files,
including the three-file save follow-up. The compiler-only reduced-optimization
flag also reproduced C1060 in unchanged PerformanceProfiler.cpp. A further
profiler-only /Od /Ob0 compilation also failed with C1060 (original log retained). This is not
an engine runtime failure or Windows acceptance; no VM memory/disk settings,
SDK/Mesa contents or runtime simulation quality were changed.

## Compiler isolation and narrow constructor correction

Two tiny native MSVC Release probes contain identical profiler lane fields,
member initializers and the existing 32-lane array. Nested aggregate initialization
fails C1060 after 21.49 s; a user-provided empty Lane constructor compiles in
1.37 s. Thus disabling optimization or increasing VM memory was not justified.
The actual profiler now uses that constructor; bounds and initial values are
unchanged. No global compiler option, VM setting or simulation parameter changes.
Linux affected rebuild passes; profiler 27, foundation 34 and controller 118
checks pass, including a fresh-profiler default-zero regression. Native stock
Release build and interactive VM checks are recorded separately below.

The final API identity now names the actual accepted M76 base instead of the
older inherited inventory checkpoint. Live enumeration/types/drift still agree:
345 public symbols, 30 exports, 239 native operations and 39 examples. A first
local check omitted the developer-only TypeScript path and failed to load it;
the corrected explicit existing TypeScript 5.9.3 path passes. Both logs remain.

## Native Windows discrete foot-choice failure and focused follow-up

Native Release foundation/controller/M70/motor, support/recovery/radial/lifetime/
region/fault/standing-save/recovery-save tests pass. Actual lab stepping and a
cold in-progress step failed: the initial symmetric step chose the other chain.
Windows pre-plan solved loads were 1.9622135 / 1.9617927 N s, a 0.0004208 N s
(.0107%) difference. Linux loads were 1.9601752 / 1.9638262 N s. The Windows
right-foot trial landed once, oscillated into another attempt, then failed its
physical touchdown deadline. This is preserved, not counted as acceptance.

The generic discrete selector now treats differences within .1% of summed actual
support impulse as ties and uses stable authored chain order. Meaningfully lower
load still wins. Forces, contacts, effort, cadence and planning/standing gates
are unchanged. Final Linux controller 119 checks, real-JS step/denied/lifetime,
all three cold-save families and all seven performance scenarios pass. Native
Windows follow-up is recorded separately. The first additional selector test
incorrectly assumed planning occurred on the first loop iteration; it now checks
the first actual planned step, using its actual observed loads. Both logs remain.

A Windows profiler test also used literal /tmp; it now uses the ordinary standard
filesystem temporary directory and passes its 27 checks. The first VM export
failed because the disposable test executable's copied engine font lacked nearby
notices/license files. The owned staging fixture is corrected; no exporter/runtime
semantics were changed. An initial trace task was dispatched before its test link
completed and used the older executable; that failed missing-scene result is kept
as orchestration evidence, not numerical evidence. The verified trace uses the
completed build and a fresh output directory.

## Final native Windows acceptance boundary

Stock Release rebuild and all 27 focused final VM cases pass, including actual
JS-controlled stepping, in-progress-step cold restore, editor Play/Stop, ordinary
services and source-hidden moved Unicode package. Least-privilege session 1,
admin false. Final 853-file source verification has zero mismatches. Original
Windows failures remain retained; no full unrelated production run was repeated.
The pristine package excludes Mesa; only disposable VM test copies use software
GL. Real Windows GPU/input and human acceptance remain outstanding.

The VM had only ~42 MB free after validation. The prior narrow ICU-only object
cleanup found no remaining objects. After verifying no validation/compiler process
was running, 267 disposable .obj files only under the owned validation Build
were reclaimed (~116 MB); source, tools/SDK, headers/libraries/native binaries,
packages, user data and evidence remain. VM settings were unchanged.
