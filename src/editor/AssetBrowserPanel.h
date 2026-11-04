#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include <set>
struct AssetRecord;
class AssetDatabase;
class EditorDocument;
struct EditorPanelState;
struct EditorRequests;
class ResourceManager;

struct EditorAssetThumbnail {
    std::uintptr_t image = 0;
    int width = 0, height = 0;
    explicit operator bool() const {
        return image && width > 0 && height > 0;
    }
};

// Browser demand, not a second resource cache. Pixels and GPU objects stay in
// ResourceManager. The editor retires this owner before replacing project
// assets or shutting down its host; hidden/Play/list views release it as well.
class EditorAssetThumbnailDemand {
  public:
    static constexpr unsigned MaxActive = 16, RequestsPerFrame = 2, MaxPending = 2;
    static constexpr std::uint64_t MaxPinnedBytes = 32ull * 1024 * 1024;
    EditorAssetThumbnailDemand() = default;
    ~EditorAssetThumbnailDemand();
    EditorAssetThumbnailDemand(const EditorAssetThumbnailDemand &) = delete;
    EditorAssetThumbnailDemand &operator=(const EditorAssetThumbnailDemand &) = delete;
    void Begin(ResourceManager *);
    EditorAssetThumbnail Resolve(const AssetRecord &);
    void End();
    void Reset();
    unsigned ActiveCount() const {
        return unsigned(held.size());
    }
    unsigned RequestsStarted() const {
        return requested;
    }
    unsigned PendingCount() const;

  private:
    bool Want(const std::string &, bool sheet = false);
    bool FitsPinBudget(const std::string &) const;
    ResourceManager *resources = nullptr;
    std::set<std::string> held, wanted;
    unsigned requested = 0;
    std::set<std::uintptr_t> frameImages;
    int imageFrame = -1;
};

// Browser-owned navigation state. Stable asset selection is shared with other
// authoring tools; folder/view choices do not belong to the project or scene.
struct EditorAssetBrowserState {
    std::string directory;
    std::string projectRoot;
    bool grid = true;
    bool recursive = false;
    unsigned page = 0; // Browse, Details, Import tools; editor-session state only.
    float tileSize = 112;
    std::shared_ptr<EditorAssetThumbnailDemand> thumbnails;
};
void DrawAssetList(EditorDocument &, EditorPanelState &, EditorRequests &, const AssetDatabase &);
void DrawAssetDetails(EditorDocument &, EditorPanelState &, EditorRequests &, const AssetDatabase &);
void DrawAssetImportTools(EditorDocument &, EditorPanelState &, EditorRequests &, const AssetDatabase &,
                          bool revealModelTools = false);
void DrawUIAssetDocument(const AssetRecord &, EditorPanelState &);

void ResetEditorAssetThumbnails(EditorPanelState &);
void PumpEditorAssetThumbnails(EditorPanelState &);
