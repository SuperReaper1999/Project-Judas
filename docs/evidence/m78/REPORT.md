# M78 frozen audit — results so far

Subject: `9e77b2226edcf217ab789fe4f0fea42a3415a72d`.
**Audit handoff with partial behavioural coverage and explicit gaps. This is not
an engine correctness certificate or proof that every original requirement is met.**

M77 was committed/pushed with explicit operator authorization as NOT HUMAN
VALIDATED. M78 has made no product or existing-test correction. New observers,
owned fixtures and these audit records remain uncommitted/unstaged.

## Strongest findings

| Finding | Observation | Interpretation |
|---|---|---|
| PHY-002 | One 13-body articulation on 8192 planar mesh triangles: median 23.91 ms, p95 58.56 ms, max 105.10 ms per fixed step. Four: 170.13 / 258.71 / 338.87 ms. | Current triangle-mesh articulated contact is impractical in this envelope. Matching box controls sleep and are much cheaper. Repeated CCD iteration/limit counters identify a suspect mechanism; precise cause is unresolved. |
| PHY-001 | Torque-free asymmetric box has world angular-momentum vector residual ~100.3% of initial norm across four timesteps; sphere control is <2.6e-7. | Missing gyroscopic transport is inherited and already disclosed in M77. This quantifies an independent free-body envelope, not a new M77-only regression or 100% scalar energy loss. |
| PERF-001 | 6400 inert objects build in ~2 seconds; ~1.85 seconds is ResolvePrefabs, even with no prefabs. | Linear source-rank lookup inside stable_sort is a concrete construction cost. Steady fixed-step script cost is almost invariant with inert count in the sampled workload. |
| FLU-001 | Rotated legacy PBF half-immersion body ends at 0.55870 m/s against <0.5 m/s; repeat identical. | A real accepted fixture failure. Mass/finite checks pass. It does not implicate the M54 conserved ledger or M55 surface solve. Numerical cause unresolved. |
| TST-001 | Four 100-object snapshots need 5.188 MB; default retained budget is 4 MiB. Last region correctly pins. | Scalability assertion no longer fits current snapshot size. Larger-budget and smaller-count controls retire normally; this is not a demonstrated deadlock. Memory overhead remains significant. |
| WORK-001 | Shared terrain edit leaves the second scene's navigation stale; publication rejects it. | Correct guard, but open-scene rebake is not whole-project recook. Rebuilding the second bake permits publication. |
| DOC-001 / REQ-001 | Stale checkpoint labels and missing original M77 brief. | Historical documentation and requirement availability must be kept distinct from implementation failure. |

[FINDINGS.json](FINDINGS.json) contains controls, exact receipts, impact, unresolved
causes and proposed correction boundaries. No recommendation authorizes a fix.

## Breadth and denominators

All 184 discovered CMake test targets were built in Release and executed once:
177 initial zero exits, seven nonzero. An incremental build is not described as a
clean build or zero-warning certification. No CTest registration exists here.

Five original failures were isolated to observer prerequisites: frozen archive
lacked build/judas for normal exports; storage guard required its owned basename;
model reimport needed a local original FBX; one cookbook case needed license-restricted
ignored cooked Skate assets. Focused reruns pass with these inputs recorded.
Fluid acceptance and scalability failures remain preserved and adjudicated above.
No failed original run is replaced by the corrected receipt.

Discovery has 89 mechanism groups, 345 public JS symbol rows, 34 authored component
rows and 17 asset families. These overlap and are not summed into a flattering
engine pass percentage. There are 324 editor control locations and 22 tool sources;
these are source inventory, not gesture/CLI execution passes. 4424 assertion
locations are an index, not executed assertion or branch coverage counts.

[CAPABILITIES.json](CAPABILITIES.json) is the authoritative provisional register;
[SOURCE_TEST_INDEX.json](SOURCE_TEST_INDEX.json) holds precise suite assertion
locations; [REVERSE_SURFACES.json](REVERSE_SURFACES.json) accounts for editor/tools.
**Individual effect tracing and negative controls are incomplete unless directly
attached. Related suites do not certify every listed member.**

## Targeted and cross-system evidence

- Actual PhysicsWorld and independent direct integration probes agree on the
  asymmetric-body defect; refinement and isotropic controls are retained.
- Current imported 13-body rig uses normal bodies/constraints and authored resistance,
  not scripted pose correction. Box/mesh controls retain all 900 per-step samples.
- M77 supported, recovery, radial, denied, blocked-recovery, low-grip, lifecycle,
  regions, fault, variants and step-trace cases executed. Fresh-process step,
  stand and recovery saves/readbacks executed. These do not replace human motion review.
- Modern save/application and streaming cold readbacks execute actual application
  replacement and participant restoration, not merely serializer round-trips.
- Static/types and actual VM enumeration agree: 30 exports, 345 public symbols,
  239 native operations, 16 callbacks, 39 typed examples, three drift controls,
  TypeScript 5.9.3. This checks surface identity, not all semantic branches.
- Owned editor project: batch transform/undo, duplicate/undo, Play/Stop and unchanged
  authored scene pass in the actual editor. Screenshot inspected: panel/scene rendering
  exists. Offscreen mouse capture is unavailable, so real capture/usability is unproven.
