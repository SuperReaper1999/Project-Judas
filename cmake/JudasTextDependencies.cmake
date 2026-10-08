# Immutable local source archives: no network access during configure/build/export.
include(FetchContent)
include(ExternalProject)
if(POLICY CMP0135)
 cmake_policy(SET CMP0135 NEW)
endif()
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
FetchContent_Declare(judas_freetype URL "${CMAKE_SOURCE_DIR}/third_party/text/freetype-2.13.3.tar.gz" URL_HASH SHA256=5c3a8e78f7b24c20b25b54ee575d6daa40007a5f4eea2845861c3409b3021747)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(judas_harfbuzz URL "${CMAKE_SOURCE_DIR}/third_party/text/harfbuzz-10.4.0.tar.xz" URL_HASH SHA256=480b6d25014169300669aa1fc39fb356c142d5028324ea52b3a27648b9beaad8)
FetchContent_MakeAvailable(judas_harfbuzz)
FetchContent_MakeAvailable(judas_freetype)
if(WIN32)
 include("${CMAKE_SOURCE_DIR}/cmake/JudasWindowsICU.cmake")
 return()
endif()
set(JUDAS_ICU_PREFIX "${CMAKE_BINARY_DIR}/unicode/icu")
file(MAKE_DIRECTORY "${JUDAS_ICU_PREFIX}/include")
ExternalProject_Add(judas_icu_build
 URL "${CMAKE_SOURCE_DIR}/third_party/text/icu4c-76_1-src.tgz"
 URL_HASH SHA256=dfacb46bfe4747410472ce3e1144bf28a102feeaa4e3875bac9b4c6cf30f4f3e
 PREFIX "${CMAKE_BINARY_DIR}/unicode/build"
 CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CFLAGS=-O2 -fPIC" "CXXFLAGS=-O2 -fPIC" <SOURCE_DIR>/source/runConfigureICU Linux --prefix=${JUDAS_ICU_PREFIX} --disable-shared --enable-static --with-data-packaging=static --disable-tests --disable-samples --disable-extras --disable-icuio
 BUILD_COMMAND make -j4
 INSTALL_COMMAND make install
 BUILD_BYPRODUCTS "${JUDAS_ICU_PREFIX}/lib/libicui18n.a" "${JUDAS_ICU_PREFIX}/lib/libicuuc.a" "${JUDAS_ICU_PREFIX}/lib/libicudata.a"
 LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON)
add_library(judas_unicode INTERFACE)
add_dependencies(judas_unicode judas_icu_build)
target_include_directories(judas_unicode SYSTEM INTERFACE "${JUDAS_ICU_PREFIX}/include")
target_link_libraries(judas_unicode INTERFACE freetype harfbuzz "${JUDAS_ICU_PREFIX}/lib/libicui18n.a" "${JUDAS_ICU_PREFIX}/lib/libicuuc.a" "${JUDAS_ICU_PREFIX}/lib/libicudata.a" ${CMAKE_DL_LIBS})
