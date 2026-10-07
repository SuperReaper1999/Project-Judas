#pragma once

#include <cstdint>
#include <vector>
#include <memory>
struct SkeletalAsset;

#include <glm/glm.hpp>
#include "Material.h"

// Judas-owned, importer-agnostic CPU-side mesh representation. This is what
// a model importer (src/ModelLoader.h) produces and what Renderer::CreateMesh
// consumes to upload GPU buffers — nothing above this point in the engine
// (Application, gameplay code) ever needs to know an OBJ file or
// tinyobjloader exists; it only ever sees a MeshData. See
// docs/ARCHITECTURE.md, "Milestone 9," for the ownership boundary this
// enforces.
//
// Position/normal/UV/tangent are importer-independent. M46 skin indices and
// weights stay in a separate optional stream; M57 primitive slots refer to
// immutable material definitions without coupling collision geometry to shading.
struct MeshVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
    glm::vec2 uv1{0.0f};
    glm::vec4 tangent{1,0,0,1};

};

struct MeshSkinVertex {glm::uvec4 joints{0};glm::vec4 weights{1,0,0,0};glm::uvec4 joints1{0};glm::vec4 weights1{0};};

struct MeshPrimitive {unsigned first=0,count=0;int material=-1;std::string part;int node=-1;MeshPrimitive(unsigned start=0,unsigned length=0,int slot=-1,std::string key={},int sourceNode=-1):first(start),count(length),material(slot),part(std::move(key)),node(sourceNode){}};
struct ModelSourceLocation {uint32_t node=0,element=0;};
struct MeshData {
    // Import-time provenance, indexed like vertices and triangles respectively.
    // Binary source IDs are not text line numbers.
    std::vector<std::string> sourceNodes;
    std::vector<ModelSourceLocation> vertexLocations,faceLocations;
    std::vector<MaterialDefinition> materials;
    std::vector<std::string> materialKeys;
    std::vector<MeshPrimitive> primitives;
    std::vector<std::string> importWarnings;
    std::string importRecord; // immutable source recipe/digests, used only by authoring/export
    std::vector<MeshSkinVertex> skinVertices;
    std::shared_ptr<const SkeletalAsset> skeletal;
    std::vector<MeshVertex> vertices;
    // Importer position identities, independent of UV/normal seams. Empty means
    // the indexed vertices themselves are the deliberate topology contract.
    std::vector<uint32_t> sourceVertexIds;
    // Empty means "draw non-indexed" (glDrawArrays) — Judas's existing
    // built-in cube/sphere primitives use this; an imported model always
    // populates it (glDrawElements). 32-bit indices: this engine's meshes
    // are all small (a demo's worth of hand-authored/imported geometry),
    // so there's no evidence 16-bit indexing is worth the added complexity
    // of two index-type code paths.
    std::vector<std::uint32_t> indices;
};
