#pragma once

#include "../GraphicsApi.h"
#include <string>
#include <string_view>

// Shader ownership is independent of scene extraction and draw scheduling.
// Canonical sources use the portable GLSL 330 dialect. The selected device
// profile owns version/precision/palette adaptation and compile diagnostics.
namespace JudasRender::Shaders {
enum class Profile { Desktop330, Desktop450, ES300 };
enum class Builtin {
    MeshVertex,
    MeshFragment,
    ShadowVertex,
    ShadowFragment,
    UIVertex,
    UIFragment,
    DebugVertex,
    DebugFragment
};

constexpr Profile PortableProfile() {
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    return Profile::ES300;
#else
    return Profile::Desktop330;
#endif
}

const char *ProfileName(Profile profile);
const char *Source(Builtin shader);
const char *Identity(Builtin shader);

// Aliases retain existing pass call sites while the canonical sources have one
// owner. Prefer Source/Identity for new passes and explicit diagnostic labels.
extern const char *const kVertexShaderSource;
extern const char *const kFragmentShaderSource;
extern const char *const kShadowVertexShaderSource;
extern const char *const kShadowFragmentShaderSource;
extern const char *const kUIVertexShaderSource;
extern const char *const kUIFragmentShaderSource;
// Shared scene-linear -> display conversion, used at main and UI boundaries.
const char *DisplayTransformSource();
extern const char *const kDebugVertexShaderSource;
extern const char *const kDebugFragmentShaderSource;

// CPU-only preparation. Failure preserves the previously prepared output.
// Sources declaring a newer/native profile are never silently downgraded.
bool PrepareSource(std::string_view canonical, Profile profile, std::string &prepared,
                   std::string &error);

// Requires the owning graphics context. Outputs are published only on success;
// failed shader/program objects are destroyed before returning. Identity is a
// stable pass/source label; built-in aliases infer their names when omitted.
bool CompileShader(GLenum type, const char *source, GLuint &shader, Profile profile = PortableProfile(),
                   const char *identity = nullptr);
bool LinkProgram(GLuint vertex, GLuint fragment, GLuint &program, const char *identity = nullptr);
bool LinkComputeProgram(GLuint compute, GLuint &program, const char *identity = nullptr);
} // namespace JudasRender::Shaders
