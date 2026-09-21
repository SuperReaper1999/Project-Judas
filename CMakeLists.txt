cmake_minimum_required(VERSION 3.16)
project(ProjectJudas CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)

add_executable(judas
    src/main.cpp
    src/Application.cpp
    src/Window.cpp
    src/Renderer.cpp
    src/Camera.cpp
    src/gl_core33.cpp
)

target_include_directories(judas PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(judas PRIVATE SDL2::SDL2 glm::glm-header-only)
target_compile_options(judas PRIVATE -Wall -Wextra)
