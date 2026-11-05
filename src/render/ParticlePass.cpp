#include "ParticlePass.h"
#include "../GraphicsApi.h"
#include "ShaderLibrary.h"
#include "StreamArena.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include <new>
#include <thread>

namespace JudasRender {
namespace {
constexpr char VertexSource[] = R"GLSL(#version 330 core
layout(location=0) in vec4 centerSize;
layout(location=1) in vec4 particleColour;
uniform mat4 vp;
uniform vec3 cameraRight,cameraUp;
out vec2 texcoord;
out vec4 tint;
void main(){
    vec2 corners[4]=vec2[4](vec2(-.5,-.5),vec2(.5,-.5),vec2(.5,.5),vec2(-.5,.5));
    int indices[6]=int[6](0,1,2,0,2,3);
    vec2 corner=corners[indices[gl_VertexID]];
    vec3 world=centerSize.xyz+centerSize.w*(cameraRight*corner.x+cameraUp*corner.y);
    gl_Position=vp*vec4(world,1);
    texcoord=corner+vec2(.5);
    tint=particleColour;
}
)GLSL";
// Identical texture, tint, alpha and colour-space arithmetic to the legacy
// particle fragment shader; only billboard construction moves to the GPU.
constexpr char FragmentSource[] = R"GLSL(#version 330 core
in vec2 texcoord;in vec4 tint;uniform sampler2D image;uniform bool modern,imageLinear;out vec4 result;
vec3 linear(vec3 c){return mix(c/12.92,pow((c+0.055)/1.055,vec3(2.4)),step(vec3(.04045),c));}
void main(){vec4 sampleColour=texture(image,texcoord);result=sampleColour*tint;if(modern)result.rgb=(imageLinear?sampleColour.rgb:linear(sampleColour.rgb))*linear(tint.rgb);}
)GLSL";
struct Instance {
    std::array<float, 4> centerSize;
    std::array<float, 4> colour;
};
static_assert(sizeof(Instance) == ParticlePass::InstanceBytes);
constexpr std::size_t InstancesPerChunk = StreamArena::PageBytes / sizeof(Instance);
static_assert(InstancesPerChunk > 0);

bool Finite(const glm::mat4 &matrix) {
    const auto *values = glm::value_ptr(matrix);
    return std::all_of(values, values + 16, [](float value) { return std::isfinite(value); });
}
bool Finite(const ParticleBillboard &particle) {
    return std::isfinite(particle.position.x) && std::isfinite(particle.position.y) &&
           std::isfinite(particle.position.z) && std::isfinite(particle.size) &&
           std::isfinite(particle.color.r) && std::isfinite(particle.color.g) &&
           std::isfinite(particle.color.b) && std::isfinite(particle.color.a);
}
std::uintptr_t CurrentContext() {
#ifdef __EMSCRIPTEN__
    return std::uintptr_t(emscripten_webgl_get_current_context());
#else
    return reinterpret_cast<std::uintptr_t>(SDL_GL_GetCurrentContext());
#endif
}
int ProfileIndex(Shaders::Profile profile) {
    switch (profile) {
    case Shaders::Profile::Desktop330:
        return 0;
    case Shaders::Profile::Desktop450:
        return 1;
    case Shaders::Profile::ES300:
        return 2;
    }
    return -1;
}
// Match the state restored by the original particle presentation. Program,
// texture/sampler zero and active unit zero remain selected; VAO/array buffer
// return to zero. No framebuffer, viewport or depth-test ownership changes.
class ParticleState {
  public:
    ParticleState() {
        cull = glIsEnabled(GL_CULL_FACE);
        blend = glIsEnabled(GL_BLEND);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &sourceRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &destinationRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &sourceAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &destinationAlpha);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
    }
    ~ParticleState() {
        glDepthMask(depthMask);
        if (cull)
            glEnable(GL_CULL_FACE);
        if (!blend)
            glDisable(GL_BLEND);
        glBlendFuncSeparate(sourceRgb, destinationRgb, sourceAlpha, destinationAlpha);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

  private:
    GLboolean cull = GL_FALSE, blend = GL_FALSE, depthMask = GL_TRUE;
    GLint sourceRgb = 0, destinationRgb = 0, sourceAlpha = 0, destinationAlpha = 0;
};
} // namespace

