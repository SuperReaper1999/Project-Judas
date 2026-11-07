# Explicit source-preview target. It does not select a runtime backend, alter
# the Alpha lock, enter SDK packaging, or add work to ordinary projects.
option(JUDAS_BUILD_GAME_WATER_LAB "Build the source-only coarse-water preview" OFF)
if(NOT JUDAS_BUILD_GAME_WATER_LAB)
    return()
endif()
if(ANDROID OR EMSCRIPTEN)
    message(FATAL_ERROR "The game-water preview lab is currently a desktop qualification target.")
endif()
add_library(judas_game_water STATIC
    src/fluid/GameWater.cpp
    src/fluid/GameWaterAir.cpp
    src/fluid/GameWaterGeometry.cpp
    src/fluid/GameWaterPressure.cpp
    src/fluid/GameWaterTransport.cpp
    src/fluid/GameWaterCoupling.cpp
    src/fluid/GameWaterPersistence.cpp
    src/fluid/GameWaterSurface.cpp)
target_link_libraries(judas_game_water PUBLIC judas_engine)
judas_warnings(judas_game_water)
add_executable(judas_game_water_lab
    tools/diagnostics/GameWaterLab.cpp tools/diagnostics/GameWaterLabScene.cpp)
target_link_libraries(judas_game_water_lab PRIVATE judas_game_water judas_world_authoring)
judas_warnings(judas_game_water_lab)
judas_add_engine_test(judas_game_water_tests tests/GameWaterTests.cpp LIBRARIES judas_game_water)
enable_testing()
add_test(NAME experimental.game-water COMMAND judas_game_water_tests)
set_tests_properties(experimental.game-water PROPERTIES LABELS "experimental;game-water" TIMEOUT 120)
