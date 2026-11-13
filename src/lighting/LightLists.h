#pragma once
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>
struct RenderLightGrid;
struct GraphicsCapabilities;
namespace JudasLighting {
// Context-owned transport/optional deterministic GPU tile construction. Scene
// lights and relevance selection are borrowed, never separately authoritative.
class LightLists {
public:
    LightLists();~LightLists();
    bool Upload(const RenderLightGrid&,bool gpuBins,const GraphicsCapabilities&,std::string& error);
    bool Bind(std::string& error);
    // Engine handle of the current light-list buffer (0 when not ready).
    unsigned GraphicsBuffer();
    bool ReadForDiagnostics(std::vector<glm::vec4>&,std::string& error);
    bool Shutdown(std::string& error);
    std::size_t AllocatedBytes()const;
private:struct Storage;std::unique_ptr<Storage> state;
};
}
