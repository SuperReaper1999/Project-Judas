# Terrain V2 residency

This describes the native Terrain V2 paging implementation on Experimental. It
is not a release announcement or human acceptance record. The final qualification
record is [TERRAIN_V2_QUALIFICATION.md](TERRAIN_V2_QUALIFICATION.md).

## Independent responsibilities

A terrain region owns small chunk descriptions, physical body identities, bounds
and immutable source identities. Heavy source samples and collision products are
paged separately. Removing a visual patch never removes its physical support.

- **Source data:** native 64×64 sample pages, checked against their existing SHA-256
  digests. An 8 MiB cache retains recently used height/material samples. Runtime
  sources do not retain complete height and weight arrays. Authoring tools retain
  their editable arrays and existing undo limits.
- **Exact collision:** 64 MiB of cached prepared products per owner. Bodies retain
  cheap bounds and geometry identities. Queries acquire shared lifetime leases;
  an in-use product cannot be evicted. Ordinary heightfield pages use the existing
  exact grid cook. Expensive excavated/curved products have private, checked
  prepared-data files, so returning to a cave does not repeat extraction/cooking.
- **Presentation:** an independent 32 MiB GPU cache. Height/normal samples are
  lossless at selected grid vertices. Native material coverage is partitioned
  independently of geometric LOD, preserving small paint details even on a flat
  landscape with a very coarse grid. Geometric partitions preserve the original
  triangles and stitching. Different LODs share material pages. All cameras use
  the same bounded resource cache; their existing LOD histories remain separate.

Publication invalidates only affected GPU pages. Excavation replaces its unique
meshes while retaining unchanged height/material images and unrelated cave
uploads. Sample changes are compared on the worker and invalidate their inclusive
rectangle plus the normal/stitching dependency halo. Replacement bases/layouts
still clear the old cache. This prevents repeated whole-landscape uploads during
an ordinary queue of local edits; it does not skip physical publication.

Characters, wheels, dynamic/sleeping bodies, global casts, navigation and particle
snapshots obtain actual collision geometry. Fluid occupancy continues to use the
immutable terrain source, including excavation air. Neither path depends on a
camera seeing the region. Origin changes update placement without changing local
page contents. Body/feature identities and public collision results are preserved.

## Edits and lifetime

An edit creates a new immutable source revision. Raster overrides and the volume
journal are written through the existing atomic save-storage protocol to private
spill storage before publication. Replacement collision/navigation is prepared
before retiring old support. A chunk recipe retains only its local edit
neighbourhood, rather than retaining a full chain of previous terrain revisions.

Base pages are immutable and may be evicted immediately; dirty overrides remain
owned until committed. Private derived geometry files are disposable, bounded,
CRC-checked same-process products, not a new asset or save format. Each recipe
owns its file and releases it independently of neighbouring chunks. A leased
geometry product remains valid after its terrain region is removed.

Normal project/world saves remain authoritative. Private spill storage is **not
an automatic game save**. Save before quitting to retain gameplay progress. Cold
loads recreate caches from the checked native base, sparse raster changes and
volume journal. Existing changed-base rejection and migration policy remain.

## Budgets and failure reporting

The cache budgets above apply per terrain owner and allocate on demand. The
existing limit of eight owners bounds their sum to 576 MiB of CPU caches and
256 MiB of GPU pages. These figures exclude scene/physics metadata, navigation,
shared material assets, driver allocations and bounded preparation scratch.
Use process RSS and renderer accounting alongside the terrain counters.

Preparation may hold an unpublished worker product and one serialized query-load
product in addition to the cache. Expensive worker construction happens outside
the cache mutex, so cached physical queries continue while edits are prepared. The LOD worker uses at most 16 MiB of height-only scratch, released before
publication. A private derived page is at most 32 MiB, with a 512 MiB derived-file
quota per owner. Reading it also uses a bounded byte buffer. These are explicit
working allowances, not permanently resident copies of the source.

Pinned pages are not silently removed to satisfy a budget. Excessive simultaneous
consumer demand reports a resource-budget error. Failed preparation and unavailable spill storage retain the last complete published
revision. A live query that cannot read its required page reports an error rather
than returning empty terrain; continued simulation is not promised with missing
or damaged backing data. Native bounds-only shape access
is for broadphase: use `PrimitiveAt` or ordinary `GetBodyShape` to acquire geometry
before reading its arrays. The default public getter returns a complete lease.

Other unchanged bounds include 1,024 chunks per owner, 65,536 persistent native
raster overrides and the existing 8 MiB continuation string limit. Split large
editable content into regions rather than bypassing these limits. Procedural
continuations still encode their source; native assets use identity references.

## Diagnostics and measurement

`entity.terrain.state` exposes `sourcePageBytes`, `sourcePageBudget`, `sourcePages`,
`collisionPageBytes`, `collisionPageBudget`, `collisionPages`, `gpuPageBytes`,
`gpuPageBudget`, `gpuPages`, the three `*Evictions` counters, `pagePrepareMs` and
`worstPagePrepareMs`. Native Terrain V2 `bytes` includes estimated current CPU
payload (samples, recipes, cooked geometry and navigation). Legacy bounded volume
terrain retains its previous grid estimate. These counters are not whole-process
RAM or total driver VRAM.

`pagePrepareMs` is the most recent GPU-page preparation/upload duration, and
`worstPagePrepareMs` is its high-water mark; neither is a per-frame sum. The
residency benchmark measures complete frames and cold physical queries separately.
Run `judas_terrain_v2_game_benchmark PROJECT --traverse` only with the generated
one-kilometre qualification fixture: it visits all 1,024 chunks three times and
asserts the actual runtime camera. It is a diagnostic workload, not an FPS gate.

See the final qualification record for measured costs, full commands, platform
coverage and remaining limitations. Alpha's Advanced Fluids lock is unchanged.
