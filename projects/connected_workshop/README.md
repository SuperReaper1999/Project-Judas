# Connected Workshop — M99

A small cooperative freight yard with two authored CharacterMotor operators,
twelve ordinary rigid crates, delivery bays and one host-authoritative world.
All controls, shoving, scoring and camera policy live in `Assets/scripts/game.js`.
No session starts until you choose Host/Join; offline practice works immediately.

## Play locally with two independent processes

Quick launcher from the repository: `python3 projects/connected_workshop/launch_local.py`.
If `launch_local.py` is copied beside an exported runtime, `python3 launch_local.py`
uses its ordinary `game.judasproj`. `--port 29099` selects another local port.
Or launch each instance yourself:

1. Open `connected_workshop.judasproj` in the editor and press Play, or launch
   the exported `judas` runtime. Choose **HOST** / **H** in the first process.
2. Launch another standalone process. Leave the address as `127.0.0.1` and choose
   **JOIN** / **J**. The guest controls the amber operator; host controls blue.
3. Focus each window separately to drive its operator. Both see the same crates.
   The editor can host while the standalone joins. Session input is independent
   of keyboard/device/player identity; nothing polls raw keyboard codes.

**WASD / left gamepad stick:** move in the overhead view. **E / South:** shove
the crate ahead. Move close, face it, and shove it into either coloured bay.
Each crate scores once. The shove is a normal ray query and impulse at its hit
point; crates collide, rotate, stack and push one another physically.

**L / Disconnect:** leave the session. **J / Join:** reconnect to the entered
address. The host keeps running if a guest leaves. **T / Transfer Cargo 1:** the
host delegates the first crate to the guest, or returns it to host authority.
Simulation remains server-owned; project code gates permitted interactions.
The UI activation binding is **Return / East**, separate from the South shove
binding, so leaving the pointer over a connection button cannot swallow a shove.

No whole-world pause button is provided: a client's menu/focus cannot pause
everyone else's physical world. Stop/restart the project for a fresh round.

## LAN / Android

Host on the desktop and enter its LAN **IPv4** address in the phone's Server
field before Join. Use the same project/package content and port **27999**.
Allow the selected port through the host firewall. This does not configure NAT,
port forwarding or relay services. The initial demo supports one active guest
operator; extra connected peers can observe but are not assigned more avatars.

Touch: drag in the lower-left arena area (above the footer), touch lower-right
to shove. Host/Join/Disconnect/address controls are normal Judas runtime UI.
Portrait uses the same overhead camera policy; landscape is easier to play.
Physical Android device acceptance is separate from desktop validation.

## Launch configuration

The generic runtime network launch settings can choose host/join automatically;
`game.js` reads `network.launch` and applies ordinary public session operations.
The `serverAddress` and `port` exported script properties are editor-authorable.
Command-line address/port overrides are displayed and retained for subsequent
Disconnect/Join/Host actions; an edited address field selects a new destination.
Dedicated mode skips camera/input policy while running the same host game rules.
See the current networking reference for the exact runtime command-line options.

## Ownership and scope

- Authored entity IDs are ordinary project references. Native network identities
  are separate; no local player/device/entity handle is used as a wire identity.
- The host runs both motors, queries, impulses, score and target state. Clients
  send only three validated channels: `move_x`, `move_y`, `interact`.
- All replicas are authored roots in the same scene. This project deliberately
  does not imply runtime prefab, terrain, scene-transition or whole-world physics
  replication. It does not claim client physics lockstep or rollback prediction.
- Live sessions, sockets, device tokens and peer control claims are not saved
  game progress. Reconnection and persistent profile policy are separate concerns.
- No public Internet security, matchmaking or production anti-cheat claim.
- Shared score is project state in a replica; the engine contains no cargo or
  cooperative-game rules. Changing a shove impulse/speed needs no C++ rebuild.

## Human check

Host and join two processes; move both independently; shove the same crate and
watch its physical endpoint agree; deliver several crates and compare scores;
transfer cargo authority; disconnect/reconnect; stop/restart editor Play; repeat
using a relocated standalone. Test a second machine/phone separately if available.
