# M83 wall-crash demonstration follow-up

**Experimental candidate; human review pending.** Original checkpoint:
`5799683b272f85e27d9e38c1d03ded456697c416`. Accepted main remains
`7bfa1ac3f11221ecdc450a5778023f8aff380c52`. This follow-up also contains the
independently established [finite-triangle response correction](../m83-collision-followup/INVESTIGATION.md).

## Original human failure and real scope

The operator did not see cars drive into walls and break in the original tour.
Inspection confirmed there was no crash wall: the tour loaded bearings and
launched separate rams. That tested real mechanisms but was an unclear vehicle
crash demonstration. `HUMAN_FAILURE.md` preserves the feedback. The original 117
M83 evidence files and seven original scene/cook/prefab payloads remain exact.

The new ordinary `Scenes/crash.judas` contains two recognizable four-wheel cars,
solid walls, normal powertrains, dynamic bearing bodies and real tetrahedral
mounts. It is the project's startup scene; 2/3 open the original flat/oblique
fixtures. Project JavaScript supplies driver commands and presentation. The
watch tour never sets damaged poses, broken flags or hidden launch velocities.
After observed impact/failure it releases throttle and applies ordinary brakes
so the result can be inspected. The end-to-end physical test holds the same full
logical drive input throughout, independently proving damage under held commands.

## Construction investigation — failures retained

The initial crash content was not accepted merely because wheel APIs existed.
The unchanged proof requires >8 m driven travel, >2 m/s forward approach,
actual solved wall contacts, failure at/after contact, healthy material, actual
independent detached-body motion and physical drive loss with commands held.

| Control | Measured result | Interpretation |
|---|---|---|
| Original long soft arms, 4 material passes | ~2.53 m travel, no wall; chassis floor contact ~17.29 N s/step | Car sagged onto ground; tyres could not propel it normally |
| Stiffer original arms, 4 passes | ~3.5 m; significant pin drift/plastic rest change | Compliance alone insufficient |
| Original arms, 12 passes, 1,500 N capacity | Rest fails at 1,848.85 N during authored initial drop | Capacity below actual initial load; not an impact proof |
| Original arms, 12 passes, 2,500 N | Both flip before wall (~0.3 m); weak release at tick 116 | Small low-inertia bearing/long arm unsuitable for full axle reactions |
| Ordinary rigid wheel control, same gross mass/mounts/source | 17.644 m; 9.929 m/s approach; actual wall impulse ~248 N s; 8/8 pass | Normal wheel/powertrain path can drive a real car |
| Equivalent four arms combined into one material owner | Same early flip (~0.3 m) | Grouping/order alone does not fix construction |
| Original arms, 20 N m / 1,500 W source | Strong 5.59 m, 1.62 m/s; no wall; weak fails before wall | Simply reducing drive does not meet unchanged proof |
| Compact physical arms/bearings, 2,500 N weak capacity | Strong 17.03 m, 5.04 m/s, wall; weak fails on initial 3,232 N drop | Construction supports driving; weak capacity still below normal loading |
| Same compact construction, capacities 6,000 / 15,000 N | Both reach walls; only weaker mount fails after actual contact | Final meaningful strength comparison |

The compact cook is a new distinct asset: 0.7 × 0.5 × 0.6 m material, same
4 × 1 × 1 topology, density 80 kg/m³, 16.8 kg/member. Real bearing boxes are
half dimensions (0.20, 0.28, 0.32) m at unchanged 8 kg, raising Ixx from
0.181333 to 0.482133 kg m². Complete car mass is 219.2 kg. Both cars use identical
80 N m / 6 kW sources, 0.25 links, tyre settings, material/plasticity and geometry;
only finite mount capacities differ. Four material substeps / 60 Hz remain;
12 iterations are authored for this heavier construction, not a global budget
or scene-specific engine branch. The original lower-quality fixtures are preserved.

Measured original carrier reactions (~80 N m aggregate per carrier with
Ixx ~0.1813) predict ~7.3 rad/s change over a 1/60 s interval; the trace measures
~6.9–7.3 rad/s and growing pin drift before launch. No new wheel/powertrain defect
was established. The existing partitioned material/rigid model is approximate;
this construction stays within its measured useful envelope. It is not a promise
of stable arbitrary extremely compliant drivetrains or industrial crash FEM.

