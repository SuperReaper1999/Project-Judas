# Judas Engine Alpha v1.2.0

Machine version **1.2.0-alpha.1**. Alpha, not Stable/GA. This release adds optional
game AI and an editable life game, and carries the separately gated browser work
developed after Alpha v1.1.0. Previous tags and packages remain immutable.
Publication is provisional pending exact-candidate gates. The final manifest and
checksums identify shipped source and artifacts; these notes do not publish them.

## Game AI developers control

- Native Utility AI scores eligible action/target pairs using facts, weights and
  response curves, with explicit ties, cadence, commitment, interruption and cooldowns.
- Project JavaScript owns what an action actually does: movement, attacks, meals,
  jobs and rewards. No model download, machine learning or external AI service is
  required. Projects without agents have no background decision loop.
- Normal `.ai.js` Script assets hold the same constrained decision definitions
  edited in the authoring tool and consumed by scripts.
- **Create AI behaviour** gives a selected fresh object a paired definition and
  action script, explicit target markers and suitable supported movement. Existing
  conflicting physical ownership is rejected instead of silently converted.
  Decision settings, Action code and Inspect this NPC are separate shortcuts.
- Inspection shows real candidates, eligibility reasons, score contributions,
  actions and timing without changing decisions.
- Optional `AISenses` uses existing physical queries and line-of-sight tests.
  Optional `AITargetReservations` provides exclusive, renewable simulation-time
  leases. Both are created lazily and follow script ownership and teardown.
- Agent snapshots preserve explicit decision state through ordinary script saves.
  A fresh process reconstructs handlers and agents; it never serializes closures,
  resumes a JavaScript stack or revives old runtime lock tokens.
- Windows authoring uses native Unicode staging paths; final platform setup also
  covers executable targets declared by later subsystems.

See [Game AI](GAME_AI.md) and [JudasJS](judasjs/ai.md).

## Two editable learning projects

[Little Haven](../projects/ai_workshop/README.md) compares everyday needs with
patrol/damage/hide decisions. Its normal action code performs physical travel,
service effects, cancellation and saved continuation.

[Willow Town](../projects/willow_town/README.md) is a small top-down life game with
six selectable residents, two homes, a shop, workshop, allotments and a town green.
Watch native decisions or directly control one resident. Needs compete with work;
jobs have real ownership, vacancies, applications, dismissal and role-based output.
Health, compressed town-day aging and permanent death affect those opportunities.
Progress and physical positions survive cold saves.

Town rules remain readable project JavaScript. Residents intentionally pass one
another while scenery blocks movement; this is not a crowd-dynamics solver.
The static planning grid needs matching authored footprints. No births, newcomers
or full social/economic simulation are included. Version-2 life
saves use a separate identity; original saves remain intact without automatic migration. Both projects ship as editable SDK/source learning examples; package inventories
record their exact files.

## Experimental browser workflow

Browser export remains hidden unless the editor is launched with
**`-ExpBrowserSupport=1`**. It compiles the same engine, MK2 and JudasJS to
WebAssembly/WebGL2. The initial export host is Linux with a separate Emscripten
toolchain; ordinary desktop development does not acquire that dependency.

Build & play offers prerequisite diagnostics and local preview. Web packages
include verification/serving helpers, provenance and notices. The port adds durable
IndexedDB saves, isolated storage, bounded GPU submission, responsive input,
fullscreen/touch handling and scoped background/audio qualification. Shared shadow
code corrects PCF receiver depths; repeated physics initialization releases the
previous live state.

This is still an **experimental source workflow**, not complete browser support
or a browser compiler installed inside the desktop SDK. Hosting requires the
documented isolation headers. Native TCP networking, mods and direct filesystem
workflows are not supported browser promises. WebGL context loss requires reload.
The audio backend uses deprecated ScriptProcessorNode; hardware audio quality and
latency are not certified. AI/Willow Town browser execution is not newly qualified
by the earlier browser-game checks. See [browser support and evidence](BROWSER_SUPPORT.md).

## Qualification and limits

The operator authorized promotion and publication. Exact-source gates and SDK
receipts must pass; prior scoped trials are not universal human acceptance.
Development evidence keeps its honest base-plus-dirty provenance.

Linux development checks include fresh-object AI authoring, real script ownership,
native/scoped leases, physical sensing and occlusion, positive/negative lifecycle
controls, cold-process continuation and moved standalone games. See
[initial AI qualification](GAME_AI_QUALIFICATION.md) and
[authoring/sensing/lease qualification](AI_USABILITY_QUALIFICATION.md).

Willow Town's life checks passed **150 application checks, 72 cold-process checks
and 25 automatic absence checks**, alongside unchanged **115 application and 56
cold-process regressions**. A normal visible 20-minute observation at 3x speed
recorded 3,600.600 game seconds, five survivors, one old-age death, three hires
and two dismissals. All 362 observations passed wallet/job-ownership checks.
The final save reopened with matching state/physical positions. This is scoped
evidence, not tuned survival balance or a large-population performance guarantee. The life report preserves an
intermittent startup/capture graphics anomaly and successful controls; it does
not claim a renderer fix where a cause has not been established. See
[the full life qualification](../projects/willow_town/Tests/LIFE_QUALIFICATION.md).

Sensing caps returned/processed results and rays, but the underlying broadphase
still materializes its full match list. Cached observations are explicitly stale;
untested visibility is unknown. Partial results are not guaranteed globally
nearest. Leases have no fairness queue or cross-server authority. Determinism
requires matching definitions, ordered inputs and fixed-step calls; arbitrary
JavaScript/physics is not promised bit-identical across platforms. Utility AI is
not a planner, behaviour-tree graph or navigation replacement, and does not
automatically replicate project actions over the network.

**Advanced Fluid Dynamics remains locked in Alpha.** Basic Water and swimming
remain independent and usable. Experimental wet-support, dry-gap and curved-surface
repairs do not authorize an Alpha unlock or erase remaining immersion and
numerical limitations. Protected research and historical failures are preserved.

The native 2D workflow from v1.1.0 remains included. Planar and 3D rigid bodies
cannot coexist in one physical world. Earlier Windows 11/Android human
acceptance keeps its recorded scope. New platform qualification requires its own
receipts; Linux AI tests do not establish Android/browser execution. Hosted Windows
checks cover native builds/selected contracts, not GPU, controllers, audio devices
or clean-machine certification. The
[support matrix](ALPHA_SUPPORT_MATRIX.md) records the wider boundaries.

## Quick test

1. Create AI behaviour on an empty object, place its targets and inspect decisions.
2. Change a need/health fact; verify the selected action and its physical effect.
3. Remove or block a target and check eligibility, cancellation and cleanup.
4. In Willow Town, select/control residents, close a workplace and watch applications.
5. Save during travel, restart, load and compare actual position and progress.
6. Export the complete game, move its whole folder and run from another directory.

Use [release preparation](RELEASING.md) for the repeatable package/publication
procedure and [distribution](DISTRIBUTION.md) for installation and dependencies.
