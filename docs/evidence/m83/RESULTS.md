# M83 executed results — coupled mechanical damage

**Experimental candidate; human acceptance pending.** Baseline:
`7bfa1ac3f11221ecdc450a5778023f8aff380c52`. Linux is the automated acceptance
platform. Windows is informational and nonblocking; no Windows M83 pass is
claimed. This is a scoped milestone result, not a new full-engine certification.

## What was established

The existing M80 material/body constraints move real finite carriers. Material
plasticity therefore changes actual wheel-bearing geometry, suspension contact
and mechanical load. M83 adds optional force/moment limits measured from the
signed impulses that those constraints actually apply. A successful completed
material step publishes irreversible overload release without an extra impulse
or prescribed damaged pose.

An ordinary M82 transmission link may depend on a named attachment and an
optional attachment-to-root material path. Released mounts, removed patches or
loss of the last M79 cohesive path disconnect the selected link. Driver commands
remain enabled; a surviving alternate material path continues transmitting.
Detached carriers retain ordinary collision, mass, gravity and inherited motion.
No project script sets failure flags, bent transforms or fake loss-of-drive state.

See [implementation and limits](../../M83_MECHANICAL_DAMAGE.md),
[attachment numerical evidence](ATTACHMENT_NUMERICS.md),
[lab controls](../../../projects/damage_lab/README.md) and
[reproduction](REPRODUCE.md).

## Focused Linux receipts

The [sequential cases](linux/final-focused-cases.json) record 21 passing focused
suites: mechanical damage/continuity/JS, structural fracture, bending, wheels
and their lifetime, powertrains and their lifetime/JS, deformables, fracture,
collision geometry, rigid physics, collisions/casts, joints/contact lifetime,
physics continuation, save storage and streaming. The full production suite was
not rerun.

| Evidence | Observed result |
|---|---|
| [Material attachments](linux/final-mechanical_damage.txt) | 33 checks pass: signed force/moment, actual release boundary, sustained/impact load, strong/unlimited controls, arbitrary orientation, translated coordinates, offset COM, malformed state and failed-step release suppression. |
| [Mechanical continuity](linux/final-mechanical_connection.txt) | 37 checks pass: live targets, actual rooted components/alternate paths, destroy/reuse, asynchronous publication, qualified foreign-material references and invalid dependencies. |
| [Actual QuickJS](linux/final-mechanical_damage_script.txt) | 18 checks pass: authored strengths, real load and disconnected drive, invalid requests/phases, copied observations, stale handles and executed public cookbook. |
| [API/type/cookbook check](linux/api-final.txt) | 364 public symbols, 32 exports, 42 typed examples, three deliberate drift negatives and actual VM enumeration pass. |
| [Native application write](linux/application-write.txt) | 19 checks pass; sustained deformation, physical carrier rotation/contact, real drive before failure, real ram impact, stronger control, cohesive disconnection and save transaction. |
| [Fresh-process restoration](linux/application-read.txt) | 10 checks pass; exact material and powertrain participant bytes before resumed simulation, durable release/disconnection and a new physical impulse after restore. |
| [Prefab/retained streaming/reload](linux/application-lifecycle.txt) | 15 checks pass; actual endpoint damage, suspension/revisit, remapped physical references, unchanged retained material/drive data, destruction and reload. |
| [Oblique application](linux/application-oblique.txt) | 19 checks pass with rotated physical content and actual oblique gravity, including the same deformation, drive and failure checks. |

The late `glm::vec3 v{0}` initialization corrects an invalid-offset option
short-circuit in the existing JS error path. Invalid target/kind plus offset
negative controls remain part of the 18-check fixture. The earlier successful
receipts are retained in `development/pre-invalid-option-final/`; affected narrow
checks and application/package runs refresh the final receipts. This is not a
physics-quality, controller or acceptance-threshold change.

The Release build receipts retain two pre-existing warnings in untouched
RuntimeWorld code from the first compile. Late affected builds contain no
warnings. This does not claim unrelated warning cleanup.

## Measured physical endpoints

In the flat application, the same 60 N bearing load produces **0.062519 m**
permanent rest-shape change in orange and zero in the nonplastic green control.
The orange carrier moves **0.300917 m** and rotates (axis alignment **0.998050**).
The wheel remains in contact. Before failure, actual source speed is
**22.3229 rad/s**, wheel speed **5.58084 rad/s**, and transmitted torque
**24.0423 N m**. Overload releases the finite mount; actual link torque then
becomes zero while its command remains enabled. The separate cohesive bench
loses an actual M79 material path and also disconnects.

These values are snapshots from the recorded application trajectory, not a
continuum-calibrated suspension specification. The oblique result has comparable
plastic rest change (**0.062480 m**) and carrier movement (**0.300715 m**);
bitwise rotational equivalence is not claimed.

### Current-boundary positive/negative control

The collision-geometry proof places a **0.05 kg, 0.05 m-radius** sphere partly
inside the current bent midspan (25 mm initial penetration), alongside an
identical filtered-out sphere. The chosen boundary is displaced **0.186028 m**
from its earlier state. Sixteen material contacts are solved; the excluded sphere
receives ordinary gravity only (**0.1635 m/s**), while penetration recovery ejects
the positive probe at approximately **114.7 m/s**. Probes are then destroyed.

This deliberately adversarial penetration test establishes current physical
boundary collision and filtering. **It does not certify realistic impact energy
or high-speed soft-contact fidelity.** A separate ordinary **12 kg moving ram**
is launched by project JS and hits the finite carrier through solved rigid-body
contacts; its real contact impulse is checked independently.

## Application performance