bool PrepareParticleOrder(const glm::mat4 &view, const std::vector<ParticleBillboard> &particles,
                          std::vector<ParticleDepth> &output, std::string &error) {
    error.clear();
    output.clear();
    if (!Finite(view)) {
        error = "Particle presentation requires a finite view matrix.";
        return false;
    }
    try {
        output.resize(particles.size());
    } catch (const std::bad_alloc &) {
        error = "Cannot allocate particle depth ordering.";
        return false;
    }
    for (std::size_t index = 0; index < particles.size(); ++index) {
        const auto &particle = particles[index];
        // Preserve the original floating-point expression, not a reordered
        // dot product which could change nearly equal depths and alpha order.
        const float depth = (view * glm::vec4(particle.position, 1)).z;
        if (!Finite(particle) || !std::isfinite(depth)) {
            output.clear();
            error = "Particle presentation contains a nonfinite value or camera depth.";
            return false;
        }
        output[index] = {depth, index};
    }
    std::sort(output.begin(), output.end(), [](const auto &a, const auto &b) {
        return a.depth == b.depth ? a.index < b.index : a.depth < b.depth;
    });
    return true;
}

struct ParticlePass::Storage {
    struct Program {
        unsigned handle = 0;
        int vp = -1, right = -1, up = -1, image = -1, imageLinear = -1, sceneLinear = -1;
    };
    std::array<Program, 3> programs;
    unsigned vao = 0;
    std::uintptr_t context = 0;
    std::thread::id thread;
    StreamArena arena;
    std::vector<ParticleDepth> order;
    std::vector<Instance> chunk;
    ParticlePassStats stats;

    bool OwnContext(std::string &error) {
        const auto current = CurrentContext();
        if (!current) {
            error = "Particle presentation requires a current owning graphics context.";
            return false;
        }
        if (context && (current != context || thread != std::this_thread::get_id())) {
            error = "Particle presentation cannot use another context or thread.";
            return false;
        }
        if (!context) {
            context = current;
            thread = std::this_thread::get_id();
        }
        return true;
    }
    bool HasResources() const {
        return vao || arena.AllocatedBytes() ||
               std::any_of(programs.begin(), programs.end(),
                           [](const auto &program) { return program.handle != 0; });
    }
    bool PrepareProgram(Shaders::Profile profile, Program *&result, std::string &error) {
        const int index = ProfileIndex(profile);
        if (index < 0) {
            error = "Particle presentation received an unknown shader profile.";
            return false;
        }
        result = &programs[std::size_t(index)];
        if (!result->handle) {
            unsigned vertex = 0, fragment = 0, linked = 0;
            if (!Shaders::CompileShader(GL_VERTEX_SHADER, VertexSource, vertex, profile,
                                        "Particle billboards")) {
                error = "Particle billboard vertex compilation failed; see shader diagnostics.";
                return false;
            }
            if (!Shaders::CompileShader(GL_FRAGMENT_SHADER, FragmentSource, fragment, profile,
                                        "Particle colour")) {
                glDeleteShader(vertex);
                error = "Particle colour compilation failed; see shader diagnostics.";
                return false;
            }
            const bool success =
                Shaders::LinkProgram(vertex, fragment, linked, "Particle presentation");
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            if (!success) {
                error = "Particle presentation program linking failed; see shader diagnostics.";
                return false;
            }
            result->handle = linked;
            result->vp = glGetUniformLocation(linked, "vp");
            result->right = glGetUniformLocation(linked, "cameraRight");
            result->up = glGetUniformLocation(linked, "cameraUp");
            result->image = glGetUniformLocation(linked, "image");
            result->imageLinear = glGetUniformLocation(linked, "imageLinear");
            result->sceneLinear = glGetUniformLocation(linked, "modern");
        }
        if (!vao) {
            glGenVertexArrays(1, &vao);
            if (!vao) {
                error = "Cannot allocate the particle presentation vertex array.";
                return false;
            }
        }
        return true;
    }
};

