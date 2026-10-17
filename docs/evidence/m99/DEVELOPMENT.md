# M99 development findings and preserved failures

These are observed development outcomes, not invented pre-existing regressions.
Original raw outputs are retained with the delivery evidence.

- Initial transport checks ran in a network-restricted sandbox and could not open
  sockets. The unchanged native executable passed with loopback access. See
  TRANSPORT.md; no weakening of transport checks.
- First native-session fixture supplied a non-hex save identity and failed project
  parsing. Corrected the newly authored fixture to the existing 64-hex contract.
- Second native-session fixture omitted gravity while asserting a settled box.
  Unclaimed space correctly sampled zero. Added an ordinary uniform gravity
  region, kept the physical assertions and added a sampled-gravity check. The
  failed fixture and result are preserved.
- First ordinary-game launch failed strict scene parsing (`render.radius`
  missing). Authored the required existing render/body fields using normal
  defaults. No parser or engine physics change.
- Source review found remote logical input initially used a default map and lost
  short pulses by retaining only the final packet. Final implementation uses the
  project map, publishes each accepted packet into its existing edge accumulator,
  preserves reliable action transitions and neutralizes before fixed observation.
- Source review found a two-sample 100 ms buffer would clamp rather than interpolate
  at 20 Hz. Final bounded 16-sample history uses absolute double positions. Tests
  cover actual endpoints and rebase/close placement; no second physics solver.
- Review corrected replica-capacity double-counting, stale per-player HUD cleanup,
  and processing later buffered messages after rejecting their socket.
- Existing editor legacy-delta save and native persistence paths bypassed the
  new slot gate. All corresponding capture/restore entry points now reject active
  sessions before mutation. Offline positive controls remain passing.
- Deliberate script reload is rejected during a session. Reloaded worlds need
  Stop/Play before a new content-checked session.
- Source review caught default `state:null` being validated as object-only. Final
  validation accepts bounded plain JSON; actual JS checks cover default null and
  array state in addition to object state.

A build command initially named a nonexistent `judas_scene_session_tests` target;
the correct existing target is `judas_scene_transition_tests`. One API check used
a relative TypeScript module path and was rerun with its absolute installation
path. These are tooling-invocation failures, not subsystem passes or engine defects.

Final integration review also caught the replica early-return freezing the
existing `time.elapsed` despite executing fixed callbacks. The client path now
advances its local fixed script clock without integrating physics. A positive
clock/negative body-integration check covers this; no synchronized-server-clock
guarantee is invented. Android compilation exposed a pre-existing missing GLM
dependency on the shared navigation cooker; its direct CMake dependency is now
explicit. No navigation algorithm changed.

Final renderer tracing found ordinary dynamic geometry bypassed the common
entity presentation accessor. Replica history therefore did not reach box/mesh
colour/shadow/transparent submissions or residency selection. These consumers now
use the same optional network-presentation overlay; offline dynamic presentation
retains its existing pose. An actual pixel-level renderer test covers the gap,
in addition to service interpolation and authoritative query checks.

Final adversarial review found two additional session boundaries. A script could
join during its first fixed callback and the enclosing tick would continue into
local physics. The authoritative path now rechecks that role before integration.
An established TCP service could also keep a client waiting for welcome by sending
valid pings. A separate five-second welcome deadline now bounds that handshake;
ordinary idle and connect deadlines retain their own meanings. Dedicated focused
controls cover both boundaries, including timely delayed welcome.

Content review corrected custom launch endpoint retention across Disconnect/Join,
separated gamepad UI activation from shoving, normalized remote diagonal intent
on the host, and made the optional launcher recognize the exporter's normal
`game.judasproj` filename. These are project policy and packaging corrections,
not engine gameplay features. The early APK/export outputs are superseded by
the final fingerprints; neither early package is claimed as the delivered build.
