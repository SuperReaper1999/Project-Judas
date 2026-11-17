include_guard(GLOBAL)
include(FetchContent)

# Native MSVC only. Other platforms retain their existing dependency selection.
function(judas_windows_dependencies)
    if(NOT MSVC OR NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Native Windows dependencies require MSVC x64")
    endif()
    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    set(root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
    if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM OR CMAKE_POLICY_VERSION_MINIMUM VERSION_LESS 3.10)
        set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    endif()
    set(BUILD_SHARED_LIBS OFF)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_LIBC ON CACHE BOOL "" FORCE)
    set(SDL_FORCE_STATIC_VCRT OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN OFF CACHE BOOL "" FORCE)
    # SDL's optional Unix backend detection is not a native MSVC dependency.
    # CMake target/link/runtime consumers below validate the Windows contract;
    # no vcpkg port or pkg-config metadata-validation claim is made here.
    set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)
    FetchContent_Declare(judas_sdl
        URL "${root}/third_party/windows/SDL2-2.30.12.tar.gz"
        URL_HASH SHA512=0ce3aa0bd340465039c4dad8e562b4ac612932e23f011629522ceaf9cd3317b2a1905b30f190a5e6d79ac43a7a5db75dff712600570776e5bbef41fbedaab5c4)
    set(GLM_BUILD_LIBRARY ON CACHE BOOL "" FORCE)
    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    if(NOT DEFINED GLM_BUILD_INSTALL)
        set(GLM_BUILD_INSTALL OFF CACHE BOOL "")
    endif()
    FetchContent_Declare(judas_glm
        URL "${root}/third_party/android/glm-1.0.1.tar.gz"
        URL_HASH SHA512=c6c6fa1ea7a7e97820e36ee042a78be248ae828c99c1b1111080d9bf334a5160c9993a70312351c92a867cd49907c95f9f357c8dfe2bc29946da6e83e27ba20c)
    FetchContent_MakeAvailable(judas_sdl judas_glm)
    # Upstream GLM otherwise installs both configurations as glm.lib.
    set_target_properties(glm PROPERTIES DEBUG_POSTFIX d)
    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 ALIAS SDL2-static)
    endif()
    set(JUDAS_GLM glm::glm PARENT_SCOPE)
    set(JUDAS_SDL_LICENSE "${judas_sdl_SOURCE_DIR}/LICENSE.txt" PARENT_SCOPE)
    set(JUDAS_GLM_LICENSE "${judas_glm_SOURCE_DIR}/copying.txt" PARENT_SCOPE)
endfunction()
