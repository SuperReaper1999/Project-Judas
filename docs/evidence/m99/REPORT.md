# M99 — Judas Learns to Connect

Recorded 2026-10-17. Base: `84121564230242a404785af4d60a1475cdb83f4c`.
**Linux automated candidate; M98 and M99 human acceptance pending.**

The exact M98 checkpoint was fast-forwarded to `main` as explicitly authorized,
provisionally for combined review. M99 is developed and checkpointed on
`Experimental` only. No M100 or Pre-Alpha stabilization work is included.

## Implementation and ownership

- `src/NetworkTransport.{h,cpp}`: optional nonblocking native IPv4 TCP, explicit
  framing, timeouts, byte/frame/queue/work bounds, fair polling and diagnostics.
  Partly transmitted frames and action transitions cannot be coalesced away.
- `src/NetworkSession.{h,cpp}`: asynchronous matching-content handshake, host
  authority, distinct peer/player/network-entity identities, authored physical
  root replication, public JSON state, explicit ownership, interest and reconnect.
  No socket or session allocation is required by projects that never use networking.
- `src/ScriptNetwork.inl` and `src/ScriptSystem.cpp`: `network` and `NetworkEntity`;
  [types](../../judas.d.ts), [reference](../../judasjs/network.md), inventory and
  live enumeration/drift checks agree. No native-dispatch access in game scripts.
- Existing M89 logical input receives each accepted snapshot through its existing
  edge accumulator. The project map is authoritative; unknown channels, nonfinite
  axes, invalid actions and unauthorized state are rejected. Stale/disconnected
  input neutralizes before fixed observation.
- Existing fixed-step simulation is shared by listen and dedicated hosts. Client
  worlds run scripts and a local fixed clock, without a duplicate physics solver.
  Opening a client session inside a fixed callback suppresses same-tick physics;
  closing a captured replica tick resumes offline integration next complete tick.
- Host physics stays authoritative even when ownership metadata changes. Latest
  received physical/query state is distinct from presentation. A bounded 16-sample,
  100 ms interpolation buffer feeds ordinary geometry, shadows, transparent draws,
  cameras and residency demand. Absolute double positions convert into local charts.
- Scene/save replacement and script reload reject active sessions before mutation.
  Closing restores offline client state, removes remote players/UI associations,
  invalidates network handles and releases gates. Sockets/peer authority are not saves.
- `src/DedicatedMain.cpp`: genuine `judas_server`, with normal project resources,
  scripts and authoritative physics; no SDL initialization, window, renderer or
  audio context. Generic `--host PORT` / `--join IPv4:PORT` are project-consumed
  launch intent, not a hidden demo switch.
- Android uses the same C++ engine, transport, protocol and project scripts.
  Export adds `INTERNET`; the builder can label the APK version. Native Android
  compilation exposed the navigation cooker's missing direct GLM dependency;
  declaring that existing dependency changed no navigation algorithm.

## Playable deliverables

Ordinary editable project:
`projects/connected_workshop/connected_workshop.judasproj`.

**Connected Workshop** is a cooperative freight yard: two independent operators,
twelve colliding rigid crates, two delivery bays, host-owned shoving and scoring,
visible connection state/address entry and explicit cargo-control delegation.
All movement, camera, touch, interaction, scoring and assignment policy is project JS.
Additional peers can observe; the game assigns only its first guest an operator.

Artifacts outside Git:

- `.cache/m99/ConnectedWorkshop.apk`: ARM64 + x86_64, Android API 26 minimum / 36 target,
  development-signed, GLES 3, mobile shadows off. **106 MB approximately**.
- `.cache/m99/ConnectedWorkshop-Linux-Release/`: normal standalone export.
- `/tmp/judas-m99-candidate/ConnectedWorkshop/`: relocated tested copy, including
  optional two-process launcher and play instructions. Not durable project source.
- `.cache/m99/ConnectedWorkshop-Linux.tar.gz`: convenient copy of that package.
- `build/judas_server`: optional native headless host; use matching exported content.

The APK fingerprint and native ABI fingerprints are in [android/BUILD.json](android/BUILD.json).
Source and binary manifests identify the final candidate; generated executables,
SDKs, signing credentials, APK and build caches are excluded from the commit.

## Focused final-source validation

Release build succeeded. This was an incremental final-source build, not a new
clean build or full production-suite run. The final desktop compile log has no
warnings. Earlier Android compilation logs retain existing unrelated warnings;
no claim of a warning-free entire Android codebase is made.

