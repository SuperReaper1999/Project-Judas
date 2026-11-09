# Desktop qualification and opt-in diagnostics. The engine runtime sources are
# shared through GameWaterRuntimeSources.cmake on every platform.

# Regression targets are available to the normal focused validation manifest,
# independently of the optional interactive diagnostic application.
add_library(judas_game_water INTERFACE)
target_link_libraries(judas_game_water INTERFACE judas_engine)
judas_add_engine_test(judas_game_water_tests tests/GameWaterTests.cpp)
judas_add_engine_test(judas_game_water_runtime_tests tests/GameWaterRuntimeTests.cpp)
judas_add_engine_test(judas_game_water_script_tests tests/GameWaterScriptTests.cpp)
judas_add_engine_test(judas_water_workshop_tests tests/WaterWorkshopApplicationTests.cpp)
judas_add_engine_test(judas_water_visual_tests tests/WaterVisualTests.cpp)
target_sources(judas_water_visual_tests PRIVATE tests/WaterVisualGpuControls.cpp)
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