Host and measurement scope: [environment](linux/ENVIRONMENT.json), Linux Release
on an i7-4790, native SDL/OpenGL, dummy audio, VSync disabled, synthetic 1/60 s
frame progression. Times below are measured fixed-step and render-loop work;
they are not physical-display refresh or Windows performance certification.

Initial lab workload: **17 bodies / 13 dynamic**, **11 wheels**, **5 powertrains**,
**11 material instances / 232 nodes / 264 tetrahedra**, material **4 substeps ×
4 iterations**, authoritative **60 Hz**. The lifecycle run also spawns a complete
buggy and loads the annex; its workload is larger during those phases.

| Native receipt | Samples | Fixed median / p95 / maximum, ms | Frame work median / p95, ms |
|---|---:|---:|---:|
| Flat write/physical sequence | 633 | 6.096 / 8.217 / 12.632 | 7.657 / 10.228 |
| Oblique physical sequence | 630 | 6.186 / 8.207 / 10.597 | 7.625 / 10.193 |
| Prefab/retained-region lifecycle | 195 | 8.971 / 11.156 / 19.716 | 10.549 / 13.347 |
| Moved-package write | 633 | 5.950 / 7.547 / 13.195 | 7.331 / 9.413 |

Ordinary p95 fixed work is below one 60 Hz interval on this host. The recorded
lifecycle maximum is reported rather than hidden by averages. Initial probes
with simultaneous compilation/software rendering are preserved as development
diagnostics and are not used as final performance evidence. Earlier successful
timings remain in `development/pre-invalid-option-final/`; the table above uses
the refreshed final receipts.

## Editor and package

[Editor automation](linux/editor.txt) opened a scratch copy, exercised duplicate/
undo, Play/Stop and save. Authored scene bytes after Play/Stop were identical;
the checked-in project was not overwritten by this authoring smoke. Screenshots
show the editor and played lab. Automation does not
replace human authoring review.

[Export](linux/export.txt) includes 13 registered assets and three scenes.
[Package cases](linux/package-cases.json) pass a normal standalone watch sequence,
pause/resume, oblique transition and reload from an unrelated `/tmp` working
directory with a package path containing spaces. Moved-package application
write/read/lifecycle pass **19/10/15** checks. Authored payload/move comparison is
recorded separately in [package fingerprints](linux/PACKAGE_FINGERPRINTS.json).

## Preserved failures and corrections

- [First numerical attachment failures](ATTACHMENT_NUMERICS.md): an incorrectly
  centred pure-couple fixture, absolute-point float quantization, and authored
  pivot versus actual COM. The translated bound and physical controls were
  retained. An independently rebuilt old-COM negative fails the offset-body
  comparison while the corrected mechanism passes.
- First public-script build/prerequisite and type diagnostics remain in their
  original logs; corrected narrow follow-ups are retained.
- [Over-soft lab member](development/CONTENT_STIFFNESS_FOLLOWUP.md): the initial
  M80-style 0.0005 shear compliance was unsuitable for the slim section and
  released its mount during the intended normal-load phase. Exact failed content
  and result are preserved; authored compliance became 0.00005. Loads, strengths,
  solver cadence/budgets and check thresholds were unchanged.
- [Annex authoring](development/MANIFEST_FOLLOWUP.md): a zero Z half extent was
  correctly rejected by the existing parser. Current content uses positive
  bounds covering the specimen. The failed startup log and exact manifest remain.
- The first missing `player-start.yaw` scene-format failure remains in
  [its original receipt](linux/application-probe-first.txt); content was corrected.

## Identity and acceptance boundaries

[Validated inputs](VALIDATED_INPUTS_SHA256.txt),
[final source](FINAL_SOURCE_SHA256.txt),
[final binaries](FINAL_BINARIES_SHA256.txt) and
[package comparison](linux/PACKAGE_FINGERPRINTS.json) carry identity evidence.
Final refreshed hash verification and the Experimental checkpoint are supplied
separately by the integrating agent; this document does not substitute for those
checks or invent a commit hash.

No human acceptance, main promotion, native Windows pass or full-production-suite
pass is claimed here. [Windows status](WINDOWS.md) records that the existing VM
was shut off. Existing games, protected historical evidence, accepted fluid work,
independent research and unrelated deferred defects remain outside this change.

## Limits and human checklist

The model uses approximate bounded XPBD/rigid splitting, authored small material
patches and instantaneous isotropic connection capacities. It has no automatic
assembly discovery, fatigue law, industrial crash FEM, exact soft CCD, pneumatic
tyres or automatic repair. Failed material steps retain existing partial-solve
stop semantics; pending irreversible release is suppressed, but applied impulses
are not rolled back transactionally. Unavailable physical dependencies fail
closed and resume from actual publication; old saves do not acquire invented
historical strength data. The flat annex is a retained specimen, not a separately
rotated third scene.

1. Use B then unload: inspect permanent orange carrier/wheel misalignment and
   compare the stronger green material under the same load.
2. Power the intact mechanism with W/S; then use I/H and verify real detachments
   stop drive while released carriers remain physical.
3. Observe the purple path fracture and compare the stronger control.
4. Drive both four-wheel assemblies; inspect suspension, steering and post-damage
   traction. V supplies the finite watch tour without prescribing damaged poses.
5. Repeat in scene 2 under actual oblique gravity.
6. Save/load damaged geometry and disconnected mechanics; request F3, select the
   annex with Tab, damage it, release and revisit it. Spawn an independent prefab.
7. Edit strengths/materials in the editor; Play/Stop, reload and test the exported
   standalone. Human visual/mechanical acceptance remains outstanding.
