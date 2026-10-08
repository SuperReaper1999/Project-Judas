# M59 — additive worlds and asynchronous region streaming

**Status through M69:** M59 is operator-accepted and checkpointed. Candidate/pending
statements and measurements below record the original milestone review state,
not a current outstanding acceptance gate. Later contracts take precedence;
start with [Architecture](ARCHITECTURE.md) and [JudasJS](JUDASJS.md).

Candidate based on `6fb90d56562651d027a8740cc8d17bb655d9b8b0` (accepted M58).
Human acceptance is pending. [JudasJS API](judasjs/streaming.md).

Judas owns residency and safe composition. JavaScript owns travel gates and game
policy. A region is an ownership group inside **one** RuntimeWorld, PhysicsWorld,
GravityContextMap, NavigationSystem and QuickJS VM. M43 replacement/reload remains
a separate operation; the root identity/session/locale/HUD do not change on additive
activation. Optional manifests leave ordinary single-scene projects unchanged.

## Manifest and editor

Streaming projects opt out of the built-in legacy gameplay path (`legacy-gameplay false`); ordinary historical projects remain unchanged. Set project `world-manifest` to a registered `.judasworld` asset ID. Version 1:

```
JudasWorld 1
budget 2 8 2 8388608 8388608 2097152 2097152
region "gallery" "Scenes/gallery.judas" 0 0 -24 0 0 0 1 12 4 12 10 "snapshot" 65536 ""
```

Budget fields are main-thread milliseconds, registration units/frame, parallel
readers, pending descriptor bytes, live descriptor bytes, retained snapshot bytes,
and shared-resource cache target bytes. Region fields are stable ID, registered
source, absolute double translation, quaternion x/y/z/w, oriented 3D half extents,
priority, `snapshot` or `resident` policy, estimated preparation bytes, space-separated
hard dependency IDs. Unit placement scale only; unknown/trailing fields fail.
4096 declarations, 4096 objects/source, 8 MiB source text, 128 tile layers/surface,
1024 request tokens, 32 interests, 32 manual pins/region are bounded limits.

Qualified references use:

```
reference "gallery" 101 "bodyB" "neighbour" 100 hard
reference "gallery" 60 "textureCamera" "neighbour" 500 soft
```

Supported fields: parent, textureCamera, bodyA/bodyB, liquid source/destination.
Physical/hierarchy references require hard dependencies; soft references are camera
textures only. Cycles reject with a named diagnostic. Unavailable soft targets are
unresolved and rebind when active. Local IDs/prefab maps are remapped to fresh
runtime IDs, never rewritten into source files. Revisit gives new safe handles.

The asset browser edits/validates the manifest; Project settings select it. The
World regions panel previews additive sources read-only and selects a source edit
target. Edits/save apply to that source document, not the flattened played world.
Preview refresh is explicit. Play shows requests, stages, pins and budgets. Stop
uses normal authored-state restoration. Region environment/exposure/locale/input
settings do not replace root settings.

## Coordinates and precedence

Region origin is the destination of the source scene's local origin; its stored
source `worldOrigin` is not a request to move the session frame. Subtract destination
and fixed session origins in double precision **before** narrowing. Rotate local
geometry, root poses, velocities, world-anchored constraint frames, gravity volumes
and navigation frames together. Native bakes are validated in original local source
coordinates before placement. Scale is not a placement option.

Region gravity priority descends numerically, ties use region ID then source ID.
Root fields keep their original order and serve after declared region fields.
Publication rebuilds this routing, independent of worker completion. Support normal
remains separate. An external actor using a region's winning gravity pins it.
There is no live rebasing. Tests demonstrate millimetre/sub-metre local offsets at
an absolute 1e12 m origin; the demo stays within ~200 m of its fixed float frame.
Unbounded travel precision is **not** claimed.

## Preparation, integration and rollback

JobSystem workers read/parse/resolve prefabs/validate certificates and produce
immutable CPU descriptors. They do not touch a live VM, physics, liquids or GL.
Requests coalesce; each token holds its own preload/activation demand. Release only
removes that demand; obsolete jobs cancel cooperatively. Products capture immutable
inputs rather than a world pointer, so late completion cannot mutate a replacement.
No runtime WaitAll or retrieval of unfinished jobs is used.

