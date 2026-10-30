cmake_minimum_required(VERSION 3.20)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/engine-version.json" judas_version_document)
string(JSON judas_project_version GET "${judas_version_document}" version)
string(REGEX REPLACE "[-+].*$" "" judas_project_version "${judas_project_version}")
project(ProjectJudas VERSION "${judas_project_version}" LANGUAGES C CXX)

if(WIN32)
    if((NOT MSVC AND NOT MINGW) OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Windows requires MSVC x64 or the optional MinGW-w64 x64 toolchain")
    endif()
    if(MSVC AND NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows-static-md")
        message(FATAL_ERROR "Windows SDK requires -DVCPKG_TARGET_TRIPLET=x64-windows-static-md")
    endif()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN SDL_MAIN_HANDLED _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        add_compile_options(/utf-8 /bigobj)
    else()
        add_compile_options(-Wa,-mbig-obj)
        add_link_options(-Wl,--no-insert-timestamp)
    endif()
endif()

function(judas_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra ${ARGN})
    endif()
endfunction()

include(cmake/JudasTestTargets.cmake)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/JudasBuildIdentity.cmake)
judas_build_identity("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/judas-build-info-$<CONFIG>.json"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/judas-build-info.json.in")
add_library(judas_build_identity STATIC src/BuildIdentity.cpp src/BuildUpdate.cpp src/ToolProcess.cpp)
set_target_properties(judas_build_identity PROPERTIES POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)
target_link_libraries(judas_build_identity PUBLIC Threads::Threads)
target_include_directories(judas_build_identity PUBLIC src PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_compile_definitions(judas_build_identity PRIVATE JUDAS_BUILD_CONFIGURATION="$<CONFIG>")
judas_warnings(judas_build_identity)
add_executable(judas_build_identity_tests tests/BuildIdentityTests.cpp)
target_link_libraries(judas_build_identity_tests PRIVATE judas_build_identity)
add_executable(judas_release_update_tests tests/ReleaseUpdateTests.cpp)
target_link_libraries(judas_release_update_tests PRIVATE judas_build_identity)
judas_warnings(judas_release_update_tests)
judas_warnings(judas_build_identity_tests)
option(JUDAS_BUILD_IDENTITY_ONLY "Configure only dependency-light build provenance checks" OFF)
if(JUDAS_BUILD_IDENTITY_ONLY)
    if(MSVC)
        target_link_options(judas_build_identity_tests PRIVATE "/MANIFEST:EMBED"
            "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/judas.manifest")
    endif()
    return()
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(ANDROID)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    include(cmake/JudasAndroidDependencies.cmake)
elseif(MINGW)
    include(cmake/JudasWindowsCrossDependencies.cmake)
else()
    find_package(SDL2 REQUIRED)
endif()
if(WIN32 AND TARGET SDL2::SDL2-static)
    # Keep the existing consumer target while using the static Windows port.
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 ALIAS SDL2::SDL2-static)
    endif()
endif()
# The original app artwork is embedded, so neither editor nor game windows
# need a development-tree path. Reconfigure when the PNG changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS assets/branding/judas-icon.png)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/judas-icon.png" JUDAS_APP_ICON_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1," JUDAS_APP_ICON_BYTES "${JUDAS_APP_ICON_HEX}")
configure_file(cmake/JudasAppIcon.h.in generated/JudasAppIcon.h @ONLY)
configure_file(cmake/judas-editor.desktop.in judas-editor.desktop @ONLY)
add_library(judas_app_icon STATIC src/AppIcon.cpp src/TextureLoader.cpp src/CookedTexture.cpp)
target_include_directories(judas_app_icon PUBLIC src PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(judas_app_icon PUBLIC SDL2::SDL2)
judas_warnings(judas_app_icon)
file(GLOB JUDAS_MESHOPT_SOURCES CONFIGURE_DEPENDS third_party/meshoptimizer/src/*.cpp)
add_library(judas_meshoptimizer STATIC ${JUDAS_MESHOPT_SOURCES})
target_include_directories(judas_meshoptimizer PUBLIC third_party/meshoptimizer/src)
include_directories(SYSTEM third_party/quickhull)
set_source_files_properties(third_party/quickhull/QuickHull.cpp PROPERTIES COMPILE_OPTIONS "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>")
if(NOT ANDROID AND NOT MINGW)
    find_package(glm REQUIRED)
endif()
include(cmake/JudasTextDependencies.cmake)

# GLM is header-only, but the imported target it exports depends on the
# packaged version: Android declares `glm::glm-header-only` explicitly, while
# desktop packages commonly provide `glm::glm` or `glm`. Resolve once here so a stock
# `apt install libglm-dev` configures cleanly (M28 portability fix).
if(TARGET ${JUDAS_GLM})
    # The optional Android dependency setup already selected its public target.
elseif(TARGET glm::glm)
    set(JUDAS_GLM glm::glm)
elseif(TARGET glm)
    set(JUDAS_GLM glm)
else()
    message(FATAL_ERROR "GLM was found but exports no known imported target")
endif()

# Optional native 2D value/projection checks need no physics or GL host.
add_executable(judas_sprite_math_tests tests/SpriteMathTests.cpp src/Sprite.cpp)
target_include_directories(judas_sprite_math_tests PRIVATE src)
target_link_libraries(judas_sprite_math_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_sprite_math_tests)

# No physics middleware. Judas owns rigid-body state, integration,
# collision detection, contact generation, and contact resolution itself
# (src/RigidBody.*, src/Contacts.*, src/ContactSolver.*, src/PhysicsWorld.cpp)
# — see docs/ARCHITECTURE.md, "Physics ownership," for the milestone this
# decision belongs to and why Jolt Physics (used through the first M7-Final
# attempt, recoverable via git history/the milestone-7b tag for the last
# Jolt-backed milestone) was replaced rather than kept as an authoritative
# dependency.

include(cmake/PhysicsWorldSources.cmake)

set(JUDAS_PHYSICS_SOURCES
    src/PlanarGeometry.cpp
    src/CollisionAsset.cpp
    src/CollisionGeometry.cpp
    third_party/quickhull/QuickHull.cpp
    src/JointSolver.cpp
    src/PhysicsCastGeometry.cpp
    src/ImpactSolver.cpp
    src/RigidBody.cpp
    src/RigidRotation.cpp
    src/RigidBodyGravity.cpp
    src/Contacts.cpp
    src/ContactSolver.cpp
    src/RadialTerrain.cpp
    src/Broadphase.cpp
    src/Narrowphase.cpp
)

# Pinned QuickJS-NG core; intentionally excludes libc (filesystem/process modules).
add_library(judas_quickjs STATIC third_party/quickjs/quickjs.c third_party/quickjs/dtoa.c
    third_party/quickjs/libregexp.c third_party/quickjs/libunicode.c)
set_target_properties(judas_quickjs PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_compile_definitions(judas_quickjs PRIVATE _GNU_SOURCE QUICKJS_NG_BUILD)
target_include_directories(judas_quickjs SYSTEM PUBLIC third_party/quickjs)
target_link_libraries(judas_quickjs PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(NOT WIN32)
    target_link_libraries(judas_quickjs PUBLIC m)
endif()

# Pinned RecastNavigation 1.6.0. Bounded geometry baking is shared with optional live terrain.
file(GLOB JUDAS_DETOUR third_party/recastnavigation/Detour/Source/*.cpp third_party/recastnavigation/DetourTileCache/Source/*.cpp third_party/recastnavigation/DetourCrowd/Source/*.cpp)
add_library(judas_navigation_backend STATIC ${JUDAS_DETOUR})
target_include_directories(judas_navigation_backend SYSTEM PUBLIC third_party/recastnavigation/Detour/Include third_party/recastnavigation/DetourTileCache/Include third_party/recastnavigation/DetourCrowd/Include)
target_compile_definitions(judas_navigation_backend PUBLIC DT_VIRTUAL_QUERYFILTER)
file(GLOB JUDAS_RECAST third_party/recastnavigation/Recast/Source/*.cpp)
add_library(judas_navigation_cook STATIC src/NavigationGeometryBake.cpp ${JUDAS_RECAST})
target_include_directories(judas_navigation_cook SYSTEM PRIVATE third_party/recastnavigation/Recast/Include)
target_include_directories(judas_navigation_cook PUBLIC src)
target_link_libraries(judas_navigation_cook PUBLIC judas_navigation_backend ${JUDAS_GLM})
add_library(judas_navigation_baker STATIC src/NavigationBake.cpp)
target_include_directories(judas_navigation_baker SYSTEM PRIVATE third_party/recastnavigation/Recast/Include)
target_include_directories(judas_navigation_baker PUBLIC src)
target_link_libraries(judas_navigation_baker PUBLIC judas_engine judas_navigation_backend)

# GLAD 2 loader generated for precisely OpenGL 3.3 Core, no extensions.
if(ANDROID)
    add_library(judas_glad INTERFACE)
    target_link_libraries(judas_glad INTERFACE GLESv3)
else()
    add_library(judas_glad STATIC third_party/glad/src/gl.c)
    target_include_directories(judas_glad PUBLIC third_party/glad/include)
    judas_warnings(judas_glad)
endif()

# Milestone 28: engine sources shared by the runtime, the editor and the
# scene/runtime test suites. Neither executable reimplements engine
# startup, scene loading, instantiation, stepping or presentation.
add_library(judas_profiler STATIC src/PerformanceProfiler.cpp)
target_include_directories(judas_profiler PUBLIC src)
judas_warnings(judas_profiler)

add_library(judas_materials STATIC third_party/mikktspace/mikktspace.c)
target_include_directories(judas_materials PUBLIC src)
target_link_libraries(judas_materials PUBLIC ${JUDAS_GLM} judas_profiler)

add_library(judas_engine STATIC
    src/NetworkSession.cpp
    src/NetworkTransport.cpp
    src/Wheel.cpp
    src/WorldWheels.cpp
    src/Powertrain.cpp
    src/PowertrainAuthoring.cpp
    src/WorldPowertrains.cpp
    src/modding/ModContent.cpp
    src/modding/ContentStorage.cpp
    src/modding/ModStore.cpp
    src/modding/ModRuntime.cpp
    src/WorldFracture.cpp
    src/Fracture.cpp
    src/DeformableAsset.cpp
    src/Deformable.cpp
    src/WorldDeformables.cpp
    src/TextLayout.cpp
    src/TextEditing.cpp
    src/Localization.cpp
    src/Material.cpp
    src/Environment.cpp
    src/Tangents.cpp
    src/LiquidGeometry.cpp
    src/LiquidTypes.cpp
    src/LiquidSystem.cpp
    src/LiquidSurface.cpp
    src/VolumeTerrainGeometry.cpp
    src/RuntimeTerrain.cpp
    src/NavigationSystem.cpp
    src/NavigationTypes.cpp
    src/NavigationAsset.cpp
    src/FullBodyIK.cpp
    src/PhysicalAnimation.cpp
    src/PhysicalBalance.cpp
    src/PhysicalBalanceCapture.cpp
    src/PhysicalBalanceGetUp.cpp
    src/WorldPhysicalBalance.cpp
    src/WorldAnimation.cpp
    src/PoseComposition.cpp
    src/WorldRagdoll.cpp
    src/GltfLoader.cpp
    src/WorldCharacter.cpp
    src/WorldGravity.cpp
    src/WorldStreaming.cpp
    src/WorldRegions.cpp
    src/WorldScripts.cpp
    src/RuntimeUI.cpp
    src/RuntimeUIControls.cpp
    src/ScriptSystem.cpp
    src/ScriptBindingsUI.cpp
    src/ScriptBindingsJoint.cpp
    src/Classification.cpp
    src/GamePackage.cpp
    src/ProjectExporter.cpp
    src/AssetDependencies.cpp
    src/AudioStream.cpp
    src/AudioEnvironment.cpp
    src/AudioData.cpp
    src/AudioSystem.cpp
    src/WorldAudio.cpp
    src/Window.cpp
    src/InputDeviceHub.cpp
    src/LocalMultiplayer.cpp
    src/InputSystem.cpp
    src/Renderer.cpp
    src/RenderLightGrid.cpp
    src/SkeletalAnimation.cpp
    src/CharacterMotor.cpp
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/StepClimb.cpp
    src/DynamicBody.cpp
    src/FlyingPrimitiveControl.cpp
    src/PilotAttachment.cpp
    src/PilotControl.cpp
    src/ReferenceFrame.cpp
    src/CollisionAuthoring.cpp
    src/CollisionSource.cpp
    src/ModelLoader.cpp
    src/ModelArchive.cpp
    src/AssetCook.cpp
    src/FontLoader.cpp
    src/UIWidgets.cpp
    src/PauseMenu.cpp
    src/HUD.cpp
    src/LightTransforms.cpp
    src/LightAttenuation.cpp
    src/ShadowTransforms.cpp
    src/HingeTransform.cpp
    src/InteractionSystem.cpp
    src/ObjectManipulation.cpp
    src/FluidWorld.cpp
    src/ProductionFluidCoupling.cpp
    src/FluidHydrostatics.cpp
    src/FluidSurface.cpp
    src/CombustionWorld.cpp
    src/FirePresentation.cpp
    src/TerrainDemo.cpp
    src/TerrainLibrary.cpp
    src/Door.cpp
    src/LightSwitch.cpp
    src/FaithfulGravity.cpp
    src/RadicalGravity.cpp
    src/SphericalVolume.cpp
    src/BoxVolume.cpp
    src/GravityContextMap.cpp
    src/CelestialGravity.cpp
    src/AtmosphereField.cpp
    src/AerodynamicDrag.cpp
    src/VisualParticles.cpp
    src/Prefab.cpp
    src/Scene.cpp
    src/Sprite.cpp
    src/PlanarAuthoring.cpp
    src/Ragdoll.cpp
    src/SceneSerialization.cpp
    src/NamedAuthoring.cpp
    src/SceneFingerprint.cpp
    src/Project.cpp
    src/ProjectStarter.cpp
    src/AssetDatabase.cpp
    src/AsyncFile.cpp
    src/ResourceManager.cpp
    src/EnginePaths.cpp
    src/DebugDraw.cpp
    src/WorldDebugView.cpp
    src/SceneSession.cpp
    src/PlanetTerrain.cpp
    src/PlanetTiles.cpp
    src/WorldPlanets.cpp
    src/Weather.cpp
    src/WorldWeather.cpp
    src/WorldOrigin.cpp
    src/RuntimeWorld.cpp
    src/GameSession.cpp
    src/Simulation.cpp
    src/BasicWater.cpp
    src/WorldWater.cpp
    src/WorldPresentation.cpp
    src/GameplayHud.cpp
    src/EngineHost.cpp
    src/InteractivePlay.cpp
    src/ScreenshotWriter.cpp
    src/FidelityPolicy.cpp
    src/CoarseSimulation.cpp
    src/WorldState.cpp
    src/SaveStorage.cpp
    src/SaveStorageWindows.cpp
    src/PlatformServices.cpp
    src/SaveEvolution.cpp
    src/SaveService.cpp
    src/MotorPersistence.cpp
    src/LiquidPersistence.cpp
    src/WorldPersistence.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_engine PUBLIC src ${SDL2_INCLUDE_DIRS})
find_package(Threads REQUIRED)
add_library(judas_jobs STATIC src/JobSystem.cpp)
target_include_directories(judas_jobs PUBLIC src)
target_link_libraries(judas_jobs PUBLIC Threads::Threads)
judas_warnings(judas_jobs)
add_library(judas_audio_backend STATIC src/AudioBackend.c src/ReverbBackend.c)
target_include_directories(judas_audio_backend PUBLIC third_party/miniaudio third_party/verblib)
target_compile_definitions(judas_audio_backend PUBLIC MA_NO_RESOURCE_MANAGER MA_NO_ENCODING MA_NO_GENERATION)
target_link_libraries(judas_audio_backend PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(judas_audio_backend PUBLIC m)
endif()
target_link_libraries(judas_engine PUBLIC judas_build_identity judas_app_icon judas_materials SDL2::SDL2 ${JUDAS_GLM} judas_glad Threads::Threads judas_audio_backend judas_navigation_backend judas_meshoptimizer judas_navigation_cook)
judas_warnings(judas_engine)
if(WIN32)
    target_link_libraries(judas_engine PUBLIC ws2_32)
endif()

# Android builds the same engine and application, loaded by SDL's native Activity.
# Host editor, import/bake tools and test executables remain desktop-only.
if(ANDROID)
    target_sources(judas_engine PRIVATE src/AndroidPlatform.cpp)
    target_include_directories(judas_engine SYSTEM PRIVATE third_party/cgltf)
    target_link_libraries(judas_engine PUBLIC judas_quickjs judas_unicode judas_profiler judas_jobs android log)
    add_library(main SHARED src/main.cpp src/Application.cpp src/RuntimeOptions.cpp
        src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
    target_link_libraries(main PRIVATE judas_engine SDL2::SDL2main)
    target_compile_definitions(main PRIVATE JUDAS_RUNTIME_BUILD_CONFIG="$<CONFIG>")
    judas_warnings(main)
    return()
endif()

add_executable(judas
    src/main.cpp
    src/Application.cpp
    src/RuntimeOptions.cpp
    src/RuntimeDiagnostics.cpp
    src/TestHarness.cpp
)
target_link_libraries(judas PRIVATE judas_engine)
target_compile_definitions(judas PRIVATE JUDAS_RUNTIME_BUILD_CONFIG="$<CONFIG>")
judas_warnings(judas)

# Milestone 28: the editor. Dear ImGui (third_party/imgui, MIT — see its
# README.md) is compiled into this target ONLY; judas_engine and judas
# never include or link it, so the dependency direction stays
# engine <- runtime, engine <- editor.
add_library(judas_imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl2.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(judas_imgui PUBLIC third_party/imgui third_party/imgui/backends ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_imgui PUBLIC SDL2::SDL2)

add_executable(judas_editor
    src/editor/EditorMain.cpp
    src/editor/EditorMilestoneNotice.cpp
    src/editor/EditorApplication.cpp
    src/editor/ScriptTools.cpp
    src/editor/EditorCamera.cpp
    src/editor/EditorPicking.cpp
    src/editor/EditorHistory.cpp
    src/editor/EditorPanels.cpp
    src/editor/RuntimeInspector.cpp
    src/editor/EditorCreationMenu.cpp
    src/editor/EditorScriptWorkflow.cpp
    src/editor/EditorAssetActions.cpp
    src/editor/EditorPreferences.cpp
    src/editor/EditorPrimitiveAssets.cpp
    src/editor/EditorPauseExample.cpp
    src/editor/ProjectSettingsPanel.cpp
    src/editor/FileBrowser.cpp
    src/editor/RenderPreview.cpp
    src/editor/RetargetPanel.cpp
    src/editor/AnimationTimeline.cpp
    src/editor/WorldTools.cpp
    src/editor/TerrainTools.cpp
    src/editor/VolumeTerrainTools.cpp
    src/editor/ModTools.cpp
    src/editor/ProfilerView.cpp
    src/editor/EditorWidgets.cpp
    src/editor/ComponentEditors.cpp
    src/editor/GizmoMath.cpp
)
target_link_libraries(judas_editor PRIVATE judas_engine judas_imgui judas_navigation_baker judas_model_import)
target_include_directories(judas_editor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
judas_warnings(judas_editor)

# Milestone 30: the scene authoring tool that writes the shipped demonstration
# scenes and the tiny game scene (tools/SceneAuthor.cpp tools/StructuredAuthor.cpp).
add_executable(judas_scene_author tools/SceneAuthor.cpp tools/StructuredAuthor.cpp tools/AuthoringCLI.cpp tools/RecipeCLI.cpp tools/ContentServicesCLI.cpp)
target_link_libraries(judas_scene_author PRIVATE judas_engine)
judas_warnings(judas_scene_author)

# Milestone 31: the resource stress demonstration (real GL context, real
# asset decode on workers, a scene playing meanwhile) — tools/ResourceStress.cpp.
add_executable(judas_resource_stress tools/ResourceStress.cpp)
target_link_libraries(judas_resource_stress PRIVATE judas_engine)
judas_warnings(judas_resource_stress)

# Milestone 28: scene data, serialization and runtime instantiation —
# headless (RuntimeWorld built with no ResourceManager creates no GPU state).
add_executable(judas_scene_tests
    tests/SceneTests.cpp
)
target_link_libraries(judas_scene_tests PRIVATE judas_engine)
judas_warnings(judas_scene_tests)

# Milestone 29: entity lifecycle, simulation fidelity, coarse evolution,
# world-state deltas and the reduced-work measurement — headless.
add_executable(judas_lifecycle_tests
    tests/LifecycleTests.cpp
)
target_link_libraries(judas_lifecycle_tests PRIVATE judas_engine)
judas_warnings(judas_lifecycle_tests)

# Milestone 30: projects, asset identity, the resource manager boundary,
# project -> startup scene -> runtime, gizmo mathematics with undo/redo,
# inspector round trip and Edit/Play separation — headless. The gizmo
# mathematics and the editor document are compiled in directly (no ImGui).
add_executable(judas_project_tests
    tests/ProjectTests.cpp
    src/editor/GizmoMath.cpp
    src/editor/EditorHistory.cpp
)
target_link_libraries(judas_project_tests PRIVATE judas_engine)
judas_warnings(judas_project_tests)

# Milestone 31: the job system, asynchronous file IO and the asynchronous
# ResourceManager (headless residency) — real assets decoded on workers.
add_executable(judas_job_tests
    tests/JobTests.cpp
)
target_link_libraries(judas_job_tests PRIVATE judas_engine)
judas_warnings(judas_job_tests)


# Judas-owned physics primitive/collision tests: deterministic, headless,
# no window/GL at all. Two separate executables (each its own `main`)
# rather than a JUDAS_TEST_SCRIPT harness run, since these test the physics
# primitives/collision/gravity-context layers in isolation, not end-to-end
# gameplay.
set(JUDAS_TEST_SUPPORT_SOURCES
    ${JUDAS_PHYSICS_SOURCES}
    src/FaithfulGravity.cpp
    src/RadicalGravity.cpp
    src/SphericalVolume.cpp
    src/BoxVolume.cpp
    src/GravityContextMap.cpp
)

add_executable(judas_physics_tests
    tests/PhysicsPrimitiveTests.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_physics_tests PRIVATE src)
target_link_libraries(judas_physics_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_physics_tests)

# Milestone 32: the accumulated-impulse contact solver inside PhysicsWorld —
# resting drift, stacks, friction, restitution, momentum/energy, rotated
# universes. Headless.
add_executable(judas_rigid_contact_tests
    tests/RigidContactTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_rigid_contact_tests PRIVATE src)
target_link_libraries(judas_rigid_contact_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_rigid_contact_tests)

# Judas Physics MK2 production acceptance. Headless.
add_executable(judas_mk2_physics_tests
    tests/Mk2PhysicsTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_mk2_physics_tests PRIVATE src)
target_link_libraries(judas_mk2_physics_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_mk2_physics_tests)

# Milestone 32: dynamic AABB tree and PhysicsWorld broadphase against a
# brute-force all-pairs + narrowphase oracle. Headless.
add_executable(judas_broadphase_tests
    tests/BroadphaseTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_broadphase_tests PRIVATE src)
target_link_libraries(judas_broadphase_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_broadphase_tests)

add_executable(judas_collision_tests
    tests/CollisionTests.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_collision_tests PRIVATE src)
target_link_libraries(judas_collision_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_collision_tests)

# M20: headless Newtonian two-body force and integration checks against the
# same PhysicsWorld and rigid-body integrator used by the live demonstration.
add_executable(judas_celestial_gravity_tests
    tests/CelestialGravityTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
    src/CelestialGravity.cpp
)
target_include_directories(judas_celestial_gravity_tests PRIVATE src)
target_link_libraries(judas_celestial_gravity_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_celestial_gravity_tests)

# Milestone 9: headless tests for the CPU-side model/texture loaders (see
# src/ModelLoader.*, src/TextureLoader.*) — no window, no GL context, same
# reasoning as the two test targets above. Reads the real committed demo
# assets (assets/models/beacon.obj, assets/textures/beacon.png) directly,
# so it must be run with the repository root as the working directory,
# exactly like judas itself (see docs/ARCHITECTURE.md, "Milestone 9,
# Assets").
add_executable(judas_asset_tests
    tests/AssetTests.cpp
)
target_include_directories(judas_asset_tests PRIVATE src)
target_link_libraries(judas_asset_tests PRIVATE judas_engine)
judas_warnings(judas_asset_tests)

# Milestone 10: headless tests for the step-up/step-down primitives (see
# src/StepClimb.*) — a real PhysicsWorld (no window/GL needed, see
# src/PhysicsWorld.cpp) with a handful of static bodies, exercised via the
# same rotate-the-whole-scenario technique judas_physics_tests/
# judas_collision_tests already established for "no global up" claims.
add_executable(judas_step_climb_tests
    tests/StepClimbTests.cpp
    src/StepClimb.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_step_climb_tests PRIVATE src)
target_link_libraries(judas_step_climb_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_step_climb_tests)

# Milestone 11: headless tests for the secured-pilot attachment primitives
# (see src/PilotAttachment.*) — pure CPU math against hand-crafted
# BodyTransform values, no PhysicsWorld simulation needed at all (only the
# BodyTransform struct from src/PhysicsWorld.h, a header dependency).
add_executable(judas_pilot_attachment_tests
    tests/PilotAttachmentTests.cpp
    src/PilotAttachment.cpp
)
target_include_directories(judas_pilot_attachment_tests PRIVATE src)
target_link_libraries(judas_pilot_attachment_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_pilot_attachment_tests)

# Pilot release clearance across spacecraft rotations and gravity frames.
add_executable(judas_pilot_dismount_tests
    tests/PilotDismountTests.cpp
    src/PilotAttachment.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_pilot_dismount_tests PRIVATE src)
target_link_libraries(judas_pilot_dismount_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_pilot_dismount_tests)

# Milestone 11: headless tests for the spacecraft's full 6DOF control (see
# src/FlyingPrimitiveControl.*) — a real PhysicsWorld plus a real Window
# constructed without Init (test-input mode only, no SDL/GL call ever
# happens — see tests/SpacecraftControlTests.cpp's own header comment).
add_executable(judas_spacecraft_control_tests
    tests/SpacecraftControlTests.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/InputSystem.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_spacecraft_control_tests PRIVATE src)
target_include_directories(judas_spacecraft_control_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_spacecraft_control_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_spacecraft_control_tests)

# M21: coupled spacecraft/celestial-gravity orbit and torque-based SAS tests.
add_executable(judas_spacecraft_flight_tests
    tests/SpacecraftFlightTests.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/InputSystem.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
    src/CelestialGravity.cpp
)
target_include_directories(judas_spacecraft_flight_tests PRIVATE src)
target_include_directories(judas_spacecraft_flight_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_spacecraft_flight_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_spacecraft_flight_tests)

# M22: moving/rotating reference-frame transforms and live rigid-body data.
add_executable(judas_reference_frame_tests
    tests/ReferenceFrameTests.cpp
    src/ReferenceFrame.cpp
    src/PilotAttachment.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_reference_frame_tests PRIVATE src)
target_link_libraries(judas_reference_frame_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_reference_frame_tests)

# Milestone 13: headless tests for the UI navigation/input-ownership logic
# (src/UIWidgets.*, src/UIStack.h, src/PauseMenu.*, src/HUD.*) — pure CPU
# state (screen stack, focus, button hit-testing, pause menu flow, HUD
# view-data formatting), never a GL context or window. Links Renderer.cpp/
# GLAD/FontLoader.cpp only because UIMenuScreen::Draw/HUD::Draw
# reference Renderer's UI drawing methods at compile time (same reasoning
# judas_spacecraft_control_tests already applies to Window.cpp: the code
# must link, but this suite never actually calls Draw or touches a real GL
# context — see tests/UITests.cpp's own header comment).
add_executable(judas_ui_tests
    tests/UITests.cpp
    src/UIWidgets.cpp
    src/PauseMenu.cpp
    src/HUD.cpp
    src/Renderer.cpp
    src/RenderLightGrid.cpp
    src/SkeletalAnimation.cpp
    src/FontLoader.cpp
)
target_include_directories(judas_ui_tests PRIVATE src)
target_include_directories(judas_ui_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_ui_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM} judas_glad)
judas_warnings(judas_ui_tests)

# Milestone 14: headless tests for the dynamic-lighting math (src/
# LightTransforms.*, src/LightAttenuation.*) and the torch's input-
# ownership gating (src/PauseMenu.*, mirroring the same pattern
# tests/UITests.cpp's own Section G already established) — pure CPU
# geometry/arithmetic and pause-state logic, no window, no GL context, no
# font. Actual GLSL shader correctness (Renderer.cpp's fragment shader) is
# verified by human visual validation instead, per this milestone's own
# brief — see docs/ARCHITECTURE.md, "Milestone 14, Automated evidence."
add_executable(judas_lighting_tests
    tests/LightingTests.cpp
    src/LightTransforms.cpp
    src/LightAttenuation.cpp
    src/PauseMenu.cpp
    src/UIWidgets.cpp
    src/HUD.cpp
    src/Renderer.cpp
    src/RenderLightGrid.cpp
    src/SkeletalAnimation.cpp
    src/FontLoader.cpp
)
target_include_directories(judas_lighting_tests PRIVATE src)
target_include_directories(judas_lighting_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_lighting_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM} judas_glad)
judas_warnings(judas_lighting_tests)

# Milestone 15: headless tests for the shadow light-view/projection
# transform math (src/ShadowTransforms.*) — pure CPU matrix construction,
# rotate-the-universe invariance, and finite-matrix checks, no window, no
# GL context. Actual GLSL shadow-mapping correctness (does the real
# fragment shader's PCF/bias/shadow-map sampling produce a correct-looking
# shadow) is verified by a one-time offscreen-rendering spot-check plus
# human visual validation instead — see docs/ARCHITECTURE.md, "Milestone
# 15, Automated evidence."
add_executable(judas_shadow_tests
    tests/ShadowTests.cpp
    src/ShadowTransforms.cpp
)
target_include_directories(judas_shadow_tests PRIVATE src)
target_link_libraries(judas_shadow_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_shadow_tests)

# Milestone 16: headless tests for the interaction system (src/
# HingeTransform.*, src/InteractionSystem.*, src/Door.*, src/LightSwitch.*)
# and the interact key's input-ownership gating (src/PauseMenu.*,
# mirroring judas_lighting_tests' own Section F pattern). Door needs a
# real PhysicsWorld (no window/GL — PhysicsWorld has never needed either,
# see tests/StepClimbTests.cpp); Renderer.cpp/FontLoader.cpp/GLAD
# are linked only because Door::Draw/LightSwitch::Draw/PauseMenu::Draw
# reference Renderer's drawing methods at compile time — this suite never
# calls any of them (same reasoning judas_lighting_tests/judas_shadow_tests
# already document for their own Renderer-touching dependencies).
add_executable(judas_interactable_tests
    tests/InteractableTests.cpp
    src/HingeTransform.cpp
    src/InteractionSystem.cpp
    src/Door.cpp
    src/LightSwitch.cpp
    src/PauseMenu.cpp
    src/UIWidgets.cpp
    src/HUD.cpp
    src/Renderer.cpp
    src/RenderLightGrid.cpp
    src/SkeletalAnimation.cpp
    src/FontLoader.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_interactable_tests PRIVATE src)
target_include_directories(judas_interactable_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_interactable_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM} judas_glad)
judas_warnings(judas_interactable_tests)

# Milestone 17: player camera-pose math and view-input ownership. Pure CPU,
# including arbitrary-orientation and rotate-the-universe checks.
add_executable(judas_player_view_tests
    tests/PlayerViewTests.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
)
target_include_directories(judas_player_view_tests PRIVATE src)
target_link_libraries(judas_player_view_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_player_view_tests)

# Milestone 18: pickup/carry/drop/throw semantics against the real Judas
# PhysicsWorld, plus interaction targeting and rotate-the-universe carry
# transform checks. No renderer/window is initialized.
add_executable(judas_object_manipulation_tests
    tests/ObjectManipulationTests.cpp
    src/ObjectManipulation.cpp
    src/InteractionSystem.cpp
    src/DynamicBody.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_object_manipulation_tests PRIVATE src)
target_link_libraries(judas_object_manipulation_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_object_manipulation_tests)

# Long fixed-step player locomotion against sphere and flat supports,
# including rotated worlds and the ascending spacecraft-release regression.
add_executable(judas_player_curved_locomotion_tests
    tests/PlayerCurvedLocomotionTests.cpp
    src/CharacterMotor.cpp
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
    src/PilotControl.cpp
    src/PilotAttachment.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/InputSystem.cpp
    src/StepClimb.cpp
    ${JUDAS_PHYSICS_WORLD_SOURCES}
    ${JUDAS_PHYSICS_SOURCES}
    src/RadicalGravity.cpp
)
target_include_directories(judas_player_curved_locomotion_tests PRIVATE src)
target_include_directories(judas_player_curved_locomotion_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_player_curved_locomotion_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_player_curved_locomotion_tests)

# M23: exercise global double placement and precise local simulation across
# physics, gravity, orbital flight, player presentation, frames and pickup.
add_executable(judas_world_coordinates_tests
    tests/WorldCoordinatesTests.cpp
    src/CelestialGravity.cpp
    src/DynamicBody.cpp
    src/FlyingPrimitiveControl.cpp
    src/InteractionSystem.cpp
    src/ObjectManipulation.cpp
    src/CharacterMotor.cpp
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
    src/ReferenceFrame.cpp
    src/ShadowTransforms.cpp
    src/Window.cpp
    src/InputSystem.cpp
    src/StepClimb.cpp
    ${JUDAS_PHYSICS_WORLD_SOURCES}
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_world_coordinates_tests PRIVATE src)
target_include_directories(judas_world_coordinates_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_world_coordinates_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_world_coordinates_tests)

# M24: the bounded CPU fluid, generic compound rigid-body geometry, and
# presentation-only water surface are exercised without a GL context.
add_executable(judas_compound_body_tests
    tests/CompoundBodyTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_compound_body_tests PRIVATE src)
target_link_libraries(judas_compound_body_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_compound_body_tests)

add_executable(judas_fluid_world_tests
    tests/FluidWorldTests.cpp
    src/FluidWorld.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_fluid_world_tests PRIVATE src)
target_link_libraries(judas_fluid_world_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_fluid_world_tests)

add_executable(judas_fluid_rigid_coupling_tests
    tests/FluidRigidCouplingTests.cpp
    src/FluidWorld.cpp
    src/ObjectManipulation.cpp
    src/DynamicBody.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_fluid_rigid_coupling_tests PRIVATE src)
target_link_libraries(judas_fluid_rigid_coupling_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_fluid_rigid_coupling_tests)

add_executable(judas_fluid_surface_tests
    tests/FluidSurfaceTests.cpp
    src/FluidSurface.cpp
)
target_include_directories(judas_fluid_surface_tests PRIVATE src)
target_link_libraries(judas_fluid_surface_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_fluid_surface_tests)

# M25: the same continuous terrain surface feeds real player/rigid queries
# and fluid contact. Both suites are headless; the interactive composition
# root and GL presentation remain covered by the gameplay harness/operator.
add_executable(judas_terrain_physics_tests
    tests/TerrainPhysicsTests.cpp
    src/TerrainDemo.cpp
    src/CharacterMotor.cpp
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
    src/StepClimb.cpp
    src/Window.cpp
    src/InputSystem.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
    src/RadicalGravity.cpp
)
target_include_directories(judas_terrain_physics_tests PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_terrain_physics_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_terrain_physics_tests)

add_executable(judas_terrain_fluid_tests
    tests/TerrainFluidTests.cpp
    src/TerrainDemo.cpp
    src/FluidWorld.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_terrain_fluid_tests PRIVATE src)
target_link_libraries(judas_terrain_fluid_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_terrain_fluid_tests)

# M26: equilibrium gas profile and spacecraft/atmosphere forces, including
# a real PhysicsWorld orbital pass. These suites need no window or GL context.
add_executable(judas_atmosphere_tests
    tests/AtmosphereTests.cpp
    src/AtmosphereField.cpp
    src/ReferenceFrame.cpp
    src/TerrainDemo.cpp
    src/RadialTerrain.cpp
)
target_include_directories(judas_atmosphere_tests PRIVATE src)
target_link_libraries(judas_atmosphere_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_atmosphere_tests)

add_executable(judas_atmospheric_flight_tests
    tests/AtmosphericFlightTests.cpp
    src/AtmosphereField.cpp
    src/AerodynamicDrag.cpp
    src/ReferenceFrame.cpp
    src/CelestialGravity.cpp
    src/DynamicBody.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/InputSystem.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_atmospheric_flight_tests PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_atmospheric_flight_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_atmospheric_flight_tests)

# M27: finite thermal fuel state and atmosphere-coupled combustion, plus
# presentation transforms tested without a window or GL context.
add_executable(judas_combustion_tests
    tests/CombustionTests.cpp
    src/CombustionWorld.cpp
    src/FirePresentation.cpp
    src/AtmosphereField.cpp
    src/TerrainDemo.cpp
    src/ReferenceFrame.cpp
    src/CelestialGravity.cpp
    src/RadicalGravity.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/InputSystem.cpp
    ${JUDAS_PHYSICS_SOURCES}
    ${JUDAS_PHYSICS_WORLD_SOURCES}
)
target_include_directories(judas_combustion_tests PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_combustion_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
judas_warnings(judas_combustion_tests)

# stb_image_write (public domain / MIT, see third_party/stb_image_write.h) —
# used only by the opt-in test harness (src/TestHarness.cpp) to write
# screenshots to PNG. A single vendored header, not fetched via CMake:
# writing images is a solved commodity problem and the file has no
# dependencies of its own, so there is nothing for a package manager or
# FetchContent to add value over just committing the one file.
#
# stb_image (Milestone 9, public domain / MIT, see third_party/stb_image.h)
# — the engine's own runtime texture loader (src/TextureLoader.cpp). Same
# vendoring reasoning as stb_image_write: image decoding is a solved
# commodity problem, and this is the read-side sibling of a header already
# vendored the same way.
#
# tiny_obj_loader (Milestone 9, MIT, see third_party/tiny_obj_loader.h) —
# the engine's own runtime OBJ model importer (src/ModelLoader.cpp). See
# docs/ARCHITECTURE.md, "Milestone 9, Model loading," for why OBJ/
# tinyobjloader was chosen over a more complex format/importer.
#
# stb_truetype (Milestone 13, public domain, see third_party/stb_truetype.h)
# — the engine's own runtime font rasterizer (src/FontLoader.cpp), same
# vendoring reasoning as stb_image/stb_image_write/tiny_obj_loader: text
# rasterization is a solved commodity problem, not something Judas needs to
# own. The one font baked with it, assets/fonts/DejaVuSans.ttf, is
# DejaVu Sans under the Bitstream Vera License (a permissive,
# redistribution-friendly license — see assets/fonts/DejaVuSans-LICENSE.txt
# for the full text) — see docs/ARCHITECTURE.md, "Milestone 13, Text
# rendering," for why it was chosen.

# FTFT1: canonical authored-baseline identity and atomic delta validation.
judas_add_engine_test(judas_scene_fingerprint_tests tests/SceneFingerprintTests.cpp)
judas_add_engine_test(judas_world_state_tests tests/WorldStateTests.cpp)

# FTFT2: runs the SAME Application.cpp normal loop with deterministic observers.
add_executable(judas_async_application_tests
    tests/AsyncApplicationTests.cpp
    src/Application.cpp
    src/RuntimeOptions.cpp
    src/RuntimeDiagnostics.cpp
    src/TestHarness.cpp
)
target_link_libraries(judas_async_application_tests PRIVATE judas_engine)
judas_warnings(judas_async_application_tests)

# FTFT3: serialized authored gravity through RuntimeWorld and StepPlayedWorld.
add_executable(judas_uniform_gravity_scene_tests tests/UniformGravitySceneTests.cpp)
judas_add_engine_test(judas_gravity_selection_tests tests/GravitySelectionTests.cpp SOURCES src/editor/EditorHistory.cpp)
target_link_libraries(judas_uniform_gravity_scene_tests PRIVATE judas_engine)
judas_warnings(judas_uniform_gravity_scene_tests)

# FTFT4A: actual geometry queries with independent represented-input oracles.
judas_add_engine_test(judas_contact_geometry_tests tests/ContactGeometryProbe.cpp)

# First-contact reconstruction and persistence through the ordinary runtime.
judas_add_engine_test(judas_contact_lifecycle_tests tests/ContactLifecycleTests.cpp)

# FTFT4A-P: prepared geometry and actual world-cache invalidation checks.
judas_add_engine_test(judas_contact_cache_tests tests/ContactCacheTests.cpp)

# FTFT4A-P allocation/lifetime regressions; actual engine, unchanged physics.
add_executable(judas_contact_storage_tests tests/ContactStorageTests.cpp)
target_link_libraries(judas_contact_storage_tests PRIVATE judas_engine)

# FTFT4B-1 diagnostics: current production path; physical failures remain failures.
judas_add_engine_test(judas_timing_defect_probe tests/TimingDefectProbe.cpp)

add_executable(judas_rigid_motion_tests tests/RigidMotionTests.cpp)
target_link_libraries(judas_rigid_motion_tests PRIVATE judas_engine)

# FTFT4B selected-policy stop-condition witness, not a replacement world solver.
add_executable(judas_poisson_policy_witness tests/PoissonPolicyWitness.cpp)
target_link_libraries(judas_poisson_policy_witness PRIVATE judas_engine)

add_executable(judas_impact_solver_tests tests/ImpactSolverTests.cpp)
target_link_libraries(judas_impact_solver_tests PRIVATE judas_engine)
add_executable(judas_impact_timing_tests tests/ImpactTimingTests.cpp)
target_link_libraries(judas_impact_timing_tests PRIVATE judas_engine)
judas_add_engine_test(judas_growing_impact_tests tests/GrowingImpactTests.cpp)
judas_add_engine_test(judas_friction_jacobian_tests tests/FrictionJacobianTests.cpp)
judas_add_engine_test(judas_reciprocal_inertia_tests tests/ReciprocalInertiaTests.cpp)
judas_add_engine_test(judas_tracked_contact_rows_tests tests/TrackedContactRowsTests.cpp)

judas_add_engine_test(judas_overlap_pair_index_tests tests/OverlapPairIndexTests.cpp)
judas_add_engine_test(judas_contact_block_tests tests/ContactBlockTests.cpp)
judas_add_engine_test(judas_contact_block_noop_tests tests/ContactBlockNoopTests.cpp)
judas_add_engine_test(judas_contact_patch_physical_tests tests/ContactPatchPhysicalTests.cpp)
judas_add_engine_test(judas_contact_block_continuation_tests tests/ContactBlockContinuationTests.cpp)
judas_add_engine_test(judas_block_activation_tests tests/BlockActivationTests.cpp)
judas_add_engine_test(judas_ccd_anchor_reuse_tests tests/CCDAnchorReuseTests.cpp)
judas_add_engine_test(judas_native_mesh_growth_tests tests/NativeMeshGrowthTests.cpp)
judas_add_engine_test(judas_clip_polygon_tests tests/ClipPolygonTests.cpp)
# Continuous-impact cost of a jointed ragdoll resting in static triangle meshes (timings reported, not asserted).
judas_add_engine_test(judas_impact_mesh_cost_tests tests/ImpactMeshCostTests.cpp)
# Fast small bodies: impact spin-up must not throw; thin boxes must not cross tilted supports.
judas_add_engine_test(judas_impact_robustness_tests tests/ImpactRobustnessTests.cpp)

# FTFT6: ordinary standalone application on a tiny authored project.
add_executable(judas_project_application_tests
    tests/ProjectApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp
    src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_project_application_tests PRIVATE judas_engine)
judas_warnings(judas_project_application_tests)

# FTFT6: mixed fidelity/persistence and asset identity integrity.
judas_add_engine_test(judas_lifecycle_integration_tests tests/LifecycleIntegrationTests.cpp)
judas_add_engine_test(judas_asset_integrity_tests tests/AssetIntegrityTests.cpp)

# FTFT7: actual fixed-step celestial reciprocity across simulation fidelities.
judas_add_engine_test(judas_coarse_celestial_tests tests/CoarseCelestialTests.cpp)
judas_add_engine_test(judas_orbital_integrity_tests tests/OrbitalIntegrityTests.cpp)
judas_add_engine_test(judas_spacecraft_frame_integration_tests tests/SpacecraftFrameIntegrationTests.cpp)

# FTFT8: independent finite-fuel and state-based thermal energy accounting.
judas_add_engine_test(judas_thermal_accounting_tests tests/ThermalAccountingTests.cpp)

# FTFT8: serialized-scene thermal/heater path through ordinary played-world steps.
judas_add_engine_test(judas_combustion_integration_tests tests/CombustionIntegrationTests.cpp)

judas_add_engine_test(judas_player_swimming_tests tests/PlayerSwimmingTests.cpp)

judas_add_engine_test(judas_fluid_metadata_tests tests/FluidMetadataTests.cpp)

# FTFT9: deterministic shape/particle-field hydrostatic approximation.
judas_add_engine_test(judas_fluid_hydrostatics_tests tests/FluidHydrostaticsTests.cpp)

judas_add_engine_test(judas_production_fluid_tests tests/ProductionFluidTests.cpp)

# FTFT9: actual authored compound-cavity load and moving-container integration.
judas_add_engine_test(judas_fluid_container_integration_tests tests/FluidContainerIntegrationTests.cpp)

# FTFT9: ordinary played-world swimming consumes the actual coarse particle field.
judas_add_engine_test(judas_player_fluid_integration_tests tests/PlayerFluidIntegrationTests.cpp)

# FTFT9: actual contained particle / finite-mass wall energy budget.
judas_add_engine_test(judas_fluid_cavity_impact_tests tests/FluidCavityImpactTests.cpp)

# FTFT9: isolated configured smoothing momentum/energy accounting.
judas_add_engine_test(judas_fluid_smoothing_budget_tests tests/FluidSmoothingBudgetTests.cpp)

# FTFT9: all corner normals and exact-touch particle sweeps.
judas_add_engine_test(judas_fluid_boundary_contact_tests tests/FluidBoundaryContactTests.cpp)

# FTFT9: existing contact-solver batch adapter, independent impulse/support oracles.
judas_add_engine_test(judas_particle_contact_batch_tests tests/ParticleContactBatchTests.cpp)

# FTFT9: shipped-scene whole-step timing, including fluid preparation/coupling.
judas_add_engine_test(judas_production_fluid_performance tests/ProductionFluidPerformance.cpp)

# M33: actual Renderer/RuntimeWorld GL passes, authored round-trip and Play/Stop.
judas_add_engine_test(judas_render_camera_tests tests/RenderCameraTests.cpp SOURCES src/editor/EditorHistory.cpp)
judas_add_engine_test(judas_portal_camera_tests tests/PortalCameraTests.cpp)

judas_add_engine_test(judas_audio_tests tests/AudioTests.cpp SOURCES src/editor/EditorHistory.cpp)

judas_add_engine_test(judas_input_tests tests/InputTests.cpp)
judas_add_engine_test(judas_pointer_harness_tests tests/PointerHarnessTests.cpp SOURCES src/TestHarness.cpp)
judas_add_engine_test(judas_consumer_pointer_tests tests/ConsumerPointerTests.cpp)

judas_add_engine_test(judas_prefab_tests tests/PrefabTests.cpp SOURCES src/editor/EditorHistory.cpp)

judas_add_engine_test(judas_prefab_application_tests tests/PrefabApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_visual_particle_tests tests/VisualParticleTests.cpp SOURCES src/editor/EditorHistory.cpp)
judas_add_engine_test(judas_particle_application_tests tests/ParticleApplicationTests.cpp APPLICATION)

# M38 standalone export uses the same project/export implementation as editor.
add_executable(judas_export src/ExportMain.cpp)
target_link_libraries(judas_export PRIVATE judas_engine)
judas_warnings(judas_export)
judas_add_engine_test(judas_export_tests tests/ExportTests.cpp)
judas_add_engine_test(judas_package_application_tests tests/PackageApplicationTests.cpp APPLICATION)

# M39: normal project/runtime/physics/GL classification consumers.
judas_add_engine_test(judas_classification_tests tests/ClassificationTests.cpp SOURCES src/editor/EditorHistory.cpp)

target_link_libraries(judas_engine PUBLIC judas_quickjs)
judas_add_engine_test(judas_script_tests tests/ScriptTests.cpp)
judas_add_engine_test(judas_script_application_tests tests/ScriptApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_runtime_ui_tests tests/RuntimeUITests.cpp)
judas_add_engine_test(judas_runtime_ui_application_tests tests/RuntimeUIApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_touch_event_tests tests/TouchEventTests.cpp)
judas_add_engine_test(judas_touch_application_tests tests/TouchApplicationTests.cpp APPLICATION)

# M43: normal Application loop, actual M42 callbacks and fresh world lifetimes.
judas_add_engine_test(judas_scene_transition_tests tests/SceneTransitionTests.cpp APPLICATION)

judas_add_engine_test(judas_physics_cast_tests tests/PhysicsCastTests.cpp)
judas_add_engine_test(judas_physics_cast_application_tests tests/PhysicsCastApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_joint_tests tests/JointTests.cpp)

judas_add_engine_test(judas_joint_authoring_tests tests/JointAuthoringTests.cpp)

judas_add_engine_test(judas_joint_application_tests tests/JointApplicationTests.cpp APPLICATION)

target_include_directories(judas_engine SYSTEM PRIVATE third_party/cgltf)

judas_add_engine_test(judas_skeletal_animation_tests tests/SkeletalAnimationTests.cpp)
judas_add_engine_test(judas_animation_application_tests tests/AnimationApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_pose_composition_tests tests/PoseCompositionTests.cpp)

add_executable(judas_pose_composition_application_tests tests/PoseCompositionApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_pose_composition_application_tests PRIVATE judas_engine)

add_executable(judas_ragdoll_tests tests/RagdollTests.cpp)
target_link_libraries(judas_ragdoll_tests PRIVATE judas_engine)

add_executable(judas_ragdoll_application_tests tests/RagdollApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_ragdoll_application_tests PRIVATE judas_engine)

# Focused fluid-demo performance observation of the ordinary application.
add_executable(judas_fluid_demo_probe tools/FluidDemoProbe.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_fluid_demo_probe PRIVATE judas_engine)
judas_warnings(judas_fluid_demo_probe)

judas_add_engine_test(judas_fluid_demo_integration_tests tests/FluidDemoIntegrationTests.cpp)

judas_add_engine_test(judas_character_motor_tests tests/CharacterMotorTests.cpp)
judas_add_engine_test(judas_character_motor_application_tests tests/CharacterMotorApplicationTests.cpp)

# M50 developer documentation examples; uses the normal engine/QuickJS runtime.
judas_add_engine_test(judasjs_examples_tests tests/JudasJSExamplesTests.cpp)

# M51 ordinary project behaviour composition and explicit compatibility boundary.
judas_add_engine_test(judas_boundary_migration_tests tests/BoundaryMigrationTests.cpp)

# M52 executes the ordinary JS game against production physics and runtime systems.
judas_add_engine_test(judas_shooter_game_tests tests/ShooterGameTests.cpp)

judas_add_engine_test(judas_shooter_application_tests tests/ShooterApplicationTests.cpp APPLICATION)

# Consumer survival game: actual tool interactions, physical construction,
# excavated collision and cold-process persistence through Application.
judas_add_engine_test(judas_hearthworld_application_tests tests/HearthworldApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_script_presentation_tests tests/ScriptPresentationTests.cpp)

add_executable(judas_navigation_bake tools/NavigationBake.cpp)
target_link_libraries(judas_navigation_bake PRIVATE judas_navigation_baker)
add_executable(judas_navigation_tests tests/NavigationTests.cpp)
target_link_libraries(judas_navigation_tests PRIVATE judas_navigation_baker)
add_executable(judas_navigation_game_tests tests/NavigationGameTests.cpp)
target_link_libraries(judas_navigation_game_tests PRIVATE judas_engine)

add_executable(judas_liquid_tests tests/LiquidTests.cpp)
target_link_libraries(judas_liquid_tests PRIVATE judas_engine)
add_executable(judas_liquid_bake tools/LiquidBake.cpp)
target_link_libraries(judas_liquid_bake PRIVATE judas_engine)

add_executable(judas_liquid_application_tests tests/LiquidApplicationTests.cpp)
target_link_libraries(judas_liquid_application_tests PRIVATE judas_engine)

add_executable(judas_liquid_surface_tests tests/LiquidSurfaceTests.cpp)
target_link_libraries(judas_liquid_surface_tests PRIVATE judas_engine)

add_executable(judas_liquid_surface_application_tests tests/LiquidSurfaceApplicationTests.cpp)
target_link_libraries(judas_liquid_surface_application_tests PRIVATE judas_engine)

add_executable(judas_liquid_surface_refill_tests tests/LiquidSurfaceRefillTests.cpp)
target_link_libraries(judas_liquid_surface_refill_tests PRIVATE judas_engine)

add_executable(judas_liquid_surface_loop_tests tests/LiquidSurfaceLoopTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_liquid_surface_loop_tests PRIVATE judas_engine)

add_executable(judas_profiler_tests tests/PerformanceProfilerTests.cpp)
target_link_libraries(judas_profiler_tests PRIVATE judas_engine)

add_executable(judas_profiler_script_tests tests/ProfilerScriptTests.cpp)
target_link_libraries(judas_profiler_script_tests PRIVATE judas_engine)
add_executable(judas_profiler_application_tests tests/ProfilerApplicationTests.cpp src/editor/ProfilerView.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_profiler_application_tests PRIVATE judas_engine judas_imgui)

# Historical focused executables also compile engine subsets directly. They use
# the same headless-capable service, rather than stub profiling implementations.
target_link_libraries(judas_engine PUBLIC judas_profiler)
get_property(JUDAS_LOCAL_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(JUDAS_LOCAL_TARGET IN LISTS JUDAS_LOCAL_TARGETS)
    get_target_property(JUDAS_LOCAL_TYPE ${JUDAS_LOCAL_TARGET} TYPE)
    if(JUDAS_LOCAL_TYPE STREQUAL "EXECUTABLE")
        target_link_libraries(${JUDAS_LOCAL_TARGET} PRIVATE judas_profiler)
    endif()
endforeach()

# M57 offline preprocessing; exported games only load the derived asset.
add_executable(judas_environment_bake tools/EnvironmentBake.cpp)
target_link_libraries(judas_environment_bake PRIVATE judas_engine)
add_executable(judas_material_tests tests/MaterialTests.cpp)
target_link_libraries(judas_material_tests PRIVATE judas_engine)
add_executable(judas_material_application_tests tests/MaterialApplicationTests.cpp src/Application.cpp src/TestHarness.cpp src/RuntimeDiagnostics.cpp src/RuntimeOptions.cpp)
target_link_libraries(judas_material_application_tests PRIVATE judas_engine)

# Legacy direct-Renderer test targets need the reusable M57 material implementation.
foreach(target judas_ui_tests judas_lighting_tests judas_interactable_tests)
    target_link_libraries(${target} PRIVATE judas_engine)
endforeach()

target_link_libraries(judas_engine PUBLIC judas_unicode)

judas_add_engine_test(judas_text_localization_tests tests/TextLocalizationTests.cpp)
judas_add_engine_test(judas_text_render_tests tests/TextRenderTests.cpp)
add_executable(judas_localization_application_tests tests/LocalizationApplicationTests.cpp src/Application.cpp src/TestHarness.cpp src/RuntimeDiagnostics.cpp src/RuntimeOptions.cpp)
target_link_libraries(judas_localization_application_tests PRIVATE judas_engine)
judas_warnings(judas_localization_application_tests)

judas_add_engine_test(judas_streaming_tests tests/WorldStreamingTests.cpp)
add_executable(judas_streaming_demo_builder tools/StreamingDemo.cpp)
target_link_libraries(judas_streaming_demo_builder PRIVATE judas_navigation_baker)
judas_add_engine_test(judas_streaming_application_tests tests/StreamingApplicationTests.cpp APPLICATION)

add_executable(judas_streaming_performance_tests tests/StreamingPerformanceTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_streaming_performance_tests PRIVATE judas_engine)
target_include_directories(judas_streaming_performance_tests PRIVATE src)
judas_warnings(judas_streaming_performance_tests -Wpedantic)

# M59 focused real-project spatial-interest suspension/revisit regression.
judas_add_engine_test(judas_streaming_revisit_tests tests/StreamingRevisitTests.cpp APPLICATION)

# Historical targets compile Window.cpp directly instead of linking the engine.
# Keep their window/icon dependency identical to the editor and standalone game,
# including the GL-free device hub that Window's controller adapter publishes to.
get_property(JUDAS_ICON_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(JUDAS_ICON_TARGET IN LISTS JUDAS_ICON_TARGETS)
    get_target_property(JUDAS_ICON_SOURCES ${JUDAS_ICON_TARGET} SOURCES)
    if("src/Window.cpp" IN_LIST JUDAS_ICON_SOURCES)
        target_link_libraries(${JUDAS_ICON_TARGET} PRIVATE judas_app_icon)
        if(NOT "src/InputDeviceHub.cpp" IN_LIST JUDAS_ICON_SOURCES)
            target_sources(${JUDAS_ICON_TARGET} PRIVATE src/InputDeviceHub.cpp)
        endif()
    endif()
endforeach()

# M60 production PCM streaming/acoustics checks (no device required).
judas_add_engine_test(judas_audio_acoustics_tests tests/AudioAcousticsTests.cpp)
add_executable(judas_audio_demo tools/AudioDemo.cpp)
target_link_libraries(judas_audio_demo PRIVATE judas_engine)

judas_add_engine_test(judas_audio_world_tests tests/AudioWorldTests.cpp)

judas_add_engine_test(judas_audio_application_tests tests/AudioApplicationTests.cpp APPLICATION WARNINGS -Wpedantic)

# M61 focused local-storage protocol; no desktop or operator save directories.
judas_add_engine_test(judas_save_storage_tests tests/SaveStorageTests.cpp WARNINGS -Wpedantic)

add_executable(judas_world_persistence_tests tests/WorldPersistenceTests.cpp)
target_link_libraries(judas_world_persistence_tests PRIVATE judas_engine)
judas_add_engine_test(judas_physics_continuation_tests tests/PhysicsContinuationTests.cpp)
judas_warnings(judas_world_persistence_tests -Wpedantic)

judas_add_engine_test(judas_save_application_tests tests/SaveApplicationTests.cpp APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_character_publication_tests tests/CharacterPublicationTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_save_exclusion_tests tests/SaveExclusionTests.cpp WARNINGS -Wpedantic)
judas_add_engine_test(judas_consumer_lifecycle_audit_tests tests/ConsumerLifecycleAuditTests.cpp APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_save_streaming_tests tests/SaveStreamingTests.cpp APPLICATION WARNINGS -Wpedantic)
judas_add_engine_test(judas_save_performance_tests tests/SavePerformanceTests.cpp APPLICATION WARNINGS -Wpedantic)

add_executable(judas_deformable_tests tests/DeformableTests.cpp)
target_link_libraries(judas_deformable_tests PRIVATE judas_engine)

# M62 generic immutable topology preparation; no GL or alternate solver.
add_executable(judas_deformable_bake tools/DeformableBake.cpp)
target_link_libraries(judas_deformable_bake PRIVATE judas_engine)
add_executable(judas_deformable_performance_tests tests/DeformablePerformanceTests.cpp)
target_link_libraries(judas_deformable_performance_tests PRIVATE judas_engine)
add_executable(judas_deformable_application_tests tests/DeformableApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_deformable_application_tests PRIVATE judas_engine)

foreach(target judas_deformable_tests judas_deformable_bake judas_deformable_application_tests judas_deformable_performance_tests)
  judas_warnings(${target} -Wpedantic)
endforeach()

add_executable(judas_fracture_tests tests/FractureTests.cpp)
target_link_libraries(judas_fracture_tests PRIVATE judas_engine)

add_executable(judas_fracture_application_tests tests/FractureApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_fracture_application_tests PRIVATE judas_engine)

add_executable(judas_fracture_performance_tests tests/FracturePerformanceTests.cpp)
target_link_libraries(judas_fracture_performance_tests PRIVATE judas_engine)

foreach(target judas_fracture_tests judas_fracture_application_tests judas_fracture_performance_tests)
  judas_warnings(${target} -Wpedantic)
endforeach()

add_executable(judas_collision_geometry_tests tests/CollisionGeometryTests.cpp)
target_link_libraries(judas_collision_geometry_tests PRIVATE judas_engine)

add_executable(judas_collision_cook tools/CollisionCook.cpp)
target_link_libraries(judas_collision_cook PRIVATE judas_engine)

add_executable(judas_collision_application_tests tests/CollisionApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_collision_application_tests PRIVATE judas_engine)

judas_add_engine_test(judas_developer_integration_tests tests/DeveloperIntegrationTests.cpp WARNINGS -Wpedantic)
add_executable(judas_developer_author tools/DeveloperIntegrationAuthor.cpp)
target_link_libraries(judas_developer_author PRIVATE judas_engine)
judas_warnings(judas_developer_author -Wpedantic)
target_sources(judas_developer_integration_tests PRIVATE src/editor/EditorHistory.cpp)
judas_add_engine_test(judas_developer_application_tests tests/DeveloperApplicationTests.cpp APPLICATION WARNINGS -Wpedantic)
add_executable(judas_developer_authoring_tests tests/DeveloperAuthoringTests.cpp tools/StructuredAuthor.cpp)
target_include_directories(judas_developer_authoring_tests PRIVATE tools)
target_link_libraries(judas_developer_authoring_tests PRIVATE judas_engine)
judas_warnings(judas_developer_authoring_tests -Wpedantic)

# Ordinary application-loop observation for the post-M65 consumer review.
add_executable(judas_consumer_review tools/ConsumerReview.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_consumer_review PRIVATE judas_engine)
judas_warnings(judas_consumer_review -Wpedantic)
judas_add_engine_test(judas_consumer_authoring_tests tests/ConsumerAuthoringTests.cpp SOURCES src/editor/EditorHistory.cpp WARNINGS -Wpedantic)
add_executable(judas_consumer_capsule_repro tools/ConsumerCapsuleRepro.cpp)
target_link_libraries(judas_consumer_capsule_repro PRIVATE judas_engine)
judas_warnings(judas_consumer_capsule_repro -Wpedantic)
add_executable(judas_consumer_reference_repro tools/ConsumerReferenceRepro.cpp)
target_link_libraries(judas_consumer_reference_repro PRIVATE judas_engine)
judas_warnings(judas_consumer_reference_repro -Wpedantic)

judas_add_engine_test(judas_post_consumer_query_tests tests/PostConsumerQueryTests.cpp WARNINGS -Wpedantic)

add_executable(judas_post_consumer_metadata_tests tests/PostConsumerMetadataTests.cpp)
target_link_libraries(judas_post_consumer_metadata_tests PRIVATE judas_engine)
add_executable(judas_post_consumer_ragdoll_tests tests/PostConsumerRagdollTests.cpp)
target_link_libraries(judas_post_consumer_ragdoll_tests PRIVATE judas_engine)

# M66 source import is authoring-only; shipped runtime reads normalized models.
add_library(judas_ufbx STATIC third_party/ufbx/ufbx.c)
target_include_directories(judas_ufbx PUBLIC third_party/ufbx)
add_library(judas_model_import STATIC src/FbxImport.cpp src/ObjImport.cpp src/ModelCook.cpp src/ModelCollision.cpp src/AnimationRetarget.cpp src/authoring/AnimationEventAuthoring.cpp)
target_link_libraries(judas_model_import PUBLIC judas_engine judas_ufbx)
judas_warnings(judas_model_import)
add_executable(judas_model_import_cli tools/ModelImportCli.cpp)
target_link_libraries(judas_model_import_cli PRIVATE judas_model_import)
add_executable(judas_model_import_tests tests/ModelImportTests.cpp)
target_link_libraries(judas_model_import_tests PRIVATE judas_model_import)
judas_warnings(judas_model_import_tests)

add_executable(judas_model_reimport_tests tests/ModelReimportTests.cpp)
target_link_libraries(judas_model_reimport_tests PRIVATE judas_model_import)
judas_warnings(judas_model_reimport_tests)

add_executable(judas_model_runtime_tests tests/ModelRuntimeTests.cpp)
target_link_libraries(judas_model_runtime_tests PRIVATE judas_engine)

judas_add_engine_test(judas_model_application_tests tests/ModelApplicationTests.cpp APPLICATION)

judas_add_engine_test(judas_world_building_tests tests/WorldBuildingTests.cpp SOURCES src/editor/EditorHistory.cpp)

add_library(judas_world_authoring STATIC src/authoring/NavigationDependencies.cpp src/authoring/ContentReferences.cpp src/authoring/WorldBuilder.cpp src/authoring/AuthoringDocument.cpp src/authoring/SkeletonAuthoring.cpp src/authoring/RecipePreview.cpp src/authoring/TerrainAuthoring.cpp src/authoring/VolumeTerrain.cpp src/authoring/ModAuthoring.cpp)
target_include_directories(judas_world_authoring PUBLIC src/authoring)
target_link_libraries(judas_world_authoring PUBLIC judas_engine judas_navigation_baker)
judas_warnings(judas_world_authoring)
target_link_libraries(judas_navigation_bake PRIVATE judas_world_authoring)
target_link_libraries(judas_scene_author PRIVATE judas_world_authoring)
target_link_libraries(judas_editor PRIVATE judas_world_authoring)
target_link_libraries(judas_world_building_tests PRIVATE judas_world_authoring)

target_link_libraries(judas_scene_author PRIVATE judas_model_import)

judas_add_engine_test(judas_world_authoring_runtime_tests tests/WorldAuthoringRuntimeTests.cpp)

judas_add_engine_test(judas_world_authoring_application_tests tests/WorldAuthoringApplicationTests.cpp APPLICATION)
add_executable(judas_world_authoring_performance tests/WorldAuthoringPerformance.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_world_authoring_performance PRIVATE judas_world_authoring)
judas_warnings(judas_world_authoring_performance)

# M68 structural work counts and comparable residency captures, with owned output.
judas_add_engine_test(judas_scalability_tests tests/ScalabilityTests.cpp)
add_executable(judas_scalability_pipeline_tests tests/ScalabilityPipelineTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_scalability_pipeline_tests PRIVATE judas_model_import)
judas_warnings(judas_scalability_pipeline_tests)
judas_add_engine_test(judas_resource_handoff_tests tests/ResourceHandoffTests.cpp)

# M69 focused controller observations and one-bridge physics batches.
add_executable(judas_paired_input_tests tests/PairedInputTests.cpp tools/StructuredAuthor.cpp)
target_include_directories(judas_paired_input_tests PRIVATE tools)
target_link_libraries(judas_paired_input_tests PRIVATE judas_engine)
judas_warnings(judas_paired_input_tests)
judas_add_engine_test(judas_paired_input_script_tests tests/PairedInputScriptTests.cpp)
judas_add_engine_test(judas_physics_batch_script_tests tests/PhysicsBatchScriptTests.cpp WARNINGS -Wpedantic)

judas_add_engine_test(judas_windows_readiness_tests tests/WindowsReadinessTests.cpp)

# M75 shared CPU terrain authoring and focused checks.
add_executable(judas_terrain_author tools/TerrainAuthor.cpp)
target_link_libraries(judas_terrain_author PRIVATE judas_world_authoring)
judas_warnings(judas_terrain_author)
add_executable(judas_terrain_authoring_tests tests/TerrainAuthoringTests.cpp)
target_link_libraries(judas_terrain_authoring_tests PRIVATE judas_world_authoring)
judas_warnings(judas_terrain_authoring_tests)
add_executable(judas_terrain_pipeline_tests tests/TerrainPipelineTests.cpp)
target_link_libraries(judas_terrain_pipeline_tests PRIVATE judas_world_authoring judas_navigation_baker)
judas_warnings(judas_terrain_pipeline_tests)
judas_add_engine_test(judas_terrain_application_tests tests/TerrainApplicationTests.cpp APPLICATION)

if(WIN32)
    target_link_libraries(judas_engine PUBLIC shell32 ole32)
    target_link_libraries(judas_profiler PUBLIC psapi)
endif()

# M70 focused pose/physical authority checks; no extra runtime dependency.
judas_add_engine_test(judas_multi_target_ik_tests tests/MultiTargetIKTests.cpp)
judas_add_engine_test(judas_physical_animation_tests tests/PhysicalAnimationTests.cpp)
# M77 opt-in actuation/contact foundations. Controller acceptance is separate.
judas_add_engine_test(judas_physical_balance_foundation_tests tests/PhysicalBalanceFoundationTests.cpp)
judas_add_engine_test(judas_physical_balance_controller_tests tests/PhysicalBalanceControllerTests.cpp)
judas_add_engine_test(judas_balance_lab_tests tests/BalanceLabTests.cpp)
# Development instrument for balance stepping; not an acceptance suite.
judas_add_engine_test(judas_balance_step_harness tests/BalanceStepHarness.cpp)
judas_add_engine_test(judas_character_lab_tests tests/CharacterLabTests.cpp)
judas_add_engine_test(judas_character_lab_performance tests/CharacterLabPerformance.cpp)

# M71 prescribed-body timeline, lifecycle, bounded workload and scene-view pan.
judas_add_engine_test(judas_kinematic_tests tests/KinematicTests.cpp)
judas_add_engine_test(judas_kinematic_lab_tests tests/KinematicLabTests.cpp)
judas_add_engine_test(judas_kinematic_performance tests/KinematicPerformance.cpp)
judas_add_engine_test(judas_editor_navigation_tests tests/EditorNavigationTests.cpp SOURCES src/editor/EditorCamera.cpp)

judas_add_engine_test(judas_editor_picking_tests tests/EditorPickingTests.cpp SOURCES src/editor/EditorPicking.cpp)

judas_add_engine_test(judas_editor_workspace_tests tests/EditorWorkspaceLayoutTests.cpp)

add_executable(judas_editor_widget_tests tests/EditorWidgetTests.cpp
    src/editor/EditorWidgets.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_editor_widget_tests PRIVATE judas_engine judas_imgui)
judas_warnings(judas_editor_widget_tests)

# M72 actual pixel/depth/shadow and immutable resource-control observations.
judas_add_engine_test(judas_runtime_render_tests tests/RuntimeRenderTests.cpp)

add_executable(judas_runtime_render_application_tests tests/RuntimeRenderApplicationTests.cpp src/editor/EditorHistory.cpp
    src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_runtime_render_application_tests PRIVATE judas_engine)
judas_warnings(judas_runtime_render_application_tests)

# M72 named/prefab durability, ordinary editor history and default compatibility.
judas_add_engine_test(judas_runtime_render_authoring_tests tests/RuntimeRenderAuthoringTests.cpp SOURCES src/editor/EditorHistory.cpp)

judas_add_engine_test(judas_runtime_render_retained_region_tests tests/RuntimeRenderRetainedRegionTests.cpp)

# M73: actual QuickJS slot ownership, transient signals, lanes and limits.
judas_add_engine_test(judas_signal_tests tests/SignalTests.cpp)

judas_add_engine_test(judas_signal_application_tests tests/SignalApplicationTests.cpp APPLICATION)

# M74 CPU retarget/cook checks link authoring services, never a runtime evaluator.
add_executable(judas_animation_retarget_tests tests/AnimationRetargetTests.cpp)
target_link_libraries(judas_animation_retarget_tests PRIVATE judas_model_import)
judas_warnings(judas_animation_retarget_tests)
add_executable(judas_animation_retarget_cook_tests tests/AnimationRetargetCookTests.cpp)
target_link_libraries(judas_animation_retarget_cook_tests PRIVATE judas_model_import)
judas_warnings(judas_animation_retarget_cook_tests)

add_executable(judas_retarget_editor_tests tests/RetargetEditorTests.cpp)
target_link_libraries(judas_retarget_editor_tests PRIVATE judas_model_import)
judas_warnings(judas_retarget_editor_tests)
judas_add_engine_test(judas_retarget_application_tests tests/RetargetApplicationTests.cpp APPLICATION)
add_executable(judas_retarget_lab_tests tests/RetargetLabTests.cpp)
target_link_libraries(judas_retarget_lab_tests PRIVATE judas_model_import)
judas_warnings(judas_retarget_lab_tests)

judas_add_engine_test(judas_mod_content_tests tests/ModContentTests.cpp)

add_executable(judas_mod tools/ModTool.cpp)
target_link_libraries(judas_mod PRIVATE judas_world_authoring)
judas_warnings(judas_mod)

# M76 navigation authoring shares kit registries and owner-qualified IDs.
target_link_libraries(judas_navigation_bake PRIVATE judas_world_authoring)
add_executable(judas_mod_lab_builder tools/ModLabBuilder.cpp)
target_link_libraries(judas_mod_lab_builder PRIVATE judas_world_authoring judas_navigation_baker)
judas_warnings(judas_mod_lab_builder)
judas_add_engine_test(judas_mod_application_tests tests/ModApplicationTests.cpp APPLICATION)

# M78 correction: saved-scene dependent-bake workflow, no runtime subsystem.
add_executable(judas_navigation_dependency_tests tests/NavigationDependencyTests.cpp)
target_link_libraries(judas_navigation_dependency_tests PRIVATE judas_world_authoring)
judas_warnings(judas_navigation_dependency_tests)

judas_add_engine_test(judas_motor_runtime_tags_tests tests/MotorRuntimeTagsTests.cpp WARNINGS -Wpedantic)

# Physical assembly regression targets; definitions preserve M79–M83 inputs.
include(cmake/tests/MechanicalTests.cmake)

# M84 bounded editable volumes cook into the existing runtime asset pipeline.
add_executable(judas_volume_author tools/VolumeTerrainAuthor.cpp)
target_link_libraries(judas_volume_author PRIVATE judas_world_authoring)
judas_warnings(judas_volume_author)
add_executable(judas_volume_terrain_tests tests/VolumeTerrainTests.cpp)
target_link_libraries(judas_volume_terrain_tests PRIVATE judas_world_authoring judas_navigation_baker)
judas_warnings(judas_volume_terrain_tests)
judas_add_engine_test(judas_volume_application_tests tests/VolumeTerrainApplicationTests.cpp APPLICATION)
add_executable(judas_volume_performance_tests tests/VolumeTerrainPerformanceTests.cpp)
target_link_libraries(judas_volume_performance_tests PRIVATE judas_world_authoring)
judas_warnings(judas_volume_performance_tests)

# Ordinary consumer-game integration; no Planetfall policy links into the engine.
judas_add_engine_test(judas_planetfall_application_tests tests/PlanetfallApplicationTests.cpp APPLICATION)

# Ordinary ball-pusher consumer: no arcade policy in runtime C++.
judas_add_engine_test(judas_ball_pusher_application_tests tests/BallPusherApplicationTests.cpp APPLICATION)
judas_add_engine_test(judas_contact_interval_tests tests/ContactIntervalTests.cpp)

judas_add_engine_test(judas_collision_continuity_tests tests/CollisionContinuityTests.cpp)

judas_add_engine_test(judas_sphere_impact_bounds_tests tests/SphereImpactBoundsTests.cpp)
judas_add_engine_test(judas_cooked_penetration_tests tests/CookedPenetrationTests.cpp)

judas_add_engine_test(judas_planet_terrain_tests tests/PlanetTerrainTests.cpp)

judas_add_engine_test(judas_planet_world_tests tests/PlanetWorldTests.cpp)

judas_add_engine_test(judas_planet_application_tests tests/PlanetApplicationTests.cpp APPLICATION)
judas_add_engine_test(judas_planet_streaming_tests tests/PlanetStreamingTests.cpp)
judas_add_engine_test(judas_origin_physics_tests tests/OriginPhysicsTests.cpp)
add_executable(judas_origin_assembly_tests tests/OriginAssemblyTests.cpp)
target_link_libraries(judas_origin_assembly_tests PRIVATE judas_engine judas_navigation_baker)
judas_warnings(judas_origin_assembly_tests)
add_executable(judas_create_travel_lab tools/CreateTravelLab.cpp)
target_link_libraries(judas_create_travel_lab PRIVATE judas_engine judas_navigation_baker)
judas_warnings(judas_create_travel_lab)
judas_add_engine_test(judas_origin_participant_tests tests/OriginParticipantTests.cpp)
judas_add_engine_test(judas_origin_travel_application_tests tests/OriginTravelApplicationTests.cpp APPLICATION)
add_executable(judas_origin_audio_tests tests/OriginAudioTests.cpp)
target_link_libraries(judas_origin_audio_tests PRIVATE judas_engine)

add_executable(judas_origin_script_tests tests/OriginScriptTests.cpp)
target_link_libraries(judas_origin_script_tests PRIVATE judas_engine)

judas_add_engine_test(judas_origin_stream_tests tests/OriginStreamTests.cpp)
judas_add_engine_test(judas_stream_origin_precision_tests tests/StreamOriginPrecisionTests.cpp)

# M86 optional prescribed weather; no renderer dependency in its numerical core.
add_executable(judas_weather_tests tests/WeatherTests.cpp src/Weather.cpp)
target_include_directories(judas_weather_tests PRIVATE src)
target_link_libraries(judas_weather_tests PRIVATE ${JUDAS_GLM})
target_compile_definitions(judas_weather_tests PRIVATE GLM_ENABLE_EXPERIMENTAL)
judas_warnings(judas_weather_tests)
judas_add_engine_test(judas_weather_authoring_tests tests/WeatherAuthoringTests.cpp SOURCES src/editor/EditorHistory.cpp)
judas_add_engine_test(judas_weather_render_tests tests/WeatherRenderTests.cpp)
judas_add_engine_test(judas_weather_lighting_tests tests/WeatherLightingTests.cpp)

judas_add_engine_test(judas_weather_world_tests tests/WeatherWorldTests.cpp)
judas_add_engine_test(judas_weather_streaming_tests tests/WeatherStreamingTests.cpp)

judas_add_engine_test(judas_weather_script_tests tests/WeatherScriptTests.cpp)
judas_add_engine_test(judas_weather_application_tests tests/WeatherApplicationTests.cpp APPLICATION)
judas_add_engine_test(judas_weather_performance_tests tests/WeatherPerformanceTests.cpp)

# M87: identical workload driver links the ordinary engine in baseline and candidate.
judas_add_engine_test(judas_physics_scaling_benchmarks tests/PhysicsScalingBenchmarks.cpp)
add_executable(judas_physics_scaling_diagnostics tests/PhysicsScalingBenchmarks.cpp)
target_compile_definitions(judas_physics_scaling_diagnostics PRIVATE M87_ALLOCATION_DIAGNOSTICS=1)
target_link_libraries(judas_physics_scaling_diagnostics PRIVATE judas_engine)
judas_warnings(judas_physics_scaling_diagnostics)

judas_add_engine_test(judas_physics_scaling_tests tests/PhysicsScalingTests.cpp)
judas_add_engine_test(judas_joint_scaling_tests tests/JointScalingTests.cpp)
judas_add_engine_test(judas_parallel_narrowphase_tests tests/ParallelNarrowphaseTests.cpp)
add_executable(judas_primitive_contact_boolean_tests tests/PrimitiveContactBooleanTests.cpp src/Contacts.cpp src/PlanarGeometry.cpp)
target_include_directories(judas_primitive_contact_boolean_tests PRIVATE src)
target_link_libraries(judas_primitive_contact_boolean_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_primitive_contact_boolean_tests)
add_executable(judas_rigid_motion_bounds_tests tests/RigidMotionBoundsTests.cpp)
target_link_libraries(judas_rigid_motion_bounds_tests PRIVATE judas_engine)
judas_add_engine_test(judas_rigid_motion_geometry_tests tests/RigidMotionGeometryTests.cpp)
judas_warnings(judas_rigid_motion_bounds_tests)
judas_add_engine_test(judas_physics_stress_live_benchmark tests/PhysicsStressLiveBenchmark.cpp APPLICATION)
judas_add_engine_test(judas_physics_scaling_application_tests tests/PhysicsScalingApplicationTests.cpp APPLICATION)
add_executable(judas_contact_sat_workspace_tests tests/ContactSatWorkspaceTests.cpp)
target_include_directories(judas_contact_sat_workspace_tests PRIVATE src)
target_link_libraries(judas_contact_sat_workspace_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_contact_sat_workspace_tests)
add_executable(judas_contact_sat_filter_tests tests/ContactSatFilterTests.cpp src/Contacts.cpp src/PlanarGeometry.cpp)
target_include_directories(judas_contact_sat_filter_tests PRIVATE src)
target_link_libraries(judas_contact_sat_filter_tests PRIVATE ${JUDAS_GLM})
judas_warnings(judas_contact_sat_filter_tests)
judas_add_engine_test(judas_contact_sphere_filter_tests tests/ContactSphereFilterTests.cpp)
judas_add_engine_test(judas_candidate_reuse_tests tests/CandidateReuseTests.cpp)

# Direct headless fixtures need the complete private world implementation and
# borrowed-job service, without renderer/window/resource ownership. Check both
# relative and absolute source spellings so a partial world fails at configure.
get_property(judas_current_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(judas_current_target IN LISTS judas_current_targets)
    get_target_property(judas_current_sources ${judas_current_target} SOURCES)
    get_target_property(judas_current_source_dir ${judas_current_target} SOURCE_DIR)
    set(judas_current_absolute_sources)
    foreach(judas_current_source IN LISTS judas_current_sources)
        get_filename_component(judas_current_absolute_source "${judas_current_source}"
            ABSOLUTE BASE_DIR "${judas_current_source_dir}")
        list(APPEND judas_current_absolute_sources "${judas_current_absolute_source}")
    endforeach()
    if("${CMAKE_CURRENT_SOURCE_DIR}/src/PhysicsWorld.cpp" IN_LIST judas_current_absolute_sources)
        foreach(judas_world_source IN LISTS JUDAS_PHYSICS_WORLD_SOURCES)
            if(NOT "${CMAKE_CURRENT_SOURCE_DIR}/${judas_world_source}" IN_LIST judas_current_absolute_sources)
                message(FATAL_ERROR
                    "${judas_current_target} compiles a partial PhysicsWorld. "
                    "Use JUDAS_PHYSICS_WORLD_SOURCES instead of individual world files.")
            endif()
        endforeach()
        target_link_libraries(${judas_current_target} PRIVATE judas_jobs)
    endif()
endforeach()

include(cmake/targets/LocalViews.cmake)

include(cmake/targets/RenderingAssets.cmake)

include(cmake/targets/DeveloperWorkflow.cmake)

include(cmake/targets/WorldLifecycle.cmake)

judas_add_engine_test(judas_prealpha_lifecycle_tests tests/PreAlphaLifecycleTests.cpp)

judas_add_engine_test(judas_first_project_tests tests/FirstProjectTests.cpp)
judas_add_engine_test(judas_origin_environment_tests tests/OriginEnvironmentTests.cpp)
judas_add_engine_test(judas_liquid_rotation_tests tests/LiquidRotationTests.cpp)
judas_add_engine_test(judas_liquid_bake_frame_tests tests/LiquidBakeFrameTests.cpp)
judas_add_engine_test(judas_collision_witness_tests tests/CollisionWitnessTests.cpp)
judas_add_engine_test(judas_origin_admission_tests tests/OriginAdmissionTests.cpp)

judas_add_engine_test(judas_windows_cross_audio_tests tests/WindowsCrossAudioTests.cpp)

# Finalize native packaging only after every subsystem has declared its tools.
if(MINGW)
    include(cmake/JudasWindowsCrossRuntime.cmake)
elseif(WIN32)
    include(cmake/JudasWindowsSdk.cmake)
endif()
include(cmake/JudasSdk.cmake)

# Project policy and lightweight water share no advanced solver implementation.
judas_add_engine_test(judas_project_water_tests tests/ProjectWaterTests.cpp)
judas_add_engine_test(judas_project_settings_editor_tests tests/ProjectSettingsEditorTests.cpp
    SOURCES src/editor/ProjectSettingsPanel.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_basic_water_application_tests tests/BasicWaterApplicationTests.cpp)
judas_add_engine_test(judas_basic_water_preview_tests tests/BasicWaterPreviewTests.cpp)

judas_add_engine_test(judas_project_fluid_policy_tests tests/ProjectFluidPolicyTests.cpp)
add_dependencies(judas_project_fluid_policy_tests judas)

judas_add_engine_test(judas_editor_file_browser_tests tests/EditorFileBrowserTests.cpp
    SOURCES src/editor/FileBrowser.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_editor_creation_menu_tests tests/EditorCreationMenuTests.cpp
    SOURCES src/editor/EditorCreationMenu.cpp src/editor/EditorHistory.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_editor_asset_actions_tests tests/EditorAssetActionsTests.cpp
    SOURCES src/editor/EditorAssetActions.cpp src/editor/EditorHistory.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_editor_preferences_tests tests/EditorPreferencesTests.cpp
    SOURCES src/editor/EditorPreferences.cpp src/editor/EditorHistory.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_editor_script_workflow_tests tests/EditorScriptWorkflowTests.cpp
    SOURCES src/editor/EditorScriptWorkflow.cpp src/editor/EditorPrimitiveAssets.cpp src/editor/EditorHistory.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_input_rebinding_tests tests/InputRebindingTests.cpp)
judas_add_engine_test(judas_pause_example_application_tests tests/PauseExampleApplicationTests.cpp
    SOURCES src/editor/EditorPauseExample.cpp src/editor/EditorScriptWorkflow.cpp src/editor/EditorPrimitiveAssets.cpp src/editor/EditorHistory.cpp LIBRARIES judas_imgui)

judas_add_engine_test(judas_mk2_diagnostics_tests tests/Mk2DiagnosticsTests.cpp)
judas_add_engine_test(judas_velocity_coupling_substep_tests tests/VelocityCouplingSubstepTests.cpp)
judas_add_engine_test(judas_physical_constraint_batch_tests tests/PhysicalConstraintBatchTests.cpp)
judas_add_engine_test(judas_mk2_rotation_tests tests/Mk2RotationTests.cpp)
judas_add_engine_test(judas_mk2_rotation_cold_tests tests/Mk2RotationColdTests.cpp)


# Read-only runtime diagnostics and the shipped public-API learning project.
judas_add_engine_test(judas_workflow_application_tests tests/DeveloperWorkflowApplicationTests.cpp APPLICATION)

# Native 2D Stage 1: real authoring/runtime paths, not a private prototype.
judas_add_engine_test(judas_sprite_integration_tests tests/SpriteIntegrationTests.cpp
    SOURCES src/editor/EditorPicking.cpp src/editor/EditorCreationMenu.cpp src/editor/EditorHistory.cpp src/editor/EditorCamera.cpp LIBRARIES judas_imgui)
judas_add_engine_test(judas_sprite_render_tests tests/SpriteRenderTests.cpp)

add_executable(judas_planar_benchmark tools/benchmarks/PlanarPhysicsBenchmark.cpp)
target_link_libraries(judas_planar_benchmark PRIVATE judas_engine)
judas_warnings(judas_planar_benchmark)

add_executable(judas_planar_playground tools/demos/PlanarPlayground.cpp)
target_link_libraries(judas_planar_playground PRIVATE judas_engine)
judas_warnings(judas_planar_playground)

judas_add_engine_test(judas_planar_physics_tests tests/PlanarPhysicsTests.cpp)
judas_add_engine_test(judas_planar_integration_tests tests/PlanarIntegrationTests.cpp
    SOURCES src/editor/EditorPicking.cpp src/editor/EditorCreationMenu.cpp src/editor/EditorHistory.cpp src/editor/EditorCamera.cpp LIBRARIES judas_imgui)

# Finalize after every native test has declared its authoritative entry point.
include(cmake/JudasValidation.cmake)
