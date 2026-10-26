#pragma once
#include "WorldStreaming.h"
#include <map>

// One editor-owned lease per row. Tokens are process-unique; an old-world token
// never becomes a lease on a replacement world. Gameplay demands stay independent.
class EditorRegionRequests {
public:
    bool Owns(const WorldStreaming& stream, const std::string& region) const {
        const auto found = m_tokens.find(region);
        return found != m_tokens.end() && stream.Status(found->second).has_value();
    }
    uint64_t Request(WorldStreaming& stream, const std::string& region, std::string& error) {
        if (Owns(stream, region)) return m_tokens.at(region);
        const auto token = stream.Request(region, false, error);
        if (token) m_tokens[region] = token;
        return token;
    }
    bool Release(WorldStreaming& stream, const std::string& region) {
        const auto found = m_tokens.find(region);
        if (found == m_tokens.end()) return false;
        const bool released = stream.Release(found->second);
        m_tokens.erase(found);
        return released;
    }
private:
    std::map<std::string, uint64_t> m_tokens;
};
