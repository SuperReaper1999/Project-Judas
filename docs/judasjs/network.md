# Online multiplayer — `network`, `NetworkEntity` (M99)

Optional server-authoritative sessions for small games using the same authored
scene/content. Scripts decide gameplay; the engine validates scalar controls,
replicates explicitly registered state and interpolates client presentation.
[Scope/security](../M99_NETWORK.md) · [Local players](local-multiplayer.md) ·
[CharacterMotor](character.md) · [Physics](physics.md) ·
[Presentation/cameras](effects-camera.md) · [Saves](saves.md).

## Session and connection

```js
import {network} from 'judas';

// Server: asynchronous content verification, then start listening.
network.host({address:'0.0.0.0', port:27999, name:'Outpost',
              controls:['move_x', 'move_y', 'interact']});

// A separate client process instead calls:
// network.join({address:'127.0.0.1', port:27999, name:'Visitor'});
```

These calls return `true` when startup is accepted, **not** when a peer has
connected. Invalid options/world ownership throw `TypeError`; asynchronous content,
socket and handshake failures appear in `network.status` / `network.error`.
There is no blocking DNS: addresses must be numeric IPv4.

`host(options={})` accepts:

| Option | Default | Meaning |
|---|---|---|
| `address` | `"0.0.0.0"` | Listening interface; `127.0.0.1` is local-device only. |
| `port` | `27999` | 0..65535; zero selects an ephemeral port, available as `network.port`. |
| `name` | `"Player"` | At most 64 UTF-8 bytes. |
| `maxPeers` | `4` | 1..7 remote identities, including temporary reconnect reservations. |
| `snapshotHz` | `20` | 1..30 snapshots per simulated second; physics cadence is unchanged. |
| `controls` | `[]` | Up to 32 distinct project logical action/axis names, 1..64 UTF-8 bytes each; paired vectors are excluded. |

`join(options={})` accepts only `address` (default `127.0.0.1`), nonzero `port`
(default 27999), and `name` (default `Player`). Unknown options are rejected.
Host and client must have matching computed content identities and protocol.
Current sessions require a modern, single-scene, unmodded project with the normal
resource/job service. Composed streaming projects are not a supported session root.

`network.close()` closes sockets, removes session-created remote local players,
clears replication/input and invalidates handles. A client returns its replicated
entities to their pre-replication physical/visibility state; a host retains its
authoritative game world. Closing is not a save, scene reload or game reset.

| Getter | Result |
|---|---|
| `mode` | `"offline"`, `"host"`, `"client"`. |
| `status` | `"offline"`, `"starting"`, `"listening"`, `"connecting"`, `"connected"`, `"disconnected"`. |
| `error` | Current diagnostic string, empty when none; not an exception history. |
| `peerId` | `"host"`, the connected client's opaque session ID, or empty before assignment/offline. |
| `port` | Actual host listener port; zero without a listener. It is not the client's remote port. |
| `launch` | `{mode,address,port,dedicated}` copied launch intent. Mode is `""`, `"host"` or `"join"`; scripts still choose how to act on it. |
| `peers` | Copied `{id,name,connected,player}` roster. `player` is a `LocalPlayer` or null. |
| `entities` | Current `NetworkEntity[]` wrappers, not a list of every scene entity. |
| `stats` | Counters described below. |

The host roster includes itself. Only server-side remote peers have the input
`LocalPlayer` association; it does not automatically associate a controlled entity,
camera, device or UI. A network peer is not an input device. Existing local players
and single-player scripts remain independent.

## Authority and replication

`network.replicate(entity, {owner:'host', state:null}={})` is host-only and returns a
`NetworkEntity`. It accepts at most 64 simultaneous entries and 64 distinct root
entities over one session. Forgetting/re-registering the same root is allowed;
forgetting entries does not create an unlimited client restoration cache. The entity must be an existing
authored root with an existing runtime physical-state record, represented identically
in the client's matching scene. A definition alone is insufficient: some body-free
or static roots without a script/runtime record cannot be registered. Ordinary
physical props and CharacterMotors in the game have the required records. Runtime
spawned entities, hierarchies, joint owners, wheels/powertrains, animation/ragdolls,
deformables, sockets, planets, terrain bodies and fluid owners are excluded from
this initial replication representation. Registration is explicit; no scene-name,
object-name or gameplay-class inference occurs.