ParticlePass::ParticlePass() : storage(std::make_unique<Storage>()) {}
ParticlePass::~ParticlePass() {
    if (storage->HasResources())
        std::fprintf(stderr, "[Graphics] ParticlePass owner omitted Shutdown before context teardown; "
                             "destruction performs no GL calls.\n");
}
bool ParticlePass::Draw(const glm::mat4 &view, const glm::mat4 &projection,
                        const std::vector<ParticleBillboard> &particles, unsigned nativeTexture,
                        bool imageLinear, bool sceneLinear, bool persistent, Shaders::Profile profile,
                        std::string &error) {
    error.clear();
    if (particles.empty())
        return true;
    if (!storage->OwnContext(error))
        return false;
    if (!Finite(projection)) {
        error = "Particle presentation requires a finite projection matrix.";
        return false;
    }
    if (!Finite(view)) {
        error = "Particle presentation requires a finite view matrix.";
        return false;
    }
    const auto inverse = glm::inverse(view);
    if (!Finite(inverse)) {
        error = "Particle presentation requires an invertible view matrix.";
        return false;
    }
    if (!PrepareParticleOrder(view, particles, storage->order, error))
        return false;
    try {
        storage->chunk.reserve(std::min(particles.size(), InstancesPerChunk));
    } catch (const std::bad_alloc &) {
        error = "Cannot allocate bounded particle instance staging.";
        return false;
    }
    Storage::Program *program = nullptr;
    if (!storage->PrepareProgram(profile, program, error))
        return false;
    ParticleState state;
    glUseProgram(program->handle);
    glUniform1i(program->image, 0);
    glUniform1i(program->imageLinear, imageLinear);
    glUniform1i(program->sceneLinear, sceneLinear);
    glUniformMatrix4fv(program->vp, 1, GL_FALSE, glm::value_ptr(projection * view));
    const auto right = glm::vec3(inverse[0]), up = glm::vec3(inverse[1]);
    glUniform3fv(program->right, 1, glm::value_ptr(right));
    glUniform3fv(program->up, 1, glm::value_ptr(up));
    glActiveTexture(GL_TEXTURE0);
    glBindSampler(0, 0);
    glBindTexture(GL_TEXTURE_2D, nativeTexture);
    for (std::size_t first = 0; first < storage->order.size();) {
        const auto count = std::min(InstancesPerChunk, storage->order.size() - first);
        storage->chunk.clear();
        for (std::size_t index = first; index < first + count; ++index) {
            const auto &particle = particles[storage->order[index].index];
            storage->chunk.push_back(
                {{particle.position.x, particle.position.y, particle.position.z, particle.size},
                 {particle.color.r, particle.color.g, particle.color.b, particle.color.a}});
        }
        StreamSlice slice;
        const auto beforeWait = storage->arena.WaitNanoseconds();
        const bool uploaded = storage->arena.Upload(storage->chunk.data(), count * sizeof(Instance),
                                                    persistent, slice, error);
        const auto afterWait = storage->arena.WaitNanoseconds();
        storage->stats.waitNs += afterWait >= beforeWait ? afterWait - beforeWait : afterWait;
        if (!uploaded)
            return false;
        ++storage->stats.uploadCalls;
        storage->stats.uploadedBytes += count * sizeof(Instance);
        glBindVertexArray(storage->vao);
        glBindBuffer(GL_ARRAY_BUFFER, slice.buffer);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                              reinterpret_cast<void *>(slice.offset));
        glVertexAttribDivisor(0, 1);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
                              reinterpret_cast<void *>(slice.offset + offsetof(Instance, colour)));
        glVertexAttribDivisor(1, 1);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, GLsizei(count));
        ++storage->stats.draws;
        storage->stats.triangles += count * 2;
        storage->stats.particles += count;
        first += count;
    }
    return true;
}
bool ParticlePass::Shutdown(std::string *error) {
    std::string local;
    if (error)
        error->clear();
    if (storage->HasResources() && !storage->OwnContext(local)) {
        if (error)
            *error = local;
        else
            std::fprintf(stderr, "[Graphics] %s\n", local.c_str());
        return false;
    }
    if (storage->HasResources())
        storage->arena.Shutdown();
    for (auto &program : storage->programs) {
        if (program.handle)
            glDeleteProgram(program.handle);
        program = {};
    }
    if (storage->vao)
        glDeleteVertexArrays(1, &storage->vao);
    storage->vao = 0;
    storage->context = 0;
    storage->thread = {};
    storage->order.clear();
    storage->chunk.clear();
    return true;
}
ParticlePassStats ParticlePass::Stats() const {
    auto stats = storage->stats;
    stats.arenaBytes = storage->arena.AllocatedBytes();
    return stats;
}
} // namespace JudasRender
