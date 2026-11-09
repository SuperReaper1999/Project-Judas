#pragma once
#include "authoring/TerrainEditorPreview.h"
#include "terrain/TerrainRenderPages.h"
#include <optional>

// GPU presentation of an immutable editor worker product. Picking and collision
// retain full geometry; only this view uses the runtime's error-bounded LOD.
class TerrainV2PreviewRenderer {
  public:
    void Publish(Renderer &, std::shared_ptr<const TerrainV2::EditorPreview>);
    void Draw(Renderer &, glm::vec3 position, glm::quat rotation, glm::vec3 colour,
              std::optional<TerrainV2::Edit> brush = {});
    void Clear(Renderer &);
    auto Statistics() const { return pages.Statistics(); }
  private:
    std::shared_ptr<const TerrainV2::EditorPreview> preview;
    TerrainV2::RenderPages pages{32 * 1024 * 1024, false};
};