Each entry synchronizes authoritative root pose, linear/angular velocity and a
project-owned JSON state of at most 1024 UTF-8 bytes. It does **not** automatically
replicate arbitrary component configuration, script fields, UI, inventory,
navigation intent or terrain edits. JSON is validated plain data; executable code,
native handles and functions are not a network RPC interface.

```js
// Host-only example: a physical crate already authored in both builds.
const replicated = network.replicate(crate, {state:{carried:false}});
replicated.owner = remotePeer.id; // explicit control association
replicated.state = {carried:true};
```

`owner` is `"host"`, a known session peer ID or `""` (unassigned). Only the host may
set it. This metadata tells project scripts whose intent to read; it does not
transfer the physics solver to the client, enable arbitrary client writes or
automatically drive a CharacterMotor. All physical simulation remains server-side.
Server scripts must check ownership and interpret input deliberately.

`NetworkEntity` members:

- `id`, `generation`: wrapper identity fields; preserve them unchanged. Its
  constructor only creates a wrapper and does not register anything.
- `valid`: false once forgotten, retired or after session generation changes.
- `entity`: an ordinary `Entity` wrapper. Constructing it does not test native
  lifetime; if the underlying entity disappears before the entry is retired,
  accessing that stale entity's native properties throws rather than returning null.
- `owner`: copied string; host-only assignment.
- `state`: copied JSON; host-only assignment. Mutating a returned object does not
  publish an update; assign a new value.
- `forget()`: host-only, returns true; stops replication without destroying the
  underlying entity. A subsequent snapshot removes it from the client entry list.

Other operations on a stale wrapper throw `ReferenceError`. Valid client wrappers
remain read-only in authority terms: setters/forget throw `TypeError`. Ordinary
`Entity` APIs still exist locally; using them on a replica is not authoritative
network gameplay and may be overwritten by the next snapshot.

## Input and fixed updates

`network.sendInput(values)` accepts only a connected client and returns true.
`values` is a copied plain object containing up to 32 host-declared scalar controls.
Axes must be finite and within [-1,1]; actions must be exactly 0 or 1. Their names
and kinds come from the normal project input map. Invalid data throws `TypeError`. Missing controls are
neutral, not retained commands. Action transitions enqueue reliable, noncoalesced
messages immediately, so a press followed by a release before the next transport
pump is preserved. Latest axis/held state is also sent at up to 30 Hz and may
replace wholly unsent older periodic state. Call it regularly while controlling
a character; an action is a state transition, not an unbounded remote command queue.

```js
import {network, input} from 'judas';
export default class {
  fixedUpdate() {
    if (network.mode === 'client' && network.status === 'connected') {
      network.sendInput({move_x:input.axis('move_x'),
                         move_y:input.axis('move_y'),
                         interact:input.held('interact') ? 1 : 0});
    }
  }
}
```

The host reads `network.input(peerId)` for the last accepted intent. Unknown,
disconnected or stale peers produce `{}`. Inputs neutralize after 250 ms without a
fresh accepted sample; socket receive-idle timeout is a separate deadline. The
remote peer's `LocalPlayer.input` also provides normal logical observations for
server scripts. There is no automatic movement policy, damage system or camera.

Choose `fixedUpdate` for authoritative host movement/physics. Client fixed scripts
can submit intent, but client replica worlds do not advance a second authoritative
physics simulation. `entity.transform` reports the latest authoritative received
pose; it is not the interpolated render pose. Use `presentationUpdate` and `entity.presentedTransform` for
render-following cameras. Client motion interpolation uses received samples with a
100 ms presentation delay and a bounded 16-sample history, clamping to available samples; there is no prediction,
extrapolation, rewind or movement reconciliation guarantee.

## Interest and coordinates

