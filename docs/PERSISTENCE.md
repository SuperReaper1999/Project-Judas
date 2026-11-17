# Save slots and cold restoration

Judas owns storage, consistent snapshots and reconstruction. Project JavaScript
chooses when to save and contributes bounded data. A save is not a VM/process dump.

## Participation matrix

| Participant | Required authoritative state | Reconstruction / deliberate exclusions |
|---|---|---|
| Entities | Durable identity, runtime definitions and prefab provenance, hierarchy, transforms, velocities, lifecycle/fidelity, classification, physical settings, tombstones | New native handles; broadphase/GPU/resources rebuilt |
| Constraints | Runtime enabled/limits/motor/spring settings and stable body references | Native handles reconstructed; supported contact/sleep continuation restored by their owner; no replay of old events |
| Character motors | Velocity, orientation/settings, support state and supporting identity | Presentation history initialized from restored pose; camera/input remain project policy |
| Animation | Clip/layer times and settings, interrupted mixer contributors, external/captured poses | Skin matrices rebuilt from shared skeleton; no GL object persistence |
| Articulations | Active mode, mapped physical poses/velocities, constraint state and root relation | Existing skeleton/PhysicsWorld/joint mapping reconstructed; no second solver |
| Navigation | Destination/settings, stopped state, link traversal progress | Corridors rebuilt against baked surfaces; no Detour pointers |
| Grid Water V2 | Versioned physical grid, quantity/displacement/air/source ledgers and continuation | Restore actual grid state; never reseed authored initial fill as a substitute |
| Audio | Persistent source settings/cursor/play state, group routing/fade control | Ordinary bounded prefill/seek; one-shots and reverb tails deliberately clear |
| Scripts/session | Independent slot `state`, bounded session data, selected locale | Modules/closures/globals not persisted; restore-aware callbacks reacquire handles |
| Streaming | Active/suspended qualified identities, adopted hierarchies, saved changes and tombstones | Unvisited baseline regions remain unloaded; requests/pins reacquired or derived |
| Presentation/UI | Persistent material/light/environment/view settings; runtime-rendering local entity/Render visibility, whole/slot/stable-part patches and texture IDs, root appearance/reset baseline | UI from assets and game data; transient particles, GPU handles, loading caches, focus and input edges clear |

Required participants have explicit versions. Missing, unknown or malformed records
fail before publication. The legacy delta path remains compatible and is not applied
on top of a selected modern slot. No simulation algorithms are changed for saves.

## Storage

Linux local storage uses descriptor-relative slot operations, validated opaque IDs,
an exclusive nonblocking process lock and bounded payloads. Writes synchronize a
same-directory temporary, atomically rename it, then synchronize the directory.
A validated previous generation is durably published before replacing the current
generation. Failed writes before publication preserve the current slot. Failure
after rename is reported as published with uncertain durability. This is not a
universal power-loss claim for network filesystems or faulty hardware.

