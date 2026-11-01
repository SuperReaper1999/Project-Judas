# Optional Utility AI checkpoint — 1 November 2026

Experimental feature built from `ecb0ef5aa9eaed047068561de4b58fa96a3e1dd3`.
The containing commit identifies the candidate. Main remains at
`49c4f3d9e29debe669411f01501e0276dea23231`; no release/version/channel change,
fluid unlock or human acceptance is implied.

## Delivered

[Authoring guide](GAME_AI.md) · [JudasJS reference](judasjs/ai.md) ·
[Editable Little Haven Workshop](../projects/ai_workshop/README.md)

The optional native core chooses action/target pairs from project-supplied facts.
It supports linear/inverse/power responses, explicit product/weighted-mean rules,
eligibility, deterministic tie order, minimum commitment, interruption, cooldowns,
fixed-simulation cadence and deterministic initial staggering. No world scan,
perception service, movement controller or unconditional AI update was added.

The shared editor/runtime definition is constrained JSON exported from a normal
registered `.ai.js` Script asset. This reuses import identity, module dependencies,
prefabs' existing script references, content fingerprints and standalone exports.
There is no second scene/asset pipeline. View > AI actions and decisions creates
and edits definitions, protects unsaved drafts and exposes live agent scores,
rejection reasons, contributions, selected targets and timers. Panel visibility
uses existing saved layouts, themes and UI scaling.

The JudasJS UtilityAgent wraps native decisions with synchronous start/update/stop
handlers. Ownership is per entity/script slot. Script reload, faults and normal
entity/region teardown retire native state at existing lifecycle boundaries.
There is no detached/background agent loop. Explicit snapshots travel in ordinary
project script state; restoration reacquires callbacks and re-enters an active
action on its next tick, preserving native clocks/cooldowns/commitment. Project
handlers must make that re-entry safe for their own one-time effects.

The new integration also guards two adverse callback cases: coercible values
cannot dispose an agent while a borrowed native reference is being used, and
update-to-finish/start-to-dispose return actual final outcomes rather than a
stale running status. Foreign slots cannot observe, restore, finish or dispose
another slot's agent. The original script budget and save-size limits remain.

## Verification

All results below are automated Linux checks, not operator gameplay acceptance.
Final-source suites and commands are recorded in the evidence directory.

* Native core: **165 checks, zero failures**. Known-answer curves, weights, ties,
  targets, invalidation, cooldown/commitment, missing inputs, bounds, duplicate
  JSON keys, transactional rejection and restored-versus-uninterrupted decisions.
* Real script bridge: **21 checks, zero failures**, including foreign ownership,
  exact-once automatic cleanup, lazy sensing, callback completion/disposal,
  hostile coercion rejection and deliberate reload.
* Editor authoring: **37 checks, zero failures**. Create/import/save/reopen,
  conflicts/read-only/path and symlink rejection, Unicode paths, dirty draft
  protection, and actual ImGui draw data in dark/light at .85/1/1.5 scale.
* Modern save fixtures run in separate processes: native elapsed time continues
  from .12 to .14 seconds and active age from .10 to .12, without an extra sensor
  evaluation. Testing only a restored action name was explicitly insufficient.
* Full native Workshop application: **41 checks** for real gameplay/save/load and
  **37 checks** in a separate cold process. Actual MK2 endpoints match exactly
  before stepping; native selection, clocks and cooldowns match committed save
  state; the next fixed step advances the physical actor and native clocks.
  No completed reward is replayed.
* Workshop 80-second watch and 90-second input runs pass. They cover all six
  native action types, physical travel, six meals/eight rests/four conversations,
  damage-driven cover/recovery, and the closed-pantry/restock negative control.
* Actual editor edit/Play/Stop smoke passes on an isolated project; the authored
  scene is identical afterward. A second run opens the AI panel using isolated
  SDL user data. Personal preferences are not the feature's configuration store.
* A complete exported package is renamed after export, then runs outside the
  repository from `/tmp`, with no JUDAS_ENGINE_ROOT. The final 80-second gameplay
  trace still completes six meals and two shelter arrivals. Both definition
  modules and the real script/UI/texture assets ship through ordinary export.
* Focused existing script, deliberate developer-tool reload, editor preferences,
  workspace, Basic Water, advanced-fluid policy, MK2 physics, network scripting
  and origin/streaming suites pass. No full-suite certification is claimed.
* API/declaration checker passes, including 59 examples and seven negative drift
  controls. The extractor now understands the separate built-in AI module and
  excludes actual ECMAScript private fields/methods, rather than pretending they
  are public API. Runtime API enumeration was not part of this checker run.
