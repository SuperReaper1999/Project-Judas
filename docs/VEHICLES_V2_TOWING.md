# Vehicles V2 — connected towing turn-around correction

## Diagnosis and correction

Human review found the tractor turning around while the hitch remained connected,
particularly when S was used while turning. This is not an attachment-separation
failure. The original attachment-only test did not cover it.

A controlled reproduction loads the actual Workshop chassis, masses, wheels,
powertrain and hitch, isolates the ground, and removes presentation. The old
900 kg trailer had fixed axles at ±0.95 m: a car wheelbase without a steerable
front bogie. Its substantial turning scrub and drawbar reaction folded the lighter
700 kg tractor around the still-connected hitch during coasting. The driver also
sent S directly as reverse motor torque, without service braking the trailer.

The correction is authoring and project-owned driving policy:

- Rear tandem trailer wheel mounts at Z=0.175/0.825 m; same mass, tread, tyres,
  suspension, physical law and ball joint. No rotation lock or artificial damping.
- Fresh editor trailer presets use a freely following single axle.
- Opposite pedal requests service braking until released. Release and re-press
  selects reverse. Holding S after a forward stop does not start pushing backwards.
- Brake effort acts on both bodies, sized for each mass: 721.035 and 927.045 N m
  per wheel. Neutral remains genuine coasting.
- The pedal latch is normal serializable game-script state. Native drive/brake
  torque APIs are unchanged; games remain free to implement other controls.

MK2, WheelSolver, powertrain laws and hitch constraints are unchanged. A metric
friction-disk prototype did not fix this failure and was rejected/reverted; its patch
and the original result are preserved. This is not a claimed general anti-jackknife
system: prolonged sharp reversing, insufficient grip or deliberately unbalanced
braking can still physically fold an articulated rig.

## Measured controls

The original wide-axle layout is retained as an executing negative control.
Sixty seconds of coasting/turning gives maximum relative articulation **137.42°**
for that layout, versus **15.79°** for the corrected tandem. Global heading is
not the gate: a properly attached rig must still turn through a corner normally.

| Service-brake and reverse control | Flat | +5.73° slope | −5.73° slope |
|---|---:|---:|---:|
| Maximum stopping articulation | 25.30° | 11.46° | 49.26° |
| Maximum speed during held stop | 0 m/s | 0.01967 m/s | 0.01333 m/s |
| Yaw drift during held stop | 0° | 0.180° | 0.041° |
| Deliberate reverse travel | 0.247 m | 1.490 m | 0.422 m |
| Maximum hitch error | 0.855 mm | 0.857 mm | 0.784 mm |
| Maximum compressive hitch force | 2142 N | 1988 N | 1538 N |

The slope controls permit real articulation, using positive following-direction
bounds (stopping <60°, short deliberate reversing <90°), not a straight-rig
expectation. Their velocity, yaw drift and hitch gates are separate. The initial
0.8-radian estimate proved too restrictive for a legitimate downhill turn; both
failed attempts and the reason for the final declared bounds are retained.

The original 81 dynamics checks plus 16 towing checks pass: **97 checks**.
This includes original wheel-order, sleep/wake, passive energy, terrain excavation,
origin-shift and cold-save controls. Actual QuickJS tests: **20 checks**, including
executing the exact new driver, brake/reverse symmetry and serialized pedal latch.
Fresh editor authoring: **16 checks**, including the single-axle trailer preset.
Repeated drive/brake/reverse cycles and an unbraked pushing-trailer control pass.

The real Workshop scripts also ran in separate writer/reader processes. Saving
while S was held and loading while holding S retained the brake latch, stopped
chassis and free hitch. The test uses an isolated writable user-data directory;
operator saves are untouched. Synthetic input is automated qualification, not a
second human acceptance claim.

## Performance

Three alternating old/new pairs, CPU 2 pinned, use 3,300 measured fixed steps per
run after 300 warm-up steps. Milliseconds, median of run percentiles, largest worst:

| Whole two-vehicle physics step | p50 | p95 | p99 | Worst |
|---|---:|---:|---:|---:|
| Original folding trailer | 0.1639 | 0.3902 | 0.4063 | 0.5486 |
| Corrected coasting trailer | 0.3594 | 0.3679 | 0.3745 | 0.5778 |

The old rig folds, stops and sleeps much earlier; the corrected rig remains moving.
Therefore the increased full-run median is not an equal-active-work comparison,
and the small tail difference is not claimed as a speedup. These are CPU simulation
costs, excluding rendering and JS. No new native per-wheel algorithm is shipped.

## Exact checkpoint and CI scope

Correction implementation: `6274ecdb651a955de39a092de2a33672c229a1bf`.
Test-path correction: `b9c5223ed7cbe69e5d5b33769f6b5d6476769f9e`.
The [required engine qualification](https://github.com/SuperReaper1999/Project-Judas/actions/runs/42714296052)
is tied to the latter exact commit; later documentation-only changes do not alter
its engine/project/test code. The run completed successfully: 178 Experimental cases and 174 Alpha cases,
with four correctly unavailable locked-fluid cases; both Ubuntu 24.04 SDK
qualifications passed. [Compact CI receipt](https://github.com/SuperReaper1999/Project-Judas/blob/Experimental/docs/evidence/vehicles-v2/towing/ci-final.json).

The first CI attempt at the correction ran 177/178 Experimental cases successfully;
the new towing test then failed to find its scene from the validator's isolated
working directory. The old failure is retained, not suppressed. The fixture now
uses the existing `ResolveEngineDataPath`; both an invocation from `/tmp` and the
actual registered validator pass with all assertions unchanged. The first run's
remaining Alpha job was cancelled by the normal newer-commit concurrency policy.

## Additional SDK/export checks and precision limitation

The clean local SDK was rebuilt at the towing checkpoint and passed its 15-tool,
39-command, eight-stage project/editor/export workflow with the development checkout
hidden and SDK read-only. A separate relocated Workshop export ran writer/reader
processes with the SDK, source project and engine checkout hidden. Holding S after
load retained braking and the rig returned to rest; its runtime hash matches the SDK.
An actual GTX 970 OpenGL 4.6 run also displayed the corrected, sleeping tow rig.
No Windows/Android/browser execution is claimed for this correction.

**Known precision limitation:** that additional relocated articulated-rest probe
settled 2.16 mm from the uninterrupted pose, so its extra-strict 1 mm trajectory
comparison did **not** pass. This is recorded separately from successful restoration
and brake-state checks; it is not relabelled as a pass. The unchanged wheel restore
path invalidates derived contacts, so the first support query wakes/re-equilibrates
a saved sleeping rig. The independent game-copy cold comparison above was 0.033 mm;
results are fixture-dependent. All inherited save/trajectory regression expectations
remain unchanged. No broad persistence change is bundled into the towing correction.

## Evidence and handover

[Raw checks, timings, scope and rejected experiment](https://github.com/SuperReaper1999/Project-Judas/tree/42db53a1fb7831aace7b887965457fa79c0024b9/docs/evidence/vehicles-v2/towing/).
Live logs and screenshots remain in `.cache/vehicles-v2/towing`.

Open `projects/vehicle_v2_workshop/Vehicle_V2_Workshop.judasproj`, Play, press 4.
W accelerates, A/D steer, S during forward travel brakes, Space brakes explicitly.
Release and press S again to reverse. K/L save/load. The original human acceptance
is supplemented by the operator's explicit correction-and-qualification authority;
main promotion and release publication remain separately unauthorized.

Platform scope: Linux native, actual JudasJS/SaveService and available automated
editor/export checks. No new Windows, Android or browser execution is implied.
The previously qualified native wheel/MK2 implementations remain unchanged.
