# Linux lifecycle audit results

Archive: `a5ec91426ce0f8476db3bbdbda8ad52f3094d38c37b64a0052c7ef4bd37c2e1f` (`.cache/m78-closure/final-build/libjudas_engine.a`). Three standalone supported-content cases, independently recorded here. **Linux only; no Windows result or whole-subsystem certification.** No runtime/CMake/original test changes were made for this audit.

| Case | Final result | Evidence |
|---|---:|---|
| Streamed CharacterMotor query publication | **46 checks, 8 failures** | `run-04/streaming-fixture/RESULT.json`, `streaming-run.stdout.log` |
| Resolved data-only prefab policy | **17 checks, 0 failures** | `run-03/mod-policy-fixture/RESULT.json`, `mod-policy-run.stdout.log` |
| Navigation input revision during bake | **34 checks, 0 failures** | `run-02/navigation-fixture/RESULT.json`, `navigation-run.stdout.log` |

## LIF-001: CharacterMotor query metadata/enabled state is incomplete before first update

Classification: **newly reproduced pre-existing generic lifecycle defect**, not a demonstrated M78 regression. Priority **medium / P2**: wrong query visibility/filtering at an activation/readiness boundary can affect ordinary gameplay queries before the next character update. This probe does not demonstrate a crash, volume/accounting error, physical response error or unbounded resource growth.

### Exact measured failures

The project authors collision layer 1 and mask 2. Both initial and retained fresh capsules report readable **layer 0 / mask 18446744073709551615**.

- Initial active motor: correct rotated-offset geometry and enabled=true, but wrong layer/mask and the authored layer-plus-tag cast misses.
- Initial disabled motor: correct geometry, but enabled=true instead of false, with wrong layer/mask.
- Revisited active motor: correct retained root/velocity and rotated-offset geometry, but enabled=false instead of true, with wrong layer/mask; the authored filtered cast misses.
- Revisited disabled motor: enabled=false correctly, but wrong layer/mask.

The combined layer-plus-tag query is a real query oracle. It does not separately measure the tag value while the layer/enabled settings are already wrong. The source initializes tags to zero and only publishes authored tags in `UpdateCharacters`, but that tag diagnosis is explicitly source-derived rather than a standalone tag getter measurement.

Both initial and revisit comparisons after ordinary **zero-dt `UpdateCharacters(0)` pass all five geometry/enabled/filter/query checks**. Publication, suspend and revisit did not advance simulation time. Old entity/body handles no longer resolve after retirement, and the new identity is distinct. The offset/root-pose concern from the initial source review was **not reproduced in this fixture**; those checks passed and must not be reported as an observed failure.

### Mechanism

- `src/WorldCharacter.cpp:6–14`: lazy `RuntimeCharacter` creates its query capsule at the correct root-plus-rotated-offset pose but leaves `CreateQueryCapsule`'s enabled/filter/tag defaults in place.
- `src/PhysicsWorld.cpp` (`CreateQueryCapsule`): creates a query-only body with ordinary default enabled state, layer 0 and all-category mask.
- `src/WorldRegions.cpp:61–68`: retained reconstruction correctly remains private and disables its capsule.
- `src/WorldRegions.cpp:46–56`: publication enables an authored `SceneBody`, but a motor-only entity has no such component; its retained capsule stays disabled.
- `src/WorldCharacter.cpp:29–35`: the next character update sets authored enabled/filter/tags and resolved proxy pose, explaining the measured zero-dt follow-up.

`WorldStreaming.cpp`, `WorldRegions.cpp`, `WorldCharacter.cpp` and `WorldScripts.cpp` are byte-identical to HEAD. Hashes and current/HEAD comparison are in `AUDIT_RECEIPT.json`. This is distinct from the corrected modern disk-save path: that path privately reconstructs the motor proxy before callbacks; region/lazy creation uses a different lifecycle route.

No automatic fix was attempted. A future narrow generic correction would initialize query metadata at creation and establish the authored motor query enabled state at region publication, without advancing physics or introducing gameplay rules. It would need staged invisibility, disabled handoff, filter/tag, generation and zero-step/callback regression coverage.

## Verified bounded contracts

### Mod policy

An ordinary data-only scene imports a published base prefab and script, with **no direct cached script slot**. Normal prefab resolution supplies the actual slot.

- Valid explicitly published behavior/schema composes.
- Removing only the behavior schema, while leaving file publication, rejects with `unpublished base behaviour in resolved prefab`.
- Replacing the current source prefab's numeric `speed` with a string rejects with `invalid schema/default/value for speed`.
- A valid typed follow-up composes through the same code.
- No RuntimeWorld/project ScriptSystem was instantiated; the base class constructor deliberately throws if instantiated. The test does not claim a manifest is a security sandbox, nor test every executable import/replacement combination.

The meaningful negative oracle is resolved-slot policy enforcement in `ModRuntime::ComposeContent`, beyond authored-only validation. Removing that enforcement would accept the schema-less indirect slot. Supported linked prefabs were used; nested prefabs remain explicitly unsupported.

### Navigation revision / publication

Two independently authored scenes share a linked primitive physical prefab. Both initial bakes are fresh; a real collider edit makes both stale. At the existing third cancellation poll, **after the actual bake**, the source collider changes again and the callback returns **false**.

- The operation rejects with `Navigation inputs changed during rebuild`, not a cancellation diagnostic.
- All other project/scene/assets/meta bytes match the prior snapshot exactly; no new published orphan exists. Only the legitimately externally edited prefab source is excluded from this equality.
- Private staging is removed; neither dependent is falsely reported fresh.
- A new explicit rebuild from the current source makes both fresh.
- Normal `ValidateSceneContent` export validation accepts both actual new bakes.

This measures the transitive linked-prefab input and post-bake revalidation/publication boundary. It is not a new full export/moved-package launch or read-only-kit mutation proof. Existing source-hidden/moved-package results retain their separate scope.

## Preserved fixture development records

These are actual fixture setup/observer failures, not engine failures:

1. `run-01`: all three compile attempts fail because the standalone support header used a system `nlohmann/json.hpp` include. Judas vendors JSON; the support header now uses `AuthoringJSON.h`.
2. `run-02`: mod-policy compile fails because `ModRuntime.h` only forward-declares `Project`. Explicit `Project.h` and braces fix the fixture; raw error/warning retained.
3. `run-02`: streaming assertion result is the same 46/8, but JSON metadata for `GetCollisionFilter` could be evaluated before the getter and records initial 99/0. `run-04` sequences the getter and enabled observation explicitly; identical assertions and eight failures, now accurate actual fields.

No expected runtime value, timestep, target/source geometry size or pass threshold was changed to hide an engine failure. Source snapshots exactly matching each run's recorded hashes are under `compiled-source/`. Binaries/objects remain only in owned cache; their hashes are recorded, not their build outputs copied into evidence.

## Reproduction and remaining limits

Use the current standalone sources and `run_prepared.py` described in the sibling `AUDIT_PLAN.md`. Select a fresh output directory, the requested current engine archive and matching support build. The runner refuses existing outputs; every command/stdout/stderr/source/library/executable identity is retained.

`AUDIT_RECEIPT.json` records linked archive/library hashes and exact current-vs-HEAD source identities. `MANIFEST.json` hashes all copied raw inputs/results/reports. These probes do not resolve missing original M38/M43/M53/M68 briefs, certify every declared capability or prove native Windows behavior. Current mod unsupported payload types and nested-prefab limitation remain untouched.