* Validation-registration Python tests pass seven, skip two existing cases.

The first Workshop startup rejected its incorrectly authored save-identity length;
that project configuration was corrected and the original failure retained.
An early script lifecycle test expected destruction before the existing safe
synchronization boundary; the final check proves immediate owner unpublication
and exact-once retirement at that actual boundary. No engine ordering was changed.
Legacy WorldState injection does not establish modern restore-callback semantics;
the cold-save qualification uses modern participants and verifies native timers.

## Measured cost

Release GCC 15.2, Ryzen 5 5600/Linux, 120 measured ticks at 1/60 s after warm-up.
Final timing runs were sequential without concurrent builds or qualification jobs.
These are decision/bridge workloads, **not whole-game frame rates**. Candidate
construction is included in the script case; physics, rendering, expensive world
sensors, inspector polling and saving are excluded from timed loops.

| Workload | Median ms | p95 ms | Worst ms |
| --- | ---: | ---: | ---: |
| 100 native agents, 6 candidates, synchronized | .361 | 5.934 | 7.005 |
| 1,000 native agents, 6 candidates, synchronized | 3.900 | 62.305 | 68.645 |
| 100 native agents, 6 candidates, staggered | .824 | .886 | .954 |
| 1,000 native agents, 6 candidates, staggered | 8.709 | 9.434 | 9.964 |
| 100 ordinary scripts, AI disabled | .039 | .042 | .055 |
| 500 ordinary scripts, AI disabled | .301 | .449 | .475 |
| 100 actual JudasJS agents, 3 candidates, staggered | 1.556 | 1.958 | 2.800 |
| 500 actual JudasJS agents, 3 candidates, staggered | 8.426 | 9.494 | 14.874 |

Native runs execute the same 1,000/10,000 evaluations for 100/1,000 agents in both
schedules. Staggering changes when work occurs, not its quality or count. Initial
and repeated raw runs are preserved: worst timings vary on a shared desktop.
Synchronized scoring is a demonstrated hitch risk, not an invisible solved case.
Peak RSS is recorded for entire benchmark processes, not incremental per-agent
memory. No guarantee that 1,000 arbitrary NPCs fit a game's remaining frame budget.

## Files and initial boundaries

Core: `src/UtilityAI.{h,cpp}`. Script bridge/lifecycle:
`src/ScriptUtilityAI.{h,cpp}`, `src/ScriptUtilityLibrary.h`,
`src/ScriptSystem.{h,cpp}`. Editor: `src/editor/UtilityAIEditor.{h,cpp}` plus
panel/application/preferences wiring. Tests: `UtilityAITests`,
`UtilityAIScriptTests`, `UtilityAIEditorTests`, `UtilityAIApplicationTests`.
Benchmark: `tools/benchmarks/UtilityAIBenchmark.cpp`.
CMake, the authoritative validation manifest, declarations, API extractor,
inventory and guides are updated. Evidence fingerprints list exact changed files.

This does not provide a behaviour-tree graph, GOAP planner, automatic cover finder,
resource reservations, squad coordination, crowd movement or networking authority.
Developers compose these policies from existing Judas tools and their own actions.
Only the latest bounded decision diagnostics are retained, not a history recorder.
Candidate eligibility is refreshed at authored decision cadence; action execution
must check target validity before using it. There is no global frame-budget
scheduler. Cross-platform floating-point lockstep is not promised; deterministic
replay requires the same build/inputs/ticks and deterministic project code.

Snapshots include normalized definitions and must fit the existing 64 KiB total
per-script state allowance. Changed definitions require an explicit project
migration/restart policy. Existing unsupported world combinations remain rejected.
Windows, Android and browser execution were not qualified for this feature in
this checkpoint; the implementation has no new platform service dependency.
No main promotion, release tag, publication or Advanced Fluid policy change.

## Human acceptance

1. Open Little Haven in the updated editor; Play and watch both villagers.
2. Try H/T/C needs, D injury, F pantry availability and A watch-tour toggle.
3. Inspect live scores/reasons under View > AI actions and decisions.
4. Stop; open `citizen.ai.js`, change a curve/weight and save. Play again and
   observe the changed decision. Exercise draft save/discard and existing layouts.
5. Save with K, quit/reopen and load with L; inspect progress and physical positions.
6. Try the relocated standalone and your own two-action NPC script.

[Raw evidence and reproduction](evidence/utility-ai-20261101/README.txt).
