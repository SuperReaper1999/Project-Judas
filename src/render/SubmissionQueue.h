#pragma once
#include "RenderTypes.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace JudasRender {
struct MeshInstance { glm::mat4 model; glm::vec4 tint; };
struct OpaqueBatch {
    MeshHandle mesh;
    TextureHandle texture;
    std::vector<MaterialBinding> bindings;
    bool reversed = false;
    unsigned lod = 0;
    std::vector<MeshInstance> instances;
};
// Owns only pass-scoped compatibility and draw records. It never resolves GPU
// handles or changes scene/resource ownership. Keys compare typed material state
// exactly, without encoding scene JSON in a render loop.
class SubmissionQueue {
  public:
    SubmissionQueue();
    ~SubmissionQueue();
    SubmissionQueue(const SubmissionQueue&) = delete;
    SubmissionQueue& operator=(const SubmissionQueue&) = delete;
    bool Add(MeshHandle, TextureHandle, const glm::mat4&, glm::vec4, unsigned lod,
             bool reversed, const std::vector<MaterialBinding>&);
    // Consecutive-compatible batching for already sorted alpha submissions.
    // Unlike Add, this never groups across an intervening incompatible draw.
    bool AddOrdered(MeshHandle, TextureHandle, const glm::mat4&, glm::vec4, unsigned lod,
                    bool reversed, const std::vector<MaterialBinding>&);
    const std::vector<OpaqueBatch>& Batches() const;
    void Clear();
  private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};
} // namespace JudasRender
