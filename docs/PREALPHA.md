# Pre-Alpha stabilization

Accepted starting point: `9cf491a13dbb1cd82ab9ab9b916e255af2d579d2` (M98/M99 human
accepted). The unnumbered engineering pass is Experimental-only until the operator
accepts its exact candidate. M100 and an Alpha release have not begun.

See the [first engineering report](evidence/prealpha/REPORT.md) for exact changes,
measured failures, focused results and the consolidated human checklist.

The [Pass2 finding register](evidence/prealpha-pass2/AUDIT_DISPOSITION.md) reconciles
every original priority and finding against its exact preserved assessment text.
Its in-progress status distinguishes earlier evidence, new corrections awaiting
validation and deliberate support limits. For a first visible project with no
controller/input setup, follow [Your first visible Judas project](FIRST_PROJECT.md).

## Ownership and integration contracts

| System | Authority | Supported composition | Explicit boundary |
|---|---|---|---|
| Authored scenes / prefabs | Scene and existing stable IDs | Editor, normal runtime construction, export | Runtime state does not mutate authored content during Play |
| Physical motion | Existing fixed-step PhysicsWorld / CharacterMotor | Bodies, joints, wheels, powertrains, characters | Scripts supply intent; presentation poses do not own physics |
| Resources | ResourceManager and its publishing owner | Private worker preparation, staged upload/replacement | Incomplete or failed products must retain last-good resources |
| Origin changes | RuntimeWorld transaction | M90 assemblies, conserved liquid owners/parcels, analytic atmosphere and thermal carriers within the local chart | Legacy PBF/emitted fluid and pending stream transactions still reject; no silent state deletion |
| Terrain edits | RuntimeTerrain | Local field, collision/navigation replacement, origin translation, cold restore | Bounded static unit-scale local volumes; not universal planetary excavation |
| Regional suspension | WorldStreaming | Retained supported entities, constructed prefabs and terrain | Pending work and unsupported external dependencies pin regions with reasons |
| Cold saves | SaveService / WorldPersistence | Private candidate preparation before publication, explicit M98 migration | Unknown state is not discarded; native topology and opaque inactive records are not generally migratable |
| Online replication | NetworkSession host | Authored roots with the documented transform/body/motor subset | No replication of wheels, powertrains, articulation, liquids, terrain or arbitrary runtime-spawned identities |
| Script debugging | ScriptSystem owner | Deliberate safe-boundary inspection/reload | Reloaded content requires Stop/Play before compatible cold saves or online sessions |

These are separate contracts. One subsystem passing its tests does not certify
every possible combination. Current implementation references:
[origin](../src/WorldOrigin.cpp), [terrain](../src/RuntimeTerrain.cpp),
[streaming](../src/WorldStreaming.cpp), [saves](M98_SAVE_EVOLUTION.md),
[networking](M99_NETWORK.md).

### Selected Pass2 combinations

Live-origin support, regional suspension, cold saves and online replication are
separate admission contracts. In particular:

- Conserved liquids retain owner-local capacity geometry, wave allocation,
  previous surface samples and discharge history. Only owner/parcel positions
  translate; derived world-space optical caches rebuild. Owners and detached
  parcels must remain inside the 8 km chart. Shifted uniform/radial basins can
  continue ordinary paired transfers and restore from modern cold saves.
- Analytic atmosphere translates its frame origin. Combustion retains thermal
  scalars and the same generation-safe carrier handles. This does **not** add
  modern-slot persistence for atmosphere/combustion: unsupported participants
  still fail with their entity/component named. Failed snapshot capture retains
  the caller's previous complete chunk map.
- Conserved liquid groups and active articulation remain residency pins. Removing
  those pins requires a lossless group suspension/publication contract; translating
  their live coordinates does not establish one.
- Runtime terrain remains bounded local volumetric editing with private collision
  and navigation preparation. The composed terrain/origin/save failure checks do
  not imply planetary heightfield excavation or arbitrary concurrent publication.
- Save migration retains unknown native records for their real loaders to accept
  or reject. It does not merge newly authored content automatically into inactive
  regions or rewrite opaque retained topology.
- Pausing authoritative gameplay can continue pumping transport heartbeats without
  advancing physics, scripts or snapshots. A blocking callback, stopped process or
  unpumped debugger owner is a separate timeout risk. Replication remains the
  documented M99 subset; authority does not transfer unsupported mechanical state.

The [Pass2 report](evidence/prealpha-pass2/REPORT.md) records exact exercised cases,
retained failures and remaining coverage. No independent fluid numerical algorithm,
legacy settling threshold or protected research was changed.

## Focused validation from a clean checkout

