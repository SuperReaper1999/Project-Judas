# Little Haven — AI Workshop

A small editable village demonstrating one native Utility AI foundation with two
very different policies. Mara chooses food, rest, company and a stroll. Eli
patrols, seeks shelter when injured and recovers before returning to work.

Launch `AI_Workshop.judasproj` with the matching Experimental editor/runtime.
This project requires the UtilityAgent API; it will not run on older Alpha builds.
The watch tour starts automatically. It changes needs and applies damage through
ordinary game policy; every action and target is still selected by native scoring.

| Key | Action |
| --- | --- |
| H / T / C | Raise Mara's hunger / fatigue / need for company |
| D | Damage Eli; watch hide compete with patrol |
| F | Close or restock the canteens; eating becomes ineligible or eligible |
| A | Enable/disable the watch tour |
| Space | Pause/resume this demonstration's agents and needs |
| K / L | Save/load the `little-haven` slot |
| R | Restart the scene |

Mara is the warm-coloured villager on the west side. Orange stands are canteens,
the blue bed is rest, the brown garden bench is company. Eli is the blue guard
on the east side. Grey shelter markers are cover, gold markers are patrol points.

The bottom display shows real needs, health, current selected action/target,
completed services and the latest input explanation. In the editor, open the
**AI actions and decisions** tool to inspect eligibility, curves and selection.

## Where to edit

- `Assets/citizen.ai.js`: data-only definition, editable in the AI authoring tool.
- `Assets/guard.ai.js`: the same format with a smaller self-preservation policy.
- `Assets/game.js`: project-owned sensors, movement, effects, controls and saves.
- `Scenes/main.judas`: actors, target entities, physical obstacles and presentation.
- `Assets/hud.judasui`: ordinary responsive UI, without a special AI HUD system.

AI definition modules deliberately contain exactly `export default <JSON>;`.
Other script code belongs in a separate gameplay module. Definitions are normal
tracked Script assets, so existing import/dependency/export ownership applies.
Target identity uses the scene's decimal entity ID strings; no full-width identity
is converted to a JavaScript Number. Every use reacquires a live wrapper.

## What is physical and what is game policy?

Actors move through actual native 2D kinematic bodies. Each desired move performs
an engine shape cast; collision geometry remains authoritative. This little
open courtyard uses direct steering, **not a general path planner**: adding a wall
across a route can leave an actor conservatively waiting. A full game should use
Judas navigation or its own route planner in the action implementation. Utility
scoring does not promise to solve a failed movement action automatically.

Services only progress while physically at the selected target. Completing a meal
consumes pantry stock, rest reduces fatigue, conversation reduces loneliness.
Action callbacks stop their movement on interruption. Partial service progress is
ordinary saved project data. Injury changes facts; native scoring selects cover.

## Save and lifecycle contract

`state` stores needs, health, counters and each agent's explicit `snapshot()`.
A cold load constructs fresh agent objects, restores their native decision state,
and reacquires fresh entity wrappers. Callback closures are never serialized.
The normal `restore` callback does not replay new-game initialization. The script's
`destroy` disposes both agents and releases its view.

Space pauses only this demonstration's agent ticks/needs, not the entire engine.
K/L use normal project saves; their displayed completion/error is authoritative.
Restart intentionally returns to authored defaults, while loading returns to the
saved decision/gameplay state.

## Reproduction and export

Use `Tests/watch.txt` or `Tests/manual-inputs.txt` with the normal native test harness:

```sh
JUDAS_TEST_SCRIPT=/absolute/path/to/Tests/watch.txt \
  /path/to/judas /absolute/path/to/AI_Workshop.judasproj
```

Run both test scripts into separate logs, then inspect actual gameplay and
negative controls with:

```sh
python3 Tests/qualify.py --watch-log /path/to/watch.log --manual-log /path/to/manual.log
```

The checker requires actual service effects, injury/recovery, unavailable-food
rejection, restocking and physical actor travel; an idle agent or absent decision
mechanism cannot pass by merely loading the scene.

The project emits a bounded `AI_WORKSHOP` JSON observation every two simulated
seconds for reproducible end-to-end evidence. It does not substitute a script's
preferred winner for the engine decision.

Export with the ordinary editor Standalone workflow or matching `judas_export`.
Move the complete resulting folder before launching; do not run only the executable
without its assets. No external AI service, ML dependency or network is needed.
See [the engine AI guide](../../docs/GAME_AI.md) for API and limitations.

## Human checks

1. Watch Mara visit actual service targets; counters and needs should change there.
2. Hurt Eli with D; he should run to reachable shelter, recover and resume patrol.
3. Close food with F and raise hunger: eating must remain ineligible. Restock it.
4. Save while an agent is travelling, quit, restart and load. Check location, needs,
   pantry and selected action continue without duplicate rewards.
5. Edit a scoring weight/curve and compare the resulting scores and behaviour.
6. Inspect the scene and edit target positions; add an obstacle to see the deliberate
   direct-steering limitation rather than expecting automatic pathfinding.

Native Linux application checks completed for the original implementation:
80 simulated seconds of watch-tour gameplay and 90 seconds of manual-input
qualification passed `Tests/qualify.py`. The watch run completed six meals,
eight rests, four conversations and two shelter arrivals. The unavailable-food
negative control blocked eating/rewards until restock. An actual 1280x720 capture
was visually inspected for readable HUD and scene layout. Cold-save/export
qualification and exact tested fingerprints are recorded by the containing
Experimental engineering checkpoint. This README does not claim human acceptance.
