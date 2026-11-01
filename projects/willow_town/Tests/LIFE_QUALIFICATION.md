# Willow Town life expansion — Linux qualification

Experimental continuation of `fcd9b5d9d5f79b6a5179842d6c0771a35c0b2b56`,
1 November 2026. Main and the published engine remain unchanged.

## Implemented

- A persistent six-position job board with actual ownership and vacancies.
- Utility-driven job seeking: travel to the noticeboard, application dwell,
  exclusive vacancy lease and a final ownership check before hiring.
- Dismissal for missed working time, workplace closure and manager action.
- Work output follows the employee's current role, including after changing jobs.
- Sustained hunger/exhaustion health loss, care recovery, injuries and compressed
  town-day aging. Sam starts elderly; Ada starts unemployed.
- Death stops/disables the body, releases employment and leases, disposes the
  native brain and sensing, and preserves a selectable corpse/history.
- Selection shows age, health, employment and job changes. Manager controls can
  pause hiring, dismiss, close workplaces or deliberately injure a resident.
- Version-2 saves preserve this state. A distinct save identity leaves original
  town saves intact; unsupported old state is rejected before native construction.

These are ordinary game-side rules. No engine implementation or public API changed.
The scene, original pixel-art noticeboard, scripts, UI and generators are editable.

## Automated checks before the observation

| Check | Result |
| --- | --- |
| New life application | 150 checks, zero failures |
| Fresh-process life save | 72 checks, zero failures |
| Automatic absence dismissal | 25 checks, zero failures |
| Existing town application, unchanged assertions | 115 checks, zero failures |
| Existing town fresh-process save | 56 checks, zero failures |

The life application uses actual UI input, native decisions and physical arrivals.
It proves hiring rejection despite a real vacancy, resumed hiring, dismissal and
re-hiring, controlled-resident death, replacement into a different role, role-based
output and no post-death rewards. Cold checks compare actual physical endpoints,
job ownership, resident state and living native clocks before and after stepping.

The pure model also checks starvation grace and actual eventual starvation, plus
aging. These isolated controls do not stand in for the requested real-time run.

## Failures preserved and corrected

The first test tried clicking Dismiss in the same frame that fixed gameplay hired
Ada, before the next UI publication. The final test waits for the real enabled
button; it does not force state or weaken employment assertions.

The next native run caught genuine consumer misuse: disabling a corpse made later
kinematic commands invalid. Cleanup and stopping now happen while its body is
still enabled; dead residents receive no subsequent commands, including on load.
Existing native disabling already clears actual velocities and queued movement.
No exception suppression, corpse re-enable or engine patch was needed.

An initial observation smoke used a development executable from an isolated
working directory without its engine fonts. It exited clearly. The successful
smoke and long observation use the complete exported runtime. The observer now
reports early startup failures with a bounded diagnostic log tail.

## Twenty-minute visible observation

Completed: **1,200.143 real monotonic seconds**, with **3,600.600 game
seconds observed** (3.00014 game seconds per real second). The start sample is at
game time 10; final game time is 3,610.600. This is not a fast-forwarded frame test.

- **Five survivors**, all at full health; **one death**: Sam, from old age at game
  time 1,296.000. His natural fixed-step aging path retired his decision work.
- **Two dismissals and three hires**: Ada found her first job; Theo and Sam each
  lost and regained employment. The keeper position was vacant after Sam died.
- **849 completed work tasks, 357 meals, 260 rests and 238 participant chat
  rewards**. Chat rewards count both participants, not unique conversations.
- **382 meals left in stock**. This is a deliberately generous small-town economy,
  not tuned survival scarcity; nobody died from hunger during this run.
- Twenty process-memory samples ranged **75.48–132.09 MiB RSS**. There was no
  unbounded growth in those samples; this does not prove absence of every leak.
- All **362 recorded observations** passed independent wallet and job-ownership
  checks. Sampled regular observations all reported speed 3, unpaused, with no
  directly controlled resident. Periodic samples cannot certify every intervening
  instant. No scripted incidents or manual intervention were injected.
- The final ordinary save completed. A normal game launch then loaded its exact
  bytes; every final observed gameplay field, native decision clock and physical
  position matched. The result was reopened paused in a persistent user service.

The observer used the normal visible packaged runtime, with no frame harness or
physics timestep override. Only the copied coordinator's authored startSpeed=3
and observationRun properties changed. Its owned-window F10 paused and saved.
The outer execution host cleaned up its leftover child after the observer exited
(tool status 143 after successful measurement/save), so the result window was
reopened through the normal Load key. The original save was preserved byte-for-byte;
an inspection-slot copy lives only in this run's isolated profile.

After the measurement, the operator reported deliberately killing Mara in the
reopened game. That is post-run human interaction and is excluded from the census
above. The live window is preserved for further operator interaction.

### Graphics observation retained

One observer launch's start/midpoint/finished external captures show a corrupt
upper-left house and missing solid UI rectangles. The source PNG/HUD bytes match
the successful native application fixture. Subsequent direct and wrapped normal
launches, internal backbuffer capture and external capture rendered correctly.
The 12-second paired graphics probe did not reproduce it. No reliable engine
mechanism was established, and no renderer change was made or claimed as a fix.
The bad captures and successful controls are both retained for investigation;
this intermittent startup/capture anomaly remains open on Experimental.

Raw logs, exact sources, snapshots, screenshot controls and reproduction commands:
[observation evidence](../../../docs/evidence/willow-town-life-20261101/README.txt).

## Human acceptance and limits

The operator has tried post-run injury/death, but acceptance of the complete
expansion and promotion to main remain pending. Windows, Android and browser
execution are not newly qualified. This is a compressed life game with fixed
population, authored routes, simple jobs and pedestrian passing. There are no
births, newcomers, universal crowd dynamics or automatic save migrations.

Try selecting an unemployed person, pausing/resuming hiring, dismissing an
employee, closing their workplace, changing needs and deliberately injuring a
resident. Check the cause/history after death, then save, quit and reload. The
final observation remains open and paused for inspection when successful.
