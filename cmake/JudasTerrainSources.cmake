# Terrain V2 engine sources. Shared by desktop and the Android runtime branch.
target_sources(judas_engine PRIVATE
    src/terrain/TerrainSource.cpp
    src/terrain/TerrainSamples.cpp
    src/terrain/TerrainGeometryPages.cpp
    src/terrain/TerrainGeometrySpill.cpp
    src/terrain/TerrainPlanetChart.cpp
    src/planet/PlanetCoverage.cpp
    src/terrain/TerrainPlanetBinding.cpp
    src/terrain/TerrainAsset.cpp
    src/terrain/TerrainComponent.cpp
    src/terrain/TerrainLod.cpp
    src/terrain/TerrainLodTransition.cpp
    src/terrain/TerrainBrush.cpp
    src/terrain/TerrainSurface.cpp
    src/terrain/TerrainSolid.cpp
    src/terrain/TerrainMesher.cpp
    src/terrain/TerrainTopology.cpp
    src/terrain/TerrainRuntime.cpp
    src/terrain/TerrainPreparation.cpp
    src/terrain/TerrainCollision.cpp
    src/terrain/TerrainNavigation.cpp
    src/terrain/TerrainPresentation.cpp
    src/terrain/TerrainRenderPages.cpp
    src/terrain/TerrainPersistence.cpp
    src/WorldTerrainQueries.cpp)