| Check | Result |
|---|---:|
| Native transport: framing, partial I/O, refusal, bounds, flooding, idle/rebind | 59 / 0 failures |
| Session: authority, content mismatch, input pulses, lifecycle, origin, reconnect, handshake deadline | 105 / 0 |
| Real JS fixed-callback role changes; actual rigid/motor endpoint and clock | 24 / 0 |
| Real colour pixels, second view, shadow depth, blending, hidden/offline controls | 73 / 0 |
| Public JS and actual-game adversarial diagonal intent | 37 / 0 |
| Real application: controls, scoring, portrait layout, custom-port rehost, reload/shutdown | 34 / 0 |
| Logical input | 41 / 0 |
| CharacterMotor / local multiplayer / local-player scripts | 32 / 37 / 14; all pass |
| Existing input / scripts / presentation / physical continuation | 40 / 35 / 44 / 112; all pass |
| Scene transition | 23 / 0 |
| M98 migration unit / cold application v1 write → v2 → v3 | 40 / 16 / 15 / 16; all pass |
| API types / source drift / live runtime surface | 40 exports, 473 symbols, 311 operations, 17 callbacks; pass |
| TypeScript 5.9.3 / examples / negative drift controls | 50 examples checked, 4 drift controls, 16 result shapes; pass |
| Live enumeration harness | 9 / 0; not a claim that all 50 examples executed |
| Editor automated Play/Stop, authoring and save | Pass; authored scene byte-identical after Play/Stop |
| Independent host + two clients + impairment proxy | 14 end-to-end assertions; all pass |
| Relocated export + exact APK content, native two-process game | Host 11 / client 10; score 1 each, endpoint difference 0 m |
| Real headless executable, no DISPLAY/Wayland, invalid SDL video driver | Pass; actual guest motor moves, 12 client checks |
| Shipping relocated runtime from unrelated `/tmp` working directory | Exit 0, actual HUD/render screenshot, no script diagnostic |

Raw results, commands and executable hashes: [focused/RESULTS.json](focused/RESULTS.json),
[process/RESULTS.json](process/RESULTS.json), [package/RESULTS.json](package/RESULTS.json).
Tests assert actual resolved physical endpoints and rendered pixels, not only
cached status or exported symbol presence. No whole-production-suite marathon.

## Measurements

Linux i7-4790, GTX 970 4 GB, Release; exact environment in
[ENVIRONMENT.json](ENVIRONMENT.json). Network timing run precedes Android compilation
and emulator work. These are modest-session measurements, not scale certification.

Native 9-second two-client test: one replicated dynamic body, one replicated motor,
ordinary floor, genuine impulses/control, 539 host fixed steps. Median / p95 /
worst authoritative step: **0.091 / 0.330 / 1.082 ms**. Host sent **180,017 B** and
received **30,990 B** across the run, with 183 snapshot publications. Both clients
converged to the actual resting host endpoint.

The proxy adds 75 ± 15 ms **per direction**, omits one in five application snapshot
messages and cuts one real TCP connection. It omitted 22 snapshots; peak retained
proxy queue 1,788 B; no bound violations. This is **not IP packet-loss emulation**.
TCP retransmission/head-of-line behavior is not measured by application omission.
Reserved identity reconnect: 180.76 ms connection gap, 200.10 ms presentation gap.

In-motion position error against the actual host trajectory at the declared
presentation lag (metres converted to millimetres):

| Path / subject | Median mm | p95 mm | Worst mm |
|---|---:|---:|---:|
| Direct, body, 100 ms lag | 6.55 | 16.02 | 37.92 |
| Direct, motor, 100 ms lag | 1.97 | 2.78 | 10.20 |
| Impaired, body, 175 ms nominal lag | 15.15 | 62.22 | 100.45 |
| Impaired, motor, 175 ms nominal lag | 5.22 | 12.87 | 20.35 |

All raw traces and analysis selection are preserved. Motion selection uses actual
endpoint finite differences, not motor velocity (which can include a supported
gravity contribution). The earlier incorrect analysis and its separately corrected
report remain in `development/process-final-v2`; no trace or physics retuning.

Actual twelve-crate game across relocated desktop/APK content, 6.07 s: host sent
523,608 B, received 11,500 B; host/client score 1; actual crate endpoint difference
0 m. This is a different workload, not a before/after speedup comparison.

## Android scope

APK signature verification and 16 KiB zip alignment pass. API 36 x86_64 emulator,
SwiftShader: actual native install/start, normal address editing and TCP Join to
Linux `10.0.2.2:27999`, touch-driven movement and background/resume pass. Screenshots
show peer 1 connected and the amber operator moved. A separate native observer
received the actual host motor at x=8.43259 m versus authored x=4 m. It was a later
spectator; the Android peer was controlling the game motor.

