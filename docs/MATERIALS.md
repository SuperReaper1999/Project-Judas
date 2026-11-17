# Materials and environments

Materials own appearance, not collision or gameplay. Use registered `.judasmat`
assets, model material slots and explicit runtime instance overrides. The renderer
consumes immutable shared resources; one entity's runtime patch does not rewrite
the shared asset or another instance. See [exact script fields](judasjs/materials.md),
[model import](MODEL_IMPORT.md) and [runtime appearance](RUNTIME_RENDERING.md).

## Author and assign

In the Asset Browser create/select a material, edit it and save its source. Assign
its registered ID to a Render component or imported model slot. Inspect stable
model part/material identities rather than relying on a downloaded file's display
names or slot order. Prefab overrides use the same scene property contracts.

Choose Legacy, PBR or Unlit. PBR uses metallic/roughness, base colour, normal,
occlusion and emission with the supported texture roles. Configure UV transform,
sampler, alpha mode/cutoff and double-sided policy explicitly. A translucent source
image does not establish that every associated body/interior material should blend.
Imported legacy shader graphs may require deliberate material authoring; the
importer does not reconstruct every DCC/game shading graph or guess every bump
texture's semantic role.

Opaque, Mask and Blend have different depth/order behaviour. Use Mask for cutout
coverage and Blend for genuine translucency. Opaque casts need no alpha texture
sampling; masks retain material-specific shadow coverage. Blend rendering is
sorted separately. Double-sidedness and mirrored winding retain their own cull
policy. Hiding a renderable does not disable its body, scripts or audio.

## Colour, lighting and environments

Texture colour roles, scene-linear shading and final display resolve are explicit.
Normal/roughness/metallic/occlusion data are not colour-transformed as albedo.
The shader's PBR path uses direct lighting plus its supported environment terms;
Legacy uses its own established arithmetic. AO affects ambient/environment, not
unrelated direct/emissive contribution. Emission is not a light source for neighbours.
Light multipliers are engine radiance controls, not certified photometric units.

Use `judas_environment_bake` or the matching editor tool to prepare `.judasenv`
assets. Offline integration builds diffuse irradiance, roughness-filtered specular
levels and a BRDF lookup, with source/settings/algorithm provenance. Runtime loads
those products rather than convolving source HDR images every frame. Environment
selection/intensity/rotation and background visibility are scene appearance settings;
rotation is independent of gravity. Without an environment, ambient is the practical
fallback. No dynamic GI, automatic reflection probes or arbitrary source-material
transmission model is supplied.

Directional cascades and point/spot lighting/shadows are described by
[the lighting system](LUCIFER_V2.md); the former fixed shadow-slot limits do not
describe current V2. Exposure changes final presentation, not physical light output.
[Grid Water optics](WATER_V2_PRESENTATION.md) and Basic Water have separate current
presentation owners; retired reservoir/liquid optical paths are not alternatives.

## Runtime and lifecycle

`entity.material(target)` addresses a numeric slot, stable part or whole instance.
Patch supported factors/textures/alpha policy through the public reference; invalid
or unknown options reject before mutation. Texture/environment selection can be
pending or failed after a valid write. Inspect actual readiness/error rather than
assuming that a selected ID is synchronously loaded.

World appearance patches, entity/component visibility, stable part visibility and
instance overrides have explicit reset semantics. Authored defaults return on
ordinary reload/Stop; modern saves and supported region snapshots retain durable
runtime appearance through the existing owners. GPU objects, texture units and
framebuffers are rebuilt, not serialized. Export verifies dependency closure.

Terrain paint uses its four material layers independently of heightfield LOD.
Small paint detail must not disappear merely because geometry simplifies; terrain
material pages and geometric patches have separate ownership. See
[terrain authoring](TERRAIN_V2.md) and [residency](TERRAIN_V2_RESIDENCY.md).

Test appearance in the real editor and moved runtime at the intended camera,
lighting and alpha policy. A successful bake/parser, hidden interior or unlit
screenshot is not proof that the imported material matches an external reference.