`network.setInterest(absolutePosition, radius)` is connected-client-only and returns
true when queued. This method deliberately takes **absolute double-position metres**,
unlike most local-world physics queries. Position must be finite with norm at most
1e15 m. Radius is finite 1..1e9 m; invalid input
throws. Convert a local position explicitly:

```js
import {network, world} from 'judas';
network.setInterest(world.toAbsolute(player.transform.position), 120);
```

The host filters registered entries by distance from this centre. Out-of-interest
replicas are hidden; the unchanged static scene is not streamed by this filter.
Default interest includes the small authored scene. Interest does not authorize a
client to read private state, suspend authoritative objects or independently
stream a planetary world. Snapshot positions are transmitted in absolute
coordinates and converted into the receiver's current local chart; rotations and
velocities remain ordinary world vectors, without a universal up direction.

## Disconnect, reconnect and lifetime

A disconnected peer's input neutralizes. The host retains a reconnect reservation
for 15 seconds; reconnecting the same client session to the same address/port can
recover that identity through its transient token. Calling `close()` deliberately
discards that client token. After reservation expiry the host removes the remote
local player and clears entries owned by that peer to unassigned. Projects decide
what disconnected characters do; there is no implicit gameplay deletion.

Reconnect tokens are not authentication and are not durable account/save IDs.
They do not survive process restart or become permission to load private player
data. Host migration, crash recovery of a live session and transparent reconnection
across scene replacement are unsupported.

Close the session before saves, loads, scene changes or authored reload. These
operations refuse while a session is active, including failed/disconnected startup
until closed. Save ordinary authoritative progress offline, then establish a fresh
session. Sockets, peer IDs, replica IDs and `LocalPlayer` tokens are not persistence
references. Use existing project save identities for durable game data.

Deliberate script reload also refuses while a session is active. After using the
editor's script reload, Stop and Play before starting a content-checked host/client
session; closing networking alone does not reconstruct the authored script baseline.
This prevents a live, reloaded VM from presenting a disk-content identity as proof
that its current runtime state matches a fresh matching build.

## Statistics and practical limits

`network.stats` is a detached object:

- `bytesSent`, `bytesReceived`: actual transport wire bytes, including framing.
- `framesSent`, `framesReceived`: completed transport frames.
- `backpressure`: queue-full/socket-backpressure observations.
- `coalesced`: transport dropped/coalesced records, normally unsent stale state.
- `rejected`: session and transport rejection counter, not unique offending peers.
- `snapshots`: host generated batches or client accepted snapshots.
- `snapshotAge`: client seconds since last accepted snapshot; not RTT or one-way latency.
- `entities`, `peers`: current entry count and remote/session-peer records, not the
  count of all physics objects or all local players.

Snapshots are capped at 64 KiB. Reduce entries/JSON or interest if a publication
exceeds that bound; the engine reports failure instead of silently dropping fields.
No unlimited bandwidth, malicious-client security or universal world replication
is implied. See [transport, Android LAN instructions and security limits](../M99_NETWORK.md).

### Query and presentation boundary

Client queries read the locally loaded collision world and latest received poses;
they are not lag-compensated server queries. Out-of-interest or forgotten proxies
are hidden and removed from `network.entities`, but their authored query bodies
remain at the last received pose until close/restoration. Interest culling saves
bandwidth/presentation work, not the full scene's memory. Validate gameplay hits
and physical actions on the server. The lab does this explicitly.

On Android, background suspension stops the application loop and can cause an
idle timeout/disconnect; foreground the app and join again. Background hosting
is not supported. Desktop modal UI does not stop socket polling, but pausing a
listen host's simulation pauses its authoritative world; clients cannot advance
it independently. The lab deliberately has no whole-world pause command.

Replica `time.elapsed` advances with its local fixed callbacks while connected or
connecting; it is not a synchronized server clock. Replicate authoritative timers
as project state when a game needs them. Physics still runs only on the host.

Joining from `fixedUpdate` (or its first `start`/`restore` callback) suppresses local
physics immediately for that step. Closing during a step that began as a replica
resumes offline physics on the next complete fixed step. Neither transition
duplicates the callback or advances the local clock twice.
