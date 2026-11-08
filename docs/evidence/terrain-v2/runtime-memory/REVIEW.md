# Terrain V2 runtime mesh retention

The ordinary native landscape renderer uses shared grids and its source height
image. Physics owns a separate immutable CollisionAsset, and source-owned
navigation consumes that cook. Published ordinary, unexcavated chunks therefore
do not need their extracted CPU MeshData. The coordinator now releases those draw
streams after collision preparation and successful staging. Excavated and curved
planetary chunks retain their unique draw meshes. Jobs, cooking and editor preview
products remain complete; no triangles, colliders or collision detail are evicted.
Generated triangle counts remain explicit even after unused draw streams retire.

The new live-payload control failed after12checks on the unchanged runtime, then
passed26checks with the correction. It verifies the exact released stream bytes,
retained triangle metric, logical ownership and prepared collision certificate.
The original regional >32MiB accounting sentinel still passes unchanged.

Matched513x513,2m,1km fixtures,5runs each, no compiler during timing:

| Configuration | Initial retained payload before / after | Excavated before / after |
| --- | --- | --- |
| No navigation |76,132,736 /53,268,864 bytes|77,022,588 /54,248,028 bytes|
| Full1km navigation |79,206,120 /56,342,248 bytes|80,095,972 /57,321,412 bytes|

The ordinary source releases22,863,872bytes (30.0% of its initial no-nav payload).
All1,024colliders,524,288initial and530,810edited generated triangles remain.
Every measured edited collision fingerprint aggregate is identical. Snapshot
hashes differ because each fresh fixture gets a different randomly assigned asset
identity; they are retained, NOT presented as byte-identical saves. Independent
cold tests below exercise real exact continuation with stable fixture identities.

These are estimated owned payloads, not process RSS. Preparation still builds the
complete mesh before release. Measured peak process RSS changes only156,540 to
155,744KiB without nav and230,996 to223,176KiB with nav; allocator retention and
temporary products prevent interpreting payload savings as equal peak savings.
Full-nav edit medians296.156/295.579ms and sampled worst300.866/305.508ms show no
meaningful total-latency improvement. No loading/FPS/GPU speedup is claimed.
The actual remaining full-coverage navigation preparation cost is still visible.

World93checks retain300supported seam steps and zero difference from monolithic
physical trajectories. Existing components35, surface59, navigation73,
interaction85, streaming69, planetary components40/streaming37 pass. The real
5,271,385byte runtime-source and inactive-region cold tests pass30write/18read.
Actual NVIDIA modern4.6 and portable3.3 curved rendering pass343checks each.
Both actual editable games are exported, genuinely relocated, excavated through
8chunks, traversed, saved and cold-restored by the normal gameplay driver.
Their screenshots were inspected. These are automated results, not human review.

Reproduce with the observer.patch applied to base8ca4ee17 and to the implementation:
build judas_terrain_v2_native_benchmark and run it with a fresh output directory,
optionally --navigation-full. Use /usr/bin/time -v for independent peak RSS.
The original before executable remains under
.cache/terrain-v2/runtime-memory/native-before; binary hashes and raw runs are in
RESULTS.json. The target name judas_exporter in the first multi-target command was
a driver mistake; prior targets completed, the command failed explicitly, and the
correct judas_export and judas_terrain_author targets were then built successfully.
No failed build is counted as a complete gate.

Main, the fluid lock, release channels, installed RPG SDK, unrelated screenshots,
research and games remain unchanged. Per-chunk physical interest and native data
demand paging remain separate open work; this correction does not claim either.