Development snapshots, per-step reaction/wheel/motion traces, controls and
instrumentation build failures are retained. One diagnostic build initially
attempted a private field and failed; a public initialized-state capture replaced
it. `drive-attachment-trace.txt` used the preceding executable after that build
failure and is not credited as new diagnostic proof. A first export smoke had an
incorrect relative harness creation path from /tmp; its missing-file failure is
retained separately and corrected with absolute paths. No fixture was weakened.

## Final source physical endpoints

`linux/final-drive.txt`, `final-rest.txt`, and `final-crash-cases.json` record:

- **Drive: 16 checks / 0 failures. Rest negative: 10 / 0.**
- Both travel 17.033 m and approach at 5.038 m/s. First actual wall contact: tick 362.
- Exact pre-wall patch peaks: 4,102.71 N orange / 4,102.55 N green.
- Post-wall peaks: 7,582.93 / 7,592.08 N. The 6,000 N capacity lies above normal
  driving demand and below impact demand; the 15,000 N control remains intact.
- Orange releases one mount at tick 386, 24 fixed steps after first wall contact.
  Carrier has real independent relative motion of 4.426 m. Its selected drive row
  opens while held driver enable remains true. Green remains connected/healthy.
- Three rendered witnesses cover actual approach, observed contact/failure and
  the stronger final comparison. No-drive bodies remain intact with no wall contact.
- Workload: 13 bodies / 10 dynamic, 8 wheels, 2 powertrains, 8 materials,
  160 material nodes / 192 tets, 60 Hz.

## Performance

Native Linux desktop graphics, Release, synthetic 1/60 s frames, VSync disabled.
These are measured uncapped work costs, not hardware-independent FPS guarantees.
Screenshot readback/encoding is excluded from frame-work samples.

| Final fixture | Fixed median / p95 / max | Frame work median / p95 / max |
|---|---|---|
| Rest | 9.827 / 12.795 / 23.815 ms | 12.627 / 16.961 / 28.058 ms |
| Drive/crash | 10.457 / 13.882 / 21.667 ms | 13.459 / 17.756 / 27.226 ms |

Drive median frame work corresponds to about 74 uncapped frames/s; p95 work to
about 56. Both construction and fidelity differ from the original five-rig lab,
so these are not advertised as an optimization before/after comparison.

## Combined focused validation

- Corrected guard: 1,303 collision-geometry checks; original impact robustness
  4,412 checks, deepest corner 0.0179729 m within unchanged 0.03 m threshold.
- Sixteen affected physics/mechanical regressions pass; exact binary receipts
  are in `../m83-collision-followup/final-regressions.json`.
- Original native M83 application write 19, fresh read 10, lifecycle 15 and
  oblique 19 checks pass against corrected guard/shared scripts. The fixture now
  explicitly opens its original flat scene instead of following changed startup.
  Original geometry, controller inputs, timestep and thresholds are unchanged.
- Editor copied-project duplicate/undo, Play/Stop, authored scene identity and
  save pass; zero failed resource/job reports. Actual project is not edited by automation.
- Normal M38 export includes 15 assets / 4 scenes. Moved package launches from
  unrelated /tmp, exercises autopilot, screenshots, save/load, pause/resume,
  all three scene selections and reload. Final package receipt/fingerprints are separate
  from the earlier pre-braking presentation package.
- No public JS API/binding/type changes, no full production-suite repetition,
  no deformable integration, solver cadence or accepted fluid/research changes.
- Windows is unperformed for this follow-up, informational/nonblocking; no pass inferred.

## Human review and limitations

1. Launch the default crash lane and press V (or use the supplied autopilot launch).
2. Watch both real cars drive into solid walls; orange should lose a mount/wheel
   after contact, with independent motion and a drive-path count change.
3. Compare green: stronger mounts preserve connections. Collision can still tip
   or roll either vehicle; strength is not an upright-pose guarantee.
4. R resets; try W/S, A/D and Space. Change a mount capacity in the editor and
   observe a different physical outcome rather than a scripted animation.
5. F5/F8 preserve damaged physical state; pause/reload and scene selection work.
6. Press 2/3 for plastic/elastic/cohesive and genuinely oblique fixtures; test a
   spawned prefab and retained annex. Repeat representative checks in standalone.

Finite patch resultants remain instantaneous capacities, not fatigue/calibrated
metal failure. Query tyres are not solid tyre CCD; material contact is approximate
and out-of-envelope solves retain their documented stop semantics. The contact
correction preserves deep protection but does not close existing fast-box mesh
CCD limitations. Protected history and existing games remain unchanged.

Main promotion and human M83 acceptance remain pending. No milestone tag.
