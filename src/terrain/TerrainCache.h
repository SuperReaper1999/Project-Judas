#pragma once
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace TerrainV2 {
struct CacheStatistics {
    size_t bytes = 0, peakBytes = 0, entries = 0, pinnedBytes = 0, budget = 0;
    uint64_t hits = 0, misses = 0, evictions = 0;
    double loadMs = 0, worstLoadMs = 0;
    size_t scratchPeakBytes = 0;
};
// Terrain-only bounded LRU. A borrowed shared_ptr is a lease: an in-use product
// cannot be evicted. Miss loads are serialized independently of cached hits.
// Unpublished worker preparation uses Put and owns its bounded scratch.
template <class T> class PageCache {
  public:
    explicit PageCache(size_t budget) { stats.budget = budget; }
    std::shared_ptr<const T>
    Get(uint64_t key, const std::function<std::pair<std::shared_ptr<const T>, size_t>()> &load) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (auto value = Find(key))
                return value;
        }
        // Serialize misses, not hits. A worker preparing a page must never
        // block consumers that already have a complete cached product.
        std::lock_guard<std::mutex> loading(loadMutex);
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (auto value = Find(key))
                return value;
            ++stats.misses;
        }
        const auto start = std::chrono::steady_clock::now();
        auto product = load();
        const auto ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
        std::vector<std::shared_ptr<const T>> retired;
        std::shared_ptr<const T> result;
        {
            std::lock_guard<std::mutex> lock(mutex);
            stats.loadMs = ms;
            stats.worstLoadMs = std::max(stats.worstLoadMs, ms);
            result = Insert(key, std::move(product.first), product.second, retired);
        }
        return result; // Retired heavy arrays are destroyed outside the mutex.
    }
    // An unpublished worker may prepare its product independently of live
    // query misses. Only the short cache publication/eviction holds the mutex.
    std::shared_ptr<const T> Put(uint64_t key, std::shared_ptr<const T> value, size_t bytes) {
        std::vector<std::shared_ptr<const T>> retired;
        std::shared_ptr<const T> result;
        {
            std::lock_guard<std::mutex> lock(mutex);
            result = Insert(key, std::move(value), bytes, retired);
        }
        return result;
    }
    CacheStatistics Statistics() const {
        std::lock_guard<std::mutex> lock(mutex);
        auto result = stats;
        result.entries = entries.size();
        for (const auto &[_, entry] : entries)
            if (entry.value.use_count() > 1)
                result.pinnedBytes += entry.bytes;
        return result;
    }

  private:
    struct Entry {
        std::shared_ptr<const T> value;
        size_t bytes;
        typename std::list<uint64_t>::iterator position;
    };
    std::shared_ptr<const T> Find(uint64_t key) {
        auto found = entries.find(key);
        if (found == entries.end())
            return {};
        ++stats.hits;
        order.splice(order.end(), order, found->second.position);
        return found->second.value;
    }
    std::shared_ptr<const T> Insert(uint64_t key, std::shared_ptr<const T> value, size_t bytes,
                                    std::vector<std::shared_ptr<const T>> &retired) {
        if (auto existing = Find(key))
            return existing;
        stats.scratchPeakBytes = std::max(stats.scratchPeakBytes, bytes);
        if (!value || bytes > stats.budget)
            throw std::runtime_error("terrain page exceeds its residency budget");
        while (stats.bytes + bytes > stats.budget) {
            bool evicted = false;
            for (auto at = order.begin(); at != order.end(); ++at) {
                auto found = entries.find(*at);
                if (found->second.value.use_count() != 1)
                    continue;
                stats.bytes -= found->second.bytes;
                retired.push_back(std::move(found->second.value));
                entries.erase(found);
                order.erase(at);
                ++stats.evictions;
                evicted = true;
                break;
            }
            if (!evicted)
                throw std::runtime_error("terrain residency budget is pinned by active consumers");
        }
        order.push_back(key);
        entries.emplace(key, Entry{value, bytes, std::prev(order.end())});
        stats.bytes += bytes;
        stats.peakBytes = std::max(stats.peakBytes, stats.bytes);
        return value;
    }
    mutable std::mutex mutex;
    std::mutex loadMutex;
    std::list<uint64_t> order;
    std::map<uint64_t, Entry> entries;
    CacheStatistics stats;
};
} // namespace TerrainV2
