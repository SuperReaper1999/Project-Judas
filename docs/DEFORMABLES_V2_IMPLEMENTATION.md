# Deformables V2 — Stages 2–4 implementation record

This is a chronological engineering record. Early "remaining work" and "in
progress" sections refer to the checkpoint being described, not a current backlog.
Use [the qualification handover](DEFORMABLES_V2_QUALIFICATION.md) for the current
implementation, measured limitations, private launch instructions and review scope.

The operator human-accepted revised Stage 1 at
`6a1d49b2add0e45bab6933f544797a8faa6e3d21`: imported Challenger wheels,
local bonnet bending, progressive mounting-sheet failure and physical detachment.
Stages 2–4 are now authorised on Experimental. This record is **work in progress**,
not full-milestone qualification or human acceptance. Main, the fluid lock and the
operator's RPG SDK remain protected. Third-party assets stay private.

## Contact and sleep correction

- `deformables/MaterialContacts.cpp` owns soft/rigid contact sampling and response.
  Capsules use the existing authoritative segment evaluator; direct radial terrain
  uses the existing analytic surface. Cooked Terrain V2 contacts remain available.
- `deformables/RigidResponse.h` shares effective point mass and live rigid velocity
  with attachments. Reactions retain a binary64 local lever arm before crossing
  MK2's binary32 impulse API, including at large simulation coordinates.
- `deformables/SleepContacts.*` owns instance-local collision publication signatures.
  Candidate identity, immutable geometry, pose and applicable filtering detect new,
  moved, removed and replaced nearby geometry. No world-wide edit epoch wakes
  unrelated materials. Terrain geometry pages are not acquired just to check sleep.
- Dynamic attachments now test actual pose/velocity motion, rather than treating
  every dynamic body as permanently moving. Genuine target impulses still wake.
  The accepted released-target wake correction is preserved.

The contact/sleep checkpoint soft test executable passed **86 checks**, including 18 new controls.
Vehicle dynamics (97) and structural/crash controls (174) also pass with the accepted
rigid cadence restored. The build-registration controls pass 27 tests.
The new tests cover static/rotated capsule contact, analytic radial contact, local
static creation/movement, same-body cooked collision replacement, quiet dynamic
attachments, wake impulses and an unrelated-geometry negative control.

An attempted dynamic/kinematic capsule fixture exposed an existing MK2 limitation:
its general capsule mass/motion factory is unsupported. Static capsules are tested;
query-only character proxies are not silently advertised as physical rigid actors.
Dynamic reactions remain exercised using supported rigid shapes. No capsule mass
implementation or unsupported interaction is invented by these tests.

## Rejected scheduling shortcut — retained evidence

Removing the structural-quality maximum's outer subdivisions while retaining MK2's
native four substeps improved ordinary timing, but did not qualify as a final fix.
On identical authored geometry/controls, the 30 m/s crash changed drivetrain failure
and worsened the worst measured interval from about 115 to 196 ms. Its trajectory
was not physically identical. The faster mild/four-car numbers are **not** claimed
as an accepted improvement or permission to reduce fidelity.

The accepted cadence is restored while native coupling/local scheduling is developed.
Raw private results and the tested source are retained under
`.cache/deformables-v2-stages234-evidence/` (with SHA-256 manifest); include them in the final durable
final qualification evidence before cleanup. The original accepted private binary
and assets remain unchanged.

## Remaining authorised work

Reusable multi-panel settings/cooking and detailed bindings; native attachment part
references and physical tearing; normal editor presets/visual authoring; scripting,
prefabs and persistence; correct soft/rigid scheduling without unrelated whole-world
subdivision; sustained terrain/streaming/origin/platform/SDK qualification and actual
performance/visual checks. These are not yet reported complete.

## Native local panels — integration in progress

