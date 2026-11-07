#pragma once
#include "EditorPanels.h"
#include "FileBrowser.h"
#include "terrain/TerrainGeneration.h"

// One editor workflow owns recipe inputs, background cooking and the final
// undoable scene insertion. No document or asset database is touched by workers.
class TerrainV2Creation {
  public:
    void Show() { open = true; }
    bool Active() const { return open || bool(work); }
    bool Busy() const { return bool(work); }
    void Panel(EditorDocument &, EditorPanelState &, EditorRequests &);
    void Shutdown(JobSystem *);
    std::string TakeCreatedPath();

  private:
    struct Work {
        JobHandle job;
        std::string name, path, meshId, terrainId, previewId, error;
        float width = 0, depth = 0, halfHeight = 0;
        std::optional<SceneObject> replacedGround;
    };
    std::shared_ptr<Work> work;
    EditorFileBrowser browser;
    bool open = false;
    bool replaceGround = false;
    char name[96] = "Landscape", heightmap[1024] = "";
    int preset = 0, resolution = 1, seed = 7;
    float spacing = 1, base = 0, height = 32;
    std::string error, createdPath;
    void Start(const EditorDocument &, const EditorPanelState &);
};
