# Materials and environment lighting — M57 foundation, M72 candidate

Current optional OpenGL 3.3 material pipeline. Judas owns rendering primitives; projects own appearance, scripts own its meaning. [JudasJS API](judasjs/materials.md), [M72 ownership/review](M72.md), [material lab](../projects/material_lab/material_lab.judasproj), [Spring Range](../projects/shooter_game/shooter_game.judasproj).

M72 extends the existing pipeline with validated runtime sun/ambient/environment
controls, local entity/component visibility and isolated whole/slot/stable-part
material patches. Sun direction points toward the source in world coordinates;
linear radiance multipliers are not certified photometric units. Reset removes
runtime values and restores authored defaults, including material assignment.
Explicit opacity uses the alpha modes described below. Shared source assets remain
immutable; resource replacement and durable state use ordinary IDs/generations.
The [API reference](judasjs/materials.md) defines ranges, timing, resource readback
and reset precedence. M72 remains an uncommitted candidate awaiting human review.

## Authoring

In the editor Asset Browser, **Create material** registers a `.judasmat` asset. Select it and edit **SHARED material source**, then save intentionally. Assign its ID to a Render component's material slot. The scene is the preview. Instance parameters are labelled **INSTANCE OVERRIDE**; revert removes overrides without changing the source. Ordinary scene edit transactions support assignment/override undo. Shared asset saves are explicit disk operations, not scene undo. Texture reimport invalidates dependent material resources.

Scene settings opt into **Linear rendering**, exposure, a baked environment, its quaternion orientation and intensity, and independent background visibility. **Bake HDR environment** imports a Radiance `.hdr` source into a registered `.judasenv`. CLI: `build/judas_environment_bake input.hdr output.judasenv 128 128`. Width is power of two 16–512; samples 32–1024. These are preprocessing controls, not per-frame settings.

Material model is **Legacy**, **PBR metallic/roughness**, or **Unlit**. Maps: base colour, metallic/roughness, tangent normal, ambient occlusion, emission. Factors are linear RGB; alpha is unencoded. The editor's factor colour fields show linear numeric values, not an sRGB colour picker. Base colour channels/metallic/roughness/occlusion/cutoff are 0–1, normal strength 0–8, emission/intensity nonnegative ≤100000. UV scale/offset and repeat/clamp/mirror wrapping plus nearest/linear/mipmap filters are authored per map.

Opaque writes depth. Cutout discards below cutoff in both colour and shadow passes. Blend draws after opaque, back to front by object-origin camera depth, depth test on/depth writes off, no shadows. Mixed meshes submit opaque primitives first, blended primitives later. Intersecting transparent surfaces and per-triangle sorting are not solved. Alpha blending is not refractive glass. Double-sided flips the shading normal on back faces.

## Colour and pass ordering

Base-colour/emission images use sRGB texture views, so hardware decoding, interpolation and mip filtering occur in linear light; alpha is linear. Normal/AO/metallic-roughness maps are numerical linear data. Roughness uses G; metallic uses B; AO uses R. CPU RGBA8 resources retain raw bytes; material uploads use separate role-correct sRGB/linear views, so one image can serve colour and data safely. Legacy disk textures keep their original raw-byte presentation. PBR/unlit consumers of an ordinary Render texture slot and modern particles lazily cache a separate sRGB upload from already-decoded GPU texels on the context thread (once per source lifetime, not per frame). Source destruction also releases the view. Legacy display camera targets remain live raw images; modern targets are already scene-linear. Missing maps use white/no perturbation/unit AO. Material loads take immutable CPU texture snapshots, upload on the context thread and own those GPU maps; no dependent texture handle dangles during eviction/reload.

Legacy scenes without new authored appearance remain on the original presentation policy. Imported glTF PBR materials are used when the scene opts into linear rendering; explicitly assigned materials select their model. Optional HDR/environment work is absent for legacy scenes. A PBR material without HDR uses bounded per-fragment Reinhard + sRGB; unlit without HDR encodes linear colour directly.

Modern world lighting accumulates in RGBA16F, then manual exposure, luminance-preserving Reinhard (`rgb / (1 + dot(rgb, [.2126,.7152,.0722]))`), clamp for SDR gamut, then exact piecewise sRGB transfer, once. The representable lighting ceiling is 60000 per channel (below binary16 overflow); over-bright material output saturates there before resolve. HDR input outside finite 0–60000 fails with an import diagnostic. This is internal HDR, not HDR-monitor output. UI/debug authored colours remain display-referred and render after world resolve, independent of exposure. Particle authored tint and colour-texture RGB are decoded separately before multiplication into modern linear lighting; alpha is unchanged.

