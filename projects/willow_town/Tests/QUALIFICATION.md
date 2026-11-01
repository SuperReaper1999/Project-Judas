# Willow Town — automated Linux qualification

Base: `bc65f6f079d22a7c5789a99759dce028972c8c50`, Experimental, 1 November 2026.
Main and existing games are unchanged. This report records automation and visual
inspection by the agent; it does not record human acceptance.

## Final results

- **115/115 native application checks**: six actual native brains, camera-based
  world clicking, correct selected name and need percentages, Take control,
  ordinary WASD physical motion, Return to AI, pause and 3x policy speed.
- **600 game seconds** of live gameplay. Every resident worked, ate and rested,
  and every resident completed more services between 400 and 600 seconds.
  At 600 seconds: 167 work tasks, 64 meals, 46 rests, 46 participant chat rewards.
  Conversation rewards include both participants; they are not 46 distinct chats.
- A closed shop grants no meals/sales despite increased hunger, and actual native
  diagnostics reject eating. Reopening/restocking permits real purchases again.
  Each wallet independently matches `8 + 3*workDone - 2*meals`.
- **56/56 fresh-process save checks**: exact six physical endpoints before
  stepping, needs/economy/service progress, native selection/cooldowns/clocks, and
  continued real movement after the next step. Same-process restore also passes.
- Normal editor Play/Stop and saving preserve the exact authored scene; isolated
  user preferences and project copies avoid touching the operator's editor state.
- A fresh complete standalone export runs from `/tmp` with `JUDAS_ENGINE_ROOT`
  unset, for 1,200 frames. Runtime and exporter provenance match. No authoring-time
  Python/font dependency is needed by the game.
- Validation manifest: 106 entries/98 native targets; registration module passes
  nine tests with two existing skips. New test compilation is warning-free.

Native runs used GCC 15.2 Release, Ryzen 5 5600, GTX 970/NVIDIA 580.178.04,
1280×800 on Linux. The final application's diagnostic worst fixed-step sample was
5.589 ms; cold restoration recorded 3.708 ms. These are observed samples, not a
cross-machine performance guarantee or a controlled before/after solver benchmark.
The isolated editor reported 16 textures and about 10 MiB resident resources.
No engine solver/render implementation was changed.

## Actual problems found and corrected

1. The first generated scene omitted mandatory settings and sprite fields.
   Native loading rejected it clearly. The generator now emits the current
   schema; native scene/project parsing and real application startup pass.
2. Initially, forward-only person avoidance caused shop congestion, including
   overlapping kinematic residents whose casts could all be blocked by another
   person. Static route replanning could not resolve that interaction. The game
   now explicitly lets its six pedestrians pass each other, while casts against
   walls/furniture remain authoritative and service leases remain exclusive.
   This is a game policy change, not reduced engine collision quality.
3. Aggregate town progress then concealed a single stalled resident. Mara's
   off-grid route omitted its first grid node; the diagonal to the next node cut
   a real wall corner. Independent box/segment geometry found an intersection
   8.856% along that segment. Restoring the first waypoint and completing turns
   within 1 cm keeps the route within its authored clearance. No teleport,
   random nudge or ignored wall was added. The final test now detects individual
   stagnation, and the retained old run fails that stronger condition.

In the same six-resident/input workload, old forward-avoidance gameplay completed
71 services at 400 seconds and 88 at 600. Pedestrian passing produced 180/270, but
Mara still remained at six completions and had never rested. The final route repair
produced 220/323 and each resident progressed. These are behavioural outcomes
under deliberately changed game policy, not a physics speedup claim.

## Scope and reproduction

Only the new project, its deterministic art/UI generators, one native application
test and its CMake/validation registration were added. No engine API, fluid lock,
release identity, independent research or accepted project was modified.

Use the commands in [the project README](../README.md). The native test lives at
`tests/WillowTownApplicationTests.cpp` in the repository and needs a fresh output
directory. Its optional `cold` argument loads the save from that same test output
in a genuinely new process. Test outputs are private copies, not this project.

[Raw evidence and fingerprints](../../../docs/evidence/willow-town-20261101/README.txt)
retain failed generation logs, the original crowd/corner sources, exact fixtures,
raw before/after observations, final app/cold/editor/export logs and screenshots.
Tested binaries carry honest base-plus-dirty development identities; exact file
hashes identify what ran, rather than claiming the later commit was embedded.

This is a static authored town with proximity-based social sensing, a small
job/service economy and pedestrian passing. It is not a generic crowd simulator,
universal pathfinder or full social RPG. Fresh Windows, Android and browser
execution, and human gameplay acceptance, are not claimed.
