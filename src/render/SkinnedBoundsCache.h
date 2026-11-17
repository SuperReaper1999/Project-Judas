#pragma once
#include "../Visibility.h"
#include <cstring>
#include <unordered_map>
#include <vector>

namespace JudasRender {
// CPU presentation products only. A palette address is a lookup hint, never an
// identity: animation may mutate it or an allocator may reuse it between draws.
class SkinnedBoundsCache {
public:
    static constexpr size_t MaxEntries = 256, MaxPoseBytes = 4 * 1024 * 1024;
    void Clear() { m_entries.clear(); m_bytes = 0; }
    size_t Entries() const { return m_entries.size(); }
    size_t PoseBytes() const { return m_bytes; }
    VisualBounds Resolve(const VisualBounds& source, const std::vector<glm::mat4>& pose) {
        if (pose.empty()) return source;
        auto found = m_entries.find(&pose);
        if (found != m_entries.end()) {
            const auto& entry = found->second;
            if (entry.source.min == source.min && entry.source.max == source.max &&
                entry.pose.size() == pose.size() &&
                std::memcmp(entry.pose.data(), pose.data(), pose.size()*sizeof(glm::mat4)) == 0)
                return entry.bounds;
            m_bytes -= entry.pose.size()*sizeof(glm::mat4);
            m_entries.erase(found);
        }
        VisualBounds bounds;
        for (const auto& matrix : pose) {
            const auto transformed = TransformBounds(source, matrix);
            bounds.Include(transformed.min); bounds.Include(transformed.max);
        }
        const size_t bytes = pose.size()*sizeof(glm::mat4);
        if (bytes <= MaxPoseBytes) {
            if (m_entries.size() >= MaxEntries || m_bytes + bytes > MaxPoseBytes) Clear();
            m_entries.emplace(&pose, Entry{source, bounds, pose}); m_bytes += bytes;
        }
        return bounds;
    }
private:
    struct Entry { VisualBounds source, bounds; std::vector<glm::mat4> pose; };
    std::unordered_map<const void*, Entry> m_entries;
    size_t m_bytes = 0;
};
}