This protocol follows the supported local Linux meanings of
[fsync](https://man7.org/linux/man-pages/man2/fsync.2.html),
[rename](https://man7.org/linux/man-pages/man2/rename.2.html) and
[descriptor-relative/no-follow open](https://man7.org/linux/man-pages/man2/open.2.html).
Controlled child exits and injected failure boundaries verify ordering; they do
not simulate physical power loss.

Incomplete temporaries are ignored; recovery uses only a checksum-valid committed
previous generation and reports recovery. Saves are data, never executable code.

## Consistency, identity and load order

`SceneSession` owns one lazy project save service across world replacements.
Requests capture at a later outer frame boundary after authoritative fixed steps,
events and current script presentation. A partly published streaming unit defers
capture. Required assets must be ready. The owner thread copies one immutable
snapshot; JobSystem workers encode/read/write/list without world or VM access.
An idle service does not serialize the world or load suspended regions.

`save-identity` is the project's stable namespace, also copied into exports.
New projects mint an authored random identity. Legacy projects fall back to a
content-independent name/filename identity; equal legacy identities intentionally
share slots unless a unique explicit identity is authored.
Content compatibility is a separate SHA-256 of project settings, registered scene
bytes and registered asset identities/types/bytes. Absolute locations, selected
locale, current runtime poses and Git commit are not content identity. A changed
script, prefab, nav bake or cavity certificate rejects an old slot by default.
The strict policy includes all registered assets, including unused ones.

Entity identity uses current world IDs and region qualified region-instance/local
keys. Runtime allocation counters, prefab provenance, ownership/adoption and
tombstones are saved. References are fixed against new entities/native generations;
the same ID does not make an old handle valid. `saves.reference/resolve` stores
opaque strings without rounding IDs through JavaScript Number. Optional absent
targets return null; required supports/constraints/mappings reject missing targets.

Load ordering:

1. Read/decode detached data; verify checksum, schemas, project/content/scene.
2. Build a non-ticking private `RuntimeWorld` through normal construction and
   resources, using captured runtime definitions rather than resolving prefab
   sources a second time.
3. Restore required participants, fix references, prepare locale and quiet audio.
   Validate the current RuntimeWorld construction without running gameplay.
4. At the outer boundary, retire the old session/world and publish the candidate
   once. Restore session data and streaming ownership; suppress the legacy overlay.
5. Initialize observations/interpolation coherently, clear load input edges and
   invoke `restore` rather than `start`. Project callbacks rebuild menus/cameras and
   reacquire logical residency requests. Simulation then resumes normally.

Corruption, incompatibility or preparation failure before step 4 leaves the old
world playable. Step 4 is the no-return point: arbitrary later JavaScript effects
are not rollback transactions. A restore callback fault uses normal script error
isolation. Constructors must likewise avoid unconditional new-game side effects.
The saved simulation clock resumes; offline wall time is not catch-up physics.

Contact-pair history preserves Stay/Exit semantics instead of replaying Enter on
already touching objects. Broadphase, constraint warm starts and numerical caches
rebuild. This promises validated immediate state and bounded continuation, not
bit-identical indefinite future physics.

## Policy, limits and compatibility

- One active request per project; conflicting save/load/delete/scene requests fail
  busy. At most 32 status records and 128 slots. Separate processes use a
  nonblocking exclusive storage lock; unsupported concurrent access fails busy.
- Queued/preparing operations can cancel. Reading/writing/publication cannot.
  Destroying the caller does not cancel a project request. Project shutdown cancels
  queued jobs and detaches worker products safely; an already running storage job
  may finish. Wait for completed before intentional quit if the game needs a save.
- Slot IDs: 1–64 ASCII letters/digits/underscore/hyphen. Display name: 1024 UTF-8
  bytes. Plain JSON metadata/state uses existing validation; wrappers, functions,
  cycles and nonfinite values reject. Module globals, closures and the VM heap do
  not persist. A project saves semantic data in independent `this.state` slots and
  bounded `session` values; no second mandatory game-data store.
- Portable little-endian numeric fields preserve double quantities and full-width
  IDs. File bound 64 MiB; effective combined payload 8 MiB; collections 65536;
  reconstructed entities 8192. Private preparation has a
  30-second deadline. These are explicit bounds, not silent truncation policies.
- Container version, each required participant version and game-data version are
  independent. Supported native participant versions vary by owner. The detached container-1 timestamp-seconds migration checks overflow
  and upgrades to milliseconds. Unknown/newer versions reject. Automatic game-data
  or content migration is not implemented; source files are never rewritten on load.
- Checksums detect payload corruption, not authenticity or malicious authors.
  Metadata listings validate payload checksums without constructing worlds.
- Historical pose-delta saves and native demonstration gameplay are retired without import/migration. Current unsupported native participants still reject before publication with their component reason.
- Explicit transient simple render/body entities may be omitted. Required motor,
  articulation, animation, navigation, current water, persistent audio, gravity, scripts
  and referenced dependencies cannot silently become transient. Particle pools,
  one-shot sounds, reverb/filter memory, render targets, atlases, UI focus/cursor
  and diagnostic history deliberately reconstruct or clear.
- Persistent authored voices use ordinary cursor seek/prefill, settings and bus
  fade control. Stream seek accuracy is codec/backend limited; the tested FLAC
  cursor is within 10 ms. No waveform/tail phase continuity is claimed. Machine
  output device/master gain remain current preferences; project groups are saved.
- Save does not change region eviction policy. Required articulation families
  retain their existing pins. Changed unloaded regions remain suspended until an
  ordinary demand activates them; old request tokens are never revived.
