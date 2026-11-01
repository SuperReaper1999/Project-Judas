include_guard(GLOBAL)

# Subsystems may declare executables after SDK staging is configured. Apply the
# process manifest and runtime ownership once the directory has finished, rather
# than silently omitting late tools/tests from the Windows platform contract.
function(judas_finalize_windows_targets)
    get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(kind ${target} TYPE)
        if(kind STREQUAL "EXECUTABLE")
            if(MINGW)
                target_sources(${target} PRIVATE
                    "${CMAKE_BINARY_DIR}/generated/judas-cross-$<CONFIG>.rc")
            else()
                target_link_options(${target} PRIVATE "/MANIFEST:EMBED"
                    "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/packaging/windows/judas.manifest")
            endif()
            add_dependencies(${target} judas_windows_runtime)
        endif()
        if(target MATCHES "_tests$|_performance$|_stress$")
            set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
        endif()
    endforeach()
endfunction()

cmake_language(DEFER CALL judas_finalize_windows_targets)
