# The runtime owns the optional scene backend. Ordinary projects allocate no
# grid state. The separate diagnostics executable remains opt-in.
target_sources(judas_engine PRIVATE
    src/fluid/GameWater.cpp src/fluid/GameWaterAir.cpp src/fluid/GameWaterGeometry.cpp
    src/fluid/GameWaterPressure.cpp src/fluid/GameWaterTransport.cpp src/fluid/GameWaterSources.cpp
    src/fluid/GameWaterCoupling.cpp src/fluid/GameWaterPersistence.cpp
    src/fluid/GameWaterSurface.cpp src/fluid/GameWaterRuntime.cpp
    src/fluid/GameWaterRuntimeGeometry.cpp src/fluid/GameWaterRuntimeQueries.cpp
    src/fluid/GameWaterRuntimePersistence.cpp src/fluid/GameWaterRuntimePresentation.cpp)
# Regression targets are available to the normal focused validation manifest,
# independently of the optional interactive diagnostic application.
add_library(judas_game_water INTERFACE)
target_link_libraries(judas_game_water INTERFACE judas_engine)
judas_add_engine_test(judas_game_water_tests tests/GameWaterTests.cpp)
judas_add_engine_test(judas_game_water_runtime_tests tests/GameWaterRuntimeTests.cpp)
judas_add_engine_test(judas_game_water_script_tests tests/GameWaterScriptTests.cpp)
judas_add_engine_test(judas_water_workshop_tests tests/WaterWorkshopApplicationTests.cpp)
add_custom_target(judas_game_water_qualification DEPENDS judas_game_water_tests
    judas_game_water_runtime_tests judas_game_water_script_tests)

option(JUDAS_BUILD_GAME_WATER_LAB "Build the source-only coarse-water diagnostic" OFF)
if(JUDAS_BUILD_GAME_WATER_LAB)
    if(ANDROID OR EMSCRIPTEN)
        message(FATAL_ERROR "The game-water diagnostic is currently a desktop qualification target.")
    endif()
    add_executable(judas_game_water_lab
        tools/diagnostics/GameWaterLab.cpp tools/diagnostics/GameWaterLabScene.cpp)
    target_link_libraries(judas_game_water_lab PRIVATE judas_game_water judas_world_authoring)
    judas_warnings(judas_game_water_lab)
endif()
