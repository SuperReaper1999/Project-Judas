#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace TerrainV2 {
struct GeometryPage;
struct TemporaryDirectory;
struct GeometrySpillState {
    mutable std::mutex mutex;
    size_t bytes = 0;
    static constexpr size_t budget = 512 * 1024 * 1024;
    std::shared_ptr<TemporaryDirectory> directory;
};
// Private, same-process derived data, NOT a game save or public asset format.
// Each immutable blob owns its own file; retiring one recipe removes that file
// even when neighbouring recipes still share the directory and disk quota.
class GeometrySpill {
  public:
    ~GeometrySpill();
    static std::shared_ptr<const GeometrySpill> Store(const GeometryPage &, uint64_t,
                                                      std::shared_ptr<GeometrySpillState>);
    std::shared_ptr<const GeometryPage> Load() const;
    const std::string &Path() const { return path; }

  private:
    std::shared_ptr<GeometrySpillState> state;
    std::shared_ptr<const GeometryPage> header;
    size_t counts[6]{};
    std::string path;
    size_t bytes = 0;
    uint32_t checksum = 0;
};
} // namespace TerrainV2
