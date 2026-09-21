cmake_minimum_required(VERSION 3.20)
project(ProjectJudas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)

# Jolt Physics (see docs/ARCHITECTURE.md, "Physics middleware", for why it
# was chosen). Not packaged by apt, so it's fetched at configure time
# rather than vendored into this repository or assumed pre-installed.
# Pinned to an exact tag for reproducible builds.
include(FetchContent)

# Jolt's own CMakeLists enables "warnings as errors" for its own target by
# default. That's a lint setting from a project we don't control, not a
# guarantee about this repo's own compiler/version — disabling it avoids an
# unrelated dependency breaking our build on a newer compiler than Jolt was
# tested against. It has no effect on warnings for our own code.
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.6.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  Build
)
FetchContent_MakeAvailable(JoltPhysics)

add_executable(judas
    src/main.cpp
    src/Application.cpp
    src/Window.cpp
    src/Renderer.cpp
    src/PlayerController.cpp
    src/DynamicBody.cpp
    src/FaithfulGravity.cpp
    src/RadicalGravity.cpp
    src/PhysicsWorld.cpp
    src/TestHarness.cpp
    src/gl_core33.cpp
)

target_include_directories(judas PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas PRIVATE SDL2::SDL2 glm::glm-header-only Jolt)
target_compile_options(judas PRIVATE -Wall -Wextra)

# stb_image_write (public domain / MIT, see third_party/stb_image_write.h) —
# used only by the opt-in test harness (src/TestHarness.cpp) to write
# screenshots to PNG. A single vendored header, not fetched via CMake:
# writing images is a solved commodity problem and the file has no
# dependencies of its own, so there is nothing for a package manager or
# FetchContent to add value over just committing the one file.