Native development does not require Node/npm. Build the desired native targets
with the documented platform prerequisites. To register a bounded CTest subset:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJUDAS_ENABLE_VALIDATION=ON
cmake --build build --target judas_prealpha_tests -j2
ctest --test-dir build -L prealpha --output-on-failure
```

`application` tests need a functioning desktop OpenGL environment. CTest registers
each manifest entry once with group labels and runs native tests serially. Use
the named groups to avoid unrelated workloads:

```sh
python3 scripts/prealpha_validate.py --list --group lifecycle
python3 scripts/prealpha_validate.py --build-dir build --group core --group network
```

The `resource-handoff` gate creates its own deterministic multipart, skinned
`.judasmodel` and embedded images inside its fresh output directory. It needs a
real OpenGL context, but no private import-lab source files or ignored cooked
models. The case checks actual partial GPU allocations, cancellation, complete
publication and retirement. Its `fixture.json` records generated content hashes
and dimensions. One bounded 5 ms observation pause after an actual map allocation
forces a private upload boundary on fast GPUs too; reported pump times include
that pause and are functional diagnostics, not normal loading benchmarks.
To additionally exercise the original full-quality consumer
asset, restore the local import-lab assets and run
`build/judas_resource_handoff_tests /absolute/new-owned-output --original`.
Missing original assets fail explicitly; they do not silently substitute the
generated fixture or count as consumer validation.

Every invocation creates a fresh evidence directory, records commands, executable
SHA-256 hashes, embedded identities, platform, elapsed time, source revision/worktree status and exact
failure logs in `RESULTS.json`. Missing binaries, timeouts and nonzero exits fail.
The opt-in CMake wrapper gives each selected test its own provenance query while
calling its original entrypoint. The gate rejects stale binaries by comparing
their embedded source fingerprint to the current checkout, not a neighboring
runtime or file timestamp. Reconfigure/rebuild after editing source.
Deferred findings are listed separately and are never counted as passes. The
manifest is an explicit focused subset, not a replacement for all production
regressions or human acceptance. Multi-configuration builds should pass the
configuration executable directory as `--build-dir`.

Pass2 corrects the curved-mesh **first-arrival** contract with finite swept-SAT
bounds and retains the unchanged asserting test in the ordinary collision group.
The longer glancing/continuation contract remains failed: contact ownership across
fixed boundaries needs further work. Rejected expensive patch experiments remain
preserved. Run `--group known-failures` for continuation and the newly recorded
rotated-liquid bake failures; failures still return nonzero. They are explicitly excluded from the default focused subset, not marked
expected-pass. To include them in CTest use
`-DJUDAS_INCLUDE_KNOWN_FAILURES=ON`. A green focused run does not close this release
blocker or establish complete CCD correctness.

JudasJS API/type checking has one optional pinned developer dependency:

```sh
npm ci --prefix tools/verification
node scripts/check_judasjs_api.mjs
```

An existing compiler can instead be selected by `--typescript /absolute/module`
or `JUDAS_TYPESCRIPT`. No milestone cache path, automatic download or npm runtime
is assumed. Live VM enumeration remains a separate check using `--runtime`.
The checker includes negative controls for missing/phantom members and accessors.

## Findings that remain separate

- **FLU-001:** original legacy-PBF settling issue remains deferred and failed.
  This pass does not change fluid code, thresholds or independent research.
- **Historical continuation:** the original a106 snapshot lacks necessary state;
  new-format cold-save passes do not retroactively repair it.
- **Dense physics latency:** the M91 1,024-body pile includes severe tails.
  Correctness gates do not certify real-time performance. Matched measurements
  and any bounded changes belong in the current evidence, not rewritten M91 logs.
- **CCD arrival and glancing continuation:** the newly asserted fast-box contracts
  still cross curved meshes. Per-triangle and scalar-bound repair variants fixed
  arrival but introduced measured costs in representative curved-mesh workloads;
  they were rejected. Production physics remains byte-identical to the accepted
  baseline. This is a release blocker, not a pass or an implemented correction.
- **Platform scope:** Linux is the acceptance platform. Windows/Android checks
  are recorded individually; missing physical hardware is not a synthetic pass.
- **Network security/scope:** no public-Internet security certification, universal
  replication or live online save/scene migration is claimed.

Original evidence remains under `docs/evidence/m*`. New findings, raw results,
commands, source/binary fingerprints and the engineering handoff are under
`docs/evidence/prealpha/`. Preserve genuine failures and classify their follow-ups.

## Maintaining current documentation

Update the current reference with each public capability change, run API/type
drift and package checks, and keep historical reports immutable. Human acceptance
status belongs in explicit acceptance records; regenerated inventories must not
reintroduce stale milestone status. Prefer a short current guide plus linked
contracts over copying another milestone chronology into every document.
