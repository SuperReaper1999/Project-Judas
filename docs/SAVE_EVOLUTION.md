# Save compatibility, migration and recovery

## Identities and ownership

A slot has independent identities:

- Storage namespace: project `saveIdentity`, retained across deliberate game updates.
- Container: formats 1–3 remain readable; format 4 adds project compatibility/version
  and a checksum covering identity metadata plus payload. SHA-256 detects damage;
  it is not a signature or a security/trust boundary.
- Native snapshot: `gameVersion` (currently 1), with independently versioned native
  participants. Existing strict readers and physical continuation checks remain authoritative.
- Game content: exact SHA-256 of normalized project settings, registered scenes and
  asset identities/bytes (including resolved mods). Packaging-only normalization is
  unchanged. `saveEvolution` is excluded to avoid a self-referential policy hash.
- Project compatibility: developer-owned string plus integer content schema version.
  Empty / 0 identifies existing exact-content saves, not universal compatibility.
- Mods: exact ordered resolved identities, versions, hashes, signature and game
  contract hash. Dependency resolution and executable-package trust still happen
  in the existing mod launcher, before runtime loading.

Keeping a compatibility string does **not** authorize changed content. Every
non-exact source must also be pinned in the installed policy. Changed policy is
trusted installed game content; never take a policy from the save being loaded.
Engine revision labels are diagnostics, not compatibility promises. A compatible
revision means its actual native readers accept the required records and context.

## Project policy

Project settings → **Save compatibility and migrations** edits/validates the same
JSON stored as the `saveEvolution` string in named `.judasproj` data (legacy key
`save-evolution`). Save project and restart Play after changing it. Empty disables
migration and preserves existing exact-content behaviour. The exporter copies the
policy with ordinary project settings; no internet service or migration VM exists.

```json
{
  "format": 1,
  "compatibility": "my-game-campaign",
  "version": 1,
  "sources": [{"content": "<64 lowercase hex characters>", "compatibility": "", "version": 0}],
  "steps": [{
    "from": 0, "to": 1,
    "operations": [{
      "kind": "script", "entity": 10, "slot": 1,
      "patch": [
        {"op": "add", "path": "/inventory", "value": {}},
        {"op": "move", "from": "/salvage", "path": "/inventory/ore"}
      ]
    }]
  }]
}
```

Obtain an **unmodded** source fingerprint with
`build/judas_save identity path/to/game.judasproj`, or read `saves.list()` after an
actual save. For modded content use the actual resolved runtime slot, not the base
project's offline identity. Preserve the shipped old project and its saves.

Sources specify the exact old content hash, old compatibility string and old
version. Steps have unique consecutive edges (`n` → `n+1`). An old version can
traverse several steps; missing edges, downgrades, ambiguous edges, unknown fields,
unapproved sources, failed preconditions or invalid output fail the transaction.
All steps run on a detached copy; partial output is never published.

### Operations

| Kind | Required fields | Effect / boundaries |
|---|---|---|
| `script` | exactly one of `entity` / `asset`, plus `slot`, `patch` | Patch one stable script identity, or every active instance of a script asset at that slot. Each instance keeps its own values. Missing identities/state fail. |
| `session` | `key`, `patch` | Patch an existing project session value; preserves all other keys and locale. |
| `scenePath` | `from`, `to` | Guarded scene registration rename. Target must be registered for the in-game load. |
| `scene` | `patch` | Patch the named scene document in the entity participant. |
| `entity` | `entity`, `patch` | Patch one named scene object selected by stable numeric identity, without depending on array order. |
| `prefab` | `asset`, `patch` | Patch active snapshot instance roots selected by their `prefabAsset`. No matched instance fails. It does not reload the source prefab or regenerate its children. |

