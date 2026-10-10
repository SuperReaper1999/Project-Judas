#pragma once

#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <string>

#include "MeshData.h"
#include "Scene.h"

class AssetDatabase;
class ResourceManager;

struct EditorPickHit {
    SceneObjectId id = kInvalidSceneObjectId;
    float distance = std::numeric_limits<float>::infinity();
    bool visual = false;
};

// Editor-only CPU picking of the same flattened authoring geometry submitted
// by DrawAuthoredScene. Mesh files are decoded on demand when clicked, never
// every frame; no renderer readback or physics query is involved.
class EditorScenePicker {
public:
    EditorPickHit Pick(const Scene& authored, glm::vec3 origin, glm::vec3 direction,
                       const AssetDatabase* assets = nullptr,
                       const ResourceManager* resources = nullptr,
                       std::string* error = nullptr);
    void Clear();
    void SetDraftMesh(const std::string& id,std::shared_ptr<const MeshData> mesh){m_draftId=id;m_draft=std::move(mesh); }

private:
    std::string m_draftId;std::shared_ptr<const MeshData> m_draft;
    struct CachedMesh {
        std::string path;
        std::filesystem::file_time_type modified{};
        std::uintmax_t bytes = 0;
        std::shared_ptr<MeshData> mesh;
        std::vector<bool> mirroredParts;
    };
    const MeshData* AssetMesh(const std::string& id, bool collision, const AssetDatabase* assets);
    const MeshData* TerrainMesh(const std::string& identifier);
    std::map<std::string, CachedMesh> m_meshes;
    std::map<std::string, MeshData> m_terrains;
};
