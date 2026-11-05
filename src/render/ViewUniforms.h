#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <string_view>

namespace JudasRender {
// Explicit std140 transport layout: column-major mat4 values, then one vec4.
// Plain floats keep packing independent of GLM's optional aligned-type flags.
struct alignas(16) ViewUniformData {
    std::array<float, 16> view{}, projection{};
    std::array<std::array<float, 16>, 9> shadows{};
    std::array<float, 4> camera{};
};
static_assert(offsetof(ViewUniformData, projection) == 64);
static_assert(offsetof(ViewUniformData, shadows) == 128);
static_assert(offsetof(ViewUniformData, camera) == 704);
static_assert(sizeof(ViewUniformData) == 720 && alignof(ViewUniformData) == 16);

struct ViewUniformStats {
    std::uint64_t uploads = 0, bytesUploaded = 0, binds = 0, configurations = 0;
    std::size_t lastUploadBytes = 0;
};

// One owning GL context/thread. Construction and value setters are CPU-only.
// Bind changes UBO binding point zero (and the generic UBO binding), so each
// draw can safely recover from state changes made by another presentation pass.
// Call Shutdown while that context still exists. Destruction never calls GL.
class ViewUniforms {
  public:
    static constexpr unsigned BindingPoint = 0, ShadowCount = 9;
    ViewUniforms();
    ~ViewUniforms();
    ViewUniforms(const ViewUniforms &) = delete;
    ViewUniforms &operator=(const ViewUniforms &) = delete;

    bool SetCamera(const glm::mat4 &view, const glm::mat4 &projection);
    bool SetShadow(unsigned slot, const glm::mat4 &matrix);
    // Configure after a new program link, including a reused numeric handle.
    // Bind configures unseen programs, but cannot infer external program reuse.
    bool Configure(unsigned program, std::string &error);
    void ForgetProgram(unsigned program);
    bool Bind(unsigned program, std::string &error);
    bool Shutdown(std::string *error = nullptr);
    const ViewUniformData &Data() const;
    ViewUniformStats Stats() const;

  private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};

// Closed transformation for the canonical mesh uniform declarations only.
// The caller selects the known mesh VS/FS before invoking it. Canonical shader
// storage/profile preparation remains untouched; unknown layouts fail explicitly.
std::string MakeViewBlockSource(std::string_view canonical, std::string &error);
} // namespace JudasRender