`deformables/PanelCook.*` fits a bounded coarse physical sheet to a selected
single-surface imported region. Its detailed render mesh is separate from the
solver resolution. Full local rest offsets, smooth source normals, tangents,
material slots and embedded images survive baking using the existing model codec.
Closed shells/overhangs must be split into sensible panel regions; the fitted
rectangular physical envelope does not claim exact per-polygon collision.

`DeformableSettings::panel` owns signed-dihedral stiffness, yield moment and
hardening. `DeformableAttachment::tearWork` is a mounting-sheet energy capacity
in joules, independently of hardware force/moment limits. Local irreversible
hinge work is apportioned by physical group membership. Completed-step release
adds no velocity, transform correction or ejection force. Different scene panels
have independent mass, connections, material state and detailed visual binding.

Body attachments may name a stable structural `part` on their target entity.
Offsets are in that actual section body's local COM frame. Runtime body handles
are resolved, never persisted as authored IDs. An attached panel excludes the
coarse carrier's connected component, not every vehicle sharing a collision
layer. Complete detachment clears those derived exclusions. Independent rigid
frame supports do not accept panel section mounts; they continue to use ordinary
world/body supports.

Editor implementation lives in `editor/DeformableEditor.*`, registered through
the existing component table. It owns presets, source/region selection, panel
fitting, material controls, attachment patches and diagnostics; no frame-loop
change is required. The CLI shares the cooker:

```sh
judas_deformable_bake panel output.judasdeform source.judasmodel 6 6 1 sourceAssetId
```

Normal axis is `0=X`, `1=Y`, `2=Z`. Select an imported part in the editor before
fitting a closed vehicle. The CLI consumes the supplied mesh region as a whole.

Scene additions: `deformable.panelMaterial="stiffness yieldMoment hardening"` and
`deformable.attachmentMounts='count "group" "part" tearWork ...'`. Both are
optional authoring capabilities. JudasJS attachment options expose `part` and
`tearWork`; attachment state exposes local `plasticWork`, capacities and release.

**Intentional preview-format break:** current material snapshots use whole-world
participant **7** and per-region **Judas.DeformableState.5**. They store complete
attachment fields, panel constitutive state and permanent work. Earlier material
snapshot schemas are rejected, not migrated or silently repaired. The accepted
Stage 1 binary and its private saved evidence remain separately preserved.
Panel exterior assets use **JudasDeformable6**. Distinct current cloth/solid and
structural asset representations remain supported; no source-asset conversion
is performed automatically.

Current native checks: **109/109**, including detailed curved binding/material
round trips, transformed rest geometry, local failure and unchanged momentum,
fresh-instance continuation, two-panel normal-scene integration, section target
resolution, independent collision exclusions and fresh-world restoration.
Mechanical attachment checks: **32/32**; solid bending checks: **39/39**. Dedicated
old snapshot-layout assertions were retired with the formats; current strength,
corruption, material-frame and continuation assertions remain. This is not yet
cold-process, full SDK, platform or final visual qualification.

Additional scoped checks: existing fracture core **33/33**, Vehicles V2 dynamics
**97/97**, real-VM mechanical damage/authoring examples **18/18**, CMake registration
**27/27**, and current-doc link validation passed. Finite desktop editor automation
on GTX 970 / OpenGL 4.6 imported an original procedural panel through action 8,
then passed duplicate/undo, Play/Stop authored-state identity and save, with no
resource/job failures. Its default Game camera did not frame the panel: those
captures prove workflow execution, **not** final visual acceptance. A correctly
framed playable demonstration remains required.

The Challenger now consumes the native panel cooker, material, mounting-sheet
work and detailed mapper. A 900-step 30 m/s frontal run retained four drive links,
released all three bonnet mounts and accumulated permanent panel plasticity.
Its outcome is close to the accepted reference, not bit-identical; no performance
improvement is claimed from that integration run. Full collision/coupling and
sustained multi-model qualification remains outstanding.

## Chronological material coupling — qualification in progress

