# Pre-Alpha Engineering Pass 2

Development base: `528cfb1b430d710ba5a4d2d13569aad106e3044e`, **Experimental**.
Main remains the accepted M99 baseline `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2`.
This is an automated, bounded engineering checkpoint. Human review is pending;
it is **not Alpha qualification**, M100, or a main promotion.

## Corrections and engineering scope

| Area | Established mechanism and change | Evidence / boundary |
|---|---|---|
| First-arrival curved-mesh CCD | A finite swept-SAT interval bounds actual box/convex arrival against cooked triangle patches. One swept BVH traversal supplies conservative translation/rotation bounds; ordinary contact truth and response remain authoritative. | [Physics report](physics/REPORT.md). Initial arrival passes; subsequent contact-episode continuation remains defective. No CCD, timestep, contact-fidelity or solver-budget reduction. |
| Collision query cost | Full and Boolean cooked queries share accepted-witness traversal; reusable clipping/poly/triangle scratch has reentrant leases, including owned spill beyond 32 frames. An exact bounded pair bitset has a hash fallback; impact scratch retains capacity within a step. | 120,283 conformance checks, identical full-witness digest; no cross-time geometry cache. Rejected memoization and contact-side experiments preserved. |
| Dense world retirement | Whole-world shutdown no longer individually destroys every remaining rigid body and repeatedly wakes the same contact island immediately before destroying the physics world. | Dense process now exits normally. Normal gameplay body removal/wake semantics and script/subsystem teardown ordering remain. |
| Conserved-liquid origin shifts | Translate published owner poses and detached parcels once, retaining owner-local geometry, wave history, velocities and conserved ownership. Invalidate only derived world-space optical caches. | Actual uniform/non-Y and radial query, buoyancy, wave-byte, cold-process and failed-admission checks. Full liquid extents/parcels must fit the existing 8 km chart. Unpublished owners refuse nonzero shifts until publication. |
| Atmosphere/combustion origin shifts | Translate atmosphere reference origin. Thermal state is scalar and continues to refer to existing generation-safe body handles; physics already translates those bodies. | Actual gas/thermal endpoints and continued exchange. Origin support does not add modern atmosphere/combustion save participants. |
| Gravity equivalence | Compare only the physical parameters used by the selected equilibrium: plane direction or radial centre, plus magnitude. Previously irrelevant plane centre/radial preferred-up could reject physically identical fields. | Preserved baseline coordinate/equivalence probe; unchanged active-parameter tolerances and gravity sampling. |
| Failed snapshot capture | Prepare a detached chunk map and publish it only after every serializer succeeds. Preflight unsupported owners before lazy participant construction. Share the existing entity inclusion rule between entity/contact/sleep participants. | Prior bytes and versions survive early unsupported ownership and late participant failure; repaired success replaces the map. This is caller-output atomicity, not rollback of all lazy world preparation. |
| Save provenance | Record the actual shared build ID, rather than a fixed milestone label. An old header lacking that optional diagnostic reads as `unknown`. | Real SaveService write/fresh-process read and old-header controls. No save schema or compatibility policy change. |
| Distribution qualification | Bounded ELF/PE readers inspect ABI/import requirements. Independent SDK/APK inspectors verify actual inventories and bytes, coherent JDKs, Android alignment/import closure and builder publication rollback. | [Packaging report](packaging/REPORT.md). Exact host evidence is separate from older-Linux, native Windows and device qualification. |
| Build organization | Extract the unchanged 20 M79–M83 target contracts and centralize ordinary linkage/warnings/application sources. | Six compiler-free positive/negative execution/parity controls, plus native builds. Large runtime/editor/script files and distributed lifecycle bookkeeping remain. |
| First project | Add a complete ordinary render/camera JavaScript starter and practical creation/debug/reload/export instructions. | Actual VM endpoints, refused/repaired reload, editor Play/Stop and moved export. No hidden game controller or new framework. |

Only the new `LiquidSystem` coordinate-translation integration is changed in fluid
production code. Liquid numerical algorithms, M54/M55 thresholds, legacy PBF,
JSGT/FTFT, independent research and accepted historical evidence remain untouched.

## Matched Linux measurements

Intel i7-4790, eight logical CPUs, GNU 15.2 Release, 60 Hz fixtures. Exact hardware,
commands, timings, source receipts and physical observations are in the physics
report. The Windows VM was paused before paired timing. Timing columns are
inclusive measurements, not additive stage totals.

