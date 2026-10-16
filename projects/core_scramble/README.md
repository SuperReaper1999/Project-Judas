# Core Scramble — M89 local multiplayer party game

Two to four bumper bots, one physical arena, 90 seconds. Push/shove white energy
cores into **your coloured goal**. Most cores wins; ties draw. Cores, bots and cargo
are ordinary Judas rigid bodies. Movement is force-driven; shoves and jumps are
real impulses. A core entering an active goal scores and is replaced by a new
body. Fallen bots recover at home with a one-core penalty and the **same player
identity** controlling a new body. No separate game simulation per camera.

Open `core_scramble.judasproj`, or run the packaged `judas` executable. Start with
2 players; use **2/3/4 Players** or F2/F3/F4. **Split/V** switches shared/split views
without resetting bodies, round state or points. Four-player controls:

| Seat | Move | Shove | Jump | Local options |
|---|---|---|---|---|
| P1 Cyan | WASD | Space | Left Shift | Tab |
| P2 Coral | Arrows | Right Shift | Right Ctrl | Keypad 0 |
| P3 Gold | IJKL | O | U | P |
| P4 Violet | TFGH | Y | R | B |

- **Gamepads:** left stick move, South shove, West jump, Start local options.
  A connected unclaimed gamepad takes a keyboard seat, then joins a free seat
  up to four. Device assignment is game policy, not engine ownership.
- **Touch:** each player's area captures its own fingers. Drag from the left/lower
  play area to move; use **Shove** or tap the right play area. Two fingers per
  player can move and shove together. Two-player portrait uses stacked areas;
  landscape uses side-by-side; four players use a grid. Four touch players are
  most practical on a tablet; keyboard rollover may limit four people on one keyboard.
- **Local menu:** `...`/owner pause/Start opens options for that player. The arena
  stays live; other players continue. Owner movement keys/stick/D-pad navigate,
  shove/South confirms. Resume, change input or leave. A local pause is not invulnerability.
- **Shared lobby/pause:** Lobby button, Escape/Android Back or gamepad Back. All
  physics pauses here. Resume/new round/exit; **P1–P4 Input** cycles an available
  gamepad or the seat's keyboard/touch profile. This also lets someone recover a
  disconnected player's controls. Devices are never stolen from another seat;
  release to keyboard first when swapping two occupied controllers.
- **New round:** Backspace or lobby button. **View toggle:** V/right shoulder or UI.

Use the [JudasJS local multiplayer reference](../../docs/judasjs/local-multiplayer.md)
for the underlying API. `Assets/scripts/game.js` owns joins, devices, forces,
objectives, recovery, UI, score and camera policy. `controls.js` is ordinary project
mapping data; `math.js` only supplies camera/vector arithmetic. Scene, UI and
prefabs remain editable normal content. The generator recreates authored assets,
not gameplay scripts. No C++ party-game flag/component/name check exists.

Runtime player/device handles are not save IDs. The round's script state uses
M61 entity references; restore rebinds saved physical bots instead of duplicating
or resetting them. Input gestures, local focus and device tokens are reinitialized.
Save/load is available through existing public developer APIs, not a new save menu.

## Build

```sh
cmake --build build --target judas judas_export -j4
build/judas_export projects/core_scramble/core_scramble.judasproj .cache/m89/CoreScramble-desktop build/judas
python3 tools/build_android.py projects/core_scramble/core_scramble.judasproj \
  --id org.projectjudas.corescramble --name 'Core Scramble' \
  --cache .cache/m89/android --output .cache/m89/CoreScramble.apk
```

See [Android setup](../../docs/ANDROID.md). Use one ABI to reduce APK size.
Android and desktop use the same gameplay scripts/assets. M89 hardware acceptance
is pending; synthetic controller/emulator results are distinguished in
[the M89 report](../../docs/evidence/m89/REPORT.md). No networking is included.