At the outer boundary after callbacks/render submission, installation registers
one ordinary component object/unit with disabled bodies and unpublished visibility.
Liquid and ragdoll skeletal geometry resources hand off individually; required geometry must become ready.
Navigation caches build privately, one bounded native surface/unit. Relocation and
atomic publication are measured indivisible units. Published identities/components
exist before scripts synchronize/start on the following normal script boundary.
`active` means playable; `visualReady` separately reports region asset-reference
readiness. Optional visuals retain normal placeholders/failure diagnostics.

Pause allows CPU/resource preparation but prevents installation/publication/unload.
Preparation/validation failures leave the existing world intact; staged failure or
cancellation unregisters the partial group. Publication is the no-return point:
arbitrary subsequent JS side effects cannot be transactionally undone.

Suspension captures existing JSON `state` and physical state, invokes destroy while
services/handles remain valid, hides the entire group, then unregisters incrementally
and releases asset refs. Runtime-spawned prefab entities default to root ownership; scripts can explicitly adopt complete assemblies into a region. A callback may queue new work, which runs at a later safe
boundary. Budgets limit dispatch between units, **not** the duration of one native
registration, serialization, callback or GPU upload. Diagnostics expose worst unit;
this is not a hard real-time scheduler.

## Registration / suspension matrix

| Family | Incremental registration and release | Suspension / ownership |
|---|---|---|
| Plain entities, tags/layers, hierarchy/prefabs | Normal definitions/ID remap/hierarchy | Definitions, pose and destruction tombstones retained |
| Static/dynamic box/sphere/capsule bodies | Same PhysicsWorld; disabled during stage | Pose/velocities retained; ordinary M39/M42/M44 paths |
| M45 joints | Same constraints/local or qualified body refs | Body poses and current control settings retained; solver warm-start impulses restart. Complete assembly; external participants/dependencies pin |
| CharacterMotor | Normal per-entity motor | Reconstructed from retained root state; external support pins; travelling actors adopt root |
| Navigation surface/agent/obstacle/link/modifier | Local bake validation, private Detour registration, normal guidance | Corridor/link dependencies and owned active/stopped intent pin; idle guidance reconstructs; removal invalidates refs/repath |
| Gravity fields/zones | Ordinary uniform/radial fields, rotated volumes | Winning in-use region pins; deterministic precedence |
| Mesh/material/texture/lights/render cameras | Normal async resources and per-camera rendering | Ref release, targets/lights unregister; camera soft refs rebind |
| Audio emitters / particles | Normal voice/pool registration; **M60 later extension:** bounded audio streams and authored environment zones | Visual particles/region voices restart on reconstruction; retained root/adopted voices keep their cursor. Streams detach and retire asynchronously; zone removal fades shared reverb. Authoritative unique state belongs in JS `state` |
| Animation / ragdoll | Ordinary shared asset / pose / articulation paths | Region pinned: no lossless pose/articulation suspension |
| M54/M55 basin/container/connection/interaction | Required baked geometry and ordinary liquid service | Conserved owners/connections pinned; no parked-parcel then duplicate initial water |
| Scripts | Existing played-world VM / normal lifecycle | JSON `state` retained; handles reacquired in start; module cache remains per-world |
| Runtime UI, audio listener, player-start | Root/session-owned; region import rejects | Root keeps active view, input, locale and HUD |
| Legacy vehicle/door/switch, celestial/coarse bodies, terrain, atmosphere/combustion, legacy PBF volume | Region import explicitly rejects | Root compatibility paths unchanged; no second implementation |

Physical definitions and JSON are supported session records, **not** universal VM
snapshots. Closures/module singletons/custom native handles cannot be suspended.
Use `resident`/manual pins for unique non-JSON state. Native ignored-handle lists or application-private engine state must be reconstructed from JSON intent in start, or kept resident. Root liquid consumers should co-own their gravity field with the conserved group, or explicitly pin required external regions. Liquid and articulation pins
remain until whole reset; no lossless suspension is invented. Visual-only particle
age and audio cursor are not retained. Modules are bounded by the existing per-world
VM limits but aren't reclaimed by region unload.

