# Native regional streaming baseline

```sh
cmake --build build -j2 --target judas_terrain_v2_streaming_tests
build/judas_terrain_v2_streaming_tests --write /tmp/native-region-unique
build/judas_terrain_v2_streaming_tests --read /tmp/native-region-unique
```

Use fresh paths, no concurrent builds or benchmarks. Same fixture serves before
and after. A1024m513² native source loads1024 actual collision chunks, is sculpted
and dug, suspends/returns5times, retires all physical/source ownership and restores
in a separate process. Triangle count and object counts are preserved.

The first fixture correctly failed archive validation because its2e12/3e12 origin
exceeded the declared±1e12 contract. The corrected fixture uses OriginLimits.
Native source and region snapshot states pass67/13 controls before correction.
The new preparation module is also independently checked against source93,
surface44, solid/water/particle85 and streaming67/cold13 controls; seam trajectory
difference remains exactly zero. No speedup is claimed for extraction.

Measured defects are frame-thread Wait/cooking during registration and omission
of native source/mesh/BVH payload from region budget accounting.
