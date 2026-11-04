#pragma once
#include "ResourceManager.h"
#include <memory>
#include <string>
#include <vector>
struct EditorContext;
class RuntimeUI;
class LocalizationSession;

// Render-only asset previews and the authored scene's resource leases.
struct EditorAssetPreview {
    EditorAssetPreview();
    ~EditorAssetPreview();
    void Pump(EditorContext &);
    void Destroy(EditorContext &);
    void DrawTextPreview(EditorContext &);
    void DrawModelImportPreview(EditorContext &, float);
    void DrawUIAuthoringPreview(EditorContext &);
    void RefreshAssetDemand(EditorContext &);
    RenderTargetHandle modelTarget, uiTarget;
    std::unique_ptr<RuntimeUI> ui;
    uint64_t uiRevision = 0;
    std::unique_ptr<LocalizationSession> localization;
    std::string localizationKey, modelAsset;
    std::weak_ptr<const MeshData> modelData;
    glm::vec3 modelMin{0}, modelMax{0};
    std::vector<std::string> heldAssets;
};
