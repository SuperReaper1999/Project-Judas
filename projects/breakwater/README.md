# BREAKWATER

A single-level JudasJS block-breaker. Clear 42 ceramic blocks before losing three
lives. The native engine resolves the moving paddle, sphere contacts, restitution,
gravity, impulses and independent multiball bodies. Project scripts own scoring,
block retirement, power-ups, menus, camera and round policy.

## Play

- **Touch:** drag in the lower playfield; tap **Launch**. Use the upper-right pause
  button for resume, new game, save, load and exit.
- **Desktop:** drag with the mouse, or A/D/Left/Right; Space launches; Escape pauses;
  R starts a new game. Menus support pointer, keyboard and logical gamepad input.
- **Gamepad:** left stick moves, South launches/activates, Start pauses, Back resets.
  D-pad navigates menus, East returns. Real controller hardware testing is pending.
- **Gold capsules:** catch one with the paddle. Rewards cycle between 15 seconds
  of physically wider paddle, two additional real balls, and an impulse that slows
  existing balls. Capsules fall under the ordinary authored gravity field. Balls
  use the public per-entity zero-gravity selection, not a different integrator.

The same scripts run on desktop and Android. Rotate to landscape or portrait; the
camera fits the board and UI uses anchors. Save/load is explicit through the pause
menu. Uninstall/clear app data removes Android saves. Reload/New Game restores the
level. There are no in-app purchases or external services.

## Open / package

Open `breakwater.judasproj` in the editor, or run:

```sh
build/judas projects/breakwater/breakwater.judasproj
build/judas_export projects/breakwater/breakwater.judasproj .cache/Breakwater build/judas
python3 tools/build_android.py projects/breakwater/breakwater.judasproj --output .cache/Breakwater.apk
```

See [native Android setup](../../docs/ANDROID.md) and
[M88 evidence/limitations](../../docs/evidence/m88/REPORT.md).

`Assets/scripts/game.js` owns the round, touch mapping, paddle intent and UI;
`ball.js` reacts to real contacts and adds paddle-hit steering through impulses;
`drop.js` reacts to real capsule/paddle contacts. `content.js` holds normal stable
asset IDs. Prefabs/scene/UI are ordinary editable Judas content. The optional
`tools/create_breakwater.py` recreates authored geometry/UI/assets; gameplay scripts
remain independently editable. No scene name or game rule exists in C++.

Current approximations: the assembly is planar with no out-of-plane forces;
destroyed blocks are removed rather than cut into arbitrary fragments; steering
uses authored bounded impulses after actual solver bounces. Slow ball persists
until subsequent collisions/steering change its velocity, rather than a global
simulation-speed cheat. No mobile editor, cloud save, gamepad remapping screen or
physical phone certification is claimed.
