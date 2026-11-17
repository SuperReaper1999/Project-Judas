# Runtime lighting, visibility and materials

## State publication

Authored defaults → isolated validated runtime patches → one rendered sample →
ordinary resource resolution and draw/shadow passes is the ownership path.
Judas supplies direct lighting, environment, visibility and material primitives;
project JavaScript supplies time, transitions, sun paths and hiding policy.

| State / phase | Owner and contract |
| --- | --- |
| Authored settings | Root scene settings own sun, ambient, background, exposure and environment defaults. Scene objects own entity visibility; optional Render components own component/part visibility and authored materials. Normal editor/named/prefab serializers share these fields. |
| Runtime appearance | RuntimeWorld validates a complete prospective patch, then publishes its values together. The first change retains the root reset baseline; reset restores it. Additive regions retain the root owner's settings. Sequential valid writers use last-write-wins without a new arbitration service. |
| Runtime render state | Live entity/component definitions own independent gates and bounded material patches. Ordinary cached static/dynamic submissions update from those definitions; a logical member does not share another entity's visibility/material patch. |
| Material resolution | Imported/default or assigned material → authored whole/numeric/stable-part values → runtime whole/numeric/stable-part patches. More specific runtime fields win. Explicit imported/default source selection is distinct from inheriting authored assignment. |
| JS write/read | Existing world, Entity and Material handles validate before mutation and return detached state. Accepted values appear in subsequent reads and the next rendered sample; there is no renderer-only write discarded by the next simulation step. No new fixed scheduler or async JS callback is introduced. |
| Render sample | WorldPresentation copies root appearance once and publishes lighting/environment before the existing shadow and camera passes. Those passes run no scripts and advance no transition clock. Sun direction/enabled state control direct lighting and the supported directional shadow together. |
| Resource resolution | Shared ResourceManager requests registered materials/textures/environments, holds demanded references and retires work by generation. Renderer owns GL creation, upload, sampling and deletion. Factor/opacity writes reuse geometry/programs/resources. |
| Reset/retirement | Material reset removes the selected runtime patch, including assignment; authored values and other target patches remain. Latest durable IDs are resolved for each sample, so newer selection/reset/destruction/unload cannot publish obsolete maps. |

Implementation authority: `src/ScriptSystem.cpp`, `src/RuntimeWorld.cpp`,
`src/Material.cpp`, `src/WorldPresentation.cpp`, `src/RendererMaterials.inl` and
the ordinary scene/resource/editor systems. Source and the focused receipts
determine behaviour; the lab does not introduce engine-owned gameplay.

## Lighting and environment

`world.appearance`, `world.setAppearance(patch)` and `world.resetAppearance()`
extend the existing root appearance surface. The sun has enabled state, intensity,
direction and linear colour. Direction is a finite nonzero world vector **toward
the light source**, normalized on write; gravity supplies no compulsory up or orbit.
Intensity and colour are bounded engine radiance multipliers, not certified
photometric units. Ambient/background, baked environment selection/intensity/
rotation/background visibility and exposure remain explicit independent controls.

Disabling the sun removes its direct contribution and invalidates its directional
shadow slot. Moving it updates the normal directional shadow matrix for the sample.
Ambient or IBL may continue to illuminate the scene; emission still does not light
neighbours. Exposure changes final presentation rather than direct illumination.
Existing point/spot lights and bounded counts are retained; no new light class,
weather, astronomy, atmospheric scattering or GI technology is added.

Environment status/error readback exposes the ordinary ResourceManager states.
An empty environment selection reports ready because no resource is required. Read-only observations
are ignored when restoring a captured appearance snapshot. Selected resources can
be pending or failed after a valid write; selection is not synchronous loading.
Exact arguments, ranges and errors are in the [public reference](judasjs/materials.md).

## Visibility and instance materials

`Entity.renderVisible` is local to one entity, including its owned renderables;
children retain their own visibility. `Entity.rendererVisible` addresses its
existing Render component independently and throws if that component is absent.
Imported `setPartVisible(identity, bool)` remains independently addressable;
`modelParts.visible` is the part flag, not combined effective visibility.
Ordinary imported submissions require entity, component and part gates together.
Applicable visible/depth/shadow submissions consume those same gates.

Hiding preserves bodies, queries, scripts, audio, animation, joint reads and sockets.
It does not move/scale/despawn/deactivate anything. Hierarchy and inspector access
remain available to restore hidden geometry. Geometry combined into one authored
mesh is addressable at its actual supported parts, not lost original entities;
joint-subtree hiding/scaling is outside this milestone.
The Render-component gate addresses its primary surface; other owned particle,
fire and current water visuals use the entity gate. The legacy M25 globally
merged fluid mesh has no individual volume owner/granularity. Accepted fluid
ownership and simulation are not changed to introduce one.

