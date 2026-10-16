# M89 — Judas Learns to Play Together

## Candidate and delivery

Base: accepted M88 `109c4386ef750162391655f522c237ecbf51aa7f`.
**Human accepted on 2026-10-16; authorized for main promotion.**
See [operator acceptance](HUMAN_VALIDATION.md) for the exact tested checkpoint
and remaining platform coverage. The measurements below describe that candidate.
No networking, new physics solver, game-specific C++ behavior or later milestone.

- Editable project: [Core Scramble](../../../projects/core_scramble/README.md),
  `projects/core_scramble/core_scramble.judasproj`, `Scenes/arena.judas`.
- Linux deliverables: `.cache/m89/CoreScramble-desktop-final/judas` and
  `.cache/m89/CoreScramble-Linux-x86_64.zip`.
- Android: `.cache/m89/CoreScramble.apk`, development-signed universal ARM64/x86_64,
  target SDK36/minimum26. x86_64 executed natively in the emulator; ARM64 compiled,
  linked and packaged, not executed for M89. Wider Android compatibility is not certified.
- [Architecture/scope](../../M89_LOCAL_MULTIPLAYER.md),
  [JudasJS reference](../../judasjs/local-multiplayer.md),
  [reproduction](REPRODUCE.md), [genuine failures/follow-ups](FAILURES_AND_FOLLOWUPS.md),
  [provenance](provenance.json), [exact inventory](CHANGED_FILES.txt),
  [source fingerprints](final-source.sha256), [binary identities](binaries.json).
  Packages/SDKs/keys/build products are generated deliverables, not committed assets.

## Reusable mechanisms

`RuntimeWorld` lazily owns `LocalPlayers`; identity is independent of device,
controlled entity and camera. Up to eight process-monotonic, non-recycled tokens;
leaving retires input/HUD/view resources, not the entity. Entity destruction does
not delete a player. Game seats/entity references are saved by project policy;
restoration reconstructs transient IDs/device/focus/GPU state. Headless identities
work without Window/input and reject absent physical assignments.

Window's optional, world-owned hub publishes aggregate SDL keyboard/mouse,
16 gamepad instances at most and captured touch regions. Window keeps a weak hub
reference, releases extra controller handles on teardown and republishes devices
when the world changes. Existing InputSystem maps/edge/catch-up semantics are reused.
Keyboard sets/touch rectangles cannot conflict; mouse/gamepad assignment is exclusive.
Raw matching source levels compose deterministically; independent held buttons cannot
cancel each other. Cancellation, focus, overflow, disconnect/reconnect and world
replacement have focused controls. Touch events cannot masquerade as another
player's physical mouse; actual touch release positions survive an idle mouse.

One authoritative world/fixed clock, one script update/physics step per normal
phase. Cameras never simulate. Optional views reuse Renderer targets, display-tone
mapping, explicit FBO orientation and a proportional 2048-pixel longest-side bound.
Shadow maps are shared; view projection/rays use the actual rounded/capped aspect.
Targets are lazy; failed allocation retries only after a meaningful change or
clear/re-enable. Single-player worlds allocate neither the hub/players nor view targets.
Null checks/weak-reference checks remain small ordinary fixed overhead.

Normal UI documents bind to players/viewports, carry `playerId` in events and keep
independent focus/pointer/slider state. Local modals block owner gameplay while
physics/other players continue; global modals retain whole-world pause. Removal
invalidates bound UI handles. Existing global input/UI and selected-controller
semantics remain the compatibility path. OS capture/cursor policy is still global.

Public surface: `players`, `LocalPlayer`, `PlayerInput`, `UIDocument.player`,
`UIEvent.playerId`. Existing entity, motor, rigid body, prefab, resource, M38 export,
M61 persistence and presentation APIs remain the authority. Types/reference/
inventory and an executed cookbook cover the real runtime, not hypothetical APIs.

## Game

Two to four colored bumper bots compete for 90 seconds, pushing five physical
cores into their own goals. Four cargo crates and bumpers also collide normally.
Finite forces drive motion; jumps/shoves apply real impulses. A goal samples a
real body's position, retires that scored body and spawns a distinct replacement.
Fallen bots lose one point and rebind the same identity to a new body. No decorative
physics clones, teleport-driven movement or extra simulation per view.

