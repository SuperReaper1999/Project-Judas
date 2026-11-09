# AI usability, sensing and leases — 1 November 2026

Experimental continuation of `0bfb3a433d73362ee12e66fd37e667a36ed45ea2`.
Main remains unchanged. This is automated Linux qualification, not new human
acceptance, a release, a fluid unlock or platform-wide certification.

[Developer guide](GAME_AI.md) · [API and bounds](judasjs/ai.md) ·
[Raw evidence (historical)](https://github.com/SuperReaper1999/Project-Judas/tree/2ee4203a0053cc545eab4a184c627f4f5507c4bd/docs/evidence/ai-usability-20261101/README.txt)

## Delivered

**Fresh-object workflow.** Select an empty object, open **Add gameplay script** in
its Inspector and choose **Create AI behaviour**. A single scene transaction
attaches normal action source plus a linked `.ai.js` definition, adds a visible
CharacterMotor actor when needed, and creates three explicit root markers for
patrol/return/cover. Existing kinematic native 2D actors use the same workflow.
Existing dynamic or otherwise conflicting pose ownership is rejected clearly.
The workflow reuses a selected empty script slot, preserves its identity, and
supports one Undo/Redo without deleting source assets. Failed publication removes
only its newly minted AI files; invalid existing scenes remain untouched.

**Selected-NPC inspection.** The Inspector separates Decision settings, Action
code and Inspect this NPC. The AI window can follow the selected object's real
script ownership. Shortcuts select the intended tab and uncollapse the window.
The source/definition link is explicit literal metadata plus the matching
registered import, parsed without evaluating project code. Keep that generated
header when editing action code; this is not a general JavaScript semantic linker.
Coordinators stay coordinators; no visual-target ownership is guessed.

**Copyable two-action starter.** The embedded project template demonstrates real
patrol, targeted `ai.damage` signals, hiding/recovery, target checks, cancellation,
renewable resource claims and modern save restoration. Movement is collision-aware
CharacterMotor or exact supported native planar geometry, not direct pose changes.
It does not plan around obstacles; developers can use existing navigation in their
actions. Marker height differences use the same tangent geometry for eligibility
and movement. Include the explicit target markers when authoring reusable prefabs
so normal entity-reference remapping can resolve them.

**Optional sensing.** AISenses uses existing MK2 spatial queries and 3D/2D LOS.
Results, processed matches and ray counts are explicitly capped; untested LOS is
null. Target and occluder layers are independent. Cached observations retain their
original sample time and are always explicitly stale, even at age zero. Destroyed
or changed retained targets and origin changes invalidate relevant caches. Native
CharacterMotor query geometry works after normal lazy publication; no target
motor getter is needed to create the geometry before querying.

**Optional exclusive leases.** AITargetReservations scopes are distinct even in
one script. Claims are world-session local, timed by simulation seconds, renewable
and explicitly releasable. A contested/full board returns false without eviction;
invalid/unrepresentable time or keys reject without altering ownership. Leases
are released before teardown/action cleanup, and retiring callbacks cannot create
agents/sensors/scopes or acquire new resources. No saved runtime lock tokens are
revived: actions explicitly reacquire after restoration.

The core and bridges allocate lazily. There is no unconditional sensing, claim
maintenance or background agent loop for projects that do not use the helpers.

## Results

* Fresh workflow: **49 checks, zero failures**. Actual fresh project creation,
  typed defaults/references, asset/import links, empty slot reuse, undo/redo,
  overwrite/path/invalid-scene and injected publication failure controls. Real
  CharacterMotor patrol -> damage -> hide -> recovery, exact modern physical
  endpoint and continued native clock, missing targets and disabled script.
  Height-offset routes visit outward and return markers. Native 2D movement stops
  at the real wall using its actual collider.
* Native reservations: **65 checks, zero failures**, including limits, expiry,
  renewal, foreign release and transactional invalid inputs.
* Real reservation scripts: **26 checks, zero failures**, plus separate-process
  modern save writer/reader. Two NPCs, same-slot independent scopes, real simulation
  clock, faults, destruction, deliberate reload and foreign-handle/coercion controls.
* Native sensing: **53 checks, zero failures**. Real occluders, rotated and planar
  cases, independent filters, zero/exhausted budgets, cache age/membership, origin
  shifts and native CharacterMotor attribution/publication/tag/removal controls.
* Combined real scripts/physics: **10 checks, zero failures**. Two NPCs sense the
  same physical food target; a real utility action obtains its lease, moves using
  MK2 and consumes it exactly once. Unknown visibility remains unknown. Removing
  an owner runs sensor disposal and subsequent cleanup exactly once and rejects
  four kinds of new work during retirement.
* Existing utility core/script/editor, script authoring, pause-menu application,
  fresh 2D workflow, editor preferences/workspace, Basic Water, fluid policy,
  origin scripting and networking scripting suites pass.
* API/type consistency and validation manifest/registration checks pass. Runtime
  API enumeration and an entire-engine test-suite run are not claimed.
* Native app/export/editor and cold-process commands/results are retained in the
  evidence directory. Source fixtures and user projects are separate.
* Existing Little Haven application: **41 checks**, then **37 checks in a fresh
  process**, zero failures. The restored running action advances its actual
  physical actor. The new source fixture also exports and runs after relocation
  from an unrelated working directory for 1,200 frames. Editor Play/Stop preserves
  the exact authored scene and saves successfully with isolated user preferences.

Tested binaries carry honest dirty-development provenance based on the starting
checkpoint, not a claim to have been built from the later evidence/checkpoint
commit. Exact source and binary hashes are recorded together. A stale
exporter/runtime fingerprint pair was correctly rejected; the final package uses
a rebuilt, matching pair rather than bypassing that guard.

### Issues found during this pass

The first generator produced syntactically valid JS with incorrect entity-property
encoding. Actual runtime checks exposed it; the helper now validates both defaults
and authored references before committing. A selected empty script placeholder
also needed a narrow baseline-preflight exemption before it could be filled;
other invalid scene slots remain rejected. File staging cleanup cannot turn a
successful copy into an unjournalled failure.

Review caught disposal after unpublication: the initial sensor bridge rejected
`senses.dispose()` in a removed actor's destroy callback, potentially skipping
later cleanup. Owned disposal now remains valid, while create/sample need a live
owner. Resource acquisition is also blocked throughout the whole project destroy
callback, not merely the subsequent automatic retirement.

Some failing probes were test-contract mistakes and remain preserved. Reload
uses ordinary start, whose fixture explicitly reacquires; a pre-start observer now
proves the board was empty before that legitimate acquire. Directly stepping only
physics omitted queued script kinematic publication; the combined test uses the
normal played-world scheduler. A four-second patrol endpoint was phase-dependent:
actual trace reached x=2.76667, returned to x=.133333, then moved out again. The
corrected test proves both visits instead of requiring one final phase. Motor
sensing tests use normal FixedScripts publication, not a direct getter shortcut.
No physics tuning, collision tolerance or original accepted fluid test changed.

## Measured sensing work

Release GCC 15.2, Ryzen 5 5600/Linux. Stationary native fixtures, 100 observations
per arm, maxResults16/maxRaycasts8/maxCandidates128. No concurrent build/test jobs
launched during measurement. These are observation calls, not whole-game frames.

| Physical targets | Observation | median / p95 / worst microseconds | New matches / rays, total |
| --- | --- | --- | --- |
| 64 | fresh | 16.937 / 28.915 / 34.064 | 6,400 / 800 |
| 64 | cached | 1.212 / 1.272 / 2.865 | 0 / 0 |
| 256 | fresh | 39.044 / 50.676 / 69.752 | 25,600 / 800 |
| 256 | cached | 1.243 / 1.412 / 3.116 | 0 / 0 |

This is deliberate reuse of stale observations, not an equivalent-freshness
speedup claim. All rows retain truncation: the fixture has more matches than the
returned budget. Ray budget exhaustion and result truncation are separate facts.
The shared broadphase still materializes its full match vector; maxCandidates
bounds later processing, not broadphase traversal/allocation. Partial subsets
are not guaranteed globally nearest. Dense/complex geometry can still make a
fresh query expensive. Refresh/sensing frequency and scheduling remain explicit.

## Limits and human check

There is no reservation queue, fairness/preemption policy, inventory transaction
system or cross-server lock service. First successful acquisition wins in the
script execution order. Targets are opaque keys; game code releases destroyed
resources or lets their bounded leases expire. Observations use body centres and
ordinary published entity-owned bodies; render-only markers are not physical
sensor targets. Higher-level perception, teams and path planning remain project
policy. Use the documented bounds and existing per-script state-size limit.

Windows, Android and browser execution are not freshly qualified in this pass.
Main, engine version/release channels, fluid code, protected history and independent
research remain unchanged. The new candidate still needs human acceptance.

1. On a fresh object try Create AI behaviour, then Play; inspect its patrol.
2. Check Decision settings / Action code / Inspect this NPC, selection following,
   author/live tab switching, and existing dark/light themes and saved layouts.
3. Edit the generated action code or definition, then deliberately reload/restart.
4. Send damage, delete a target, and check clear reasons and safe cancellation.
5. Try save/load and the relocated standalone; inspect the recorded lease and
   sensing negative controls before promotion.
