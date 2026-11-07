# Terrain data, extraction and runtime publication have separate ownership.
target_sources(judas_engine PRIVATE
    src/terrain/TerrainSource.cpp
    src/terrain/TerrainBrush.cpp
    src/terrain/TerrainMesher.cpp
    src/terrain/TerrainTopology.cpp
    src/terrain/TerrainRuntime.cpp
    src/WorldTerrainQueries.cpp)
judas_add_engine_test(judas_terrain_v2_tests tests/TerrainV2Tests.cpp)
judas_add_engine_test(judas_terrain_v2_world_tests tests/TerrainV2WorldTests.cpp)
# Matched geometry preparation benchmark, intentionally not a timing gate.
add_executable(judas_terrain_v2_benchmark tests/TerrainV2Benchmark.cpp)
target_link_libraries(judas_terrain_v2_benchmark PRIVATE judas_world_authoring)
judas_warnings(judas_terrain_v2_benchmark)

judas_add_engine_test(judas_terrain_v2_editor_tests tests/TerrainV2EditorTests.cpp)
target_link_libraries(judas_terrain_v2_editor_tests PRIVATE judas_editor_core)
