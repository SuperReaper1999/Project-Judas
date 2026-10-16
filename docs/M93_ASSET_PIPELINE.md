# M93 — cooked assets, mesh LOD and visual residency

Current Experimental asset contracts. These extend M92 rendering; old projects
retain their existing behavior unless they explicitly opt into residency.
Human review and non-Linux validation are separate from automated evidence.

## Authoring and cooking

Preserve source art. In the editor Asset Browser, select a texture or rigid
model, expand **Cook / LOD / residency**, choose settings and cook. CPU preparation
runs on the existing import worker queue; publication is accepted only in Edit
mode for the same project. A new asset receives an ordinary asset ID. Recooking
keeps that ID and invalidates its resource generation. Assign the cooked asset
through the existing mesh/texture fields; cooking does not silently rewrite scenes.

The standalone authoring tool is also available:

```sh
cmake --build build --target judas_cook -j2
build/judas_cook artwork/column.obj Assets/column.judasmesh
build/judas_cook artwork/paint.png Assets/paint.judastex 2048
```

Arguments after destination are maximum texture dimension, LOD level count,
maximum estimated object-space LOD error, texture semantic (0 sRGB color,
1 linear data, 2 tangent-space normal), texture mode (0 automatic BC1/BC3,
1 RGBA). Settings are validated, not silently repaired. Byte determinism is tested for
the same cooker build/settings/input; cross-architecture byte identity is not promised. The C++ cook settings
also expose reduction ratio and projected error tolerance. Animated meshes are
explicitly rejected by the rigid LOD cooker; keep their normal imported assets.

`.judascook` recipes record relative source/dependency paths, SHA-256 hashes,
settings and the output hash. Publication rechecks source dependencies. Stale
prepared work cannot overwrite a newer source. The editor can check freshness
explicitly; export refuses a stale cook when its authoring recipe is present.
Copy the source, output and recipe together when relocating an authoring project.
Keep recipes under version control alongside cooked assets.

Recipes/source art are not runtime dependencies. An exported game can load a
valid cooked asset without its authoring recipe. This is intentional, not an
assertion that an absent source is up to date. Export a project from its authoring
copy; do not discard recipes to bypass freshness checks.

## Textures: `.judastex`

Source RGBA8 images produce a complete mip chain down to 1×1. Opaque color/data
uses BC1; alpha uses BC3. Normals retain RGBA channels. Lossless RGBA is selectable.
Color mips filter in linear light with premultiplied alpha, data mips linearly,
and normal mips renormalize vectors. Set the semantic correctly; filenames do not
infer it. Non-power-of-two dimensions and partial compression blocks are supported.
Mips are optionally losslessly deflated on disk, then uploaded in their GPU format.

Hardware without the applicable block format expands the chain to RGBA8. This
preserves function but costs more GPU memory and decode time. PBR color maps use
sRGB views while data maps remain linear. Source imported materials remain
self-contained; rigid mesh cooking also cooks their embedded images by map role.
External material asset IDs remain normal dependencies.

This is whole-texture residency, not virtual texturing or per-mip streaming.
BC compression is lossy and unsuitable for every image: inspect UI, text,
transparent edges, masks and normal maps; use RGBA where needed. No BC5, ASTC or
ETC2 cooker is included. Mobile-specific compression optimization is deferred.

## Geometry: `.judasmesh`

The cooker preserves base triangle positions, order and physical source identity.
It welds only identical complete attributes **and** source identities. Physics and
import consumers see the base index stream; extra LOD indices are render-only.
Rigid models retain material/primitive boundaries, source provenance and maps.

Reduced levels use meshoptimizer attribute-aware simplification, preserving
borders and material parts. Their stored absolute errors are estimates from the
simplifier, not certified Hausdorff bounds. Visual inspection is still required.
One shared vertex buffer and additional index ranges trade some resident index
memory for reduced drawing work. Vertex fetch is not independently decimated.

Each actual view selects a level from projected error, viewport height, projection,
nearest bounding-sphere distance and maximum transform scale. Perspective,
orthographic, split and secondary views make their own selection. Large-world
positions use existing local presentation frames. Source geometry and physics
are unchanged by camera distance. Runtime vertex deformation disables precomputed
LOD selection because its original error estimates are no longer valid.

`render.lod-multiplier` is authorable, serialized and prefab-overridable. Default
1 uses the cooked tolerance; 0 forces full geometry; larger values permit coarser
geometry (range 0–32). This is rendering policy, not collision quality. No new
JudasJS runtime binding is introduced by M93.

## Optional memory budget

Set project `visualBudgetMiB` (0–65536), or its editor project setting. **0 is the
default and keeps legacy eager, pinned ownership.** Reopen the project after
changing it. A positive value enables demand residency for cooked mesh/texture
visual references. Source assets, explicit references and nonvisual dependencies
retain normal ownership. This does not make simulation assets evictable.

Main, split and enabled secondary camera interests contribute demand, including
cached cameras between updates. Visibility honors render layers and publication.
Two frames of grace absorb adjacent-frame changes. Unreferenced, no-longer-demanded
resources become LRU eviction candidates. Shared asset IDs share one resident
resource. Returning views reload it through the ordinary asynchronous pipeline.

The budget is soft: an active or pinned working set can exceed it. Statistics
report that overcommit instead of discarding required resources. GPU generations
make stale mesh/texture/material handles fail safely when slots are reused.
The byte ledger includes actual buffers, compressed/uncompressed mips, material
images and color views; prepared CPU bytes and pinned bytes are reported separately.
Driver allocation overhead, built-ins, text atlases, framebuffer pools and the complete application RSS
are different measurements. A single upload is not preemptible, so an individual
large upload can exceed the normal resource-pump time slice.

GPU handles never enter saves. Asset IDs and author settings do; restored worlds
reconstruct their owners and visual demand. The visual budget is excluded from
save content identity because changing a render memory policy must not invalidate
an otherwise compatible save. Scene streaming still owns entity publication;
visual residency does not resurrect unloaded simulation objects.

## Formats, failures and limits

Versioned little-endian envelopes validate checksums, sizes, indices, mip chains,
provenance and bounded metadata before publishing resources. They are trusted
project formats, not a security sandbox. Publication retains the previous output
on ordinary errors; interrupted process/filesystem failure can leave recovery
files. It is not a crash-atomic transactional filesystem or a content-addressed
remote asset service.

Meshoptimizer v0.25 and stb_dxt provenance/licenses are in `third_party` and runtime
notices. Standard sources are still supported; neither cooking nor the residency
option is mandatory for small projects.

See [The Packed Museum](../projects/asset_lab/README.md),
[M93 evidence](evidence/m93/REPORT.md), [model import](MODEL_IMPORT.md),
[JudasJS rendering](judasjs/rendering.md) and
[architecture](ARCHITECTURE.md).
