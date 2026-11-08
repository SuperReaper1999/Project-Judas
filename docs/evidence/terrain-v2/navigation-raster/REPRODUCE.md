# Navigation rasterization and reference capacity

Base34b6ab5b / Experimental / Release / GCC15.2 / Ryzen5 5600,8GB.
Before executable/source retained under .cache/terrain-v2/navigation-raster-reference.
The exact before runtime rejects full1km data: 975 compressed layers reserve3900
native tiles, rounded4096. With2048poly slots,32-bit Detour has9saltbits, below
its10-bit requirement. This is not a usable whole-game timing baseline.

The correction uses pinned Detour's supported DT_POLYREF64 definition PUBLIC
on the backend, rebuilding every consumer consistently. No third-party source
patch or weakened reference checks. Compressed .judasnav files contain layer
geometry, not polygon refs: the unchanged SpringRange resources still run26
checks and existing cold saves pass. Internal native ABI changes; rebuild tools
and native consumers together. Eight-layer obstacle query limits remain.

Rasterization keeps the exact old float clipping, area classification and source
triangle order. NavigationRasterBins rejects only disjoint XZ bounds using exact
float tile intervals including erosion borders. Inclusive/one-ULP and32768m local
offset tests compare against exhaustive membership. Six terrain/cave/material
modifier bakes match every compressed layer byte against a frozen full-scan
operator.4M index references cap storage; overflow releases the index and uses
the original complete scan. No geometry/resolution/physics coverage is dropped.

```sh
cmake -S . -B build
cmake --build build -j2 --target judas_navigation_raster_tests judas_terrain_v2_native_benchmark judas_terrain_v2_navigation_tests
build/judas_navigation_raster_tests
/usr/bin/time -v build/judas_navigation_raster_tests --full
build/judas_terrain_v2_native_benchmark /tmp/full-nav-FRESH --navigation-full
build/judas_terrain_v2_navigation_tests --write /tmp/native-nav-FRESH
build/judas_terrain_v2_navigation_tests --read /tmp/native-nav-FRESH
```

No compiler/other benchmark during timing. Five alternating whole-bake pairs,
524288triangles, same1m cells and entire1024m bounds: median2277.29ms→239.448ms,
all975layer bytes EXACT. Native integration now loads1,024colliders with full
configured navigation: median704.70ms startup,292.79ms complete sculpt,296.74ms
excavation,26.23ms publication. The last is still a visible owner-thread cost;
we have not eliminated large-region edit hitches. Five samples aren't broad
performance distributions. Native voxel settings can exclude unwalkable areas;
configured coverage does not promise every point is routable.

Qualification: index59, full-reference17, native64/cold7, world93/exact300-step
seam trajectory, component35/surface59/handoff25/interactions85, nav92, actual
SpringRange26, saveExclusions77/export18, relocatedWorkshop all PASS. Main,
Alpha fluid lock, supported releases and installed RPG SDK untouched. No human
acceptance claimed. Remaining work includes native publication cost, planetary
excavation, physical residency and final SDK/developer qualification.
