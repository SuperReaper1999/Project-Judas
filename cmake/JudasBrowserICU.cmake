# Same pinned Unicode implementation/data. Cross compilation needs native ICU
# data tools; an existing matching build can be reused, never target executables.
if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
 message(FATAL_ERROR "Experimental browser builds currently require a Linux host")
endif()
set(JUDAS_BROWSER_ICU_HOST_TOOLS "" CACHE PATH "Matching ICU 76.1 native build with config/icucross.mk")
set(archive "${CMAKE_SOURCE_DIR}/third_party/text/icu4c-76_1-src.tgz")
set(hash dfacb46bfe4747410472ce3e1144bf28a102feeaa4e3875bac9b4c6cf30f4f3e)
if(JUDAS_BROWSER_ICU_HOST_TOOLS)
 if(NOT EXISTS "${JUDAS_BROWSER_ICU_HOST_TOOLS}/config/icucross.mk")
  message(FATAL_ERROR "Browser ICU native build is incomplete")
 endif()
 set(hostDependency "")
else()
 ExternalProject_Add(judas_browser_icu_host
  URL "${archive}" URL_HASH SHA256=${hash} PREFIX "${CMAKE_BINARY_DIR}/unicode/host"
  CONFIGURE_COMMAND <SOURCE_DIR>/source/runConfigureICU Linux --disable-shared --enable-static --disable-tests --disable-samples --disable-extras --disable-icuio
  BUILD_COMMAND make -j2 INSTALL_COMMAND "" LOG_CONFIGURE ON LOG_BUILD ON)
 set(JUDAS_BROWSER_ICU_HOST_TOOLS "${CMAKE_BINARY_DIR}/unicode/host/src/judas_browser_icu_host-build")
 set(hostDependency judas_browser_icu_host)
endif()
# Static archives use ICU's generic Unix make rules; target/compiler probes
# remain wasm32-unknown-emscripten, not a fake Linux target. No shared ELF flags
# are used by this static-only build.
set(prefix "${CMAKE_BINARY_DIR}/unicode/icu")
file(MAKE_DIRECTORY "${prefix}/include")
ExternalProject_Add(judas_icu_build
 URL "${archive}" URL_HASH SHA256=${hash} PREFIX "${CMAKE_BINARY_DIR}/unicode/build" DEPENDS ${hostDependency}
 CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}"
 "AR=${CMAKE_AR}" "RANLIB=${CMAKE_RANLIB}" "icu_cv_host_frag=mh-linux" "CFLAGS=-O2 -pthread" "CXXFLAGS=-O2 -pthread -fexceptions"
 <SOURCE_DIR>/source/configure --host=wasm32-unknown-emscripten --with-cross-build=${JUDAS_BROWSER_ICU_HOST_TOOLS}
 --prefix=${prefix} --disable-shared --enable-static --with-data-packaging=static --disable-tests --disable-samples --disable-extras --disable-icuio
 BUILD_COMMAND make -j2 "PKGDATA_OPTS=-w -O <BINARY_DIR>/data/icupkg.inc"
 INSTALL_COMMAND make install "PKGDATA_OPTS=-w -O <BINARY_DIR>/data/icupkg.inc"
 BUILD_BYPRODUCTS "${prefix}/lib/libicui18n.a" "${prefix}/lib/libicuuc.a" "${prefix}/lib/libicudata.a"
 LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON)
add_library(judas_unicode INTERFACE)
add_dependencies(judas_unicode judas_icu_build)
target_include_directories(judas_unicode SYSTEM INTERFACE "${prefix}/include")
target_link_libraries(judas_unicode INTERFACE freetype harfbuzz "${prefix}/lib/libicui18n.a" "${prefix}/lib/libicuuc.a" "${prefix}/lib/libicudata.a")
