# M87 slow live scene: authored-content and preserved-core classification

Read-only investigation of the exact original human-live failure. No engine source, scene, script, solver setting, build, or simulation was changed by this investigation. The frozen core is inspected as data; its executable is never run.

## Authored configuration

The original source commit is `cee55a898aa0e4938665903f3b207d828acdb1df`. The generator, current 1024-body scene, and controller still match that commit byte for byte; see `source-layout-results.json` for exact hashes.

The 1024-body room contains 1285 physical bodies: one floor, 256 cabinet bodies (32 sets of four static and four dynamic bodies), four pit walls, and 1024 loose pieces. Slots are floor 0, cabinets 1–256, pit 257–260, loose 261–1284, matching authored construction order and the core's positions/masses/radii.

Loose pieces are centered on a 16×16×4 grid. They are 0.60 m across, with 0.74 m horizontal and 0.68 m vertical spacing: initial gaps 0.14 m and 0.08 m. Bottom-layer centers are 0.34 m above a floor at 0, leaving 0.04 m clearance. All have mass 2 kg, friction 0.65 and restitution 0.025. One in three is a sphere; the remainder are boxes. Thus the aligned towers are intentionally unstable mixed-shape stacks, rather than an initially intersecting solid block.

The exhaustive source-only AABB scan found **zero positive-volume initial overlaps involving any loose piece** against any other authored body. Since all authored primitive orientations are identity, this independently excludes initial loose interpenetration.

The nearest loose extent starts at z 9.70 m. The furthest cabinet front extent ends at z −7.70 m: a 17.40 m separation, with the pit's front wall between them. The moving shelf's 0.80 m half-length plus 0.26 m stroke stays within 1.15 m rear-wall clearance. Shelf side half-width 1.12 m stays inside 1.23 m wall clearance. The reviewed powered-wall-overlap correction is present.

## What the original frozen failure proves

At the captured unfinished step, 396 impact events and 314709 TOI queries have occurred by simulation time 0.003821390017670875 s of a 0.01666666753590107 s step. There are **zero spinning pairs**. Maximum physical angular speed is 7.3005 rad/s; this is not a high-spin penetration-target avalanche.

Directly inspecting the original `impactCounts` vector gives:

| Body group | Summed impact-island inclusions | Participating bodies | Maximum per body |
|---|---:|---:|---:|
| Floor |0|0|0|
| Cabinets |0|0|0|
| Pit walls |0|0|0|
| Loose pieces |17192|931|69|

Static boundaries are never included as dynamic island members, so their zero counts do not exclude floor contacts. The cabinet group contains 96 articulated bodies and 32 free balls; **none participated in this avalanche**. All 25 preserved next event pairs are loose-versus-loose. Every one of the 20 highest-inclusion bodies is an ordinary 2 kg loose piece.

For example, slot 1168 is authored entity 5907, a box initially at (2.59,2.38,15.92)m. It has 69 island inclusions while moving at 0.02487 m/s. Slot 1152 is authored entity 5891, a sphere with 67 inclusions at 0.02039 m/s. These are repeatedly coupled low-speed pile contacts, not powered machinery jamming the pile or a runaway high-energy launch.

## Chronology

The loose bodies move immediately under gravity: maximum speed 0.1635 m/s after step 1 and 0.4905 m/s after step 3. Bottom-layer contact around step 5 produces 256 events. The towers then nearly rest; steps 20–60 have no impact events, although all bodies remain awake. Around step 91 renewed contact events appear as the unstable mixed towers tilt. The original fixed-input run reaches 232 events at step 155, with 703.234 ms fixed-step time. The original human-live core captures a later, more severe unfinished cascade.

The motor tour drives shelves during the first 8 simulated seconds, then turns power off through 20 seconds, then applies one physical impulse. It changes no loose-piece velocities/transforms. Severe fixed-step slowdown stretches the first 8 simulated seconds over much longer wall time; the operator can perceive a stuck tour while it is still computing that phase.

## Reproduction of this classification

From any directory:

```
python3 /tmp/m87-pile-classification/scan.py
gdb -q -batch -nx /tmp/judas-m87-watch-ecpbqnfz/judas /tmp/judas-m87-live-slow.core -x /tmp/m87-pile-classification/core-groups.gdb
```

`original-core-provenance.json` retains the already-recorded exact SHA-256 identities for the original executable,167591936-byte core and original debug-type object. These inputs remain at their original paths; they are not copied or replaced here. The source-path hash inside that historical provenance describes the original capture, not today's dirty corrective source.

## Conclusion and limits

This is a genuine dense-pile event-processing scalability failure with sensible, initially separated content. The source/core evidence does not justify blaming cabinet overlap, scripted impulses, reduced physical quality or a global scene-name special case. It does not by itself identify which numerical/algorithmic mechanism produces the repeated first-touch events; that requires the parent's separate measured solver investigation.
