# Grid Water engine sources. Shared by desktop and the Android runtime branch.
target_sources(judas_engine PRIVATE
    src/fluid/GameWater.cpp src/fluid/GameWaterAir.cpp src/fluid/GameWaterGeometry.cpp
    src/fluid/GameWaterPressure.cpp src/fluid/GameWaterTransport.cpp src/fluid/GameWaterSources.cpp
    src/fluid/GameWaterCoupling.cpp src/fluid/GameWaterPersistence.cpp
    src/fluid/GameWaterSurface.cpp src/fluid/GameWaterRuntime.cpp
    src/fluid/GameWaterRuntimeGeometry.cpp src/fluid/GameWaterRuntimeQueries.cpp
    src/fluid/GameWaterRuntimePersistence.cpp src/fluid/GameWaterRuntimePresentation.cpp)