`Entity.material(target = 0)` keeps numeric slots `0..63` and adds `"*"` for the
whole instance or a stable imported part identity. Existing Material operations
now support alpha mode/cutoff, normal/AO strength, double-sided state and five named
texture overrides alongside colour, PBR/emission and UV factors. Whole readback is
a first-primitive representative, not a claim that every source material is equal.
`overridden` reports presence of this target's runtime patch. Runtime source edits
remain independent of shared material/model/texture files and other instances.

Base-colour factor alpha multiplies texture alpha. Opaque writes depth; mask
discards below cutoff in colour and shadow passes; blend uses ordinary post-opaque
object-origin back-to-front sorting, depth test, no depth writes and no shadows.
An alpha factor alone does not make opaque geometry transparent. Intersecting
transparency, refraction and coloured transparent shadows remain unsupported.
Colour textures retain sRGB interpretation; numerical maps remain linear data,
with the accepted tangent/normal and static/skinned paths.

Replacement textures use registered IDs, not filenames or GPU handles. Omitted
roles inherit; empty strings explicitly remove. Pending/failed replacements use
the selected source map as a coherent fallback, rather than retaining an earlier
runtime replacement, while reporting pending/failed/error.
Latest IDs and resource generations reject obsolete publication. All grouped
material/appearance updates validate before publishing any part of a batch.

## Main and auxiliary view support

The audit found that the existing auxiliary path already uses ordinary geometry,
material, transparency, dynamic-light and water-path submissions. runtime-rendering preserves
that implementation and its existing cheaper choices.

| Property | Main view | camera-target auxiliary view |
| --- | --- | --- |
| World/state sample | One current appearance and presented world sample | Same sample; no extra scripts, fixed work or fades |
| Visibility/material parameters | Normal applicable submissions | Same supported submission gates/material paths |
| Direct lighting and environment | Existing normal pipeline | Existing shared lighting/environment path |
| Shadows | Directional and existing legacy spot maps built around main focus | Reuses those maps; no independent auxiliary-camera shadow focus |
| Target size/cadence/mask | Main viewport/current frame | Authored target size, updateEveryFrames and renderMask |
| Modern colour result | Scene-linear lighting followed by exposure/Reinhard/sRGB display resolve | Scene-linear texture; exposure/output resolve deferred until consumed by main view |
| Target ownership/feedback | Ordinary renderer resources | Existing generated target lifetime and active-target fallback |

Auxiliary screens are not a main-view pixel or shadow-quality oracle. No parity
upgrade, recursive portal/reflection clipping, additional per-camera shadow set
or new expensive effect selector is introduced.

## Editor, authoring, saves and retention

The ordinary inspector exposes **Entity render visible**, **Render component
visible**, imported part visibility/material assignment and a whole-instance
material panel. Alpha/opacity and all five texture roles are discoverable there.
Authored settings and overrides use normal Undo/Redo; explicit shared-source saves
remain separate disk operations.

Play has **Runtime render preview**, whole/numeric/stable-part target selection,
source selection, factors/maps and **Clear runtime material binding**. The scene
settings runtime preview adds lighting/environment and **Reset runtime environment**.
Its bounded 64-action **Undo preview / Redo preview** history is separate from
authored scene undo, coalesces gestures and clears on Start/Stop. Runtime previews
do not silently write authoring files. Accepted kinematic navigation/layout/picking
remains the editor foundation.

Optional ordinary scene/prefab fields carry the state:

| Field group | Durable values |
| --- | --- |
| Object | `render-visible` |
| Render | `visible`, `instance-overrides-v1`, `part-materials-v1`, `runtime-materials-v1`, optional numeric `material-overrides-v1`; existing slot assignments and hidden-part keys remain |
| Scene settings | `sun-enabled`, `sun-intensity`, `appearance-reset-v1`, plus existing sun/ambient/background/environment settings |

Versioned material binding/override blocks contain bounded encoded plain values,
texture IDs and explicit source-selection state. The normal serializers/validators
and named converter own their encoding; no additional named document format,
save-slot participant version or region archive version is introduced. Optional fingerprint
tags extend canonical schema 5; unchanged defaults omit them and preserve historical
identity. Asset dependency gathering includes every override texture/material and
the reset baseline's environment reference.

Scene/prefab/reload/Play/Stop use ordinary authored defaults. Explicit save-slot slots
and region retained definitions preserve live visibility/material IDs and root
appearance/reset state, reconstructing disposable GPU/load caches. Restored scripts
use the existing restore lifecycle; initialization must not replay to reconstruct
visual state. export/scalability export supports conservative all-assets inclusion or opt-in
closure. Script-selected material/texture/environment IDs must be reachable or
listed as explicit runtime assets when closure is selected.
