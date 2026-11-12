# Experimental Vulkan graphics backend (branch Vulkan-Exp).
#
# The backend is compiled into desktop builds when its two dependencies are
# present and is selected at run time with JUDAS_GRAPHICS=vulkan. OpenGL
# remains the default backend of every build. See docs/VULKAN_BACKEND.md.
#   JUDAS_VULKAN=AUTO  build it when the Vulkan headers/loader and glslang exist
#   JUDAS_VULKAN=ON    require it (configuration fails when a dependency is absent)
#   JUDAS_VULKAN=OFF   never build it
set(JUDAS_VULKAN "AUTO" CACHE STRING "Experimental Vulkan backend: AUTO, ON or OFF")
set_property(CACHE JUDAS_VULKAN PROPERTY STRINGS AUTO ON OFF)
set(JUDAS_VULKAN_ENABLED OFF)
if(NOT JUDAS_VULKAN STREQUAL "OFF")
    if(ANDROID OR EMSCRIPTEN OR MINGW)
        if(JUDAS_VULKAN STREQUAL "ON")
            message(FATAL_ERROR "JUDAS_VULKAN=ON: the experimental Vulkan backend is only wired for native desktop builds")
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
    add_library(judas_vulkan STATIC
        src/vulkan/VkCore.cpp
        src/vulkan/VkShader.cpp
        src/vulkan/VkTexture.cpp
        src/vulkan/VkDraw.cpp
        src/vulkan/VkState.cpp)
    target_include_directories(judas_vulkan PUBLIC src)
    target_link_libraries(judas_vulkan PUBLIC judas_glad SDL2::SDL2 Vulkan::Vulkan
        PRIVATE glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
    judas_warnings(judas_vulkan)
    target_link_libraries(judas_graphics_caps PUBLIC judas_vulkan)
    message(STATUS "Judas: experimental Vulkan backend enabled (run with JUDAS_GRAPHICS=vulkan)")
else()
    message(STATUS "Judas: experimental Vulkan backend not built (JUDAS_VULKAN=${JUDAS_VULKAN})")
endif()
