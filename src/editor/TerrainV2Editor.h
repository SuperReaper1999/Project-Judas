#pragma once
#include "EditorPanels.h"
#include "WorldDebugView.h"
#include "terrain/TerrainBrush.h"
#include "terrain/TerrainSource.h"
#include <map>

// V2 workflow owns source, gestures, worker products and presentation. The
// legacy terrain tool only routes viewport hooks while V1 projects keep their
// existing workflow. No implementation is added to the editor frame loop.
class TerrainV2Editor {
  public:
    bool Panel(EditorDocument &, EditorPanelState &, EditorRequests &);
    void Prepare(Renderer &, const EditorPanelState &);
    bool Stroke(const Scene &, SceneObjectId, glm::vec3, glm::vec3, bool, bool, bool, bool,
                std::string &);
    void Draw(Renderer &, Scene &, ResourceManager &);
    bool Guard(EditorPanelState &);
    void Shutdown(Renderer &, JobSystem *);
    bool Active() const { return activeProject; }
    bool Loaded() const { return loaded; }
    bool Editing() const { return editing; }
    bool Publishing() const { return bool(cook); }
    bool Busy() const { return bool(task) || bool(cook); }
    const std::shared_ptr<MeshData> &Preview() const { return preview; }
    const std::string &MeshId() const { return meshId; }
    const DebugLineList &Footprint() const { return footprint; }

  private:
    struct Task {
        JobHandle job;
        uint64_t revision = 0;
        std::vector<TerrainV2::Chunk> chunks;
        std::string error;
    };
    struct Cook {
        JobHandle job;
        std::string error;
    };
    struct Resident {
        TerrainV2::Chunk chunk;
        MeshHandle mesh;
    };
    std::string projectKey, path, meshId, error, disk;
    TerrainV2::Source source, before;
    std::vector<TerrainV2::Source> undo, redo;
    TerrainV2::SurfaceBrush brush;
    std::map<TerrainV2::ChunkKey, Resident> chunks;
    std::shared_ptr<Task> task;
    std::shared_ptr<Cook> cook;
    std::shared_ptr<MeshData> preview;
    TextureHandle weights;
    DebugLineList footprint;
    glm::vec3 lastDab{0};
    bool activeProject = false, loaded = false, editing = false, dirty = false, gesture = false,
         appearanceDirty = true;
    int mode = 0;
    uint64_t revision = 0;
    bool productsDirty = false, resetPending = false;
    bool Open(EditorPanelState &);
    void Queue(const std::vector<TerrainV2::ChunkKey> &, EditorPanelState &);
    bool Dab(glm::vec3, EditorPanelState &);
    EditorPanelState *frameState = nullptr;
};