`patch` uses the vendored nlohmann JSON Patch implementation: `test`, `add`,
`replace`, `remove`, `move`, `copy`, JSON Pointer escaping (`~0`, `~1`). Parent
objects must exist. `test` is an exact equality precondition. **`add` at an existing
object key replaces it**; use intentional guards and preserve unfamiliar fields.
Explicit remove/replace operations are developer policy, never engine guesses.
There is no arbitrary JavaScript callback, arithmetic expression evaluator or
native binary patch operation.

Scene/object/prefab patches may only change names, render colour/visibility/texture,
script bindings/properties, prefab asset/provenance overrides. IDs, object order,
transforms, collision geometry and physical/component topology must remain identical.
A normalized before/after comparison rejects unsupported changes. The complete
native entity tail is retained byte-exact, including forces, counters and view.
Script slot identities still need matching saved state; the normal restore checks
this. A supported appearance patch is not permission to replace collision meshes.

Bounds: 1 MiB policy/parser document, nesting 48, 128 source identities, 64 steps,
128 operations per step, 512 operations per conversion, 128 elementary patches per
operation, 1,024-byte/32-segment pointers. Script/session values retain existing
64 KiB JSON limits. Existing archive, resource and world limits still apply.

### Mods

Default: any changed, missing or differently hashed mod rejects. Diagnostics include
saved and current contexts so a developer can identify the actual missing package.
Optional `modTransitions` entries contain exact JSON `from` and `to` contexts
(`null` means unmodded). They authorize **only** that exact pair during an otherwise
approved forward content migration. Same version/different bytes is different.
There is no wildcard version substitution, automatic enablement or dependency
installation. Explicit mod removal requires the game author to transform affected
project state and ensure all still-required resources exist; native validation may
still reject. This facility cannot make absent physical assets reconstructible.

## Runtime transaction and recovery

```js
const token = saves.load('old-progress', {migrate:true});
```

Default `load(slot)` remains exact-only. The existing save worker reads/validates
the slot and applies the installed policy to immutable data. No world/VM pointers
cross to the worker. Then the existing owner-thread private-world preparation,
participant restore, resource/audio/localization validation and publication run.
Failure leaves the live world and disk source intact. Conversion success alone
is not native restoration success; inspect request `state` and `error`.

Request observations add `migrated`, `migrationMs`, `migrationReport`; worker time
includes conversion. All milliseconds are diagnostic measured durations, not a
latency guarantee. Exact unconfigured loads use the comparison-only path: no patch parsing or
scene rewriting. Cached lists expose content/compatibility/version and classify
non-exact identities conservatively; they do not certify migration feasibility.

A successfully migrated source slot is protected from save-overwrite for the
lifetime of this session's save service. Save to a new slot. Multiple migrated
sources remain protected. Explicit delete is still an intentional deletion API.
New slots and normal overwrites retain the existing locking/sync protocol; ordinary
overwrite now backs up the **exact valid bytes**, including legacy envelopes. A current slot with an unsupported future container
is refused on overwrite; explicit deletion or a separate slot is required.
Original saves are never automatically rewritten by a successful load.

### Recovery tool

```
build/judas_save identity game.judasproj
build/judas_save inspect /absolute/slot-directory old-progress
build/judas_save check game.judasproj /absolute/slot-directory old-progress
build/judas_save migrate game.judasproj /absolute/slot-directory old-progress new-progress
build/judas_save recover /absolute/slot-directory damaged recovered-progress
build/judas_save previous /absolute/slot-directory existing previous-progress
```

`inspect` and `check` do not write saves (storage can create its scoped directory/
lock). `recover` copies the current valid or automatically recovered previous
snapshot; `previous` explicitly selects the previous-good generation, even when
the current generation is valid. Destination must be unused, checked under the
storage lock; source is never overwritten. Output uses the normal current codec;
source and `.previous` bytes are not altered. `migrate` validates conversion, not
a complete native restoration. Open/load the new slot in the actual game to test
its resources, streaming and physics. Offline project conversion intentionally
rejects mod-enabled projects; use their resolved in-game service. The tool is SDK
functionality and need not be shipped inside every game.

## Integration boundaries
