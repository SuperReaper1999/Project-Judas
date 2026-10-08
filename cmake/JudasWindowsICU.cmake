# Same immutable ICU 76.1 archive/data as Linux; upstream Visual Studio projects.
# Build native DLLs instead of attempting Unix configure/make on Windows.
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
 message(FATAL_ERROR "Windows candidate requires the Visual Studio 2022 x64 toolchain")
endif()
set(JUDAS_ICU_SOURCE "${CMAKE_BINARY_DIR}/unicode/windows-icu")
file(MAKE_DIRECTORY "${JUDAS_ICU_SOURCE}/include")
ExternalProject_Add(judas_icu_build
 URL "${CMAKE_SOURCE_DIR}/third_party/text/icu4c-76_1-src.tgz"
 URL_HASH SHA256=dfacb46bfe4747410472ce3e1144bf28a102feeaa4e3875bac9b4c6cf30f4f3e
 SOURCE_DIR "${JUDAS_ICU_SOURCE}"
 PREFIX "${CMAKE_BINARY_DIR}/unicode/build"
 CONFIGURE_COMMAND ""
 BUILD_COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" <SOURCE_DIR>/source/allinone/allinone.sln /m:4 /t:makedata /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:WindowsTargetPlatformVersion=10.0
 INSTALL_COMMAND ""
 BUILD_BYPRODUCTS "${JUDAS_ICU_SOURCE}/lib64/icuuc.lib" "${JUDAS_ICU_SOURCE}/lib64/icuin.lib" "${JUDAS_ICU_SOURCE}/lib64/icudt.lib"
 "${JUDAS_ICU_SOURCE}/bin64/icuuc76.dll" "${JUDAS_ICU_SOURCE}/bin64/icuin76.dll" "${JUDAS_ICU_SOURCE}/bin64/icudt76.dll"
 LOG_BUILD ON)
add_library(judas_unicode INTERFACE)
add_dependencies(judas_unicode judas_icu_build)
target_include_directories(judas_unicode SYSTEM INTERFACE "${JUDAS_ICU_SOURCE}/include")
target_link_libraries(judas_unicode INTERFACE freetype harfbuzz "${JUDAS_ICU_SOURCE}/lib64/icuin.lib" "${JUDAS_ICU_SOURCE}/lib64/icuuc.lib" "${JUDAS_ICU_SOURCE}/lib64/icudt.lib")
set(JUDAS_WINDOWS_ICU_DLLS "${JUDAS_ICU_SOURCE}/bin64/icuuc76.dll" "${JUDAS_ICU_SOURCE}/bin64/icuin76.dll" "${JUDAS_ICU_SOURCE}/bin64/icudt76.dll")
