#ifndef JUDAS_TEST_SDL_MAIN
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#if !defined(_MSC_VER) || !defined(_MT) || !defined(_DLL)
#error Native consumer must use the dynamic MSVC runtime
#endif
static_assert(GLM_VERSION_MAJOR == 1 && GLM_VERSION_MINOR == 0 && GLM_VERSION_PATCH == 1,
              "Expected GLM 1.0.1");
int main(int, char**) {
    SDL_SetMainReady();
    SDL_version version{};
    SDL_GetVersion(&version);
    if(version.major != 2 || version.minor != 30 || version.patch != 12) return 1;
    if(SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS)) return 2;
    const auto matrix = glm::translate(glm::mat4(1.f), glm::vec3(1.f,2.f,3.f));
    const auto position = matrix * glm::vec4(0.f,0.f,0.f,1.f);
    const auto frequency = SDL_GetPerformanceFrequency();
    SDL_Quit();
    if(position != glm::vec4(1.f,2.f,3.f,1.f) || !frequency) return 3;
#ifdef _DEBUG
    std::puts("PASS SDL2 2.30.12 / GLM 1.0.1 / MDd");
#else
    std::puts("PASS SDL2 2.30.12 / GLM 1.0.1 / MD");
#endif
    return 0;
}