| Identical workload | Before | After | Physical observation |
|---|---:|---:|---|
| 104-triangle mesh, 60-step prefix, total | 292.99 ms | 224.13 ms | Maximum penetration unchanged: 9.92 mm |
| 1,800-triangle mesh, same prefix, total | 397.56 ms | 300.94 ms | Maximum penetration unchanged: 10.80 mm |
| 7,200-triangle mesh, same prefix, total | 844.78 ms | 604.29 ms | Maximum penetration unchanged: 21.48 mm |
| Pure witness workload, median | 290.189 ms | 209.990 ms | Identical full-witness digest |
| Dense 1,024-body pile, 270-step median | 27.070 ms | 25.937 ms | All 270 physical digests/counters identical |
| Same pile, p95 | 413.328 ms | 416.695 ms | Mixed/worse; no universal speedup claim |
| Same pile, worst | 2,123.409 ms | 1,970.799 ms | 7.2171085 mm maximum penetration unchanged |
| Same process including teardown | Watchdog timeout after exported steps | Normal exit, 54.731 s | No removal of requested simulation steps |

Ordinary free bodies, active/sleeping assemblies and the matched smaller pile
also pass with identical physical digests; their small timing changes do not
support a broad speedup claim. Mesh trajectories change where the corrected
earlier impact changes response; bitwise equivalence is not claimed for those.
The matched timing capture precedes the final allocation-only spill policy for
queries nested beyond 32 frames. Exact final conformance covers that policy;
the timing table is not relabelled as a new run of those later source bytes.

## Final validation

The [integration report](integration/REPORT.md) retains commands, source/binary
identities, raw logs, original failures and corrected follow-ups.

- Combined focused Linux gate: **26/26 pass**, with one shared final embedded
  candidate identity. It covers collision/CCD, CharacterMotor, continuation,
  contacts, origin participants, terrain/stream/save, networking, resources and
  the new first-project/origin-environment contracts.
- Additional actual checks pass: impact robustness, kinematic/ragdoll/imported
  ragdoll/sphere bounds, world persistence, conservation/dynamic-surface/refill,
  combustion, M77 cold stand/recovery physical endpoints, animation cold endpoints,
  SaveService, cross-version saves/mod failure, debugger/editor Play/Stop, API
  enumeration/drift, relocated export and the complete host SDK.
- API enumeration remains **473 public symbols / 40 exports**; declarations and
  51 example sources pass the existing drift/type checks.
- The final normal origin test passes **87 parent checks**, plus two independent
  eight-check cold-process children. It checks actual liquid/gas/thermal state,
  not just admission flags.
- Linux packaging controls: 24 existing cases and 16 distribution cases pass.
  Native Windows Python distribution controls: 15 pass after preserved failures.
  Compiler-free mechanical target controls: six pass.
- Release build is incremental/focused, not a new full production marathon.
  Existing compiler warnings remain recorded. No zero-warning or exhaustive
  engine certification is claimed.

The first combined run was 25/26 because a **new test setup** tried to mark a
static metadata actor transient before it had a runtime entity record. Its log
and source inventory are retained. Independent eligible dynamic thermal and
nonthermal controls correct that setup; no engine behaviour or threshold was
changed in that follow-up. A later tooling check also exposed the old assumption
that every known finding must be unscheduled after the now-correct first-arrival
contract became an ordinary scheduled gate. Its failure and follow-up are retained.

The final arm64 Connected Workshop APK builds, signs and passes independent
native/content hash, actual signature and 16 KiB alignment checks. It is 53,484,213
bytes, SHA-256 `43b76986b69c66fbef3a37152eda9c82890a0ef2290a499af4c52163ed14569a`.
No new phone/emulator execution is claimed. Final host SDK verification covers
17 tools and 212 files. Artifact paths, exact identities and clean-CI qualification
status are recorded in [PACKAGING_STATUS.json](PACKAGING_STATUS.json).

The native 26-case gate retains its exact 52-input receipt. The subsequent
scheduling-unit-test and canonical Android ABI-name verifier corrections have
their own passing controls and actual final 53-input SDK/APK identities. No C++
engine, physical fixture, solver budget or threshold changed after that native
gate; those earlier binaries are not relabelled as a later identity.

## Remaining defects, limits and recommended Pass 3

1. **Curved-sheet contact continuation — correctness blocker.** The unchanged full
   90-step oracle still fails four assertions. Coarse/medium/dense crossings are
   11/20/15, versus 24/24/24 baseline. Actual traces identify later steps with the
   body already slightly behind a one-sided sheet and no new CCD candidate.
   Investigate persistent contact episodes/publication across steps. Rejected
   local front/support guards lost contact geometry or caused large slowdowns;
   they are not shipped.
