# MinGW-only staging; the MSVC SDK and its redistribution contract stay separate.
enable_language(RC)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/generated/judas-cross-$<CONFIG>.rc" CONTENT
 "1 24 \"${CMAKE_SOURCE_DIR}/packaging/windows/judas.manifest\"\n101 10 \"${CMAKE_BINARY_DIR}/judas-build-info-$<CONFIG>.json\"\n")
# Resource compilers do not consistently discover JSON includes. Rebuild the
# resource when configured identity changes, not only when its pathname changes.
set(_cross_configs ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
foreach(config IN LISTS _cross_configs)
 set_source_files_properties("${CMAKE_BINARY_DIR}/generated/judas-cross-${config}.rc"
  PROPERTIES OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/generated/judas-build-info.json.in;${CMAKE_SOURCE_DIR}/packaging/windows/judas.manifest")
endforeach()
get_filename_component(_cross_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(JUDAS_WINDOWS_EXTRA_DLLS "" CACHE STRING "Additional explicit MinGW runtime DLL paths")
set(_cross_dlls ${JUDAS_WINDOWS_EXTRA_DLLS})
foreach(name libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll libc++.dll libunwind.dll)
 execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "-print-file-name=${name}" OUTPUT_VARIABLE path OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
 if(EXISTS "${path}")
  list(APPEND _cross_dlls "${path}")
 elseif(EXISTS "${_cross_bin}/${name}")
  list(APPEND _cross_dlls "${_cross_bin}/${name}")
 elseif(EXISTS "${_cross_bin}/../x86_64-w64-mingw32/lib/${name}")
  list(APPEND _cross_dlls "${_cross_bin}/../x86_64-w64-mingw32/lib/${name}")
 endif()
endforeach()
list(REMOVE_DUPLICATES _cross_dlls)
set(_dll_names "")
set(_cross_copy COMMAND ${CMAKE_COMMAND} -E true)
if(_cross_dlls)
 set(_cross_copy COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_cross_dlls} "$<TARGET_FILE_DIR:judas>"
                 COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_cross_dlls} "$<TARGET_FILE_DIR:judas>/windows-runtime")
 foreach(dll IN LISTS _cross_dlls)
  get_filename_component(name "${dll}" NAME)
  string(APPEND _dll_names "${name}\n")
 endforeach()
endif()
file(WRITE "${CMAKE_BINARY_DIR}/required-dlls.txt" "${_dll_names}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/cross-licenses")
configure_file("${judas_sdl_SOURCE_DIR}/LICENSE.txt" "${CMAKE_BINARY_DIR}/cross-licenses/SDL2.txt" COPYONLY)
configure_file("${judas_glm_SOURCE_DIR}/copying.txt" "${CMAKE_BINARY_DIR}/cross-licenses/GLM.txt" COPYONLY)
# Preserve the selected toolchain's own redistribution notices. The Python
# bundle check rejects missing notices; it does not invent license statements.
foreach(pair "gcc-mingw-w64-base|GNU-runtime.txt" "mingw-w64-common|MinGW-w64.txt")
 string(REPLACE "|" ";" fields "${pair}")
 list(GET fields 0 package)
 list(GET fields 1 output)
 if(EXISTS "${_cross_bin}/../share/doc/${package}/copyright")
  configure_file("${_cross_bin}/../share/doc/${package}/copyright" "${CMAKE_BINARY_DIR}/cross-licenses/${output}" COPYONLY)
 endif()
endforeach()
add_custom_target(judas_windows_runtime
 COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:judas>/windows-runtime" "$<TARGET_FILE_DIR:judas>/windows-licenses"
 ${_cross_copy}
 COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/required-dlls.txt" "$<TARGET_FILE_DIR:judas>/windows-runtime/required-dlls.txt"
 COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_BINARY_DIR}/cross-licenses" "$<TARGET_FILE_DIR:judas>/windows-licenses"
 VERBATIM)
get_property(_cross_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS _cross_targets)
 get_target_property(kind ${target} TYPE)
 if(kind STREQUAL "EXECUTABLE")
  target_sources(${target} PRIVATE "${CMAKE_BINARY_DIR}/generated/judas-cross-$<CONFIG>.rc")
  add_dependencies(${target} judas_windows_runtime)
 endif()
 if(target MATCHES "_tests$|_performance$|_stress$")
  set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
 endif()
endforeach()
