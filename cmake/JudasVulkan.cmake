# Optional Vulkan graphics backend for native Linux.
#
# The optional backend is selected at run time with JUDAS_GRAPHICS=vulkan. OpenGL
# remains the default backend of every build. See docs/VULKAN_INTEGRATION.md.
#   JUDAS_VULKAN=AUTO  build it when the Vulkan headers/loader and glslang exist
#   JUDAS_VULKAN=ON    require it (configuration fails when a dependency is absent)
#   JUDAS_VULKAN=OFF   never build it (default)
set(JUDAS_VULKAN "OFF" CACHE STRING "Optional Linux Vulkan backend: AUTO, ON or OFF")
set_property(CACHE JUDAS_VULKAN PROPERTY STRINGS AUTO ON OFF)
if(NOT JUDAS_VULKAN MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "JUDAS_VULKAN must be AUTO, ON or OFF")
endif()
set(JUDAS_VULKAN_ENABLED OFF)
if(NOT JUDAS_VULKAN STREQUAL "OFF")
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID)
        if(JUDAS_VULKAN STREQUAL "ON")
            message(FATAL_ERROR "JUDAS_VULKAN=ON: the Vulkan backend requires native Linux")
        endif()
    else()
        find_package(Vulkan QUIET)
        find_package(glslang CONFIG QUIET)
        if(Vulkan_FOUND AND TARGET glslang::glslang AND TARGET glslang::SPIRV
           AND TARGET glslang::glslang-default-resource-limits)
            set(JUDAS_VULKAN_ENABLED ON)
        elseif(JUDAS_VULKAN STREQUAL "ON")
            message(FATAL_ERROR "JUDAS_VULKAN=ON requires the Vulkan headers/loader and the glslang "
                "development package (Ubuntu: libvulkan-dev glslang-dev spirv-tools)")
        endif()
    endif()
endif()

if(JUDAS_VULKAN_ENABLED)
    # Every translation unit must agree on whether VulkanBackend.h declares the
    # real backend or its inline no-op stand-ins.
    add_compile_definitions(JUDAS_VULKAN=1)
    set_property(TARGET judas_build_identity PROPERTY JUDAS_VULKAN_BUILD ON)
    add_library(judas_vulkan STATIC
        src/vulkan/VkCore.cpp
        src/vulkan/VkShader.cpp
        src/vulkan/VkTexture.cpp
        src/vulkan/VkDraw.cpp
        src/vulkan/VkState.cpp
        src/vulkan/VkGfx.cpp)
    target_include_directories(judas_vulkan PUBLIC src)
    target_link_libraries(judas_vulkan PUBLIC judas_glad SDL2::SDL2 Vulkan::Vulkan
        PRIVATE glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
    judas_warnings(judas_vulkan)
    target_link_libraries(judas_graphics_caps PUBLIC judas_vulkan)
    message(STATUS "Judas: Vulkan backend enabled (run with JUDAS_GRAPHICS=vulkan)")
else()
    message(STATUS "Judas: Vulkan backend not built (JUDAS_VULKAN=${JUDAS_VULKAN})")
endif()