- Owned curated mod kit is produced without private base source, editable terrain is
  sculpted/painted with native editor gestures, cooked, rendered and rebaked. Stale
  second-scene bake is rejected. Once both bakes are fresh, exact-hash publication,
  installation and explicit executable trust work. Untrusted executable is rejected.
- Source-hidden, moved owned runtime launched from /tmp: 19 application checks pass
  for actual sculpted collision, motor, prefab contacts, signals, Detour, unload/revisit
  and reload. Separate cold write/read restores facts and authoritative motor pose.
  Original failed hub/addition-only observer assumptions remain recorded.
- Optional milestone-notice Query executes through unchanged editor object code with
  audit-only private-access compiler flag. Missing CLI, malformed response, valid
  milestone selection and cancellation pass controlled child-process checks. No
  actual network/authentication, notification gesture or Windows branch claim.

## Current architectural envelopes that must not be overstated

- Double absolute coordinates / local float coordinates use a fixed origin; no live rebasing.
- Gravity zone selection, support, attachment and reference frame are distinct.
  Per-entity scripted selection does not imply automatic ragdoll-bone inheritance.
- Physics queries are read-only geometric questions; contacts/events come from simulation.
  Static triangle-mesh CCD is distinct from convex casts and has the measured cost above.
- Motor, camera, skeleton, articulation and game behaviour remain separate owners.
- M54 owns conserved quantities; M55 is a reduced local-inertia surface model,
  not full 3D Navier–Stokes. Legacy PBF is separate and remains approximate.
- M74 is offline baked retargeting; no authoring evaluator is needed at runtime.
- M75 edits finite height patches; it does not promise caves or planetary sculpting.
- M76 mods are trusted curated payloads, not a security sandbox or arbitrary numerical
  registry creation. Exact loadout/content identity applies to saves.
- M77 is a bounded configured mechanical envelope, not universal human recovery.
  Contact observations, supported geometry and effort budgets limit the task.
- Rendering consumes presentation state; it does not advance another physics simulation.

## Windows and human status

All 853 existing guest source inputs match the current manifest/host. VM configuration
remains unchanged. Guest-agent interruption and black screenshot are not a diagnosed
product crash. Operator authorized an ACPI restart. Five fresh native tests were
recovered, all exit zero: profiler, balance foundation/controller, physical animation,
CharacterMotor. Their executable hashes/logs are in [WINDOWS_COMMON.json](WINDOWS_COMMON.json).
Only ~128 MiB free space remains. Broader Windows execution, moved-package and physical
GPU/input/audio acceptance remain unfinished. The failed oversized result retrieval
is an observer transport problem, not an additional native test failure.

M67 specified authoring review, M74 motion, M76 workflow, M77 physical motion,
Lastlight/Wayfarer boarding/flight and real Windows hardware acceptance are separate
pending items. M75's recorded acceptance is not erased. No later human acceptance
has been inferred from a plan to test.

## Explicit unresolved coverage and priorities

The register records 483 Linux rows partially verified, one failed accepted fluid
contract and one bounded native notice-query verification. Windows records 481
individual rows not executed, three partially verified and one bounded profiler
case; five native target passes are linked without turning them into hundreds of
semantic passes. No row is claimed independently proved across every declared input.
Source entry cataloguing is not complete downstream effect tracing for every member.

77 milestone records are retained. Eighteen original briefs are available; 59 are
unavailable at known locations, including M77. Fine-grained sentence-by-sentence
intent reconciliation is explicitly incomplete rather than inferred from narrower
later docs. Historical Jolt removal, fixed-origin M23 and separate fluid research
are recorded without rewriting the museum. Candidate git-search hits are not unique
milestone acceptance hashes; M65/M66/M67 actual main commits are separately identified.

[INTERACTIONS.json](INTERACTIONS.json) selects cross-system routes and their uncovered
combinations. [HUMAN_CHECKLIST.md](HUMAN_CHECKLIST.md) retains runnable deferred reviews.
[CASE_INDEX.json](CASE_INDEX.json) indexes immutable receipts. Preservation and final
fingerprints are separate records; none claims unexecuted platform coverage.

Ranked corrective work, requiring separate authorization:
1. Triangle-mesh CCD/contact cost and failure to sleep in PHY-002's measured envelope.
2. Generic anisotropic angular integration/gyroscopic transport in PHY-001.
3. Inert construction source-order/index cost in PERF-001 and snapshot memory budget
   fixture reconciliation in TST-001; retain ordering/identity semantics.
4. Rotated legacy PBF settling contract FLU-001, without conflating or reopening M54/M55.
5. Shared-asset dependency freshness workflow and stale documentation labels.
6. Missing original briefs, per-member negative/phase proofs and Windows breadth/hardware.

No fixes, M78 commit or next milestone were authorized. Windows remains explicitly
unfinished where not executed, as the operator allowed. Judas still exposes reusable
engine primitives with ordinary game JS; this report measures those mechanisms and
their limits, rather than certifying all engine promises from passing suite exits.
