# M71 focused evidence

The current operator-requested editor/documentation follow-up is indexed in
[editor-polish](editor-polish/README.md). The original final-review fingerprints
remain a preserved pre-polish snapshot; the follow-up identifies current files.
The later human wheel/selection findings and current corrected candidate are
indexed in [editor-picking-zoom](editor-picking-zoom/README.md). Earlier receipts
remain intact snapshots.

Focused execution records accompany the uncommitted M71 candidate based on accepted `fb31f0244c9f41f74c439afd99855e3707937edd`. This folder does not supersede retained M70 evidence or human acceptance. Development failures and first-pass logs are retained alongside the final results.

| Phase | Owner and visible sample |
| --- | --- |
| `RuntimeWorld::FixedScripts` | Public JS issues complete validated intent; last valid write replaces earlier intent. Actual state remains the last resolved physics sample. |
| M70 motor/reference prephase | Opted-in M70 motors use the preceding resolved body interval under the accepted scheduler, then animation references and partial boundary anchors are prepared. |
| `PhysicsWorld::Step` publication | Native prescribed intent is published for this fixed interval; forces integrate only dynamic authority. |
| Contact/CCD solve | Existing broadphase, contact and impact machinery sample the prescribed path and apply impulses to dynamic neighbours. Prescribed pose remains externally owned. |
| Post-physics motor/support | Ordinary motors probe the start sample, carry the actual previous-to-current support transform once, then resolve their movement relative to the support. |
| State/presentation/events | Body state is synchronized, final animation is resolved, and existing pair events are dispatched. Presentation reads fixed samples. |

Public commands are visible in `projects/kinematic_lab/Assets/scripts/lab.js`. Velocity intent continues until replaced, stopped, placed, removed or changed in authority. A bounded target completes over its declared interval, then holds. Explicit transform writes remain placement. Rotation targets choose the short quaternion arc; velocity control supplies continuous spins. Linear readback is COM velocity, so authored pivots on off-centre shapes need `pointVelocity` to obtain their own material-point velocity.

The scripted path remains prescribed against static/kinematic obstruction and trapped objects. It supplies external work, so closed-system momentum/energy conservation is not an appropriate acceptance claim. Supported moving shapes are sphere, box, convex and compound; query-only capsules are rejected, and concave triangle meshes and radial terrain remain static.

The focused native executable covers translation crossing a 0.3 m sphere in one 1/60 s interval, a full-turn rotating strip, sleeping/wake, filters, atomic invalid requests, duration/replacement/stop, authority identity, removal/slot reuse, teleport separation, equivalent quaternion signs, sub-ULP rotations, COM offsets, normal motion-ledger sampling, dynamic friction riders and motor translation/lift/rotation/walking/jump/dismount. A rotated frame repeats the essential motor case. Static controls, independently computed point velocities, carry and release expectations distinguish these behaviours from endpoint placement.

Native execution traces retain the measured outcomes: the commanded pusher produces one contact/impact and a dynamic-neighbour x velocity of 239.999985 m/s; stationary and filtered controls produce no movement or impulse. The full-turn strip produces a dynamic response even though its final attitude matches its starting attitude. The friction rider drifts -0.012023 m over two seconds at 0.5 m/s. Motor carry/walk discrepancy is at most 0.000000359 m across the four exercised cases; material-point velocity error is at most 0.002667 m/s, release velocity error is zero and one free release interval differs by at most 0.000000059 m. These are values from these fixtures, with the stated tolerances below.

The VM executable runs actual public API calls and existing callback publication. It compares 1 versus 7 presentation pumps for identical fixed input, verifies independent prefab state/removal, conditional modern save continuation, an incomplete participant refusal, explicit legacy save limitations and Play/Stop reset. Its `--write`/`--read` modes retain a modern snapshot for fresh-process continuation without replaying movement.

| Focused execution | Checks | Failures | Retained log |
| --- | ---: | ---: | --- |
| Native physics and motors | 96 | 0 | `kinematic-final.log` |
| Public VM, cookbook and lifecycle after the retained-motion guard | 36 | 0 | `lab-after-stream-guard.log` |
| Actual streaming-coordinator v4 archive, adoption and revisit | 34 | 0 | `streaming-after-guard.log` |
| Separate-process snapshot write after the retained-motion guard | 4 | 0 | `cold-write-after-stream-guard.log` |
| Separate-process snapshot read and continuation after the retained-motion guard | 8 | 0 | `cold-read-after-stream-guard.log` |

The performance helper runs one bounded 14-body/four-motor workload twice in the same executable, with stationary and commanded cases. It retains all 360 samples, including first motion/impact, dynamic/kinematic authority changes and stop. It prints median/p95/max whole fixed cost, native physics cost and contact/CCD counts, with optional raw CSV. This is evidence for that workload and build.

Observed whole fixed cost from `performance.log` and all 720 rows in `fixed-cost.csv`:

| Workload | Samples | Median (ms) | p95 (ms) | Maximum (ms) |
| --- | ---: | ---: | ---: | ---: |
| Stationary | 360 | 0.117424 | 0.287661 | 8.326478 |
| Commanded | 360 | 0.282159 | 0.524624 | 0.749339 |

The commanded case's first impact is at step 60, with one impact event and two impact queries at that step. The run includes 8,820 contact points and three impact queries overall. The stationary maximum is retained; these wall-clock observations are one local run and do not isolate scheduler noise or imply a general performance ceiling.

