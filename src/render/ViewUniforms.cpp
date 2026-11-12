#include "ViewUniforms.h"
#include "../GraphicsApi.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <thread>
#include <vector>

namespace JudasRender {
namespace {
constexpr char BlockName[] = "JudasView";
constexpr char BlockSource[] = R"GLSL(layout(std140) uniform JudasView {
    mat4 uView;
    mat4 uProjection;
    mat4 uLightSpaceMatrix[54];
    vec4 judasViewCamera;
};
#define uCameraPosition judasViewCamera.xyz
)GLSL";

std::uintptr_t CurrentContext() {
    return GraphicsContextIdentity();
}
bool Finite(const glm::mat4 &matrix) {
    const auto *values = glm::value_ptr(matrix);
    return std::all_of(values, values + 16, [](float value) { return std::isfinite(value); });
}
std::array<float, 16> MatrixValues(const glm::mat4 &matrix) {
    std::array<float, 16> values;
    std::memcpy(values.data(), glm::value_ptr(matrix), sizeof(values));
    return values;
}
bool ExactlyOnce(std::string_view source, std::string_view token) {
    const auto position = source.find(token);
    return position != std::string_view::npos &&
           source.find(token, position + token.size()) == std::string_view::npos;
}
} // namespace

struct ViewUniforms::Storage {
    ViewUniformData values;
    ViewUniformStats stats;
    unsigned buffer = 0;
    std::uintptr_t context = 0;
    std::thread::id thread;
    std::vector<unsigned> programs;
    bool dirty = true;

    Storage() {
        values.view = values.projection = MatrixValues(glm::mat4(1));
        for (auto &shadow : values.shadows)
            shadow = values.view;
        values.camera = {0, 0, 0, 1};
    }
    bool OwnContext(std::string &error) {
        const auto current = CurrentContext();
        if (!current) {
            error = "View uniforms require a current owning graphics context.";
            return false;
        }
        if (context && (context != current || thread != std::this_thread::get_id())) {
            error = "View uniforms cannot be used from another context or thread.";
            return false;
        }
        if (!context) {
            context = current;
            thread = std::this_thread::get_id();
        }
        return true;
    }
};

