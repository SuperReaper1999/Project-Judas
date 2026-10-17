# Native SDK includes executables + engine-owned resources/notices. Games get only
# their required runtime files through the ordinary project exporter.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
if(NOT CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
 message(FATAL_ERROR "Visual C++ redistribution files were not found; install VS2022 C++ redist components")
endif()
set(JUDAS_WINDOWS_DLLS ${JUDAS_WINDOWS_ICU_DLLS} ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
set(judas_dll_manifest "")
foreach(dll IN LISTS JUDAS_WINDOWS_DLLS)
 get_filename_component(name "${dll}" NAME)
 string(APPEND judas_dll_manifest "${name}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/required-dlls.txt" "${judas_dll_manifest}")
set(judas_sdl_license "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/sdl2/copyright")
set(judas_glm_license "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/glm/copyright")
# All these executables share a configuration output directory. Stage the common
# runtime once before linking, so parallel post-build copies cannot race.
# CMP0112 NEW ensures TARGET_FILE_DIR does not introduce a dependency on judas.
cmake_policy(SET CMP0112 NEW)
add_custom_target(judas_windows_runtime
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:judas>" "$<TARGET_FILE_DIR:judas>/windows-runtime" "$<TARGET_FILE_DIR:judas>/windows-licenses"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${JUDAS_WINDOWS_DLLS} "$<TARGET_FILE_DIR:judas>"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${JUDAS_WINDOWS_DLLS} "$<TARGET_FILE_DIR:judas>/windows-runtime"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/required-dlls.txt" "$<TARGET_FILE_DIR:judas>/windows-runtime/required-dlls.txt"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${judas_sdl_license}" "$<TARGET_FILE_DIR:judas>/windows-licenses/SDL2.txt"
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${judas_glm_license}" "$<TARGET_FILE_DIR:judas>/windows-licenses/GLM.txt"
 DEPENDS judas_icu_build
 VERBATIM)
# Linux-oriented historical harnesses remain available as explicit targets, not
# prerequisites of the native editor/game SDK. No historical test is rewritten.
get_property(judas_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS judas_targets)
 get_target_property(judas_target_type ${target} TYPE)
 if(judas_target_type STREQUAL "EXECUTABLE")
  target_link_options(${target} PRIVATE "/MANIFEST:EMBED" "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/packaging/windows/judas.manifest")
  add_dependencies(${target} judas_windows_runtime)
 endif()
 if(target MATCHES "_tests$|_performance$|_stress$")
  set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
 endif()
endforeach()
