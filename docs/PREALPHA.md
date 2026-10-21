# Pre-Alpha stabilization

Judas is in unnumbered Pre-Alpha stabilization. **M100 and an Alpha release have
not begun.** Main is the human-tested release lineage; Experimental holds verified
development checkpoints pending their own human acceptance.

At the start of this documentation cleanup:

| Line | Exact checkpoint | Scope |
|---|---|---|
| Human-accepted main | `e30d5f9bb72a1a341d3eff7485b8d6b3eb6f8ea9` | Curved-mesh continuation and retained-origin precision follow-up |
| Experimental input | `be1b7d634c674dca34f3a36299da510492ca2aea` | Certified primitive fast paths and bounded support-continuation correction; no subsequent human acceptance claimed |

This page supersedes stale current-status summaries, not original reports. Start
with [documentation](README.md), [your first project](FIRST_PROJECT.md),
[architecture](ARCHITECTURE.md) or [JudasJS](JUDASJS.md).
The [current audit disposition](PREALPHA_FINDINGS.md) reconciles all original
findings and distinguishes carried-forward evidence from fresh checks.

The [event-dependency follow-up](PREALPHA_EVENT_DEPENDENCIES.md) records the
21 October Linux correction, exact physical comparisons and rejected experiments.
Most CCD queries are eliminated, but the live dense-pile frame-rate limit remains
open; no new human acceptance or main promotion is implied.

The subsequent [persistent-contact and growing-impact redesign](PREALPHA_INCREMENTAL_IMPACTS.md)
removes the demonstrated re-projection of unrelated resting residuals. Matched
600-step Linux physics cost falls from 67.57 to 30.98 ms mean, with a 535.21 to
104.85 ms worst step. Dense live rendering remains far below 24–30 FPS. This is
Experimental qualification, not human acceptance or closure of the stress limit.

## Evidence and acceptance history

- [Original assessment](evidence/prealpha-pass2/ORIGINAL_ASSESSMENT.md), frozen
  against the accepted M98/M99 baseline `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2`.
- [Pass 1](evidence/prealpha/REPORT.md) and [Pass 2](evidence/prealpha-pass2/REPORT.md):
  human accepted at `52800cedcdf47cab743ade96049eb5de1ab8aeb0`.
- [Liquid baking correction](evidence/prealpha-liquid-rotation/REPORT.md):
  accepted at `f647d7fd95f60d3a37dba70a51f4e45dc41c2922`.
- [Pass 3](evidence/prealpha-pass3/REPORT.md): accepted at
  `7da56c2cc6ce7acda827a6e4019d610622b237e2`.
- [Pass 4](evidence/prealpha-pass4/REPORT.md): accepted at
  `4c141e9bb6c68a50c63dec263dcec398200328ad`.
- [Pass 5](evidence/prealpha-pass5/REPORT.md) preserves its original failed CCD
  experiments and checkpoint-time review status. The subsequent
  [curved-mesh/origin follow-up](evidence/prealpha-ccd-continuation-20261019/REPORT.md)
  corrects the four original continuation contracts and retained-origin rounding;
  the operator accepted `e30d5f9b` for main.
- [Dense fast paths](PREALPHA_DENSE_SCALABILITY.md) and
  [support continuation](PREALPHA_DENSE_SUPPORT_FOLLOWUP.md) develop afterward on
  Experimental. Their physics measurements, rejected alternatives, mixed
  penetration observations and remaining real-time limits remain explicit.

Historical pending-review language remains in exact evidence. Acceptance of one
checkpoint does not establish unexecuted platform, performance or all-feature
coverage for later work.

## Ownership and integration contracts

| System | Authority | Supported composition | Explicit boundary |
|---|---|---|---|
| Authored scenes/prefabs | Scene and stable IDs | Editor, runtime construction, export | Play does not mutate authored content |
| Physical motion | Fixed-step PhysicsWorld/CharacterMotor | Bodies, constraints, wheels, powertrains, characters | Scripts supply intent; presentation does not own physics |
| Resources | ResourceManager and publishing owner | Private preparation, staged upload/replacement | Failure/cancellation retains last-good resources |
| Origin changes | RuntimeWorld transaction | M90 assemblies, conserved liquids, analytic atmosphere/thermal carriers within one chart | Legacy PBF/emitted fluid and unsupported pending transactions reject |
| Terrain edits | RuntimeTerrain | Local field, collision/navigation replacement, origin translation, cold restore | Bounded static unit-scale volumes; no universal planetary excavation |
| Regional suspension | WorldStreaming | Supported retained entities, constructed prefabs, terrain | Unsupported groups/external dependencies pin with reasons |
| Cold saves | SaveService/WorldPersistence | Private candidate restoration, explicit M98 migrations | Unknown/native topology and opaque inactive records are not generally migratable |
| Online replication | NetworkSession host | Documented authored-root transform/body/motor subset | No universal mechanics, liquids, terrain or arbitrary spawned-identity replication |
| Script debugging/reload | ScriptSystem owner | Safe-boundary inspection and deliberate reload | Stop/Play required before compatible cold saves/online use after reload |

These contracts are independent. One passing subsystem does not certify every
combination. Implementation: [origin](../src/WorldOrigin.cpp),
[terrain](../src/RuntimeTerrain.cpp), [streaming](../src/WorldStreaming.cpp),
[saves](M98_SAVE_EVOLUTION.md), [networking](M99_NETWORK.md).

### Important supported combinations and limits

- Conserved liquids keep capacity geometry, wave allocation, surface samples and
  discharge history owner-local. Owner/parcel positions translate, optical caches
  rebuild, and participants remain inside the 8 km chart. Supported uniform/radial
  basins continue transfers and modern cold saves after shifts. Unpublished
  geometry rejects nonzero origin changes until normal publication.
