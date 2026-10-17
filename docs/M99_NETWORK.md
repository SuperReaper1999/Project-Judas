# M99 — Online multiplayer foundation

M99 is an optional, server-authoritative multiplayer foundation. Project JavaScript
supplies game rules; a session supplies bounded transport, peer identities, input
delivery and selected entity replication. It shares the existing authoritative
world rather than creating a second network physics engine.

M98 was promoted provisionally for combined M98/M99 operator testing. That promotion
does **not** record human acceptance of M98. M99 human acceptance is also pending.

See the [JudasJS network reference](judasjs/network.md),
[local-player reference](judasjs/local-multiplayer.md), and
[validation evidence](evidence/m99/REPORT.md).

## Initial support boundary

- One host and a small number of independently running clients, either a normal
  listen-server application or a headless dedicated host.
- The server owns physical simulation. Remote input is validated data, not a
  command to mutate arbitrary engine state. Project scripts decide how accepted
  controls affect ordinary entities and CharacterMotors.
- Explicitly registered, authored root entities with runtime physical-state
  records and matching content on both ends. Definitions without a supported
  physical-state record reject. This initial foundation does not reconstruct arbitrary runtime prefabs or
  replicate every engine component automatically.
- Client presentation interpolates received motion. It does not claim predicted
  local physics, rollback, deterministic lockstep or lag-compensated shooting.
- A connection peer, a session peer, a `LocalPlayer` and a controlled entity are
  distinct identities. Disconnecting a device is not disconnecting a network peer.
- Save/scene replacement remains an offline boundary. Live sockets, transient
  ownership and remote input are not durable save data. Reconnect and re-establish
  a session after changing the authoritative world. Script reload is refused while
  connected; after a deliberate reload, Stop/Play before opening a new session.

These are declared limits, not automatic compatibility promises for every Judas
subsystem. Games must choose authoritative behaviour deliberately.

## Transport and bounded work

`NetworkTransport` uses nonblocking IPv4 TCP sockets, four-byte unsigned network
endian frame lengths, and a bounded payload length. Numeric IPv4 addresses avoid
blocking DNS. Windows uses lazy Winsock initialization; Linux and Android use
native POSIX sockets. No external networking library or service is required.

TCP supplies ordered reliable delivery. Packet loss can therefore delay later
state behind retransmissions; this initial transport is suitable for modest
cooperative/party games, not a claim of competitive shooter latency. Snapshots
can replace entirely unsent older snapshots. Already partially transmitted frames
and reliable control messages are preserved, so coalescing cannot corrupt framing
or discard a control transition.

Default transport limits are eight connections, 65,536 payload bytes per frame,
262,144 queued wire bytes and 131,072 bytes/second per peer in each direction.
The session can choose tighter limits. Each peer also has at most 1,024 queued
frames. A poll accepts at most 16 sockets, sends at most 32 socket calls per peer,
receives at most 32 complete messages per peer, and processes at most 65,536 bytes
per peer in either direction. The shared event batch is bounded to 256 records and
1 MiB of message bodies. Round-robin service prevents a low-ID sender permanently
starving later peers. Inbound saturation applies TCP backpressure instead of
growing an unbounded application queue.

Connection and receive-idle deadlines are explicit. Server connection IDs are
monotonic and are not recycled during a transport object's lifetime. Malformed
lengths are rejected before payload allocation; incomplete frames are discarded
when their connection closes. Diagnostics report failures rather than silently
changing gameplay quality or simulation cadence.

## Trust and security

This is **not an Internet security perimeter**. The initial transport has no TLS,
cryptographic authentication, account service, matchmaking, NAT traversal, relay or
host migration. A reconnect identity must not be treated as proof of a person's
identity. Run trusted development/LAN games, or place an authenticated secure
transport around a deployment. Validation and bounds reduce accidental/malformed
input risk; they do not make trusted JudasJS scripts or a listening game a sandbox.

The host is authoritative. A client must not be allowed to choose arbitrary peer
ownership, entity creation, scores or physical impulses merely by sending data.
Custom replicated JSON remains project-owned and should contain public game state,
not secrets. Interest management is a bandwidth mechanism, not access control.

## Desktop and Android LAN testing

Use two independent processes, not two views in one process. For a phone-to-Linux
test, use a Linux host plus the Android APK built from the **same** game content:

1. Connect the phone and Linux machine to the same Wi-Fi/LAN. Guest networks may
   isolate devices, and mobile data is not a substitute for the same LAN.
2. Start the host listening on an appropriate local interface. `127.0.0.1` accepts
   only that device; `0.0.0.0` listens on all local IPv4 interfaces.
3. Find Linux's LAN IPv4 address, for example with `hostname -I`. Enter that address
   on the phone. Do not enter the Linux loopback address on Android.
4. Allow the selected TCP port through the Linux firewall for the trusted LAN.
   The default session port is **27999/TCP**. No GitHub login or `gh` is involved.
5. Host and client must use matching project/content identities. Active mod overlays
   are unsupported for these sessions. If a handshake refuses the content, use
   matching builds; do not bypass the compatibility check.
6. Leave and reconnect deliberately, checking the visible session diagnostic.

Android requires its normal `INTERNET` application permission. LAN reachability,
router isolation and OS firewall failures should produce connection diagnostics;
they are not evidence of a successful multiplayer connection. Emulator validation
and physical-phone acceptance are separate results.

The desktop runtime accepts `--host PORT` or `--join IPv4:PORT` alongside an ordinary
project path. These populate `network.launch`; project scripts choose whether/how
to start the session. A dedicated process runs the same game scripts without a
window, renderer or audio device:

```sh
build/judas_server path/to/game.judasproj 27999 0
```

The final `0` means run until interrupted; a positive duration bounds an automated
server run. The host still needs the project and collision/script assets. This is
not a browser host or a promise to convert every graphical project into a server
without checking its script dependencies.

## Human review checklist

1. Host the lab; join from a second desktop process or the phone on the same LAN.
2. Move independently and interact with the same physical objects; compare both views.
3. Confirm client controls cannot directly seize a host-owned object or alter scores.
4. Disconnect, observe the retained/retired player state, then reconnect.
5. Try a wrong port and incompatible content; verify useful failure messages.
6. Try delay/loss and observe interpolation and the documented TCP delay limit.
7. Close the session, save/reload or change scene, then host/join again.
8. Repeat with the relocated standalone package. M98 save-evolution testing remains
   part of the combined operator review.

Measured results and exact tested combinations belong to the evidence report;
the mechanisms and limits above do not imply universal cross-platform acceptance.
