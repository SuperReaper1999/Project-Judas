#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace JudasPhysicsDetail {

// Step-local chronological frontier. Only the endpoint trajectories own an
// event's dependencies. Updating either endpoint invalidates that event, not
// unrelated islands. Heap updates preserve exact time/key ordering; no batching
// window, time quantization or stale queue entries are used.
template <class Event> class ImpactEventSchedule {
  public:
    using Key = std::array<unsigned, 4>;
    explicit ImpactEventSchedule(std::vector<Event>& storage, std::size_t bodies)
        : events(storage), incident(bodies) { events.clear(); }

    bool Contains(const Key& key) const { return index.find(key) != index.end(); }
    void Set(const Key& key, double time) {
        const auto inserted = index.emplace(key, events.size());
        const auto id = inserted.first->second;
        if (inserted.second) {
            events.push_back({key, time});
            positions.push_back(heap.size());
            heap.push_back(id);
            incident[key[0]].push_back(id);
            if (key[1] != key[0]) incident[key[1]].push_back(id);
            Up(heap.size() - 1);
        } else {
            events[id].time = time;
            const auto at = positions[id];
            Up(at);
            Down(positions[id]);
        }
    }
    const Event* First() const { return heap.empty() ? nullptr : &events[heap.front()]; }
    template <class Callback> void ForIncident(unsigned body, double finish, Callback&& visit) const {
        for (auto id : incident[body])
            if (events[id].time <= finish) visit(events[id].key);
    }

  private:
    struct Hash {
        std::size_t operator()(const Key& key) const noexcept {
            std::uint64_t value = (std::uint64_t(key[0]) << 32) | key[1];
            value += ((std::uint64_t(key[2]) << 32) | key[3]) * 0x9e3779b97f4a7c15ULL;
            value ^= value >> 30; value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27; value *= 0x94d049bb133111ebULL;
            return std::size_t(value ^ (value >> 31));
        }
    };
    bool Before(std::size_t a, std::size_t b) const {
        return events[a].time < events[b].time ||
               (events[a].time == events[b].time && events[a].key < events[b].key);
    }
    void Swap(std::size_t a, std::size_t b) {
        std::swap(heap[a], heap[b]); positions[heap[a]] = a; positions[heap[b]] = b;
    }
    void Up(std::size_t at) {
        while (at && Before(heap[at], heap[(at - 1) / 2])) {
            const auto parent = (at - 1) / 2; Swap(at, parent); at = parent;
        }
    }
    void Down(std::size_t at) {
        for (;;) {
            auto child = at * 2 + 1;
            if (child >= heap.size()) return;
            if (child + 1 < heap.size() && Before(heap[child + 1], heap[child])) ++child;
            if (!Before(heap[child], heap[at])) return;
            Swap(at, child); at = child;
        }
    }
    std::vector<Event>& events;
    std::unordered_map<Key, std::size_t, Hash> index;
    std::vector<std::vector<std::size_t>> incident;
    std::vector<std::size_t> heap, positions;
};
} // namespace JudasPhysicsDetail