`game.js` owns joins/leave/reassignment, forces, objectives, score, round, UI and
presentation policy; `controls.js` owns maps; `math.js` supplies ordinary vectors/
camera arithmetic; generated `content.js` contains asset IDs. Four ordinary bot
prefabs, core prefab, editable scene/UI, portrait/landscape HUD profiles, original
PCM effects and existing licensed DejaVu font are normal assets. No engine game-name
check, native party component or hidden JS dispatch is used.

Shared/split changes preserve the same bodies, identities, points and time.
Controller arrival is handled once rather than stealing back an explicitly released
seat. Local owner menus expose resume/change device/leave. The shared lobby permits
recovery/reassignment without needing a disconnected controller. A save restore
rebinds actual saved physical bots, with no duplicate bodies or endpoint reset.

### Controls

| Seat | Move | Shove | Jump | Local options |
|---|---|---|---|---|
| Cyan | WASD | Space | Left Shift | Tab |
| Coral | Arrows | Right Shift | Right Ctrl | Keypad 0 |
| Gold | IJKL | O | U | P |
| Violet | TFGH | Y | R | B |

Gamepad: left stick, South shove, West jump, Start owner menu. Touch: drag the
left/lower part of your region; tap right/use Shove. V/right shoulder changes views;
F2/F3/F4 or buttons choose roster; Escape/Android Back/gamepad Back opens shared
lobby; Backspace/new-round button reloads. **P1–P4 Input** cycles available devices.
Release an occupied controller to keyboard before giving it to a different seat.

## Focused final validation

| Actual mechanism exercised | Result |
|---|---:|
| Local source/keyboard/touch/limits/focus/edge/UI checks | 37 passed |
| Native SDL virtual gamepads, mixed touch/unplug, reconnect, world replacement | 14 passed |
| Actual QuickJS, two independent CharacterMotors, stale handles, headless identity, exact cookbook | 14 passed |
| Real legacy/HDR camera pixels, orientation, resize, no simulation during render, GPU cleanup | 14 passed |
| Real Application: independent bodies, shove/score, local UI, explicit device release, four joins, leave/rejoin, respawn, cold save endpoints, reload, full round | 34 passed |
| Existing input / paired input / paired JS | 40 / 43 / 21 passed |
| Existing UI / UI Application and moved export | 22 / 16 passed |
| CharacterMotor / render cameras / audio | 32 / 52 / 59 passed |
| Existing M88 Breakwater Application including physical contact, save/load/reload, win/loss and powerups | 31 passed |
| TypeScript 5.9.3 / source drift / actual VM enumeration | 422 symbols, 37 exports, 286 native operations; 45 checked examples; passed |
| Editor normal Play/Stop | authored scene IDENTICAL; save OK; no failed resource loads |
| Ordinary M38 export / moved Linux package from unrelated `/tmp` cwd | 15 assets, one scene; roster/split/pause/capture passed |
| Android16 native x86_64 | single-owner negative control + simultaneous touch moves actual bots; owner menus; two virtual Android InputDevice pads; four-view captures; rotation/background/resume; passed |
| APK signature / 16 KiB zip alignment | v2/v3 and zipalign checks passed |

Linux Release editor/runtime/export/tools built. Earlier baseline warnings in
RuntimeWorld and Android Clang's existing code remain; no zero-warning claim.
No full unrelated production-suite marathon. Android uses actual native engine/GL/
QuickJS/physics, not streaming or a browser shell. Virtual devices/emulator are
not human or hardware-controller certification. Windows M89 was not run and is
nonblocking. Raw logs/device dumps and the copied font licence retain their original trailing
whitespace; the scoped source/documentation whitespace check passes.
Logs are under `linux/`, `android/`, `build-package/`; negative source/
logs and corrected native UI captures are preserved in `failures/`.

