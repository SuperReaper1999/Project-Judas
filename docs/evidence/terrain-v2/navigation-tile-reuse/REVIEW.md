# Terrain V2 incremental navigation preparation

## Corrected mechanism

Native terrain already rebuilt only dirty physical chunks, but each edit still
rasterized its entire declared navigation coverage. Moving the work off-thread
removed publication stalls without removing that unnecessary work. A real1km
fixture still waited about295ms for one local excavation.

Terrain now gives the existing baker a complete immutable previous result and
changed geometry bounds. The bounds are derived from old/new immutable collision
ownership, not camera visibility or brush centres. Removed support contributes
its OLD extent; replacement/moved support contributes its NEW extent. The exact
binary32 raster bounds include erosion/filter borders and inclusive tangencies.
Untouched tiles copy their previous compressed layers, including genuinely empty
tiles. Dirty tiles use the unchanged Recast operator and ordinary source geometry.
Changing coverage, resolution, profile or other bake parameters forces a full bake.

`navigation/NavigationTileReuse.*` owns compatibility, bounded tile membership and
previous-layer indexing. The existing baker owns rasterization; terrain owns its
geometry dependencies. No new scene system, physics engine, runtime dispatcher or
JudasJS API is introduced. Previous results do not form a retained cache chain.
The optional native bake argument is source-compatible through a default value;
native consumers must rebuild against the matching preview SDK. Preparation-only
rebuilt/reused counts are not serialized or used for gameplay decisions. Saved
navigation formats, water policy and collision detail are unchanged.

## Matched results

Ryzen5 5600, GNU15.2 Release, warm native513x513source at2m pitch,1024colliders,
full1024m navigation coverage at1m raster cells. Five runs before, five after,
then five after and five before; no compiler ran during these measurements.
This is complete asynchronous edit-to-publication wall time with a waiting caller,
not physics-step cost, cold-disk latency or game FPS. Ten samples per side make
nearest-rank p95 equal the sampled worst; they do not establish tail guarantees.

| Operation | Before median / p95 / worst | After median / p95 / worst |
| --- | --- | --- |
| Local excavation |295.345 /303.981 /303.981ms|75.691 /82.351 /82.351ms|
| Surface sculpt |292.923 /315.319 /315.319ms|72.241 /89.090 /89.090ms|
| Whole initial scene setup |704.926 /790.928 /790.928ms|716.385 /764.959 /764.959ms|

Excavation median fell74.4%; sampled worst fell72.9%. Initial full preparation
has no useful improvement and its median varies upward1.6% in these samples.
No startup, memory or GPU speedup is claimed. Geometry enumeration/binning and
full private Detour-cache construction remain, so this is not constant-time editing.
The same1024colliders,530810edited triangles,967live navmesh tiles and collision
geometry fingerprints remain in every measured row. Random fresh asset IDs make
snapshot hashes differ; those hashes are retained rather than presented as equal.

## Correctness and integration

The new67-check target compares compressed layers AND geometry identity with a
full-coverage bake. It covers repeated cross-chunk/cross-tile caves, real height
sculpting, removed/empty support, replacement placement, unchanged/empty reuse,
resolution/profile fallback, cancellation, malformed prior headers and nonfinite
bounds. Independent border/tangent/one-ULP controls catch omitted filter margins.
The SAME authored kilometre source/profile/sculpt/excavation as the measured
fixture also compares byte for byte:16dependent tiles rebuild and1008reuse,
producing975compressed layers. Compressed layers and967live Detour mesh tiles
are different metrics, not contradictory counts.

The frozen pre-indexing full-raster reference remains unchanged; its59controls
pass. Original navigation92, native navigation73, preparation26, world93,
component35, surface59, streaming69, planetary component40/streaming37 and source
water/particle85controls pass. Physical seam travel remains300supported steps
with exactly zero difference from the monolithic trajectory. Separate navigation
cold73write/7read and large runtime-source/inactive-region30write/18read pass.
The5.27MB cold source remains real, not a mocked byte counter.

Actual NVIDIA editor creation/excavation/navigation/Play/preview controls pass23
each on modernGL4.6 and portableGL3.3. Both ordinary and planetary games are
exported, genuinely relocated, excavated/traversed using normal project scripts,
saved and restored in new processes. The JudasJS API contract passes unchanged;
test registration passes11with2existing environment skips. All17SDK tools build.
No human acceptance, Windows/Android/browser qualification or publication is claimed.

The first build command requested the new target before CMake regenerated its
target list and failed explicitly. Configure and subsequent builds pass; the
failed setup log is retained. Earlier44/55-check runs preceded the stronger final
67-check fixture and remain distinct. No physical or regression expectation was
weakened to obtain the result.

## Reproduction and remaining bounds

Build `judas_terrain_v2_navigation_reuse_tests`, `judas_navigation_raster_tests`
and `judas_terrain_v2_native_benchmark` with-j2. Run the first two directly.
Run the benchmark with a fresh output directory and `--navigation-full`; use
`/usr/bin/time -v` separately for process RSS. The before executable remains at
`.cache/terrain-v2/runtime-memory/native-before-navigation`; source/binary hashes,
raw rows, process measurements and workflow commands accompany this review.

Native terrain still loads complete source arrays and physical chunks while its
region is resident. Per-chunk physical interest and demand paging remain open;
this change is incremental NAVIGATION, not physical eviction. Native navigation's
16M-column/4096-tile budgets, full private runtime-cache construction and other
documented terrain limits remain. Main, supported releases, the Alpha fluid lock,
existing games/SDK selections and independent research are untouched.