`lab-generation.log` records successful creation through the ordinary author tool, with tracked public scripts, primitive geometry, independent motors, an ordinary prefab and a retained font licence. Generation establishes consumable authoring formats; it does not establish desktop handling.

Development corrections retain their original logs. `lab-final.log` contains the rerun fixture's existing-sidecar refusal; the helper now reuses identities after scanning. `cold-write.log` records invalid test save metadata, with the dependent `cold-read.log` failure; the successful follow-up supplies the canonical scene fingerprint. The original application trace in `application/lab.log` records an unwritable default save-data directory and an abrupt lift catch-up after dynamic-to-kinematic authority. The follow-up uses an isolated writable data root and the lab script now freezes its trajectory clock while stopped and replaces 0.35-second targets for smooth support catch-up. The deliberate J fast crossing keeps one fixed interval. `application/lab-followup.log` then exposes the ordinary outer save service's omitted conditional motion participant; the runtime correction extends that schema check.

The final ordinary rendered application run is `application/lab-rendered.log`, exit 0. It completes the public `saves.save` request by frame 310 and the public `saves.load` request by frame 350, captures the restored world at frame 375, reloads at frame 390 and removes the pusher at frame 430. The retained input trace includes stop/resume, reverse, spin, walking/jump, rotated rider selection, dynamic/kinematic authority, the fast crossing and removal. No script/save diagnostic or Mesa draw error appears in the final log. Screenshot inspection confirms visible ordinary floor, crates, supports and riders. These automated fixed-frame inputs and rendered snapshots establish execution through the normal application; they do not constitute desktop acceptance.

Screenshot review exposed a generic renderer startup defect: the `uBones` buffer sampler retained unit 0 before the first unskinned draw, conflicting with other sampler types. `Renderer.cpp` now assigns unit 15 when initializing the main and shadow programs. The existing material GL executable reproduces the accepted baseline with 36 checks and 13 failures in `rendering/material-before.log`; the narrow first-unskinned color/shadow extension in `MaterialTests.cpp` passes 40/40 after the fix in `rendering/material-final.log`. The prior blank screenshot and draw errors remain in `rendering/unskinned-failure.png` and `.log`. The fix does not alter physics. Its affected material, application, editor and packaged rendering paths were rerun.

The final ordinary offscreen editor follow-up exits 0 and records ten successful integrated navigation checks alongside duplicate/undo, unchanged authored state after Play/Stop, save and normal export. Filled geometry is visible in its Play viewport. The focused headless navigation executable reports 31 checks with no failures. See `editor/README.md` and `editor/followup*` for exact receipts, including seven unavailable-relative-capture messages and one EGL warning. These backend limits leave actual desktop capture untested.

`streaming-after-guard.json` records the actual budgeted coordinator's v4 archive, adoption/tombstone identity, suspended region revisit, exact target/velocity/stopped intent and first-interval continuation. The coordinator is reconstructed from durable encoded bytes into a fresh world within the same process; the separate snapshot write/read commands are distinct processes. `streaming-before-guard.log` retains the genuine one-check failure that accepted a missing retained motion blob. The corrected guard rejects incomplete intent before publication and rejects lossy older archive writes.

The final standalone package is `/tmp/judas-m71-kinematic-moved-final`, with 5 assets, 1 scene, 23 files and 52,662,061 bytes. The exporter reports 0.043 seconds; export plus move/hide receipt time is 0.050935 seconds. `package/receipt.json` retains all packaged file hashes and runtime SHA-256 `ae42790e00cf28b1aed4f5dd9b07153c623fc6dfd85a8c045359bf33fb42adf8`, matching the candidate runtime. The owned export source directory was renamed and its original path is absent; the repository itself remains present. From an unrelated `/tmp` working directory, the moved package runs 440 scripted frames with real save/load/reload/removal, then a separate 90-frame process loads the saved slot by frame 20 and continues; both exit 0. Service waits add zero-clock frames. See `package/moved.log`, `package/fresh-load.log`, input files and visible snapshots. Linux Release evidence is available; Windows build/run and physical desktop pan/capture, typing/gizmo feel and visual acceptance remain outstanding.

The final [review and command map](FINAL_REVIEW.md), [changed-file list](changed-files.txt),
[source fingerprints](final-source.sha256) and [package comparison](package/verification.json)
identify the frozen candidate and distinguish asynchronous application timing from
fixed-input numerical validation.

Focused commands, from the repository root:

```sh
build/judas_kinematic_tests
build/judas_kinematic_lab_tests
build/judas_kinematic_lab_tests --stream
build/judas_kinematic_lab_tests --write /tmp/m71-prescribed.save
build/judas_kinematic_lab_tests --read /tmp/m71-prescribed.save
build/judas_kinematic_performance docs/evidence/m71/fixed-cost.csv
python3 scripts/create_m71_lab.py --tool build/judas_scene_author
```

Acceptance tolerances are stated directly in the checks: 0.1 mm for unconstrained prescribed endpoints, 5 mm/s for motor point/release velocity, 5 mm for free release displacement, and 25 mm maximum per-step carry/walk discrepancy. Dynamic friction rider drift is bounded to 150 mm over two seconds at 0.5 m/s. Test limits do not imply arbitrary speed, thin-feature, shape or desktop acceptance.

Human checklist: push the dynamic crate and compare the stationary lane; ride each support, walk and jump off; stop/reverse and change lift authority; save/exit/relaunch/load; pan elsewhere and inspect with existing right look/F framing; check selection/gizmos and Play/Stop; repeat the lab in the moved exported package.