Explicit Disconnect returned offline. The later Join was attempted after the
bounded 180-second Linux server stopped; it showed the correct connection-refused
diagnostic. **This does not claim a successful Android reconnect test.** Linux
reserved-identity reconnect is independently covered above. Emulator automation
first tapped a keyboard key instead of Join; corrected text-entry/tap sequence is
not classified as an engine defect. App-only logs are preserved; no unrelated
Android log buffer is included. The emulator created for this test was closed.

ARM64 is compiled/packaged, not run on physical hardware in this task. Operator's
Android phone ↔ Linux acceptance remains pending. Windows networking was not run;
Winsock implementation is unvalidated and nonblocking. No emulator frame timings
are represented as device performance.

## Limits and security

- Trusted small IPv4 LAN/cooperative sessions. TCP, no encryption, authentication,
  accounts, NAT/relay, discovery, host migration or Internet security certification.
- One modern unmodded, non-composed authored scene, matching project/content.
  Start from a fresh content baseline; disk hashes are not in-memory attestation.
  Editor deliberate reload is blocked while active and requires Stop/Play before hosting.
- At most 7 remote peers, 64 distinct authored replicated roots and 64 KiB snapshot
  frame; per-entry JSON ≤1,024 B. Combined JSON/pose data can hit the frame limit
  before those independent maxima. Errors report the limit; cadence is not lowered.
- Supported root physical representations only; no arbitrary prefab creation,
  hierarchy, skeletal/deformable/ragdoll/wheel/powertrain/terrain/fluid replication.
  This does not claim a universal whole-world serializer or planetary session.
- Server physical authority, no client prediction/reconciliation, rollback,
  extrapolation, lag-compensated queries or authoritative-server-clock sync.
- Interest hides and omits snapshots; it is not security or full scene streaming.
  Out-of-interest/forgotten query bodies retain their last local geometry until
  close restores the offline world. Replica queries are local approximations.
- Ownership is explicit metadata interpreted by JS, not permission to send arbitrary
  transforms/impulses. Scene changes, save capture/restore and VM reload are offline
  boundaries. No live network identity survives a cold save.
- Demo assigns two operators, no global pause, no hardware-controller acceptance.

## Reproduce

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# Build judas, judas_editor, judas_export, judas_server and the focused targets
# listed by scripts/m99_final_checks.py (plus judasjs_examples_tests).
python3 scripts/m99_final_checks.py --output .cache/m99/my-final-checks
python3 scripts/m99_network_process.py --output .cache/m99/my-process-checks
python3 projects/connected_workshop/launch_local.py
build/judas_server projects/connected_workshop/connected_workshop.judasproj 27999 0
```

Use fresh evidence directories; commands intentionally refuse to overwrite old runs.
Local socket/window access is required. Android:

```sh
JAVA_HOME="$PWD/.cache/m88/jdk17" python3 tools/build_android.py \
 projects/connected_workshop/connected_workshop.judasproj \
 --output .cache/m99/ConnectedWorkshop.apk --cache .cache/m89/android \
 --abis arm64-v8a,x86_64 --jobs 4 --name 'Connected Workshop' \
 --id org.projectjudas.connectedworkshop --version-name M99
```

See [ANDROID.md](../../ANDROID.md) for SDK/JDK prerequisites. Package reproduction:
`scripts/m99_package_checks.py --help`; it copies to a fresh unrelated directory,
extracts only APK game content and exercises the actual dedicated/runtime paths.

## Combined human checklist

1. M98: open Save Evolution Lab, create v1 progress, migrate to v2/v3, inspect
   preserved progress, incompatible/missing-mod failures and original-save recovery.
2. M99: launch Linux export, choose **HOST**. Install the matching APK. Same LAN;
   phone's Server field = Linux LAN IPv4, **27999/TCP** allowed through firewall.
   Dismiss the phone keyboard, choose **JOIN**. Do not use 127.0.0.1 on the phone.
3. Move blue with **WASD**, amber with phone lower-left drag. Shove with **E** or
   lower-right touch. Compare physical cargo motion and scores in both views.
4. Delegate Cargo 1 with **T** / button; verify host/owner interaction policy.
5. Disconnect/rejoin, background/resume, try a wrong address/port, inspect diagnostics.
6. Try two local desktop processes, custom-port launcher and Editor Play/Stop.
   Gamepad: left stick + South shove, East UI activation (hardware test pending).
7. Close networking before save/reload/scene changes; confirm fresh start works.

Human testing remains authoritative. This delivery records no human acceptance,
changes no protected historical evidence/research and starts no later milestone.

Exact files: [CHANGED_FILES.txt](CHANGED_FILES.txt). Preserved failures and corrections:
[DEVELOPMENT.md](DEVELOPMENT.md). Final source/binary manifests are alongside this report.
