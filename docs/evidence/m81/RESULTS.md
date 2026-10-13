# M81 — reusable wheels / accepted Linux implementation

**2026-10-13. Operator human acceptance recorded.** Accepted implementation is
`7489a902d7e8158dbaed3328ffe96e29fe0936a8`; Stable promotion is explicitly
authorized. Development baseline was accepted M80
`bc1fd286d75b6db843fa9942b8d21c946c2e5c2a`. Main remains
`3102bf6c6e4b5c57eb3ab66a71fa502f8fc423f6`. See
[the acceptance record](HUMAN_VALIDATION.md). Human acceptance comes from the
operator's statement, not the automated checks below. No milestone tag is created.

## Inspection and coherent scope

Judas already supplies rigid-body inertia/momentum, prescribed moving bodies,
M39 classification, M44 broadphase/casts, M45 constraints, generic materials,
M79 fracture and M80 deformation. It did not have an independent wheel/tyre
suspension component; the historical spacecraft adapter is not a generic wheel.
No car controller, gearbox, engine model, axle count or input/camera policy was
added to C++. The ordinary project owns those decisions.

`WheelInstance` is a per-wheel scalar-spin/contact/suspension coupler on an ordinary
chassis, owned by RuntimeWorld. Contact is a sphere cast through authoritative
PhysicsWorld geometry/broadphase. A backward-Euler spring/damper normal row and
2×2 Coulomb-limited tangent rows include chassis/ground effective mass and spin
inertia. Axial torque affects spin; non-axial contact torque affects the chassis.
Drive/brake torque applies its paired chassis reaction. Dynamic ground receives
opposite impulses. Suspension orientation and contact normal are distinct from
gravity; the normal Judas gravity resolver continues to act on bodies.

Visual owners use the normal previous/current presented pose; meshes remain
separate from physical contact. Generic native angular impulses and command-aware
read-only step point velocity reuse existing RigidBody/prescribed-motion machinery.
No renderer-owned simulation or separate vehicle physics world exists.

Scene/editor/prefab references, conditional fingerprints, bounded save participant,
streamed payload/dependencies, normal export and generation-safe JS handles are
integrated. `entity.wheel` supplies detached settings/state snapshots, fixed-update
`set`, `configure`, enabled and validity. Docs/types/inventory/cookbook are aligned.
See [architecture/model equations](../../M81_WHEELS.md) and [public API](../../judasjs/wheels.md).

## Established corrections and preserved failures

1. Pre-step wheel coupling initially read the *previous* velocity of a newly
   commanded kinematic surface. The shared interval sampler now evaluates the
   same prescribed command used by PhysicsWorld.Step, without mutation. Start,
   stop and rotating target controls pass; ordinary GetPointVelocity semantics
   remain unchanged.
2. Distant managed streamed chassis could become Dormant before their wheels
   introduced the Full requirement. Wheel staging now reconstructs the chassis
   through normal fidelity machinery while it remains disabled/unpublished.
   Both registration orders pass; removal releases retention and an unreferenced
   managed actor remains reduced. Original exact source/test/failure hashes are
   preserved in `managed-stream-original-*`; [review](integration-review.md).
3. Cached rolling resistance cannot act after contact is lost. Drive/brake shaft
   reaction and dynamic-ground paired momentum are checked independently.

Genuine harness/build failures and their follow-ups are preserved in
[development notes](development/NOTES.md): an impossible initial radial travel
expectation corrected using an independent torque/inertia reference, unwritable
sandbox save directory, a stale object caused by build/source overlap, omitted
fixed-input publication in a direct test helper, missing test arguments/target,
and a wrong ACTION versus AXIS smoke directive. None was hidden by engine tuning,
weakened production fixtures or regenerated expected save bytes.

## Focused validation

| Scope | Observed result |
|---|---|
| Wheel numerical/physical checks | 30 / 30: spring load vs mg, load change, drive/brake, no-grip spin, lateral traction, steering, rotation covariance, uneven/radial geometry, one wheel, paired momentum/shaft reaction, current kinematics, filters/lifetime/save |
| Component/persistence/streaming | **51 / 51 final** including exact continuation, independent prefabs, authored reset, orphan rejection, slot reuse, both distant-managed registration orders, private publication and no-wheel negative control |
| Actual QuickJS | 18 / 18 including phase/atomic/invalid/stale handles, actual traction, exact copied cookbook and live module enumeration |
| Ordinary lab applications | 88 distinct checks across contracts, flat, oblique, radial, steering, lifetime/stream revisit, cold write/read; affected flat/radial/lifetime/cold checks rerun after late publication correction |
| Shared affected regressions | Physics, contacts/casts, joints/authoring, CharacterMotor32, kinematics96, physical continuation112, save storage28, exclusion77, gravity selection70/uniform scene, M79 structures and M80 bending pass |
| Existing composed game save/stream | Write11/read19 pass before and after publication correction; real gameplay/localization/adoption/tombstones preserved |
| API/types | TypeScript5.9.3; live 31exports /355public symbols /246bridge operations;40examples;3negative drift controls pass. Lab JS checkJs passes |
| Editor | Real Wheel selected in scratch project; duplicate/undo identical; normal Play/Stop leaves scene identical; save/reopen-format path succeeds; zero failed resources |
| Export | Normal exporter:10assets,5scenes,54,364,591bytes; no special demo mode |
| Moved package | No-argument executable from unrelated `/tmp`, folder with spaces;500-frame real-input smoke visits all gravity scenes, pause/resume/reload; packaged drive probe10, cold write11/read7 pass |

