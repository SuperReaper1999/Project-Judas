# Optional Linux -> native Windows x64. No vcpkg/MSVC/VM or Wine dependency.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(JUDAS_MINGW_ROOT "" CACHE PATH "MinGW prefix containing bin/ and x86_64-w64-mingw32/")
set(_judas_hints "${JUDAS_MINGW_ROOT}/bin")
find_program(CMAKE_C_COMPILER NAMES x86_64-w64-mingw32-gcc-posix x86_64-w64-mingw32-gcc HINTS ${_judas_hints} REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES x86_64-w64-mingw32-g++-posix x86_64-w64-mingw32-g++ HINTS ${_judas_hints} REQUIRED)
find_program(CMAKE_RC_COMPILER NAMES x86_64-w64-mingw32-windres llvm-windres HINTS ${_judas_hints} REQUIRED)
# windres otherwise searches an unqualified gcc on PATH, which may be the
# Linux compiler when using a relocated or POSIX-suffixed MinGW distribution.
set(CMAKE_RC_FLAGS_INIT "--preprocessor=${CMAKE_C_COMPILER} --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED")
if(JUDAS_MINGW_ROOT)
 set(CMAKE_FIND_ROOT_PATH "${JUDAS_MINGW_ROOT}/x86_64-w64-mingw32")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES JUDAS_MINGW_ROOT)