ViewUniforms::ViewUniforms() : storage(std::make_unique<Storage>()) {}
ViewUniforms::~ViewUniforms() {
    if (storage->buffer)
        std::fprintf(stderr, "[Graphics] ViewUniforms owner omitted Shutdown before context teardown; "
                             "destruction performs no GL calls.\n");
}
bool ViewUniforms::SetCamera(const glm::mat4 &view, const glm::mat4 &projection) {
    if (!Finite(view) || !Finite(projection))
        return false;
    const auto inverse = glm::inverse(view);
    if (!Finite(inverse))
        return false;
    auto candidate = storage->values;
    candidate.view = MatrixValues(view);
    candidate.projection = MatrixValues(projection);
    candidate.camera = {inverse[3].x, inverse[3].y, inverse[3].z, 1};
    if (std::memcmp(&candidate, &storage->values, sizeof(candidate)) != 0) {
        storage->values = candidate;
        storage->dirty = true;
    }
    return true;
}
bool ViewUniforms::SetShadow(unsigned slot, const glm::mat4 &matrix) {
    if (slot >= ShadowCount || !Finite(matrix))
        return false;
    auto values = MatrixValues(matrix);
    if (std::memcmp(values.data(), storage->values.shadows[slot].data(), sizeof(values)) != 0) {
        storage->values.shadows[slot] = values;
        storage->dirty = true;
    }
    return true;
}
bool ViewUniforms::Configure(unsigned program, std::string &error) {
    error.clear();
    if (!storage->OwnContext(error))
        return false;
    if (!program || !glIsProgram(program)) {
        error = "View uniforms cannot configure an invalid shader program.";
        return false;
    }
    const auto index = glGetUniformBlockIndex(program, BlockName);
    if (index == GL_INVALID_INDEX) {
        error = "Shader program does not contain the canonical JudasView uniform block.";
        return false;
    }
    GLint bytes = 0;
    glGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE, &bytes);
    if (bytes != GLint(sizeof(ViewUniformData))) {
        error = "JudasView shader block must use the canonical 3600-byte std140 layout; observed " +
                std::to_string(bytes) + " bytes.";
        return false;
    }
    glUniformBlockBinding(program, index, BindingPoint);
    if (std::find(storage->programs.begin(), storage->programs.end(), program) ==
        storage->programs.end())
        storage->programs.push_back(program);
    ++storage->stats.configurations;
    return true;
}
void ViewUniforms::ForgetProgram(unsigned program) {
    auto &programs = storage->programs;
    programs.erase(std::remove(programs.begin(), programs.end(), program), programs.end());
}
bool ViewUniforms::Bind(unsigned program, std::string &error) {
    error.clear();
    if (!storage->OwnContext(error))
        return false;
    storage->stats.lastUploadBytes = 0;
    if (std::find(storage->programs.begin(), storage->programs.end(), program) ==
            storage->programs.end() &&
        !Configure(program, error))
        return false;
    if (!storage->buffer) {
        glGenBuffers(1, &storage->buffer);
        if (!storage->buffer) {
            error = "Cannot create the view uniform buffer.";
            return false;
        }
        glBindBuffer(GL_UNIFORM_BUFFER, storage->buffer);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(ViewUniformData), nullptr, GL_DYNAMIC_DRAW);
        GLint bytes = 0;
        glGetBufferParameteriv(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &bytes);
        if (bytes != GLint(sizeof(ViewUniformData))) {
            glDeleteBuffers(1, &storage->buffer);
            storage->buffer = 0;
            error = "Cannot allocate the 3600-byte view uniform buffer.";
            return false;
        }
        storage->dirty = true;
    }
    if (storage->dirty) {
        glBindBuffer(GL_UNIFORM_BUFFER, storage->buffer);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ViewUniformData), &storage->values);
        storage->dirty = false;
        ++storage->stats.uploads;
        storage->stats.bytesUploaded += sizeof(ViewUniformData);
        storage->stats.lastUploadBytes = sizeof(ViewUniformData);
    }
    // ImGui/another pass may replace the indexed binding even when our data did
    // not change. Rebind every consumer; never confuse dirty data with GL state.
    glBindBufferBase(GL_UNIFORM_BUFFER, BindingPoint, storage->buffer);
    ++storage->stats.binds;
    return true;
}
bool ViewUniforms::Shutdown(std::string *error) {
    std::string local;
    if (error)
        error->clear();
    if (storage->buffer && !storage->OwnContext(local)) {
        if (error)
            *error = local;
        else
            std::fprintf(stderr, "[Graphics] %s\n", local.c_str());
        return false;
    }
    if (storage->buffer)
        glDeleteBuffers(1, &storage->buffer);
    storage->buffer = 0;
    storage->programs.clear();
    storage->context = 0;
    storage->thread = {};
    storage->dirty = true;
    return true;
}
const ViewUniformData &ViewUniforms::Data() const { return storage->values; }
ViewUniformStats ViewUniforms::Stats() const { return storage->stats; }

std::string MakeViewBlockSource(std::string_view canonical, std::string &error) {
    error.clear();
    constexpr std::string_view cameraMatrices = "uniform mat4 uView;\nuniform mat4 uProjection;\n";
    constexpr std::string_view shadows = "uniform mat4 uLightSpaceMatrix[54];\n";
    constexpr std::string_view camera = "uniform vec3 uEmission,uCameraPosition;\n";
    const bool vertex = ExactlyOnce(canonical, cameraMatrices) &&
                        canonical.find(shadows) == std::string_view::npos &&
                        canonical.find(camera) == std::string_view::npos;
    const bool fragment = ExactlyOnce(canonical, shadows) && ExactlyOnce(canonical, camera) &&
                          canonical.find(cameraMatrices) == std::string_view::npos;
    if ((!vertex && !fragment) || canonical.find("JudasView") != std::string_view::npos) {
        error = "View block preparation requires the exact canonical mesh vertex or fragment uniform "
                "declarations.";
        return {};
    }
    std::string result(canonical);
    if (vertex)
        result.replace(result.find(cameraMatrices), cameraMatrices.size(), BlockSource);
    else {
        result.replace(result.find(camera), camera.size(), "uniform vec3 uEmission;\n");
        result.replace(result.find(shadows), shadows.size(), BlockSource);
    }
    return result;
}
} // namespace JudasRender
