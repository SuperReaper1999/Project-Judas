#pragma once
#include "Material.h"
#include <array>
#include <glm/glm.hpp>

// Opaque handles into Renderer's own GPU resource tables — the same
// convention PhysicsWorld::BodyHandle already established (a small integer
// id, kInvalid sentinel, no concrete type exposed to callers). Deliberately
// not tied to a raw GLuint in any caller-visible way, so no file outside
// Renderer.cpp needs to know what a VAO/VBO/texture object even is.
struct MeshHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;
    unsigned generation=0;
    bool IsValid() const { return id != kInvalidId; }
};
struct RenderTargetHandle {
    unsigned int id = 0xFFFFFFFFu;
    bool IsValid() const { return id != 0xFFFFFFFFu; }
};

struct MaterialHandle {unsigned id=0xFFFFFFFFu,generation=0;bool IsValid()const{return id!=0xFFFFFFFFu;}};
struct EnvironmentHandle {unsigned id=0xFFFFFFFFu;bool IsValid()const{return id!=0xFFFFFFFFu;}};
struct TextureHandle {
    static constexpr unsigned int kInvalidId = 0xFFFFFFFFu;
    unsigned int id = kInvalidId;
    unsigned generation=0;
    bool IsValid() const { return id != kInvalidId; }
};
// Borrowed immutable resources resolved for one render sample. A map replacement
// is active only once Ready; an active invalid handle explicitly removes the map.
// These handles are never serialized or owned by the instance.
struct MaterialBinding {
    MaterialHandle handle;MaterialOverride overrides;bool explicitAsset=false,failed=false;
    std::array<TextureHandle,5> textures{};
    std::array<bool,5> textureOverrides{};
};
