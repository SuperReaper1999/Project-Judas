# Bring Everything Along — M90 travel lab

An ordinary editable Judas project: Earth-sized terrain, a physical electric
rover, a separate script-driven explorer and streamed service stations. Open
`travelling_lab.judasproj` in `judas_editor`, or run it with `judas`.

## Controls

| Input | Action |
|---|---|
| WASD | Walk; or throttle/reverse/steer while driving |
| Mouse | Orbit the third-person camera around the explorer or rover |
| E | Board nearby rover; leave after braking below walking speed |
| Space | Jump on foot; brake while driving |
| T | Start/stop the physical watch drive; boards automatically when nearby |
| F | On foot, aim at nearby orange cargo and push it with a real impulse |
| N | Ask the nearby turquoise navigation courier to travel across its pad |
| F6 / F7 | Save / load the expedition through normal save slots |
| Escape | Pause / resume |
| R | Reload the authored scene |

Start on foot beside the rover. Press T to watch it drive along a meridian,
through origin changes and streamed stations. Space brakes after stopping the
watch input. Every kilometre a service pad provides cargo and a courier. The
HUD shows actual shaft RPM, tyre contacts, chart transitions and residency.

## Project contents

- `Scenes/earth.judas`: ordinary terrain, RadicalGravity, script, HUD and listener.
- `Scenes/station.judas`: pad, shelter, movable cargo and navigation courier.
- `Assets/worlds/route.judasworld`: twelve geodesically placed snapshot regions.
- `Assets/prefabs/rover.judasprefab`: real wheels and mechanical graph.
- `Assets/prefabs/explorer.judasprefab`: independent CharacterMotor instance.
- `Assets/scripts/travel.js`: all control, camera, tour and residency policy.
- `Assets/navigation/station.judasnav`: baked local-frame navigation.

The watch drive never assigns chassis pose, velocity or wheel spin. Its simple
steering correction and throttle/brake policy operate the authored mechanisms.
Entering hides/disables the separate motor; leaving restores it alongside the
rover with the actual inherited point velocity. Both modes use third-person cameras, presentation poses and local gravity-up.
The on-foot view follows behind the visible character, with a sphere cast to
avoid walls and the rover. The camera explicitly translates its own smoothing
cache on origin changes. F remains limited to cargo within 5 m of the explorer,
rather than measuring interaction reach from the offset camera.

## Human checklist

1. Walk, jump, board the rover and drive/steer/brake normally.
2. Press T; watch at least two kilometres. RPM/contact readings should stay
   meaningful while origin transitions and retained stations increase.
3. Stop the watch drive, brake and leave. Walk on terrain, visit a station,
   push orange cargo with F and issue N near the courier.
4. Save with F6 after travelling. Move, then F7; position, rover mechanics,
   character and residency should return coherently.
5. Pause/resume, then R: no stale camera, handles or previous expedition state.
6. Open both scenes/prefabs in the editor; Play/Stop must preserve authored data.
7. Repeat driving, leaving, save/load and reload in the exported standalone.

This is a single local physics chart, not physics for the entire Earth at once.
Liquid/PBF and atmosphere/combustion live-origin support are excluded. See
[M90 contracts](../../docs/M90_ASSEMBLY_TRAVEL.md) and the
[automated evidence](../../docs/evidence/m90/REPORT.md). M90 release promotion is operator-authorized; the camera correction has automated
checks, without an additional human camera retest being claimed.