- Analytic atmosphere translates its frame; combustion retains scalar state and
  existing carrier handles. Origin support does not add modern-slot persistence
  for unsupported atmosphere/combustion participants. Capture failure preserves
  the caller's previous complete snapshot.
- Conserved liquid groups and active articulations retain residency pins where
  lossless group suspension is not established. Live coordinate translation is
  not proof of suspension support.
- Runtime terrain privately prepares collision/navigation replacements. Failed
  publication retains the solid; supported origin/cold-save composition does not
  imply arbitrary concurrent planet-wide excavation.
- Saves preserve unknown records for real loaders to accept/reject. They do not
  invent missing history, automatically merge new authored world content or
  rewrite opaque retained topology.
- A paused owner can pump network heartbeats without advancing scripts/physics.
  Blocking callbacks, a stopped process or an unpumped debugger remain distinct
  timeout conditions. Replication/authority changes retain the explicit subset.

## Focused validation from a clean checkout

Native/game development does not require Node/npm. Follow the
[platform prerequisites](../README.md#build-on-linux), then configure an opt-in
focused subset:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_prealpha_tests -j2
ctest --test-dir build -L prealpha --output-on-failure
```

Manifest entries are registered once, with group labels; native tests run serially.
Application cases need a working OpenGL desktop, or a functional Xvfb/software-GL
setup. Multi-config builds should select their configuration executable directory.
To run selected groups:

```sh
python3 scripts/prealpha_validate.py --list --group lifecycle
python3 scripts/prealpha_validate.py --build-dir build --group core --group network
```

Each invocation records commands, actual executable hashes/embedded identity,
source revision/status, platform, elapsed time and exact failures in a fresh
`RESULTS.json`. Missing binaries, timeouts and wrong exits fail. Wrapped binaries
must match the current source fingerprint: finish edits, configure/rebuild, then
run the final gate. Changing tracked documentation also changes provenance.
A neighboring executable or timestamp is not proof of a matching test binary.

The manifest is a focused subset, not all production suites. Asserted curved-mesh
arrival/continuation checks are ordinary collision gates after the correction.
`known-failures` preserves unresolved asserting reproductions separately; it is
excluded by default, never counted as passing. Opt into CTest with
`-DJUDAS_INCLUDE_KNOWN_FAILURES=ON`. Historical failed logs remain unchanged.

The `resource-handoff` gate generates its own deterministic multipart/skinned
model and embedded images in a fresh output directory. It needs OpenGL but no
private import-lab files. It observes real private upload/cancellation/publication
and records fixture hashes. Its bounded 5 ms observation pause forces a partial
upload boundary; those timings are functional diagnostics. `--original` explicitly
requires the original local consumer assets and fails if they are absent.

For API/types/examples, the only optional pinned dependency is developer tooling:

```sh
npm ci --ignore-scripts --prefix tools/verification
node scripts/check_judasjs_api.mjs
python3 scripts/check_current_docs.py
```

An existing TypeScript compiler can be selected through `--typescript` or
`JUDAS_TYPESCRIPT`. No old milestone cache or automatic download is assumed.
Live VM enumeration remains a separate `--runtime` check. Structural drift checks
and local-link validation do not prove every result shape, callback ordering or
prose claim. See [documentation maintenance](DOCUMENTATION_MAINTENANCE.md).

## Remaining readiness concerns

- **Dense contact latency:** the unchanged 1,024-piece/270-step workload improved
  from mean 92.742 / p95 338.077 / worst 1,658.782 ms to
  46.827 / 158.741 / 387.280 ms. These synthetic fixed-step measurements are not
  interactive FPS. A later real-time run still reached a 948.381 ms step and
  about 1.58 rendered FPS during catch-up. No 24–30 FPS or 60 Hz claim is made.
  [Exact scope and physical observations](PREALPHA_DENSE_SUPPORT_FOLLOWUP.md).
- **CCD scope:** the original four curved-mesh assertions now pass; bounded search,
  native rear selection and deep-overlap limits remain. This is correction of
  demonstrated contracts, not universal continuous-collision certification.
- **FLU-001:** the original legacy-PBF settling test remains deferred and failed.
  Its threshold, fluid implementation and independent research remain unchanged.
- **Half-turn liquid sampling:** stored quantity/geometry remain equivalent, but
  binary32 world-point aliasing can select adjacent finite-volume cells after an
  origin change. The original full query-identity reproduction still fails.
  [Sampling analysis](evidence/prealpha-pass5/liquid/REPORT.md).
- **Legacy cold snapshot:** the original a106 snapshot lacks required historical
  continuation data. Modern cold-save passes do not retroactively supply it.
- **Composition boundaries:** liquid/articulation residency pins, opaque native
  migration, authored-content merge and one-chart limits remain explicit.
- **Network security/scope:** no public-Internet security certification or general
  physical-assembly replication. M99 supports its documented subset.
- **Platform qualification:** Linux is the acceptance reference. Windows VM native
  functional receipts are not physical Windows GPU/performance acceptance. Android
  feature/hardware coverage is recorded per checkpoint; compile/link evidence is
  not current full-device qualification. Host-built glibc 2.43 binaries do not
  inherit the separately tested Ubuntu 24.04/glibc 2.39 SDK baseline.
- **Coverage:** selected regression gates do not certify every native/public API
  member, subsystem combination, controller or developer workflow. Remaining
  original-audit dispositions are [explicit](PREALPHA_FINDINGS.md).

Protected historical evidence and independent research stay unchanged. New
cleanup results are in [the documentation/API/test cleanup report](PREALPHA_CLEANUP.md).
