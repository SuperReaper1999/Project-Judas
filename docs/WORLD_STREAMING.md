# World regions, streaming and suspension

Regions are ownership groups inside one RuntimeWorld, physical world and script
VM. Whole-scene load/reload remains distinct. Use a registered `.judasworld`
manifest for region scenes, dependencies, placements, interests and budgets.
The [exact `scenes` API](judasjs/streaming.md) owns request/status/pin semantics.

## Authoring and references

Author a manifest through the editor's World tools or named source. Region IDs
and source-local entity IDs are durable; installed runtime handles are not.
Placement has a binary64 absolute origin and unit-scale rotation (XYZW in the
manifest). Subtract the active world origin before converting to local float data.
Interest bounds are oriented 3D half extents, not a separate physics chart.

Qualified references declare source field and target region/local identity.
Supported families include parent, texture camera, joint endpoints, sockets,
selected gravity, wheel chassis, powertrain terminals/connections, deformable
attachment groups and terrain planet ownership. Hard physical/hierarchy links
retain their dependencies while active. Soft references are limited to texture
cameras; they do not turn an absent physical target into a valid body.

## Prepare, integrate, publish

Workers read/validate immutable scene, asset and native-terrain products. They
never mutate the live VM/physics or call GL. Owner-boundary integration performs
bounded work with private disabled definitions/resources and remapped IDs.
Complete required collision/navigation publication precedes normal script
construction/start. Optional visual resources can still be loading: `active`
and `visualReady` deliberately answer different questions.

Requests coalesce by region but retain independent consumer tokens. Preload
prepares without activation. Releasing a token releases only its demand; another
request or safety pin can retain the region. Cancellation is cooperative. Pause
permits worker preparation while deferring installation/publication/unload.
A failed/cancelled candidate does not partially replace live content.

## Supported ownership and suspension

| Family | Runtime behaviour / retention |
| --- | --- |
| Ordinary rigid/kinematic entities | Native state/commands and supported contacts restored with fresh handles |
| Scripts and session data | Bounded JSON retained; closures, module globals and arbitrary native handles are not VM snapshots |
| Animation | Supported mixer/IK configuration/observation restored; active physical articulation or pending transitions can pin the region |
| Deformables/material parts | Native state and supported structural continuation captured; live nonmaterial fracture families/external attachments retain explicit pins |
| Wheels/powertrains | Captured through component owners; complete physical/reference assemblies must remain valid |
| Native terrain/navigation | Prepared source-owned products and edits restore before publication; pending edits pin the owner |
| Weather/audio/particles | Their declared settings/continuation policy applies; transient voices/visual effects are not arbitrary durable native objects |
| Root UI and audio listeners | Session-owned; rejected in imported regions |
| Enabled Grid Water owner | Must remain in root; colliders may stream through it, owner suspension is unsupported |
| Atmosphere, combustion, celestial/managed bodies | Region import rejects unsupported ownership rather than silently dropping state |

Dormant fluid metadata with advanced simulation disabled does not allocate/pin an
advanced solver. Region status reports actual pin/error reasons. Retention pressure
must not discard authoritative state or respawn initial content. Use resident policy
or explicit pins for unique non-JSON state. Adopt only a complete supported hierarchy;
incomplete/group-owned physical assemblies reject an independent transfer.

## Persistence, origins and SDKs

Ordinary SaveService slots capture the composition manifest identity, supported
active/suspended records, destroyed identities and travelling actors. A load stages
required resources/state before world publication. Normal reload resets world-owned
region state; Load reconstructs the chosen slot. Changing base content requires
an explicit [save evolution policy](SAVE_EVOLUTION.md).

Origin shifts defer while residency work cannot reach a safe boundary. Retained
records keep capture origins and reconstruct local positions relative to the new
chart; immutable geometry, velocity and scalar rotor state do not become positions.
See [assembly travel](ASSEMBLY_TRAVEL.md) and [persistence](PERSISTENCE.md).

Export must include startup and manifest region scenes under the project's explicit
inclusion policy. Use the matching SDK, export, move the game and cold-load it
without the source checkout. Terrain/developer workshops and native streaming/
residency fixtures exercise these contracts; they do not certify arbitrary component
combinations or universal open-world memory/performance.

Source owners: `WorldStreaming.cpp`, `WorldPersistence.cpp`, `SceneSession.cpp`,
`WorldOrigin.cpp`, `terrain/TerrainPreparation.*` and component capture/restore APIs.