The GLES render-target portability correction chooses format-compatible null
allocation types (RGBA8/UNSIGNED_BYTE, RGBA16F/HALF_FLOAT). Existing desktop sampling
semantics are unchanged. See the primary [Khronos GLES texture reference](https://registry.khronos.org/OpenGL-Refpages/es3/html/glTexImage2D.xhtml).

## Performance

Linux i7-4790 / GTX970 4 GB, Release, hidden 1024×768 GL window, uncapped swap;
logical-input pilot moves all players and periodically shoves. Same arena, masses,
solver/collision settings and 60 Hz simulation; 2 s startup/warm-up then 5 s measured.
Builds/emulator were stopped before measurement. No affinity/clock lock; ordinary
desktop/driver scheduling remains. Raw frames/steps/commands and bounded M56 captures
are in `performance/`. These are throughput measurements, not monitor refresh.

| Run | Bodies | Frame median / p95 / worst ms | Fixed median / p95 / worst ms | Peak RSS MiB |
|---|---:|---|---|---:|
| 2 shared, M56 on | 20 | 1.287 / 1.977 / 6.377 | .380 / 1.031 / 3.294 | 177.9 |
| 4 shared, M56 on | 22 | 1.446 / 1.905 / 3.026 | .485 / .980 / 1.592 | 121.9 |
| 4 split, M56 on | 22 | 3.019 / 3.751 / 6.481 | .512 / 1.000 / 1.398 | 126.5 |
| 4 split, M56 off | 22 | 2.928 / 3.729 / 6.472 | .570 / 1.003 / 2.724 | 119.2 |

M56 retains the last 120 frames, separately from the full five-second frame/step
samples. Summed script-phase CPU medians: .127/.174/.246 ms; scoped GPU pass sums:
.390/.400/.854 ms for 2-shared/4-shared/4-split. GPU sums exclude unscoped work and
are not total GPU latency. All three profiled cases have 300 physical steps during
five measured seconds; retained frames show no step caps/discarded simulation and
no profiler drops. RSS includes runtime/driver/profiler; M56 reserves an estimated
67.3 MiB, not all necessarily resident. Run-to-run RSS differences are not evidence
that adding players reduces memory.

Android final shared two-player emulator capture: frame median/p95/worst
16.640/17.852/18.615 ms; fixed .396/.660/1.334 ms, no caps/discarded time in retained
active frames. GLES GPU timestamps unavailable. This is host-accelerated emulator
behavior, not physical-phone GPU/thermal/battery performance or four-view certification.

## Limits and preservation

- SDL2 aggregates physical keyboards/mice: disjoint keys and one exclusive mouse
  work; independent raw multi-keyboard/multi-mouse streams are not implemented.
  Keyboard rollover and real-controller mappings need hardware testing.
- Eight identities/16 gamepads/ten touches; game exposes four seats. Four touch
  players are most usable on a tablet. One shared audio listener/mix and OS cursor.
- Shared primary shadows/planetary LOD; no independent world origins or automatic
  multi-player streaming-interest policy. Entity association grants no extra authority.
  Widely separated planetary cameras are not certified.
- Global documents use global primary input; independent focus belongs in player
  documents. Advanced per-player stick-history cursor API is not exposed.
- Native player/device/GPU/focus tokens are ephemeral, not save/network IDs. Games
  persist seat/profile data and entity references, then reconstruct runtime bindings.
- No networking, replication, prediction or server model. No general high-body-count
  physics certification: M87 dense-pile limits remain untouched.
- Protected history/research, independent fluid work and existing game sources are
  unchanged. Only new game content, generic input/UI/view infrastructure, current
  documentation/types/tooling and M89 evidence are in this checkpoint.

## One human checklist

1. Start two-player shared mode. Move Cyan with WASD and Coral with arrows independently.
2. Shove/jump, collide with bots/crates and push a core into your color; confirm score.
3. Toggle V/shared/split; identities, score/time and physical motion should continue.
4. Join four seats (F4/button), try all keyboard partitions or available gamepads.
5. Open one owner's options: only that owner's controls pause; others/physics stay live.
6. Resume without a stray shove. Open shared Lobby: the whole arena pauses/resumes.
7. Unplug/reconnect a controller if available; use Lobby P1–P4 Input to reassign it.
8. Leave/rejoin, recover a fallen bot, finish a round, then start a new round.
9. Reload/Editor Stop/Play: no ghost input, HUDs, bodies or old cameras should remain.
10. Run the moved Linux package. On Android try two simultaneous touch controls,
    both owner menus, split/four-player layout, rotation/background/resume and physical
    controllers when available. Assess small-screen/tablet usability and audible effects.

Automated passes alone do not constitute human acceptance. The operator's later
desktop acceptance and explicit promotion authorization are recorded separately
in [HUMAN_VALIDATION.md](HUMAN_VALIDATION.md).