Modern M33 targets are RGBA16F **scene-linear** images: environment background and water optics included, exposure/tone mapping excluded. Their resource carries its interpretation; ordinary material sampling skips sRGB decoding. The final main camera applies exposure/output conversion once. Legacy targets stay RGBA8 display-referred. Camera masks, cadence and self-feedback fallback remain intact. Target resize caches are bounded; main/secondary sizes do not allocate every frame.
Auxiliary views use the ordinary geometry/material/transparency paths and reuse
the main-focused directional/legacy spot shadow maps. Their resolution/cadence
remain authored; independent shadow focus and pixel-quality parity are not promised.

M55 optical boundary paths still come from the accepted liquid system at interpolated presentation time. World fragments apply Beer-Lambert attenuation and existing in-scattering **before** HDR display resolve. Liquid surface/particles follow opaque world rendering, then resolve, then UI. No changes to liquid quantity, surface solver, geometry or gravity are part of M57.

## Shading and environment

Direct model: metallic/roughness GGX distribution, height-correlated Smith visibility, Schlick Fresnel and Fresnel-reduced Lambert diffuse. Dielectric F0=.04; metal F0=base colour and diffuse zero. Perceptual roughness clamps to .05, alpha=roughness², avoiding an unbounded delta lobe. Existing directional, five dynamic point/spot lights and three shadow slots remain. Light colours are engine-authored radiance multipliers with existing bounded range attenuation, **not certified photometric units**. Emission does not light neighbours. AO affects environment/fallback ambient only, not direct light or emission. Assigned IBL replaces fallback ambient, never double adds it.

Offline deterministic Hammersley integration builds cosine-weighted diffuse irradiance/π, GGX roughness-prefiltered latitude/longitude specular mip levels and a split-sum BRDF lookup. `.judasenv` contains derived floats plus SHA256 of source bytes, bake settings and algorithm version; runtime just loads/uploads. Authored environment rotation is independent of gravity. No assigned environment uses the scene's ambient colour as a practical diffuse fallback. No reflection probes, dynamic convolution or GI.

## Import and lifecycle

**Historical M57 import restrictions, superseded by accepted M66** ([current model import](MODEL_IMPORT.md)): pinned cgltf handles self-contained GLB/glTF with embedded buffers/images, one mesh node, triangle primitives and up to one skin; existing 48 skin-joint/128 node limits remain. Core metallic/roughness factors/maps, sampler state, alpha, double-sided and KHR_materials_unlit import per primitive. TEXCOORD_0 only. Required unsupported extensions fail; optional unsupported extensions are reported. Texture transforms, external resource files, additional UV sets and material extension effects are unsupported. Embedded bytes survive package relocation; reimport replaces resources through their normal identity.

Imported tangents retain their sign; missing tangents use pinned MikkTSpace (zlib license) on indexed corners, splitting seams and preserving skin influences. Degenerate UVs use a finite orthogonal tangent fallback. Normal matrices account for nonuniform instance/skin transforms; determinant sign preserves mirrored handedness. Normals and tangents consume the same resolved skin palette as vertex positions; entity transforms use presentation timing. There is no separate pose pipeline.

Shared material/mesh/environment CPU decoding uses ResourceManager workers; Renderer owns all GL upload/delete. Registered IDs are serialized in scenes/prefabs with generic override support. Runtime overrides do not rewrite authored source files; explicit modern M61 saves and M59 retention preserve supported live values/IDs and rebuild disposable resources. Optional authored fingerprint extension retains canonical schema 5; worlds opting into appearance or declaring registered material/environment assets include registered material/environment/mesh/texture bytes in content identity, independent of absolute path. Absent new appearance and registered M57 assets preserves old fingerprint data. M38's conservative all-assets export remains default; M68 also supports opt-in selected-scene dependency closure plus explicit runtime assets. With closure selected, script-selected material/texture/environment IDs must be reachable or declared runtime assets. Export validates map references. Source HDR and import/bake tooling are not required by shipped runtime.

## References and limits

Algorithms/contracts: [glTF 2.0](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html), [Filament explanation](https://google.github.io/filament/main/filament.html), [MikkTSpace](https://github.com/mmikk/MikkTSpace). Implementation uses one documented Reinhard operator; no borrowed tone-mapping code. MikkTSpace pinned source and license are in `third_party/mikktspace`; notices accompany exports.

Twelve fragment texture units cover base, three shadows, water, four material maps and three IBL textures; hardware limits are checked at startup. Modest environment sizes suit small GPUs. No bloom, GI, advanced glTF material extensions, refraction, dynamic probes or general shader scripting. Screenshots/readback are engineering evidence; human review decides visual/gameplay quality.

M66 uses one additional vertex texture-buffer binding at unit 15 for size-aware
skin palettes (GL3.3; checked against the hardware limit). Main/shadow/secondary
passes share eight-weight geometry. Supported imported texture transforms/UV1
are documented in [Model import](MODEL_IMPORT.md).
