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
find_package(glm REQUIRED)

# No physics middleware. Judas owns rigid-body state, integration,
# collision detection, contact generation, and contact resolution itself
# (src/RigidBody.*, src/Contacts.*, src/ContactSolver.*, src/PhysicsWorld.cpp)
# — see docs/ARCHITECTURE.md, "Physics ownership," for the milestone this
# decision belongs to and why Jolt Physics (used through the first M7-Final
# attempt, recoverable via git history/the milestone-7b tag for the last
# Jolt-backed milestone) was replaced rather than kept as an authoritative
# dependency.

set(JUDAS_PHYSICS_SOURCES
    src/RigidBody.cpp
    src/RigidBodyGravity.cpp
    src/Contacts.cpp
    src/ContactSolver.cpp
)

# GLAD 2 loader generated for precisely OpenGL 3.3 Core, no extensions.
add_library(judas_glad STATIC third_party/glad/src/gl.c)
target_include_directories(judas_glad PUBLIC third_party/glad/include)
target_compile_options(judas_glad PRIVATE -Wall -Wextra)

add_executable(judas
    src/main.cpp
    src/Application.cpp
    src/Window.cpp
    src/Renderer.cpp
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/StepClimb.cpp
    src/DynamicBody.cpp
    src/FlyingPrimitiveControl.cpp
    src/PilotAttachment.cpp
    src/PilotControl.cpp
    src/ReferenceFrame.cpp
    src/ModelLoader.cpp
    src/TextureLoader.cpp
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
    src/Door.cpp
    src/LightSwitch.cpp
    src/FaithfulGravity.cpp
    src/RadicalGravity.cpp
    src/SphericalVolume.cpp
    src/BoxVolume.cpp
    src/GravityContextMap.cpp
    src/CelestialGravity.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
    src/TestHarness.cpp
)

target_include_directories(judas PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas PRIVATE SDL2::SDL2 glm::glm-header-only judas_glad)
target_compile_options(judas PRIVATE -Wall -Wextra)

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
target_link_libraries(judas_physics_tests PRIVATE glm::glm-header-only)
target_compile_options(judas_physics_tests PRIVATE -Wall -Wextra)

add_executable(judas_collision_tests
    tests/CollisionTests.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_collision_tests PRIVATE src)
target_link_libraries(judas_collision_tests PRIVATE glm::glm-header-only)
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
target_link_libraries(judas_celestial_gravity_tests PRIVATE glm::glm-header-only)
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
    src/ModelLoader.cpp
    src/TextureLoader.cpp
)
target_include_directories(judas_asset_tests PRIVATE src)
target_link_libraries(judas_asset_tests PRIVATE glm::glm-header-only)
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
target_link_libraries(judas_step_climb_tests PRIVATE glm::glm-header-only)
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
target_link_libraries(judas_pilot_attachment_tests PRIVATE glm::glm-header-only)
target_compile_options(judas_pilot_attachment_tests PRIVATE -Wall -Wextra)

# Pilot release clearance across spacecraft rotations and gravity frames.
add_executable(judas_pilot_dismount_tests
    tests/PilotDismountTests.cpp
    src/PilotAttachment.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_pilot_dismount_tests PRIVATE src)
target_link_libraries(judas_pilot_dismount_tests PRIVATE glm::glm-header-only)
target_compile_options(judas_pilot_dismount_tests PRIVATE -Wall -Wextra)

# Milestone 11: headless tests for the spacecraft's full 6DOF control (see
# src/FlyingPrimitiveControl.*) — a real PhysicsWorld plus a real Window
# constructed without Init (test-input mode only, no SDL/GL call ever
# happens — see tests/SpacecraftControlTests.cpp's own header comment).
add_executable(judas_spacecraft_control_tests
    tests/SpacecraftControlTests.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_spacecraft_control_tests PRIVATE src)
target_include_directories(judas_spacecraft_control_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_spacecraft_control_tests PRIVATE SDL2::SDL2 glm::glm-header-only)
target_compile_options(judas_spacecraft_control_tests PRIVATE -Wall -Wextra)

# M21: coupled spacecraft/celestial-gravity orbit and torque-based SAS tests.
add_executable(judas_spacecraft_flight_tests
    tests/SpacecraftFlightTests.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
    src/CelestialGravity.cpp
)
target_include_directories(judas_spacecraft_flight_tests PRIVATE src)
target_include_directories(judas_spacecraft_flight_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_spacecraft_flight_tests PRIVATE SDL2::SDL2 glm::glm-header-only)
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
target_link_libraries(judas_reference_frame_tests PRIVATE glm::glm-header-only)
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
    src/FontLoader.cpp
)
target_include_directories(judas_ui_tests PRIVATE src)
target_include_directories(judas_ui_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_ui_tests PRIVATE SDL2::SDL2 glm::glm-header-only judas_glad)
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
    src/FontLoader.cpp
)
target_include_directories(judas_lighting_tests PRIVATE src)
target_include_directories(judas_lighting_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_lighting_tests PRIVATE SDL2::SDL2 glm::glm-header-only judas_glad)
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
target_link_libraries(judas_shadow_tests PRIVATE glm::glm-header-only)
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
    src/FontLoader.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
)
target_include_directories(judas_interactable_tests PRIVATE src)
target_include_directories(judas_interactable_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_interactable_tests PRIVATE SDL2::SDL2 glm::glm-header-only judas_glad)
target_compile_options(judas_interactable_tests PRIVATE -Wall -Wextra)

# Milestone 17: player camera-pose math and view-input ownership. Pure CPU,
# including arbitrary-orientation and rotate-the-universe checks.
add_executable(judas_player_view_tests
    tests/PlayerViewTests.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
)
target_include_directories(judas_player_view_tests PRIVATE src)
target_link_libraries(judas_player_view_tests PRIVATE glm::glm-header-only)
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
target_link_libraries(judas_object_manipulation_tests PRIVATE glm::glm-header-only)
target_compile_options(judas_object_manipulation_tests PRIVATE -Wall -Wextra)

# Long fixed-step player locomotion against sphere and flat supports,
# including rotated worlds and the ascending spacecraft-release regression.
add_executable(judas_player_curved_locomotion_tests
    tests/PlayerCurvedLocomotionTests.cpp
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
    src/PilotControl.cpp
    src/PilotAttachment.cpp
    src/FlyingPrimitiveControl.cpp
    src/Window.cpp
    src/StepClimb.cpp
    src/PhysicsWorld.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/RadicalGravity.cpp
)
target_include_directories(judas_player_curved_locomotion_tests PRIVATE src)
target_include_directories(judas_player_curved_locomotion_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_player_curved_locomotion_tests PRIVATE SDL2::SDL2 glm::glm-header-only)
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
    src/PlayerController.cpp
    src/PlayerView.cpp
    src/LightTransforms.cpp
    src/ReferenceFrame.cpp
    src/ShadowTransforms.cpp
    src/Window.cpp
    src/StepClimb.cpp
    src/PhysicsWorld.cpp
    ${JUDAS_TEST_SUPPORT_SOURCES}
)
target_include_directories(judas_world_coordinates_tests PRIVATE src)
target_include_directories(judas_world_coordinates_tests PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas_world_coordinates_tests PRIVATE SDL2::SDL2 glm::glm-header-only)
target_compile_options(judas_world_coordinates_tests PRIVATE -Wall -Wextra)

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
