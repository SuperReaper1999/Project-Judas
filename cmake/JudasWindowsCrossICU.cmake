# Same ICU implementation/data as other Judas platforms. ICU's generators run
# on Linux; their output and libraries are built for MinGW, never executed here.
set(JUDAS_CROSS_DEPENDENCY_JOBS "4" CACHE STRING "Maximum parallel compiler jobs in ICU cross/native dependency builds")
if(NOT JUDAS_CROSS_DEPENDENCY_JOBS MATCHES "^[1-9][0-9]*$" OR JUDAS_CROSS_DEPENDENCY_JOBS GREATER 64)
 message(FATAL_ERROR "JUDAS_CROSS_DEPENDENCY_JOBS must be 1-64")
endif()
set(JUDAS_ICU_HOST_TOOLS "" CACHE PATH "Existing ICU 76.1 native build directory (config/icucross.mk)")
set(_icu_archive "${CMAKE_SOURCE_DIR}/third_party/text/icu4c-76_1-src.tgz")
set(_icu_hash dfacb46bfe4747410472ce3e1144bf28a102feeaa4e3875bac9b4c6cf30f4f3e)
if(JUDAS_ICU_HOST_TOOLS)
 if(NOT EXISTS "${JUDAS_ICU_HOST_TOOLS}/config/icucross.mk")
  message(FATAL_ERROR "JUDAS_ICU_HOST_TOOLS must name a built ICU 76.1 native build directory")
 endif()
 set(_icu_host_dependency "")
else()
 ExternalProject_Add(judas_icu_host
  URL "${_icu_archive}" URL_HASH SHA256=${_icu_hash}
  PREFIX "${CMAKE_BINARY_DIR}/unicode/host"
  CONFIGURE_COMMAND <SOURCE_DIR>/source/runConfigureICU Linux --disable-shared --enable-static --disable-tests --disable-samples --disable-extras --disable-icuio
  BUILD_COMMAND make -j${JUDAS_CROSS_DEPENDENCY_JOBS} INSTALL_COMMAND ""
  LOG_CONFIGURE ON LOG_BUILD ON)
 set(JUDAS_ICU_HOST_TOOLS "${CMAKE_BINARY_DIR}/unicode/host/src/judas_icu_host-build")
 set(_icu_host_dependency judas_icu_host)
endif()
set(JUDAS_ICU_PREFIX "${CMAKE_BINARY_DIR}/unicode/icu")
file(MAKE_DIRECTORY "${JUDAS_ICU_PREFIX}/include" "${JUDAS_ICU_PREFIX}/bin")
set(_icu_cc "${CMAKE_C_COMPILER}")
set(_icu_cxx "${CMAKE_CXX_COMPILER}")
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
 string(APPEND _icu_cc " --target=${CMAKE_C_COMPILER_TARGET}")
 string(APPEND _icu_cxx " --target=${CMAKE_CXX_COMPILER_TARGET}")
endif()
ExternalProject_Add(judas_icu_build
 URL "${_icu_archive}" URL_HASH SHA256=${_icu_hash}
 PREFIX "${CMAKE_BINARY_DIR}/unicode/build" DEPENDS ${_icu_host_dependency}
 CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${_icu_cc}" "CXX=${_icu_cxx}"
  "AR=${CMAKE_AR}" "RANLIB=${CMAKE_RANLIB}" "CFLAGS=-O2" "CXXFLAGS=-O2"
  <SOURCE_DIR>/source/configure --host=x86_64-w64-mingw32 --with-cross-build=${JUDAS_ICU_HOST_TOOLS}
  --prefix=${JUDAS_ICU_PREFIX} --disable-shared --enable-static --with-data-packaging=static
  --disable-tests --disable-samples --disable-extras --disable-icuio --disable-tools
 BUILD_COMMAND make -j${JUDAS_CROSS_DEPENDENCY_JOBS}
 # Native pkgdata correctly emits a COFF archive named sicudt.a for MinGW.
 # Upstream installs it under bin while the stub is named lib/libsicudt.a.
 # Publish the actual generated data archive at the explicit static link path.
 INSTALL_COMMAND make install
 COMMAND ${CMAKE_COMMAND} -E copy_if_different <BINARY_DIR>/lib/sicudt.a "${JUDAS_ICU_PREFIX}/lib/libsicudt.a"
 BUILD_BYPRODUCTS "${JUDAS_ICU_PREFIX}/lib/libsicuin.a" "${JUDAS_ICU_PREFIX}/lib/libsicuuc.a" "${JUDAS_ICU_PREFIX}/lib/libsicudt.a"
 LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON)
add_library(judas_unicode INTERFACE)
add_dependencies(judas_unicode judas_icu_build)
target_compile_definitions(judas_unicode INTERFACE U_STATIC_IMPLEMENTATION)
target_include_directories(judas_unicode SYSTEM INTERFACE "${JUDAS_ICU_PREFIX}/include")
target_link_libraries(judas_unicode INTERFACE freetype harfbuzz
 "${JUDAS_ICU_PREFIX}/lib/libsicuin.a" "${JUDAS_ICU_PREFIX}/lib/libsicuuc.a" "${JUDAS_ICU_PREFIX}/lib/libsicudt.a")