`MaterialIntegration.*` separates whole-fixed-step preparation/publication from
chronological material advancement. Forces last the entire fixed step and are
consumed once; interpolation history is captured once. Attachment failure and
its events publish only at the completed boundary. Saving a half-solved interval
is rejected. The standalone wrapper retains its existing solve path.

Body/world-attached material now participates through MK2's existing native
velocity callback, before its authoritative pose advance. MK2 remains the only
rigid solver. Current skeletal-target material still uses the established final
animation phase; its phase unification is outstanding. Structural owners still
retain the accepted outer cadence in this checkpoint slice: removing global
quality-driven subdivisions is **not yet claimed complete**.

The coupling's known-answer free mount is 2 kg, attached to 2 kg of material,
initial total momentum 2 kg m/s. Both local qualities (4 and 16) preserve momentum
to within 1.6e-6 kg m/s and advance the rigid mount about 0.008333 m during 1/60 s,
as expected for their shared velocity about 0.5 m/s. Reactions now affect that
interval's actual pose, not just the following frame. A rejected first run at
quality 16 advanced 0.011276 m: it incorrectly measured legitimate inner-step
motion against a stationary boundary. Native drift now uses the interval's
initial material reference and duration, and target poses come directly from MK2
rather than stale presentation records. The existing drift gain is unchanged;
no damping, velocity reset or rotation lock was introduced.

`setPanelMaterial({stiffness,yieldMoment,hardening})` edits an authored panel in
fixed callbacks without clearing its accumulated work or permanent rest angles.
State readback includes panel parameters, maximum plastic angle and total work.
`onAttachmentFailure({attachments:[{group,cause}]})` reports `force`, `torque`,
`mount-tear`, `target-lost` or `explicit`. Native world events share one runtime
lifecycle/error boundary in `scripting/WorldEvents.cpp`; existing `onFracture`
payload and semantics are preserved. Restore does not replay historical releases.

Imported hierarchy/rest-skin transforms are now applied exactly once before panel
fitting, including negative scale and triangle handedness. A panel's renderer does
not apply the original skeleton again. Original synthetic 100x/mirrored hierarchy
controls cover both selected-region authoring and direct CLI cooking.

