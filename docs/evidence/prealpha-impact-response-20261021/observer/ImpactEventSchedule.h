#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace JudasPhysicsDetail {

// Step-local chronological frontier. Only the endpoint trajectories own an
// event's dependencies. Updating either endpoint invalidates that event, not
// unrelated islands. Heap updates preserve exact time/key ordering; no batching
// window, time quantization or stale queue entries are used. No-impact keys
// retain identity but occupy neither the heap nor the endpoint adjacency lists.
template <class Event> class ImpactEventSchedule {
  public:
    using Key = std::array<unsigned, 4>;
    explicit ImpactEventSchedule(std::vector<Event> &storage, std::size_t bodies,
                                 double finish = std::numeric_limits<double>::infinity())
        : events(storage), incident(bodies), finish(finish) {
        events.clear();
    }

    bool Contains(const Key &key) const {
        return index.find(key) != index.end();
    }

    void Set(const Key &key, double time) {
        const auto found = index.find(key);
        const auto id = found == index.end() ? events.size() : found->second;
        if (found == index.end()) {
            index.emplace(key, id);
            events.push_back({key, time});
            locations.push_back({Absent, Absent, Absent});
        }
        events[id].time = time;
        auto &location = locations[id];
        if (time > finish) {
            if (location.heap != Absent) {
                EraseIncident(id, false);
                if (key[1] != key[0])
                    EraseIncident(id, true);
                const auto at = location.heap;
                Swap(at, heap.size() - 1);
                heap.pop_back();
                location.heap = Absent;
                if (at < heap.size()) {
                    const auto moved = heap[at];
                    Up(at);
                    Down(locations[moved].heap);
                }
            }
        } else if (location.heap == Absent) {
            location.first = incident[key[0]].size();
            incident[key[0]].push_back(id);
            if (key[1] != key[0]) {
                location.second = incident[key[1]].size();
                incident[key[1]].push_back(id);
            }
            location.heap = heap.size();
            heap.push_back(id);
            Up(heap.size() - 1);
        } else {
            Up(location.heap);
            Down(locations[id].heap);
        }
    }

    const Event *First() const {
        return heap.empty() ? nullptr : &events[heap.front()];
    }

    // The callback collects identities; Set must run after traversal completes.
    template <class Callback>
    void ForIncident(unsigned body, double end, Callback &&visit) const {
        for (auto id : incident[body])
            if (events[id].time <= end)
                visit(events[id].key);
    }

  private:
    struct Hash {
        std::size_t operator()(const Key &key) const noexcept {
            std::uint64_t value = (std::uint64_t(key[0]) << 32) | key[1];
            value += ((std::uint64_t(key[2]) << 32) | key[3]) * 0x9e3779b97f4a7c15ULL;
            value ^= value >> 30;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27;
            value *= 0x94d049bb133111ebULL;
            return std::size_t(value ^ (value >> 31));
        }
    };

    bool Before(std::size_t a, std::size_t b) const {
        return events[a].time < events[b].time ||
               (events[a].time == events[b].time && events[a].key < events[b].key);
    }
    void Swap(std::size_t a, std::size_t b) {
        std::swap(heap[a], heap[b]);
        locations[heap[a]].heap = a;
        locations[heap[b]].heap = b;
    }
    void Up(std::size_t at) {
        while (at && Before(heap[at], heap[(at - 1) / 2])) {
            const auto parent = (at - 1) / 2;
            Swap(at, parent);
            at = parent;
        }
    }
    void Down(std::size_t at) {
        for (;;) {
            auto child = at * 2 + 1;
            if (child >= heap.size())
                return;
            if (child + 1 < heap.size() && Before(heap[child + 1], heap[child]))
                ++child;
            if (!Before(heap[child], heap[at]))
                return;
            Swap(at, child);
            at = child;
        }
    }

    static constexpr std::size_t Absent = std::numeric_limits<std::size_t>::max();
    struct Location {
        std::size_t heap, first, second;
    };
    void EraseIncident(std::size_t id, bool second) {
        const auto body = events[id].key[second ? 1 : 0];
        auto &list = incident[body];
        const auto at = second ? locations[id].second : locations[id].first;
        const auto moved = list.back();
        list[at] = moved;
        list.pop_back();
        if (events[moved].key[0] == body)
            locations[moved].first = at;
        else
            locations[moved].second = at;
    }

    std::vector<Event> &events;
    std::unordered_map<Key, std::size_t, Hash> index;
    std::vector<std::vector<std::size_t>> incident;
    std::vector<std::size_t> heap;
    std::vector<Location> locations;
    double finish;
};
} // namespace JudasPhysicsDetail
