cmake_minimum_required(VERSION 3.20)
project(ProjectJudas CXX)

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

add_executable(judas
    src/main.cpp
    src/Application.cpp
    src/Window.cpp
    src/Renderer.cpp
    src/PlayerController.cpp
    src/DynamicBody.cpp
    src/FlyingPrimitiveControl.cpp
    src/FaithfulGravity.cpp
    src/RadicalGravity.cpp
    src/SphericalVolume.cpp
    src/BoxVolume.cpp
    src/GravityContextMap.cpp
    ${JUDAS_PHYSICS_SOURCES}
    src/PhysicsWorld.cpp
    src/TestHarness.cpp
    src/gl_core33.cpp
)

target_include_directories(judas PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas PRIVATE SDL2::SDL2 glm::glm-header-only)
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

# stb_image_write (public domain / MIT, see third_party/stb_image_write.h) —
# used only by the opt-in test harness (src/TestHarness.cpp) to write
# screenshots to PNG. A single vendored header, not fetched via CMake:
# writing images is a solved commodity problem and the file has no
# dependencies of its own, so there is nothing for a package manager or
# FetchContent to add value over just committing the one file.