Sustained native controls now pass at both tested local qualities: after 121
intervals of shared free motion, momentum error is below 2.5e-6 kg m/s, attachment
COM drift below 5e-8 m, and kinetic energy about 0.499999 J versus initial 1 J
(the attachment's expected inelastic velocity matching). Both initially resting
participants naturally sleep. Pure material worlds also advance without rigid
actors. The combined native deformable suite passes **132/132**; real-VM mechanics
**20/20**, shared-runtime module acceptance **8/8**, attachment **32/32**, solid
bending **39/39**, structural/crash **174/174**, and vehicle dynamics **97/97** pass.
These scoped passes do not replace the remaining real-car/coupled impact,
skeletal-phase, performance, SDK, cold-process or platform qualification.

The accepted detailed structural skin and its imported-surface island preparation
now live in `src/deformables/StructuralSkin.*` and `PanelIslands.h`, linked once by
the engine. The crash proof and original structural tests use that implementation;
there are no forwarding headers at their former tool paths. Imported UV/material
islands are preparation hints, not a universal semantic panel classifier.

## Native project integration checkpoint (Stages 2–4 in progress)

The Challenger proof now authors its bonnet as an ordinary scene deformable,
using stable structural-section mounts. It no longer advances a separate
proof-owned panel solver. Whole-world persistence owns the physical panel; the
proof's extra panel save participant is retired. The old accepted private binary
and its launchers remain unchanged.

Rigid structural assets may reference a detailed model through `sourceAsset` and
`sourceFingerprint`. ResourceManager validates the source and prepares one shared
immutable structural skin on its normal CPU worker. Instances retain their own
physical damage; the world renderer uses section palettes, rebuilding exterior
topology only when connection/removal topology changes. Detailed source polygons
do not become physical solver elements. Missing or changed sources fail before
publication. The Editor's **Bind detailed exterior to current structure** writes
a new physical asset at the chosen destination; local bending still uses separate
physical panels. Imported islands define presentation seams, not inferred hinges.

Native deformable checks: **136/136**. Structural/crash checks: **174/174**.
A fresh process restored the real Challenger's 900-step damaged state with exact
durable hash `c1349efc56b6192b7ebc77d72229a060141491d4ffca88fff75926336ac2929d`,
including all three released bonnet mounts. This is automated cold-process
evidence, not human acceptance of the complete milestone.

A conservative actual-shape AABB rejection avoids evaluating material samples
against impossible fat-broadphase candidates. The envelope expands after contact
projection; paged collision geometry is acquired only for plausible contacts.
In a matched 900-step 30 m/s native-panel Challenger comparison, saved physical
state remained identical. Fixed-step p50 fell **6.5758 → 3.8238 ms**, p95
**8.1731 → 4.1966 ms**, p99 **12.0859 → 5.4197 ms**; worst remained
**198.5824 → 194.9213 ms**. RSS was approximately 596 MiB in both runs.
No constraints, samples, iterations, material constants or time intervals were
removed. The remaining impact spike is not solved by this optimization.

The first native-renderer GTX 970 desktop run exercised the real Challenger,
normal world rendering and physical detachment. Its 180-step diagnostic run had
render CPU p50/p95 **0.844/0.945 ms** and GPU **1.133/1.193 ms**, with expensive
topology-change frames (render CPU worst 104.55 ms). This is not the final
one/four-car qualification. Global structural cadence, skeletal target phase,
full authoring usability, broader model fixtures, SDK and platform qualification
remain ongoing. Main, public releases, fluid lock and the RPG SDK are unchanged.

## Native cadence, terrain recovery and retained render resources

`PrepareDeformableMechanics` creates actual MK2 structural bodies/joints without
changing whole-world chronology. Soft material quality refines only that material;
MK2 keeps its own chronological intervals and rigid constraint budget. Vehicle
mechanics run on those same intervals. The earlier rejected outer-step shortcut
remains historical evidence; this implementation also includes the subsequently
qualified native soft/rigid reaction path. Its crash trajectories are not claimed
bit-identical to the old post-step solver. An unrelated freely spinning rigid
body has identical motion intervals and final pose at structural qualities 2 and
16. Native deformable tests now pass **143/143**, structural **174/174**, vehicles
**97/97**.

Terrain V2's published solid snapshot supplies containment; ordinary triangle
meshes still have no invented inside. A material node newly buried by a terrain
edit resolves against the nearest actual cooked terrain boundary. Existing
collision filters and static-contact friction apply, and the normal projection
contributes velocity through the ordinary material integration. Instantaneously
raising geometry through an object can therefore impart a contact impulse; no
velocity reset or damping hides that supplied work. The native test covers a
landed/sleeping panel, a .35 m sculpt, wake/contact, and an origin shift.
The first fixture accidentally used radial gravity; its failed raw output is
retained separately from the corrected uniform-gravity control.

Detailed structural fracture now updates only skin weights, visible indices and
primitive ranges. It retains the renderer mesh handle, rest vertices and material
resources. Ordinary motion still updates only the physical-section palette.
Per-instance presentation drops embedded image copies after initial upload; the
shared immutable source retains them. Actual NVIDIA pixel controls cover changed
weights, transactional rejection, complete removal, restoration and stale handles:
**28/28 renderer submission checks**.

A separate physical negative control reproduced permanent material joints failing
to sleep because the generic rigid-weld check required their original anchor pose.
Finite material connections now use actual island motion/support instead; the
existing speed thresholds and normal force/impulse/configuration wake semantics
are preserved. Both orientations now sleep at their plastic equilibrium and
wake for the next severe impact: **29/29 section-material checks**.

The current real-car native cadence has completed 900-step frontal tests with one
and four Challengers, each retaining permanent panel deformation and releasing
all three mounts. Four-car measurement: fixed-step p50 **10.162 ms**, p95
**13.734 ms**, p99 **23.583 ms**, worst **165.430 ms**, peak RSS **642980 KiB**.
This preceded the plastic-equilibrium sleep correction and final render-topology
benchmark, and is not the final performance claim. It preserves physical solver
resolution; there are still significant severe-impact spikes to report.

The panel baker now accepts an optional source-part key after its optional source
asset ID. An empty asset ID is allowed for local, self-contained testing. Four
actual Ural regions (hood, roof and both doors) cook using the normal panel path,
with 36 physical nodes each and retained detailed geometry. All downloaded models
and derivatives remain outside Git and public packaging. This cook evidence is
not a claim that the Ural has a complete authored/drivable crash rig.

## Normal editor/game authoring checkpoint

[The authoring guide](DEFORMABLES_V2_AUTHORING.md) describes the native frame/panel
workflow, source-local mount convention, material presets, live coarse mesh/mount/
proxy overlay and explicit support envelope. The fresh-object Inspector workflow
passes **74/74**, including the 24-section/1500 kg preset, detailed source binding,
panel fitting, undo/redo and existing-destination rejection. Named material/part
intent now resolves against the physical exterior's own primitives without loading
a duplicate undeformed model. Native deformation checks pass **144/144**.

The private crash authoring tool's `--author-project` writes a real `.judasproj`,
ordinary scene, normalized wheel/caliper derivatives, project-owned JudasJS input/
camera/save code and runtime UI into a fresh caller-owned directory. It refuses
an existing project. Normal Editor/SDK gameplay requires no diagnostic executable
or manual material update loop. A desktop application run exercised steering,
driving, braking and ordinary SaveService save/load with no script failures; its
ground, barriers and linear lighting are normal authored scene content. The
separate diagnostic tool retains controlled-impact/orbit keys for measurement.
No third-party model data is present in the committed authoring implementation.

Actual portable **OpenGL 3.3** NVIDIA submission checks also pass **28/28**, in
addition to the modern **OpenGL 4.6** run. These are desktop portable-path controls,
not native Android/browser or Windows execution. The existing Windows VM was
inspected but left shut down: its 6 GiB guest allocation exceeds the approximately
5 GiB currently available on this 8 GiB host. No VM configuration was changed.
Final SDK, sustained integration and human handover qualification remain in progress.

## Native prefab and region-residency qualification

The material suite now passes **152/152** checks. New original procedural fixtures
instantiate the complete structural-carrier/panel hierarchy twice, verify each
panel resolves its own MK2 carrier, then really unload and reload a streamed region.
Reloading preserves node positions, velocities, every hinge's plastic rest/work,
and release state while remapping retained attachment targets to fresh carrier IDs.
The released panel continues independently after restoration. These exercise the
current native lifecycle, rather than keeping private vehicle assets in tests.

The matching `01c70969` local SDK passed all 15 current tool probes and the standard
project/editor/import/cook/export workflows with the development checkout hidden
and the SDK read-only. A real private crash project also exported and cold-loaded
from a relocated standalone package with neither SDK nor development tree visible.
Persistent user data was explicitly retained between those isolated processes.
This is host-Linux qualification (glibc 2.43); the completed CI for `01c70969`
separately passed both Alpha/Experimental gates and the Ubuntu 24.04 SDK checks.
The matching Windows MinGW build and PE dependency/provenance checks passed;
native Windows execution, Android and browser execution are not claimed here.

## Material-review follow-up

The intact-model review exposed diagnostic-tool paint/opacity overrides and an
inverted light direction. The tool now preserves imported materials and uses the
authored lighting contract. Legacy FBX glass and opaque body material modes are
explicitly authored in private derivatives using existing material capabilities.
No generic renderer, importer or physics behaviour is changed. The final
qualification report records updated render timings and a matched, cold-restored
physical-state A/B; original and rejected visual evidence is preserved.
