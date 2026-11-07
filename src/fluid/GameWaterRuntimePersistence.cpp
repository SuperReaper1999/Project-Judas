#include "GameWaterRuntimeInternal.h"
#include "SaveArchive.h"
#include <set>

namespace JudasWater {
void WorldSystem::Persist(SaveArchive &archive) {
    std::uint32_t count = std::uint32_t(regions_.size());
    archive(count);
    archive.Require(count == regions_.size(), "grid-water owner count differs from the scene");
    std::set<std::uint64_t> seen;
    struct Pending {
        std::uint64_t owner;
        Grid grid;
        bool paused;
    };
    std::vector<Pending> pending;
    for (unsigned i = 0; i < count; ++i) {
        auto it = regions_.begin();
        std::advance(it, i);
        auto id = it->first;
        archive(id);
        archive.Require(seen.insert(id).second && regions_.count(id),
                        "unknown or duplicate grid-water owner");
        auto &region = *regions_.at(id);
        bool paused = region.paused;
        archive(paused);
        if (archive.reading) {
            Grid candidate;
            candidate.Persist(archive);
            const auto &before = region.grid.Configuration();
            const auto &after = candidate.Configuration();
            archive.Require(before.cells == after.cells && before.cellSize == after.cellSize &&
                                before.density == after.density &&
                                glm::length(before.origin - after.origin) <
                                    std::max(1e-5f, before.cellSize * 1e-4f) &&
                                std::abs(glm::dot(before.rotation, after.rotation)) > 1.f - 1e-5f,
                            "saved grid-water dimensions/material differ from their "
                            "scene component");
            pending.push_back({id, std::move(candidate), paused});
        } else
            region.grid.Persist(archive);
    }
    if (archive.reading) {
        archive.Finish();
        for (auto &saved : pending) {
            auto &region = *regions_.at(saved.owner);
            region.grid = std::move(saved.grid);
            region.staging = Grid{};
            region.statics.clear();
            region.gravitySignature.clear();
            region.error.clear();
            region.pendingStatics.clear();
            region.terrains.clear();
            region.pendingTerrains.clear();
            region.paused = saved.paused;
            ++region.revision;
        }
        error_.clear();
    }
}
} // namespace JudasWater