Initial focused-run JSON retains a usage exit2 for omitted exclusion-test output;
`save-exclusion-correct.txt` and final follow-up show77 passes. Initial package JSON
retains the bad harness input failure; `moved-standalone-final-result.json` is the
corrected successful replay. These are not unresolved engine failures. The final
51-check isolation rerun follows the final one-line test strengthening; original
50-check reproduction remains immutable.

Build: Release -O3/-DNDEBUG, bounded -j2. Full engine/editor/runtime/export rebuild
completed; subsequent final-source targeted builds completed. Two existing
RuntimeWorld material misleading-indentation warnings remain unchanged. New
wheel/editor/test warnings were removed. No unrelated full production/research
marathon was run.

Offscreen SDL cannot implement desktop relative capture and logs that limitation;
logical input and pause behavior are checked. Subsequent operator desktop
acceptance is recorded separately without inventing individual checklist results.
Windows VM was off: **M81 Windows not run, informational/nonblocking**.

## Representative Linux performance

Native host CPU: Intel Core i7-4790. Physics60Hz unchanged. Measurements use the ordinary 60 Hz physics cadence and unchanged quality.

| Wheel batch | Wheel p50 / p95 / max | Ordinary rigid step p95 |
|---|---|---|
|4wheels /1chassis /4contacts|0.009893 /0.011717 /0.022505ms|0.005514ms|
|24wheels /6chassis /24contacts|0.049155 /0.072255 /0.143525ms|0.025163ms|
|100wheels /25chassis /100contacts|0.209093 /0.305011 /0.321164ms|0.101621ms|

An earlier idle-build capture had100-wheel p950.341994ms; neither is a universal
performance guarantee. M44 regression also records32candidates in1024colliders.
Wheels use the existing broadphase for filtered candidate discovery and contact
cast, not a private all-body loop.

Final-source lab:54entities /13rigid bodies /10wheels at startup (two dynamic
chassis650/950kg; payload/spawns add actual bodies).

| Scene | Fixed p50 /p95 /max | Offscreen frame work p50 /p95 |
|---|---|---|
|Flat final affected rerun|0.434736 /0.830548 /3.261620ms|24.183993 /36.760556ms|
|Radial final affected rerun|0.361252 /0.633882 /3.566396ms|15.537405 /20.800983ms|
|Oblique before streaming-only correction|0.289196 /0.504499 /1.885091ms|18.659838 /26.778479ms|

These frame figures include rendering/script/frame work in synthetic fixed-dt,
uncapped offscreen tests; they are not measured desktop FPS or GPU certification.
Wheel and fixed-step costs are small relative to that render/frame workload.
The moved package measures similar physical/frame work; raw receipts retain tails.

Physical observations: flat/oblique forward4.325065/4.325063m; braking flat
~0.059m/s; steering changed actual heading0.33598rad. A225kg payload increases
measured flat suspension load6368→8667N and reduces mean travel270→221mm.
Radial braking is approximate (~0.727m/s after the scripted brake interval), with
real changing slope/gravity; it is not falsely advertised as immediate stop.

## Deliverables, boundaries and limitations

- Project: `projects/wheel_lab/wheel_lab.judasproj`, registered flat/oblique/radial
  plus streamed garage and a verification-only contract scene.
- Ordinary exported package: `/tmp/judas-m81-wheel-lab-20261013`.
- Moved tested copy: `/tmp/judas-m81-moved/Wheel Lab`.
- Exact intended changes: `CHANGED_FILES.txt`; source/binary/evidence manifests
  separately verified before checkpoint. Build outputs/packages are excluded.
- [One human checklist and controls](HUMAN_CHECKLIST.md).

Contact is a swept sphere with scalar spin: no tyre width/tread, separate unsprung
translation, full gyroscopic axis transport or wheel-specific CCD. Initial
penetration depth is unavailable, so no fabricated overlap correction exists.
Very thin/fast/lateral contacts exceed the approximation. No automatic bicycle
balancing. Query-only wheel support does not create an ordinary rigid sleep
island, so those chassis normally remain awake under gravity. Proxy masks/tags
are authored separately from joint pair suppression. Strict saves reject dangling
required chassis instead of losing state. Full limitations remain in the model
reference; these are explicit engineering bounds, not hidden car policy.

Protected history, existing game project source, accepted fluid implementation
and independent research are unchanged. Deferred unrelated defects were not
retuned. The operator authorized Stable promotion after human testing; main remains
unchanged. Original numerical receipts and failure evidence remain unchanged.
