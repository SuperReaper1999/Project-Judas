# Optional wasm32 runtime only. Desktop builds never need Emscripten.
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(judas_sdl URL "${CMAKE_SOURCE_DIR}/third_party/android/SDL2-2.32.10.tar.gz"
    URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(judas_glm URL "${CMAKE_SOURCE_DIR}/third_party/android/glm-1.0.1.tar.gz"
    URL_HASH SHA256=9f3174561fd26904b23f0db5e560971cbf9b3cbda0b280f04d5c379d03bf234c)
FetchContent_MakeAvailable(judas_sdl judas_glm)
set(JUDAS_GLM glm::glm-header-only)
