# ICU needs host tools to generate its portable static Unicode data. Never link
# Android's private platform ICU ABI; ship the same pinned Unicode implementation.
set(JUDAS_ICU_ARCHIVE "${CMAKE_SOURCE_DIR}/third_party/text/icu4c-76_1-src.tgz")
set(JUDAS_ICU_HASH dfacb46bfe4747410472ce3e1144bf28a102feeaa4e3875bac9b4c6cf30f4f3e)
set(JUDAS_ICU_HOST "${CMAKE_BINARY_DIR}/unicode/host")
set(JUDAS_ICU_HOST_TOOLS "" CACHE PATH "Existing matching ICU host build, reused across Android ABIs")
if(JUDAS_ICU_HOST_TOOLS)
 if(NOT EXISTS "${JUDAS_ICU_HOST_TOOLS}/config/icucross.mk")
  message(FATAL_ERROR "ICU host tools have not been built")
 endif()
 set(JUDAS_ICU_HOST_DEPENDENCY "")
else()
ExternalProject_Add(judas_icu_host
 URL "${JUDAS_ICU_ARCHIVE}" URL_HASH SHA256=${JUDAS_ICU_HASH}
 PREFIX "${JUDAS_ICU_HOST}"
 CONFIGURE_COMMAND <SOURCE_DIR>/source/runConfigureICU Linux --disable-shared --enable-static --disable-tests --disable-samples --disable-extras --disable-icuio
 BUILD_COMMAND make -j4 INSTALL_COMMAND ""
 LOG_CONFIGURE ON LOG_BUILD ON)
 set(JUDAS_ICU_HOST_TOOLS "${JUDAS_ICU_HOST}/src/judas_icu_host-build")
 set(JUDAS_ICU_HOST_DEPENDENCY judas_icu_host)
endif()
set(JUDAS_ICU_PREFIX "${CMAKE_BINARY_DIR}/unicode/icu")
file(MAKE_DIRECTORY "${JUDAS_ICU_PREFIX}/include")
if(ANDROID_ABI STREQUAL "arm64-v8a")
 set(JUDAS_ICU_TRIPLE aarch64-linux-android)
elseif(ANDROID_ABI STREQUAL "x86_64")
 set(JUDAS_ICU_TRIPLE x86_64-linux-android)
else()
 message(FATAL_ERROR "Android currently supports arm64-v8a and x86_64")
endif()
ExternalProject_Add(judas_icu_build
 URL "${JUDAS_ICU_ARCHIVE}" URL_HASH SHA256=${JUDAS_ICU_HASH}
 PREFIX "${CMAKE_BINARY_DIR}/unicode/build" DEPENDS ${JUDAS_ICU_HOST_DEPENDENCY}
 CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER} --target=${CMAKE_C_COMPILER_TARGET}"
 "CXX=${CMAKE_CXX_COMPILER} --target=${CMAKE_CXX_COMPILER_TARGET}" "AR=${CMAKE_AR}" "RANLIB=${CMAKE_RANLIB}"
 "CFLAGS=-O2 -fPIC" "CXXFLAGS=-O2 -fPIC" <SOURCE_DIR>/source/configure
 --host=${JUDAS_ICU_TRIPLE} --with-cross-build=${JUDAS_ICU_HOST_TOOLS}
 --prefix=${JUDAS_ICU_PREFIX} --disable-shared --enable-static --with-data-packaging=static
 --disable-tests --disable-samples --disable-extras --disable-icuio
 BUILD_COMMAND make -j4 INSTALL_COMMAND make install
 BUILD_BYPRODUCTS "${JUDAS_ICU_PREFIX}/lib/libicui18n.a" "${JUDAS_ICU_PREFIX}/lib/libicuuc.a" "${JUDAS_ICU_PREFIX}/lib/libicudata.a"
 LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON)
add_library(judas_unicode INTERFACE)
add_dependencies(judas_unicode judas_icu_build)
target_include_directories(judas_unicode SYSTEM INTERFACE "${JUDAS_ICU_PREFIX}/include")
target_link_libraries(judas_unicode INTERFACE freetype harfbuzz "${JUDAS_ICU_PREFIX}/lib/libicui18n.a" "${JUDAS_ICU_PREFIX}/lib/libicuuc.a" "${JUDAS_ICU_PREFIX}/lib/libicudata.a" dl)