2. **Dense-contact tails — performance blocker for that workload.** Nearly two
   seconds remains the worst step. Repeated island discovery, geometry and TOI
   dominate. Preserve the same physical workload while evaluating incremental
   island/event architecture; do not infer safety from ordinary lab performance.
3. **Rotated-liquid authoring — newly established defects.** One raw-versus-
   normalized quaternion produces a 3.350149682e-7 equilibrium disagreement at
   the unchanged 1e-7 tolerance; another non-axis rotation creates nonmonotonic
   Surface1 knots rejected by its normal decoder. Exact failed bakes and asserting
   commands are preserved. The positive origin case uses an exactly represented
   non-Y rotation and radial control; it does not close these bake defects.
4. **Platform qualification.** Complete current native Windows C++/SDK/moved-game
   checks need safe guest disk capacity. Only Python/PE tooling was validated
   natively in this pass. Windows VM timing is not an acceptance gate. Ubuntu
   24.04/glibc 2.39 qualification needs its own actual clean-build receipt; local
   Ubuntu 26.04 binaries retain their newer ABI requirements. Android build/byte
   qualification is distinct from fresh emulator/phone/GPU testing.
5. **Composed support boundaries.** Conserved liquid groups and selected active
   navigation/articulation dependencies remain pinned during regional suspension;
   legacy PBF/emitted liquid cannot live-shift. Thermal modern saves, arbitrary
   inactive-native-state migrations, automatic authored-content merge, distant
   independent physics charts and broad physical replication remain unsupported.
6. **Unverified risks.** Transport pumping while fixed updates are paused is tested
   with an advanced injected clock. A debugger that blocks the owner thread is
   not that case and remains unverified. Large-residency eviction has no matched
   unacceptable-cost reproduction. No Internet/sandbox/security certification.
7. **Known deferred history.** FLU-001 remains failed and unchanged. The original
   a106 save lacks historical continuation data; no retroactive reconstruction
   guarantee is invented. Existing games/research and protected evidence remain.
8. **Maintainability and onboarding.** Continue typed module extractions and
   auditable lifecycle obligations incrementally. The starter is usable but an
   unauthored UI still exposes the legacy diagnostic HUD; the guide explains how
   to hide it. This cosmetic friction is recorded, not presented as solved.

Every original priority and all 54 subfindings are mapped in the
[complete disposition](AUDIT_DISPOSITION.md) and [machine-readable register](FINDINGS.json).
An existing deliberate support limit is not silently relabelled a defect or pass.

## Consolidated human acceptance checklist — both passes

1. Open the editor and runtime build information. Confirm Experimental, exact
   build provenance and Pre-Alpha status. Main must remain the M99 release.
2. Follow [FIRST_PROJECT.md](../../FIRST_PROJECT.md), create the scene, Play/Stop,
   edit the script, debug and deliberately introduce/repair a syntax error.
   Confirm clear diagnostics, preserved live state on refused reload and clean Stop.
3. Export that ordinary project; move its directory and launch from an unrelated
   working directory. Inspect the SDK inventory/checksums and every public tool.
4. Exercise a current character/vehicle game, pause/resume, cold save/load and
   scene reload. Confirm restored physical endpoints and no stale ownership.
5. Use the existing large-world/terrain labs: edit collision, cross stream/origin
   boundaries, return and reload. Inspect refusal messages for unsupported owners.
6. Inspect the origin-environment fixture and receipts for disturbed connected
   liquid, parcels, gravity, gas and thermal continuity. Do not infer universal
   rotated baking or thermal-save support from that positive fixture.
7. Run the Physics Stress Lab watch tour and increase the workload deliberately.
   Confirm stable physical response; expect the documented extreme dense limit.
   Review both the passing arrival and still-failing continuation receipts.
8. Open Save Evolution and Connected Workshop. Test success/failure recovery,
   host/join/disconnect/reconnect and shared physical interaction. Blocked-debugger
   peer timeout policy is still a separate unverified combination.
9. On Windows, after adequate disk is available, build/inspect the full SDK and
   launch a moved game. On Android, inspect/install the newly identified APK and
   test pause/resume/touch/networking. These are pending human/device checks.

## Checkpoint readiness

The bounded Linux-validated corrections are suitable for an **Experimental
checkpoint** with the explicit failures above. They are not suitable for claiming
all original audit items resolved or an Alpha release. Exact changed files,
final source hashes and evidence hashes are in [FINAL_SOURCE_FILES.json](FINAL_SOURCE_FILES.json)
and [FINAL_FINGERPRINTS.json](FINAL_FINGERPRINTS.json). Binary identities
honestly retain their tested dirty-parent source identity; committing identical
bytes does not retroactively rebuild or relabel those artifacts.
