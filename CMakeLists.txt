cmake_minimum_required(VERSION 3.20)
project(ProjectJudas C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(SDL2 REQUIRED)
# The original app artwork is embedded, so neither editor nor game windows
# need a development-tree path. Reconfigure when the PNG changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS assets/branding/judas-icon.png)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/judas-icon.png" JUDAS_APP_ICON_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1," JUDAS_APP_ICON_BYTES "${JUDAS_APP_ICON_HEX}")
configure_file(cmake/JudasAppIcon.h.in generated/JudasAppIcon.h @ONLY)
configure_file(cmake/judas-editor.desktop.in judas-editor.desktop @ONLY)
add_library(judas_app_icon STATIC src/AppIcon.cpp src/TextureLoader.cpp)
target_include_directories(judas_app_icon PUBLIC src PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(judas_app_icon PUBLIC SDL2::SDL2)
target_compile_options(judas_app_icon PRIVATE -Wall -Wextra)
include_directories(SYSTEM third_party/quickhull)
set_source_files_properties(third_party/quickhull/QuickHull.cpp PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter")
find_package(glm REQUIRED)
include(cmake/JudasTextDependencies.cmake)

# GLM is header-only, but the imported target it exports depends on the
# packaged version: GLM 1.0+ provides `${JUDAS_GLM}` (and
# `glm::glm`), while the GLM 0.9.9.8 that Ubuntu 24.04 ships provides only
# `glm::glm`. Resolve whichever exists once here so a stock
# `apt install libglm-dev` configures cleanly (M28 portability fix).
if(TARGET ${JUDAS_GLM})
    set(JUDAS_GLM ${JUDAS_GLM})
elseif(TARGET glm::glm)
    set(JUDAS_GLM glm::glm)
elseif(TARGET glm)
    set(JUDAS_GLM glm)
else()
    message(FATAL_ERROR "GLM was found but exports no known imported target")
endif()

# No physics middleware. Judas owns rigid-body state, integration,
# collision detection, contact generation, and contact resolution itself
# (src/RigidBody.*, src/Contacts.*, src/ContactSolver.*, src/PhysicsWorld.cpp)
# — see docs/ARCHITECTURE.md, "Physics ownership," for the milestone this
# decision belongs to and why Jolt Physics (used through the first M7-Final
# attempt, recoverable via git history/the milestone-7b tag for the last
# Jolt-backed milestone) was replaced rather than kept as an authoritative
# dependency.

set(JUDAS_PHYSICS_SOURCES
    src/CollisionAsset.cpp
    src/CollisionGeometry.cpp
    third_party/quickhull/QuickHull.cpp
    src/JointSolver.cpp
    src/PhysicsCastGeometry.cpp
    src/ImpactSolver.cpp
    src/RigidBody.cpp
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
target_link_libraries(judas_quickjs PUBLIC m Threads::Threads ${CMAKE_DL_LIBS})

# Pinned RecastNavigation 1.6.0. Recast baking stays out of runtime.
file(GLOB JUDAS_DETOUR third_party/recastnavigation/Detour/Source/*.cpp third_party/recastnavigation/DetourTileCache/Source/*.cpp third_party/recastnavigation/DetourCrowd/Source/*.cpp)
add_library(judas_navigation_backend STATIC ${JUDAS_DETOUR})
target_include_directories(judas_navigation_backend SYSTEM PUBLIC third_party/recastnavigation/Detour/Include third_party/recastnavigation/DetourTileCache/Include third_party/recastnavigation/DetourCrowd/Include)
target_compile_definitions(judas_navigation_backend PUBLIC DT_VIRTUAL_QUERYFILTER)
file(GLOB JUDAS_RECAST third_party/recastnavigation/Recast/Source/*.cpp)
add_library(judas_navigation_baker STATIC src/NavigationBake.cpp ${JUDAS_RECAST})
target_include_directories(judas_navigation_baker SYSTEM PRIVATE third_party/recastnavigation/Recast/Include)
target_include_directories(judas_navigation_baker PUBLIC src)
target_link_libraries(judas_navigation_baker PUBLIC judas_engine judas_navigation_backend)

# GLAD 2 loader generated for precisely OpenGL 3.3 Core, no extensions.
add_library(judas_glad STATIC third_party/glad/src/gl.c)
target_include_directories(judas_glad PUBLIC third_party/glad/include)
target_compile_options(judas_glad PRIVATE -Wall -Wextra)

# Milestone 28: engine sources shared by the runtime, the editor and the
# scene/runtime test suites. Neither executable reimplements engine
# startup, scene loading, instantiation, stepping or presentation.
add_library(judas_profiler STATIC src/PerformanceProfiler.cpp)
target_include_directories(judas_profiler PUBLIC src)
target_compile_options(judas_profiler PRIVATE -Wall -Wextra)

add_library(judas_materials STATIC third_party/mikktspace/mikktspace.c)
target_include_directories(judas_materials PUBLIC src)
target_link_libraries(judas_materials PUBLIC ${JUDAS_GLM} judas_profiler)

add_library(judas_engine STATIC
    src/WorldFracture.cpp
    src/Fracture.cpp
    src/DeformableAsset.cpp
    src/Deformable.cpp
    src/WorldDeformables.cpp
    src/TextLayout.cpp
    src/Localization.cpp
    src/Material.cpp
    src/Environment.cpp
    src/Tangents.cpp
    src/LiquidGeometry.cpp
    src/LiquidTypes.cpp
    src/LiquidSystem.cpp
    src/LiquidSurface.cpp
    src/NavigationSystem.cpp
    src/NavigationTypes.cpp
    src/NavigationAsset.cpp
    src/WorldAnimation.cpp
    src/PoseComposition.cpp
    src/WorldRagdoll.cpp
    src/GltfLoader.cpp
    src/WorldCharacter.cpp
    src/WorldStreaming.cpp
    src/WorldRegions.cpp
    src/WorldScripts.cpp
    src/RuntimeUI.cpp
    src/ScriptSystem.cpp
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
    src/InputSystem.cpp
    src/Renderer.cpp
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
    src/Ragdoll.cpp
    src/SceneSerialization.cpp
    src/NamedAuthoring.cpp
    src/SceneFingerprint.cpp
    src/Project.cpp
    src/AssetDatabase.cpp
    src/JobSystem.cpp
    src/AsyncFile.cpp
    src/ResourceManager.cpp
    src/EnginePaths.cpp
    src/DebugDraw.cpp
    src/WorldDebugView.cpp
    src/SceneSession.cpp
    src/RuntimeWorld.cpp
    src/GameSession.cpp
    src/Simulation.cpp
    src/WorldPresentation.cpp
    src/GameplayHud.cpp
    src/EngineHost.cpp
    src/InteractivePlay.cpp
    src/ScreenshotWriter.cpp
    src/FidelityPolicy.cpp
    src/CoarseSimulation.cpp
    src/WorldState.cpp
    src/SaveStorage.cpp
    src/SaveService.cpp
    src/MotorPersistence.cpp
    src/LiquidPersistence.cpp
    src/WorldPersistence.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_engine PUBLIC src ${SDL2_INCLUDE_DIRS})
find_package(Threads REQUIRED)
add_library(judas_audio_backend STATIC src/AudioBackend.c src/ReverbBackend.c)
target_include_directories(judas_audio_backend PUBLIC third_party/miniaudio third_party/verblib)
target_compile_definitions(judas_audio_backend PUBLIC MA_NO_RESOURCE_MANAGER MA_NO_ENCODING MA_NO_GENERATION)
target_link_libraries(judas_audio_backend PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(judas_audio_backend PUBLIC m)
endif()
target_link_libraries(judas_engine PUBLIC judas_app_icon judas_materials SDL2::SDL2 ${JUDAS_GLM} judas_glad Threads::Threads judas_audio_backend judas_navigation_backend)
target_compile_options(judas_engine PRIVATE -Wall -Wextra)

add_executable(judas
    src/main.cpp
    src/Application.cpp
    src/RuntimeOptions.cpp
    src/RuntimeDiagnostics.cpp
    src/TestHarness.cpp
)
target_link_libraries(judas PRIVATE judas_engine)
target_compile_definitions(judas PRIVATE JUDAS_RUNTIME_BUILD_CONFIG="$<CONFIG>")
target_compile_options(judas PRIVATE -Wall -Wextra)

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
    src/editor/EditorApplication.cpp
    src/editor/EditorCamera.cpp
    src/editor/EditorHistory.cpp
    src/editor/EditorPanels.cpp
    src/editor/WorldTools.cpp
    src/editor/ProfilerView.cpp
    src/editor/EditorWidgets.cpp
    src/editor/ComponentEditors.cpp
    src/editor/GizmoMath.cpp
)
target_link_libraries(judas_editor PRIVATE judas_engine judas_imgui judas_navigation_baker judas_model_import)
target_compile_options(judas_editor PRIVATE -Wall -Wextra)

# Milestone 30: the scene authoring tool that writes the shipped demonstration
# scenes and the tiny game scene (tools/SceneAuthor.cpp tools/StructuredAuthor.cpp).
add_executable(judas_scene_author tools/SceneAuthor.cpp tools/StructuredAuthor.cpp tools/AuthoringCLI.cpp tools/RecipeCLI.cpp tools/ContentServicesCLI.cpp)
target_link_libraries(judas_scene_author PRIVATE judas_engine)
target_compile_options(judas_scene_author PRIVATE -Wall -Wextra)

# Milestone 31: the resource stress demonstration (real GL context, real
# asset decode on workers, a scene playing meanwhile) — tools/ResourceStress.cpp.
add_executable(judas_resource_stress tools/ResourceStress.cpp)
target_link_libraries(judas_resource_stress PRIVATE judas_engine)
target_compile_options(judas_resource_stress PRIVATE -Wall -Wextra)

# Milestone 28: scene data, serialization and runtime instantiation —
# headless (RuntimeWorld built with no ResourceManager creates no GPU state).
add_executable(judas_scene_tests
    tests/SceneTests.cpp
)
target_link_libraries(judas_scene_tests PRIVATE judas_engine)
target_compile_options(judas_scene_tests PRIVATE -Wall -Wextra)

# Milestone 29: entity lifecycle, simulation fidelity, coarse evolution,
# world-state deltas and the reduced-work measurement — headless.
add_executable(judas_lifecycle_tests
    tests/LifecycleTests.cpp
)
target_link_libraries(judas_lifecycle_tests PRIVATE judas_engine)
target_compile_options(judas_lifecycle_tests PRIVATE -Wall -Wextra)

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
target_compile_options(judas_project_tests PRIVATE -Wall -Wextra)

# Milestone 31: the job system, asynchronous file IO and the asynchronous
# ResourceManager (headless residency) — real assets decoded on workers.
add_executable(judas_job_tests
    tests/JobTests.cpp
)
target_link_libraries(judas_job_tests PRIVATE judas_engine)
target_compile_options(judas_job_tests PRIVATE -Wall -Wextra)


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
target_compile_options(judas_physics_tests PRIVATE -Wall -Wextra)

# Milestone 32: the accumulated-impulse contact solver inside PhysicsWorld —
# resting drift, stacks, friction, restitution, momentum/energy, rotated
# universes. Headless.
add_executable(judas_rigid_contact_tests
    tests/RigidContactTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_rigid_contact_tests PRIVATE src)
target_link_libraries(judas_rigid_contact_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_rigid_contact_tests PRIVATE -Wall -Wextra)

# Milestone 32: dynamic AABB tree and PhysicsWorld broadphase against a
# brute-force all-pairs + narrowphase oracle. Headless.
add_executable(judas_broadphase_tests
    tests/BroadphaseTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_broadphase_tests PRIVATE src)
target_link_libraries(judas_broadphase_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_broadphase_tests PRIVATE -Wall -Wextra)

add_executable(judas_collision_tests
    tests/CollisionTests.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_collision_tests PRIVATE src)
target_link_libraries(judas_collision_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_collision_tests PRIVATE -Wall -Wextra)

# M20: headless Newtonian two-body force and integration checks against the
# same PhysicsWorld and rigid-body integrator used by the live demonstration.
add_executable(judas_celestial_gravity_tests
    tests/CelestialGravityTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
    src/CelestialGravity.cpp
)
target_include_directories(judas_celestial_gravity_tests PRIVATE src)
target_link_libraries(judas_celestial_gravity_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_celestial_gravity_tests PRIVATE -Wall -Wextra)

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
target_compile_options(judas_asset_tests PRIVATE -Wall -Wextra)

# Milestone 10: headless tests for the step-up/step-down primitives (see
# src/StepClimb.*) — a real PhysicsWorld (no window/GL needed, see
# src/PhysicsWorld.cpp) with a handful of static bodies, exercised via the
# same rotate-the-whole-scenario technique judas_physics_tests/
# judas_collision_tests already established for "no global up" claims.
add_executable(judas_step_climb_tests
    tests/StepClimbTests.cpp
    src/StepClimb.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_step_climb_tests PRIVATE src)
target_link_libraries(judas_step_climb_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_step_climb_tests PRIVATE -Wall -Wextra)

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
target_compile_options(judas_pilot_attachment_tests PRIVATE -Wall -Wextra)

# Pilot release clearance across spacecraft rotations and gravity frames.
add_executable(judas_pilot_dismount_tests
    tests/PilotDismountTests.cpp
    src/PilotAttachment.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_pilot_dismount_tests PRIVATE src)
target_link_libraries(judas_pilot_dismount_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_pilot_dismount_tests PRIVATE -Wall -Wextra)

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
    src/PhysicsWorld.cpp
)
target_include_directories(judas_spacecraft_control_tests PRIVATE src)
target_include_directories(judas_spacecraft_control_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_spacecraft_control_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_spacecraft_control_tests PRIVATE -Wall -Wextra)

# M21: coupled spacecraft/celestial-gravity orbit and torque-based SAS tests.
add_executable(judas_spacecraft_flight_tests
    tests/SpacecraftFlightTests.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/InputSystem.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
    src/CelestialGravity.cpp
)
target_include_directories(judas_spacecraft_flight_tests PRIVATE src)
target_include_directories(judas_spacecraft_flight_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_spacecraft_flight_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_spacecraft_flight_tests PRIVATE -Wall -Wextra)

# M22: moving/rotating reference-frame transforms and live rigid-body data.
add_executable(judas_reference_frame_tests
    tests/ReferenceFrameTests.cpp
    src/ReferenceFrame.cpp
    src/PilotAttachment.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_reference_frame_tests PRIVATE src)
target_link_libraries(judas_reference_frame_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_reference_frame_tests PRIVATE -Wall -Wextra)

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
    src/SkeletalAnimation.cpp
    src/FontLoader.cpp
)
target_include_directories(judas_ui_tests PRIVATE src)
target_include_directories(judas_ui_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_ui_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM} judas_glad)
target_compile_options(judas_ui_tests PRIVATE -Wall -Wextra)

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
    src/SkeletalAnimation.cpp
    src/FontLoader.cpp
)
target_include_directories(judas_lighting_tests PRIVATE src)
target_include_directories(judas_lighting_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_lighting_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM} judas_glad)
target_compile_options(judas_lighting_tests PRIVATE -Wall -Wextra)

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
target_compile_options(judas_shadow_tests PRIVATE -Wall -Wextra)

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
    src/SkeletalAnimation.cpp
    src/FontLoader.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_interactable_tests PRIVATE src)
target_include_directories(judas_interactable_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_interactable_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM} judas_glad)
target_compile_options(judas_interactable_tests PRIVATE -Wall -Wextra)

# Milestone 17: player camera-pose math and view-input ownership. Pure CPU,
# including arbitrary-orientation and rotate-the-universe checks.
add_executable(judas_player_view_tests
    tests/PlayerViewTests.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
)
target_include_directories(judas_player_view_tests PRIVATE src)
target_link_libraries(judas_player_view_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_player_view_tests PRIVATE -Wall -Wextra)

# Milestone 18: pickup/carry/drop/throw semantics against the real Judas
# PhysicsWorld, plus interaction targeting and rotate-the-universe carry
# transform checks. No renderer/window is initialized.
add_executable(judas_object_manipulation_tests
    tests/ObjectManipulationTests.cpp
    src/ObjectManipulation.cpp
    src/InteractionSystem.cpp
    src/DynamicBody.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_object_manipulation_tests PRIVATE src)
target_link_libraries(judas_object_manipulation_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_object_manipulation_tests PRIVATE -Wall -Wextra)

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
    src/PhysicsWorld.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/RadicalGravity.cpp
)
target_include_directories(judas_player_curved_locomotion_tests PRIVATE src)
target_include_directories(judas_player_curved_locomotion_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_player_curved_locomotion_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_player_curved_locomotion_tests PRIVATE -Wall -Wextra)

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
    src/PhysicsWorld.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_world_coordinates_tests PRIVATE src)
target_include_directories(judas_world_coordinates_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_world_coordinates_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_world_coordinates_tests PRIVATE -Wall -Wextra)

# M24: the bounded CPU fluid, generic compound rigid-body geometry, and
# presentation-only water surface are exercised without a GL context.
add_executable(judas_compound_body_tests
    tests/CompoundBodyTests.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_compound_body_tests PRIVATE src)
target_link_libraries(judas_compound_body_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_compound_body_tests PRIVATE -Wall -Wextra)

add_executable(judas_fluid_world_tests
    tests/FluidWorldTests.cpp
    src/FluidWorld.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_fluid_world_tests PRIVATE src)
target_link_libraries(judas_fluid_world_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_fluid_world_tests PRIVATE -Wall -Wextra)

add_executable(judas_fluid_rigid_coupling_tests
    tests/FluidRigidCouplingTests.cpp
    src/FluidWorld.cpp
    src/ObjectManipulation.cpp
    src/DynamicBody.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_fluid_rigid_coupling_tests PRIVATE src)
target_link_libraries(judas_fluid_rigid_coupling_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_fluid_rigid_coupling_tests PRIVATE -Wall -Wextra)

add_executable(judas_fluid_surface_tests
    tests/FluidSurfaceTests.cpp
    src/FluidSurface.cpp
)
target_include_directories(judas_fluid_surface_tests PRIVATE src)
target_link_libraries(judas_fluid_surface_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_fluid_surface_tests PRIVATE -Wall -Wextra)

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
    src/PhysicsWorld.cpp
    src/RadicalGravity.cpp
)
target_include_directories(judas_terrain_physics_tests PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_terrain_physics_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_terrain_physics_tests PRIVATE -Wall -Wextra)

add_executable(judas_terrain_fluid_tests
    tests/TerrainFluidTests.cpp
    src/TerrainDemo.cpp
    src/FluidWorld.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_terrain_fluid_tests PRIVATE src)
target_link_libraries(judas_terrain_fluid_tests PRIVATE ${JUDAS_GLM})
target_compile_options(judas_terrain_fluid_tests PRIVATE -Wall -Wextra)

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
target_compile_options(judas_atmosphere_tests PRIVATE -Wall -Wextra)

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
    src/PhysicsWorld.cpp
)
target_include_directories(judas_atmospheric_flight_tests PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_atmospheric_flight_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_atmospheric_flight_tests PRIVATE -Wall -Wextra)

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
    src/PhysicsWorld.cpp
)
target_include_directories(judas_combustion_tests PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_combustion_tests PRIVATE SDL2::SDL2 ${JUDAS_GLM})
target_compile_options(judas_combustion_tests PRIVATE -Wall -Wextra)

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
add_executable(judas_scene_fingerprint_tests tests/SceneFingerprintTests.cpp)
target_link_libraries(judas_scene_fingerprint_tests PRIVATE judas_engine)
target_compile_options(judas_scene_fingerprint_tests PRIVATE -Wall -Wextra)
add_executable(judas_world_state_tests tests/WorldStateTests.cpp)
target_link_libraries(judas_world_state_tests PRIVATE judas_engine)
target_compile_options(judas_world_state_tests PRIVATE -Wall -Wextra)

# FTFT2: runs the SAME Application.cpp normal loop with deterministic observers.
add_executable(judas_async_application_tests
    tests/AsyncApplicationTests.cpp
    src/Application.cpp
    src/RuntimeOptions.cpp
    src/RuntimeDiagnostics.cpp
    src/TestHarness.cpp
)
target_link_libraries(judas_async_application_tests PRIVATE judas_engine)
target_compile_options(judas_async_application_tests PRIVATE -Wall -Wextra)

# FTFT3: serialized authored gravity through RuntimeWorld and StepPlayedWorld.
add_executable(judas_uniform_gravity_scene_tests tests/UniformGravitySceneTests.cpp)
target_link_libraries(judas_uniform_gravity_scene_tests PRIVATE judas_engine)
target_compile_options(judas_uniform_gravity_scene_tests PRIVATE -Wall -Wextra)

# FTFT4A: actual geometry queries with independent represented-input oracles.
add_executable(judas_contact_geometry_tests tests/ContactGeometryProbe.cpp)
target_link_libraries(judas_contact_geometry_tests PRIVATE judas_engine)
target_compile_options(judas_contact_geometry_tests PRIVATE -Wall -Wextra)

# First-contact reconstruction and persistence through the ordinary runtime.
add_executable(judas_contact_lifecycle_tests tests/ContactLifecycleTests.cpp)
target_link_libraries(judas_contact_lifecycle_tests PRIVATE judas_engine)
target_compile_options(judas_contact_lifecycle_tests PRIVATE -Wall -Wextra)

# FTFT4A-P: prepared geometry and actual world-cache invalidation checks.
add_executable(judas_contact_cache_tests tests/ContactCacheTests.cpp)
target_link_libraries(judas_contact_cache_tests PRIVATE judas_engine)
target_compile_options(judas_contact_cache_tests PRIVATE -Wall -Wextra)

# FTFT4A-P allocation/lifetime regressions; actual engine, unchanged physics.
add_executable(judas_contact_storage_tests tests/ContactStorageTests.cpp)
target_link_libraries(judas_contact_storage_tests PRIVATE judas_engine)

# FTFT4B-1 diagnostics: current production path; physical failures remain failures.
add_executable(judas_timing_defect_probe tests/TimingDefectProbe.cpp)
target_link_libraries(judas_timing_defect_probe PRIVATE judas_engine)
target_compile_options(judas_timing_defect_probe PRIVATE -Wall -Wextra)

add_executable(judas_rigid_motion_tests tests/RigidMotionTests.cpp)
target_link_libraries(judas_rigid_motion_tests PRIVATE judas_engine)

# FTFT4B selected-policy stop-condition witness, not a replacement world solver.
add_executable(judas_poisson_policy_witness tests/PoissonPolicyWitness.cpp)
target_link_libraries(judas_poisson_policy_witness PRIVATE judas_engine)

add_executable(judas_impact_solver_tests tests/ImpactSolverTests.cpp)
target_link_libraries(judas_impact_solver_tests PRIVATE judas_engine)
add_executable(judas_impact_timing_tests tests/ImpactTimingTests.cpp)
target_link_libraries(judas_impact_timing_tests PRIVATE judas_engine)

# FTFT6: ordinary standalone application on a tiny authored project.
add_executable(judas_project_application_tests
    tests/ProjectApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp
    src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_project_application_tests PRIVATE judas_engine)
target_compile_options(judas_project_application_tests PRIVATE -Wall -Wextra)

# FTFT6: mixed fidelity/persistence and asset identity integrity.
add_executable(judas_lifecycle_integration_tests tests/LifecycleIntegrationTests.cpp)
target_link_libraries(judas_lifecycle_integration_tests PRIVATE judas_engine)
target_compile_options(judas_lifecycle_integration_tests PRIVATE -Wall -Wextra)
add_executable(judas_asset_integrity_tests tests/AssetIntegrityTests.cpp)
target_link_libraries(judas_asset_integrity_tests PRIVATE judas_engine)
target_compile_options(judas_asset_integrity_tests PRIVATE -Wall -Wextra)

# FTFT7: actual fixed-step celestial reciprocity across simulation fidelities.
add_executable(judas_coarse_celestial_tests tests/CoarseCelestialTests.cpp)
target_link_libraries(judas_coarse_celestial_tests PRIVATE judas_engine)
target_compile_options(judas_coarse_celestial_tests PRIVATE -Wall -Wextra)
add_executable(judas_orbital_integrity_tests tests/OrbitalIntegrityTests.cpp)
target_link_libraries(judas_orbital_integrity_tests PRIVATE judas_engine)
target_compile_options(judas_orbital_integrity_tests PRIVATE -Wall -Wextra)
add_executable(judas_spacecraft_frame_integration_tests tests/SpacecraftFrameIntegrationTests.cpp)
target_link_libraries(judas_spacecraft_frame_integration_tests PRIVATE judas_engine)
target_compile_options(judas_spacecraft_frame_integration_tests PRIVATE -Wall -Wextra)

# FTFT8: independent finite-fuel and state-based thermal energy accounting.
add_executable(judas_thermal_accounting_tests tests/ThermalAccountingTests.cpp)
target_link_libraries(judas_thermal_accounting_tests PRIVATE judas_engine)
target_compile_options(judas_thermal_accounting_tests PRIVATE -Wall -Wextra)

# FTFT8: serialized-scene thermal/heater path through ordinary played-world steps.
add_executable(judas_combustion_integration_tests tests/CombustionIntegrationTests.cpp)
target_link_libraries(judas_combustion_integration_tests PRIVATE judas_engine)
target_compile_options(judas_combustion_integration_tests PRIVATE -Wall -Wextra)

add_executable(judas_player_swimming_tests tests/PlayerSwimmingTests.cpp)
target_link_libraries(judas_player_swimming_tests PRIVATE judas_engine)
target_compile_options(judas_player_swimming_tests PRIVATE -Wall -Wextra)

add_executable(judas_fluid_metadata_tests tests/FluidMetadataTests.cpp)
target_link_libraries(judas_fluid_metadata_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_metadata_tests PRIVATE -Wall -Wextra)

# FTFT9: deterministic shape/particle-field hydrostatic approximation.
add_executable(judas_fluid_hydrostatics_tests tests/FluidHydrostaticsTests.cpp)
target_link_libraries(judas_fluid_hydrostatics_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_hydrostatics_tests PRIVATE -Wall -Wextra)

add_executable(judas_production_fluid_tests tests/ProductionFluidTests.cpp)
target_link_libraries(judas_production_fluid_tests PRIVATE judas_engine)
target_compile_options(judas_production_fluid_tests PRIVATE -Wall -Wextra)

# FTFT9: actual authored compound-cavity load and moving-container integration.
add_executable(judas_fluid_container_integration_tests tests/FluidContainerIntegrationTests.cpp)
target_link_libraries(judas_fluid_container_integration_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_container_integration_tests PRIVATE -Wall -Wextra)

# FTFT9: ordinary played-world swimming consumes the actual coarse particle field.
add_executable(judas_player_fluid_integration_tests tests/PlayerFluidIntegrationTests.cpp)
target_link_libraries(judas_player_fluid_integration_tests PRIVATE judas_engine)
target_compile_options(judas_player_fluid_integration_tests PRIVATE -Wall -Wextra)

# FTFT9: actual contained particle / finite-mass wall energy budget.
add_executable(judas_fluid_cavity_impact_tests tests/FluidCavityImpactTests.cpp)
target_link_libraries(judas_fluid_cavity_impact_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_cavity_impact_tests PRIVATE -Wall -Wextra)

# FTFT9: isolated configured smoothing momentum/energy accounting.
add_executable(judas_fluid_smoothing_budget_tests tests/FluidSmoothingBudgetTests.cpp)
target_link_libraries(judas_fluid_smoothing_budget_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_smoothing_budget_tests PRIVATE -Wall -Wextra)

# FTFT9: all corner normals and exact-touch particle sweeps.
add_executable(judas_fluid_boundary_contact_tests tests/FluidBoundaryContactTests.cpp)
target_link_libraries(judas_fluid_boundary_contact_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_boundary_contact_tests PRIVATE -Wall -Wextra)

# FTFT9: existing contact-solver batch adapter, independent impulse/support oracles.
add_executable(judas_particle_contact_batch_tests tests/ParticleContactBatchTests.cpp)
target_link_libraries(judas_particle_contact_batch_tests PRIVATE judas_engine)
target_compile_options(judas_particle_contact_batch_tests PRIVATE -Wall -Wextra)

# FTFT9: shipped-scene whole-step timing, including fluid preparation/coupling.
add_executable(judas_production_fluid_performance tests/ProductionFluidPerformance.cpp)
target_link_libraries(judas_production_fluid_performance PRIVATE judas_engine)
target_compile_options(judas_production_fluid_performance PRIVATE -Wall -Wextra)

# M33: actual Renderer/RuntimeWorld GL passes, authored round-trip and Play/Stop.
add_executable(judas_render_camera_tests tests/RenderCameraTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_render_camera_tests PRIVATE judas_engine)
target_compile_options(judas_render_camera_tests PRIVATE -Wall -Wextra)

add_executable(judas_audio_tests tests/AudioTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_audio_tests PRIVATE judas_engine)
target_compile_options(judas_audio_tests PRIVATE -Wall -Wextra)

add_executable(judas_input_tests tests/InputTests.cpp)
target_link_libraries(judas_input_tests PRIVATE judas_engine)
target_compile_options(judas_input_tests PRIVATE -Wall -Wextra)

add_executable(judas_prefab_tests tests/PrefabTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_prefab_tests PRIVATE judas_engine)
target_compile_options(judas_prefab_tests PRIVATE -Wall -Wextra)

add_executable(judas_prefab_application_tests tests/PrefabApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_prefab_application_tests PRIVATE judas_engine)
target_compile_options(judas_prefab_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_visual_particle_tests tests/VisualParticleTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_visual_particle_tests PRIVATE judas_engine)
target_compile_options(judas_visual_particle_tests PRIVATE -Wall -Wextra)
add_executable(judas_particle_application_tests tests/ParticleApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_particle_application_tests PRIVATE judas_engine)
target_compile_options(judas_particle_application_tests PRIVATE -Wall -Wextra)

# M38 standalone export uses the same project/export implementation as editor.
add_executable(judas_export src/ExportMain.cpp)
target_link_libraries(judas_export PRIVATE judas_engine)
target_compile_options(judas_export PRIVATE -Wall -Wextra)
add_executable(judas_export_tests tests/ExportTests.cpp)
target_link_libraries(judas_export_tests PRIVATE judas_engine)
target_compile_options(judas_export_tests PRIVATE -Wall -Wextra)
add_executable(judas_package_application_tests tests/PackageApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_package_application_tests PRIVATE judas_engine)
target_compile_options(judas_package_application_tests PRIVATE -Wall -Wextra)

# M39: normal project/runtime/physics/GL classification consumers.
add_executable(judas_classification_tests tests/ClassificationTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_classification_tests PRIVATE judas_engine)
target_compile_options(judas_classification_tests PRIVATE -Wall -Wextra)

target_link_libraries(judas_engine PUBLIC judas_quickjs)
add_executable(judas_script_tests tests/ScriptTests.cpp)
target_link_libraries(judas_script_tests PRIVATE judas_engine)
target_compile_options(judas_script_tests PRIVATE -Wall -Wextra)
add_executable(judas_script_application_tests tests/ScriptApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_script_application_tests PRIVATE judas_engine)
target_compile_options(judas_script_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_runtime_ui_tests tests/RuntimeUITests.cpp)
target_link_libraries(judas_runtime_ui_tests PRIVATE judas_engine)
target_compile_options(judas_runtime_ui_tests PRIVATE -Wall -Wextra)
add_executable(judas_runtime_ui_application_tests tests/RuntimeUIApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_runtime_ui_application_tests PRIVATE judas_engine)
target_compile_options(judas_runtime_ui_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_touch_event_tests tests/TouchEventTests.cpp)
target_link_libraries(judas_touch_event_tests PRIVATE judas_engine)
target_compile_options(judas_touch_event_tests PRIVATE -Wall -Wextra)
add_executable(judas_touch_application_tests tests/TouchApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_touch_application_tests PRIVATE judas_engine)
target_compile_options(judas_touch_application_tests PRIVATE -Wall -Wextra)

# M43: normal Application loop, actual M42 callbacks and fresh world lifetimes.
add_executable(judas_scene_transition_tests tests/SceneTransitionTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_scene_transition_tests PRIVATE judas_engine)
target_compile_options(judas_scene_transition_tests PRIVATE -Wall -Wextra)

add_executable(judas_physics_cast_tests tests/PhysicsCastTests.cpp)
target_link_libraries(judas_physics_cast_tests PRIVATE judas_engine)
target_compile_options(judas_physics_cast_tests PRIVATE -Wall -Wextra)
add_executable(judas_physics_cast_application_tests tests/PhysicsCastApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_physics_cast_application_tests PRIVATE judas_engine)
target_compile_options(judas_physics_cast_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_joint_tests tests/JointTests.cpp)
target_link_libraries(judas_joint_tests PRIVATE judas_engine)
target_compile_options(judas_joint_tests PRIVATE -Wall -Wextra)

add_executable(judas_joint_authoring_tests tests/JointAuthoringTests.cpp)
target_link_libraries(judas_joint_authoring_tests PRIVATE judas_engine)
target_compile_options(judas_joint_authoring_tests PRIVATE -Wall -Wextra)

add_executable(judas_joint_application_tests tests/JointApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_joint_application_tests PRIVATE judas_engine)
target_compile_options(judas_joint_application_tests PRIVATE -Wall -Wextra)

target_include_directories(judas_engine SYSTEM PRIVATE third_party/cgltf)

add_executable(judas_skeletal_animation_tests tests/SkeletalAnimationTests.cpp)
target_link_libraries(judas_skeletal_animation_tests PRIVATE judas_engine)
target_compile_options(judas_skeletal_animation_tests PRIVATE -Wall -Wextra)
add_executable(judas_animation_application_tests tests/AnimationApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_animation_application_tests PRIVATE judas_engine)
target_compile_options(judas_animation_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_pose_composition_tests tests/PoseCompositionTests.cpp)
target_link_libraries(judas_pose_composition_tests PRIVATE judas_engine)
target_compile_options(judas_pose_composition_tests PRIVATE -Wall -Wextra)

add_executable(judas_pose_composition_application_tests tests/PoseCompositionApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_pose_composition_application_tests PRIVATE judas_engine)

add_executable(judas_ragdoll_tests tests/RagdollTests.cpp)
target_link_libraries(judas_ragdoll_tests PRIVATE judas_engine)

add_executable(judas_ragdoll_application_tests tests/RagdollApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_ragdoll_application_tests PRIVATE judas_engine)

# Focused fluid-demo performance observation of the ordinary application.
add_executable(judas_fluid_demo_probe tools/FluidDemoProbe.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_fluid_demo_probe PRIVATE judas_engine)
target_compile_options(judas_fluid_demo_probe PRIVATE -Wall -Wextra)

add_executable(judas_fluid_demo_integration_tests tests/FluidDemoIntegrationTests.cpp)
target_link_libraries(judas_fluid_demo_integration_tests PRIVATE judas_engine)
target_compile_options(judas_fluid_demo_integration_tests PRIVATE -Wall -Wextra)

add_executable(judas_character_motor_tests tests/CharacterMotorTests.cpp)
target_link_libraries(judas_character_motor_tests PRIVATE judas_engine)
target_compile_options(judas_character_motor_tests PRIVATE -Wall -Wextra)
add_executable(judas_character_motor_application_tests tests/CharacterMotorApplicationTests.cpp)
target_link_libraries(judas_character_motor_application_tests PRIVATE judas_engine)
target_compile_options(judas_character_motor_application_tests PRIVATE -Wall -Wextra)

# M50 developer documentation examples; uses the normal engine/QuickJS runtime.
add_executable(judasjs_examples_tests tests/JudasJSExamplesTests.cpp)
target_link_libraries(judasjs_examples_tests PRIVATE judas_engine)
target_compile_options(judasjs_examples_tests PRIVATE -Wall -Wextra)

# M51 ordinary project behaviour composition and explicit compatibility boundary.
add_executable(judas_boundary_migration_tests tests/BoundaryMigrationTests.cpp)
target_link_libraries(judas_boundary_migration_tests PRIVATE judas_engine)
target_compile_options(judas_boundary_migration_tests PRIVATE -Wall -Wextra)

# M52 executes the ordinary JS game against production physics and runtime systems.
add_executable(judas_shooter_game_tests tests/ShooterGameTests.cpp)
target_link_libraries(judas_shooter_game_tests PRIVATE judas_engine)
target_compile_options(judas_shooter_game_tests PRIVATE -Wall -Wextra)

add_executable(judas_shooter_application_tests tests/ShooterApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_shooter_application_tests PRIVATE judas_engine)
target_compile_options(judas_shooter_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_script_presentation_tests tests/ScriptPresentationTests.cpp)
target_link_libraries(judas_script_presentation_tests PRIVATE judas_engine)
target_compile_options(judas_script_presentation_tests PRIVATE -Wall -Wextra)

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

add_executable(judas_text_localization_tests tests/TextLocalizationTests.cpp)
target_link_libraries(judas_text_localization_tests PRIVATE judas_engine)
target_compile_options(judas_text_localization_tests PRIVATE -Wall -Wextra)
add_executable(judas_text_render_tests tests/TextRenderTests.cpp)
target_link_libraries(judas_text_render_tests PRIVATE judas_engine)
target_compile_options(judas_text_render_tests PRIVATE -Wall -Wextra)
add_executable(judas_localization_application_tests tests/LocalizationApplicationTests.cpp src/Application.cpp src/TestHarness.cpp src/RuntimeDiagnostics.cpp src/RuntimeOptions.cpp)
target_link_libraries(judas_localization_application_tests PRIVATE judas_engine)
target_compile_options(judas_localization_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_streaming_tests tests/WorldStreamingTests.cpp)
target_link_libraries(judas_streaming_tests PRIVATE judas_engine)
target_compile_options(judas_streaming_tests PRIVATE -Wall -Wextra)
add_executable(judas_streaming_demo_builder tools/StreamingDemo.cpp)
target_link_libraries(judas_streaming_demo_builder PRIVATE judas_navigation_baker)
add_executable(judas_streaming_application_tests tests/StreamingApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_streaming_application_tests PRIVATE judas_engine)
target_compile_options(judas_streaming_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_streaming_performance_tests tests/StreamingPerformanceTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_streaming_performance_tests PRIVATE judas_engine)
target_include_directories(judas_streaming_performance_tests PRIVATE src)
target_compile_options(judas_streaming_performance_tests PRIVATE -Wall -Wextra -Wpedantic)

# M59 focused real-project spatial-interest suspension/revisit regression.
add_executable(judas_streaming_revisit_tests tests/StreamingRevisitTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_streaming_revisit_tests PRIVATE judas_engine)
target_compile_options(judas_streaming_revisit_tests PRIVATE -Wall -Wextra)

# Historical targets compile Window.cpp directly instead of linking the engine.
# Keep their window/icon dependency identical to the editor and standalone game.
get_property(JUDAS_ICON_TARGETS DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(JUDAS_ICON_TARGET IN LISTS JUDAS_ICON_TARGETS)
    get_target_property(JUDAS_ICON_SOURCES ${JUDAS_ICON_TARGET} SOURCES)
    if("src/Window.cpp" IN_LIST JUDAS_ICON_SOURCES)
        target_link_libraries(${JUDAS_ICON_TARGET} PRIVATE judas_app_icon)
    endif()
endforeach()

# M60 production PCM streaming/acoustics checks (no device required).
add_executable(judas_audio_acoustics_tests tests/AudioAcousticsTests.cpp)
target_link_libraries(judas_audio_acoustics_tests PRIVATE judas_engine)
target_compile_options(judas_audio_acoustics_tests PRIVATE -Wall -Wextra)
add_executable(judas_audio_demo tools/AudioDemo.cpp)
target_link_libraries(judas_audio_demo PRIVATE judas_engine)

add_executable(judas_audio_world_tests tests/AudioWorldTests.cpp)
target_link_libraries(judas_audio_world_tests PRIVATE judas_engine)
target_compile_options(judas_audio_world_tests PRIVATE -Wall -Wextra)

add_executable(judas_audio_application_tests tests/AudioApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_audio_application_tests PRIVATE judas_engine)
target_compile_options(judas_audio_application_tests PRIVATE -Wall -Wextra -Wpedantic)

# M61 focused local-storage protocol; no desktop or operator save directories.
add_executable(judas_save_storage_tests tests/SaveStorageTests.cpp)
target_link_libraries(judas_save_storage_tests PRIVATE judas_engine)
target_compile_options(judas_save_storage_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(judas_world_persistence_tests tests/WorldPersistenceTests.cpp)
target_link_libraries(judas_world_persistence_tests PRIVATE judas_engine)
target_compile_options(judas_world_persistence_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(judas_save_application_tests tests/SaveApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_save_application_tests PRIVATE judas_engine)
target_compile_options(judas_save_application_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_save_streaming_tests tests/SaveStreamingTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_save_streaming_tests PRIVATE judas_engine)
target_compile_options(judas_save_streaming_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_save_performance_tests tests/SavePerformanceTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_save_performance_tests PRIVATE judas_engine)
target_compile_options(judas_save_performance_tests PRIVATE -Wall -Wextra -Wpedantic)

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
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

add_executable(judas_fracture_tests tests/FractureTests.cpp)
target_link_libraries(judas_fracture_tests PRIVATE judas_engine)

add_executable(judas_fracture_application_tests tests/FractureApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_fracture_application_tests PRIVATE judas_engine)

add_executable(judas_fracture_performance_tests tests/FracturePerformanceTests.cpp)
target_link_libraries(judas_fracture_performance_tests PRIVATE judas_engine)

foreach(target judas_fracture_tests judas_fracture_application_tests judas_fracture_performance_tests)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

add_executable(judas_collision_geometry_tests tests/CollisionGeometryTests.cpp)
target_link_libraries(judas_collision_geometry_tests PRIVATE judas_engine)

add_executable(judas_collision_cook tools/CollisionCook.cpp)
target_link_libraries(judas_collision_cook PRIVATE judas_engine)

add_executable(judas_collision_application_tests tests/CollisionApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_collision_application_tests PRIVATE judas_engine)

add_executable(judas_developer_integration_tests tests/DeveloperIntegrationTests.cpp)
target_link_libraries(judas_developer_integration_tests PRIVATE judas_engine)
target_compile_options(judas_developer_integration_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_developer_author tools/DeveloperIntegrationAuthor.cpp)
target_link_libraries(judas_developer_author PRIVATE judas_engine)
target_compile_options(judas_developer_author PRIVATE -Wall -Wextra -Wpedantic)
target_sources(judas_developer_integration_tests PRIVATE src/editor/EditorHistory.cpp)
add_executable(judas_developer_application_tests tests/DeveloperApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_developer_application_tests PRIVATE judas_engine)
target_compile_options(judas_developer_application_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_developer_authoring_tests tests/DeveloperAuthoringTests.cpp tools/StructuredAuthor.cpp)
target_include_directories(judas_developer_authoring_tests PRIVATE tools)
target_link_libraries(judas_developer_authoring_tests PRIVATE judas_engine)
target_compile_options(judas_developer_authoring_tests PRIVATE -Wall -Wextra -Wpedantic)

# Ordinary application-loop observation for the post-M65 consumer review.
add_executable(judas_consumer_review tools/ConsumerReview.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_consumer_review PRIVATE judas_engine)
target_compile_options(judas_consumer_review PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_consumer_authoring_tests tests/ConsumerAuthoringTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_consumer_authoring_tests PRIVATE judas_engine)
target_compile_options(judas_consumer_authoring_tests PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_consumer_capsule_repro tools/ConsumerCapsuleRepro.cpp)
target_link_libraries(judas_consumer_capsule_repro PRIVATE judas_engine)
target_compile_options(judas_consumer_capsule_repro PRIVATE -Wall -Wextra -Wpedantic)
add_executable(judas_consumer_reference_repro tools/ConsumerReferenceRepro.cpp)
target_link_libraries(judas_consumer_reference_repro PRIVATE judas_engine)
target_compile_options(judas_consumer_reference_repro PRIVATE -Wall -Wextra -Wpedantic)

add_executable(judas_post_consumer_query_tests tests/PostConsumerQueryTests.cpp)
target_link_libraries(judas_post_consumer_query_tests PRIVATE judas_engine)
target_compile_options(judas_post_consumer_query_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(judas_post_consumer_metadata_tests tests/PostConsumerMetadataTests.cpp)
target_link_libraries(judas_post_consumer_metadata_tests PRIVATE judas_engine)
add_executable(judas_post_consumer_ragdoll_tests tests/PostConsumerRagdollTests.cpp)
target_link_libraries(judas_post_consumer_ragdoll_tests PRIVATE judas_engine)

# M66 source import is authoring-only; shipped runtime reads normalized models.
add_library(judas_ufbx STATIC third_party/ufbx/ufbx.c)
target_include_directories(judas_ufbx PUBLIC third_party/ufbx)
add_library(judas_model_import STATIC src/FbxImport.cpp src/ObjImport.cpp src/ModelCook.cpp src/ModelCollision.cpp)
target_link_libraries(judas_model_import PUBLIC judas_engine judas_ufbx)
target_compile_options(judas_model_import PRIVATE -Wall -Wextra)
add_executable(judas_model_import_cli tools/ModelImportCli.cpp)
target_link_libraries(judas_model_import_cli PRIVATE judas_model_import)
add_executable(judas_model_import_tests tests/ModelImportTests.cpp)
target_link_libraries(judas_model_import_tests PRIVATE judas_model_import)
target_compile_options(judas_model_import_tests PRIVATE -Wall -Wextra)

add_executable(judas_model_reimport_tests tests/ModelReimportTests.cpp)
target_link_libraries(judas_model_reimport_tests PRIVATE judas_model_import)
target_compile_options(judas_model_reimport_tests PRIVATE -Wall -Wextra)

add_executable(judas_model_runtime_tests tests/ModelRuntimeTests.cpp)
target_link_libraries(judas_model_runtime_tests PRIVATE judas_engine)

add_executable(judas_model_application_tests tests/ModelApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_model_application_tests PRIVATE judas_engine)
target_compile_options(judas_model_application_tests PRIVATE -Wall -Wextra)

add_executable(judas_world_building_tests tests/WorldBuildingTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_world_building_tests PRIVATE judas_engine)
target_compile_options(judas_world_building_tests PRIVATE -Wall -Wextra)

add_library(judas_world_authoring STATIC src/authoring/ContentReferences.cpp src/authoring/WorldBuilder.cpp src/authoring/AuthoringDocument.cpp src/authoring/SkeletonAuthoring.cpp src/authoring/RecipePreview.cpp)
target_include_directories(judas_world_authoring PUBLIC src/authoring)
target_link_libraries(judas_world_authoring PUBLIC judas_engine)
target_compile_options(judas_world_authoring PRIVATE -Wall -Wextra)
target_link_libraries(judas_scene_author PRIVATE judas_world_authoring)
target_link_libraries(judas_editor PRIVATE judas_world_authoring)
target_link_libraries(judas_world_building_tests PRIVATE judas_world_authoring)

target_link_libraries(judas_scene_author PRIVATE judas_model_import)

add_executable(judas_world_authoring_runtime_tests tests/WorldAuthoringRuntimeTests.cpp)
target_link_libraries(judas_world_authoring_runtime_tests PRIVATE judas_engine)
target_compile_options(judas_world_authoring_runtime_tests PRIVATE -Wall -Wextra)

add_executable(judas_world_authoring_application_tests tests/WorldAuthoringApplicationTests.cpp src/Application.cpp src/RuntimeOptions.cpp src/RuntimeDiagnostics.cpp src/TestHarness.cpp)
target_link_libraries(judas_world_authoring_application_tests PRIVATE judas_engine)
target_compile_options(judas_world_authoring_application_tests PRIVATE -Wall -Wextra)
add_executable(judas_world_authoring_performance tests/WorldAuthoringPerformance.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_world_authoring_performance PRIVATE judas_world_authoring)
target_compile_options(judas_world_authoring_performance PRIVATE -Wall -Wextra)

# M68 structural work counts and comparable residency captures, with owned output.
add_executable(judas_scalability_tests tests/ScalabilityTests.cpp)
target_link_libraries(judas_scalability_tests PRIVATE judas_engine)
target_compile_options(judas_scalability_tests PRIVATE -Wall -Wextra)
add_executable(judas_scalability_pipeline_tests tests/ScalabilityPipelineTests.cpp src/editor/EditorHistory.cpp)
target_link_libraries(judas_scalability_pipeline_tests PRIVATE judas_model_import)
target_compile_options(judas_scalability_pipeline_tests PRIVATE -Wall -Wextra)
add_executable(judas_resource_handoff_tests tests/ResourceHandoffTests.cpp)
target_link_libraries(judas_resource_handoff_tests PRIVATE judas_engine)
target_compile_options(judas_resource_handoff_tests PRIVATE -Wall -Wextra)