Adoption preserves runtime entity identity, physical motion and existing JS instance.
It transfers a hierarchy; incomplete joint assemblies or separate gravity/liquid/
ragdoll owners reject. An adopted member of another region pins that target until
transferred to root. Original ownership leaves a tombstone, not a respawn copy.
Support, external hierarchy/joint, nav intent/corridor/link, winning gravity, hard dependency and
unsupported-state pins are reported. State-cap pressure pins rather than resets.

## Residency, identity and export

Spatial sources use oriented 3D bounds, load/retain hysteresis and priority. Bounds
metadata scans are bounded; sources aren't parsed each frame. Dependency lookup uses
reverse adjacency. Pending/live reservation limits defer work; no safety pin is
silently evicted. Descriptor/snapshot bytes are **estimates**, not heap/RSS totals. Known particle pool reserves are included before installation; native allocator, driver and VM/cache overhead are not an exact RSS ceiling.
Retained-size estimates are cached when snapshots/adoption records change; normal
frame accounting never reserializes every retained definition. This prevents
bookkeeping from starving budgeted installation/teardown after repeated visits.
World-endpoint joint settings remain joint-entity-local authored values, transformed
exactly once and converted back to that local frame when capturing native state.
Removing a body-less region component does not destroy physics bodies or joints.
Resource resident bytes are known ResourceManager accounting; unreferenced caches
evict under the configured target. Referenced resources can exceed that target and
remain visible in accounting. No pretend-free live resources.

A composed baseline uses its own domain (`Judas.ComposedWorld.1`), root canonical
fingerprint, serialized project configuration, stable region identities/source hashes
and sorted registered asset identity/type/content hashes. M60 computes the same
asset digests incrementally with a 64 KiB file buffer, so long music does not
require whole-file identity allocation. Residency/request/profiler
state and absolute filesystem roots are excluded. Legacy schema-5 canonical scene
hashing is unchanged. Composed disk-save application is explicitly rejected and
normal save actions disabled; bounded M43 session JSON is still available. Region
retention resets on deliberate full reload. Authored source/asset edits during Play require full reload to refresh the complete baseline; M59 is not general hot reload. General persistence remains future work.

M38 packages all registered runtime assets/scenes (its existing dynamic-loading
policy). It validates manifest sources, qualified dependencies and source bake
certificates before promoting the staged package. No repository/CWD fallback is
required; moved exports use package-relative ordinary asset resolution.

## Current project and controls

`projects/streamed_range/streamed_range.judasproj` retains the original Spring Range
as an unchanged reference. Bootstrap owns player, view, localized HUD/audio/particles.
Six galleries contain separate physics/targets/navigation and distinct mesh assets.
A travelling navigator adopts root ownership and uses normal links/CharacterMotor.
The small liquid exhibit proves conserved pinning. The separate oblique/radial
fixture is a placement/gravity check, not a planet-streaming framework.

WASD/look/fire, Space, V, Escape/R, L retain ordinary game meanings. F9 explicitly
requests the next gallery; F10 releases manual requests (spatial demands/pins can
remain); F7 requests liquid; Q picks/releases an ordinary nearby prop with finite
forces and adopts it to root. Project JS gates movement into unavailable geometry
and displays localized residency/error information. This is honest loading policy,
not an engine hidden collider or teleport. Full reload intentionally resets round
and region retention. Editor diagnostics can request/release any declared fixture.

Evidence and exact scope: `docs/evidence/m59/`. Operator must still validate visual
travel, carry, targets, locale/pause, liquid retention, preview and moved package.

## Later disk persistence — M61

The runtime suspension/pinning limits above remain. [M61 slots](M61_SAVES.md) now
store resident state, retained records, tombstones and adopted qualified identity
across process exit, including pinned liquid/articulation families. This replaces
the prior absence of composed-world disk persistence; it does not relax eviction
policy or turn a pin into a save.

## accepted M66: imported animation suspension

Ordinary animated snapshot regions now reuse the M61 per-instance pose record
when retiring/revisiting. Shared cooked dependencies load before publication;
active ragdolls/return transitions remain pinned. Retained animation records are
bounded to 1 MiB per instance and use streaming participant version 3 in modern
slots. Earlier versions remain readable. See [Model import](MODEL_IMPORT.md).
