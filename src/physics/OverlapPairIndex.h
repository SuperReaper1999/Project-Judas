#pragma once

#include "Broadphase.h"
#include <limits>
#include <cstdlib>

namespace JudasPhysicsDetail {

// Derived fat-proxy relationships, never contact truth. Only a proxy move or
// candidate-source change can alter this graph. Body generations are checked
// independently of recycled tree proxy IDs. All access is world-owner only.
class OverlapPairIndex {
  public:
    explicit OverlapPairIndex(std::size_t limit = 64 * 1024 * 1024) : byteLimit(limit) {}

    void SetBody(unsigned slot, int proxy, unsigned generation, bool source) {
        if (disabled)
            return;
        if (slot >= nodes.size()) {
            if ((std::size_t(slot) + 1) * sizeof(Node) > byteLimit) {
                Disable();
                return;
            }
            const auto required = std::size_t(slot) + 1;
            if (nodes.capacity() < required)
                nodes.reserve(required);
            nodes.resize(required);
            if (StorageBytes() > byteLimit) {
                Disable();
                return;
            }
        }
        auto& node = nodes[slot];
        if (node.proxy != proxy || node.generation != generation || node.source != source) {
            node.proxy = proxy;
            node.generation = generation;
            node.source = source;
            Moved(slot);
        }
    }

    void Remove(unsigned slot) {
        if (slot < nodes.size())
            SetBody(slot, DynamicAabbTree::kNull, nodes[slot].generation, false);
    }

    void Moved(unsigned slot) {
        if (slot < nodes.size() && !nodes[slot].dirty) {
            if (dirty.size() == dirty.capacity()) {
                const auto next = std::max(std::size_t(4), dirty.size() * 2);
                if (StorageBytes() + (next - dirty.capacity()) * sizeof(unsigned) > byteLimit) {
                    Disable();
                    return;
                }
                dirty.reserve(next);
            }
            nodes[slot].dirty = true;
            dirty.push_back(slot);
        }
    }

    // Origin shifts can change represented AABB endpoints. Rebuild instead
    // of assuming translation leaves every float comparison invariant.
    void Invalidate() { ready = false; }
    void UseNativeQueries() { Disable(); }

    bool Synchronize(const DynamicAabbTree& tree) {
        if (disabled)
            return false;
        std::size_t affected = 0;
        for (auto slot : dirty)
            affected += nodes[slot].neighbours.size() + 1;
        if (!ready || affected > edges + nodes.size())
            return Rebuild(tree);

        // Remove old incident edges first. Querying both moved endpoints then
        // inserts one new relationship, independently of notification order.
        for (auto slot : dirty) {
            auto& node = nodes[slot];
            for (auto other : node.neighbours) {
                auto& reverse = nodes[other].neighbours;
                const auto found = std::lower_bound(reverse.begin(), reverse.end(), slot);
                if (found != reverse.end() && *found == slot) {
                    reverse.erase(found);
                    --edges;
                }
            }
            node.neighbours.clear();
        }
        for (auto slot : dirty) {
            const auto& node = nodes[slot];
            if (node.proxy != DynamicAabbTree::kNull)
                tree.Query(tree.FatAabb(node.proxy), [&](int proxy) {
                    const auto other = tree.UserData(proxy);
                    if (other != slot && (node.source || nodes[other].source))
                        Insert(slot, other);
                });
        }
        for (auto slot : dirty)
            nodes[slot].dirty = false;
        dirty.clear();
        if (failed) {
            Disable();
            return false;
        }
        VerifyIfRequested(tree);
        return true;
    }

    template<class Visit> void Pairs(Visit&& visit) const {
        for (unsigned slot = 0; slot < nodes.size(); ++slot)
            for (auto other : nodes[slot].neighbours)
                if (slot < other)
                    visit(slot, other);
    }

    const std::vector<unsigned>* Neighbours(unsigned slot) const {
        return ready && !disabled && slot < nodes.size() && nodes[slot].source
                   ? &nodes[slot].neighbours : nullptr;
    }

    std::size_t StorageBytes() const {
        return nodes.capacity() * sizeof(Node) + dirty.capacity() * sizeof(unsigned) +
               neighbourBytes + sources.capacity() * sizeof(int) +
               scratch.marks.capacity() * sizeof(std::uint64_t) +
               scratch.stack.capacity() * sizeof(std::pair<int,int>);
    }
    std::size_t EdgeCount() const { return edges; }

  private:
    struct Node {
        int proxy = DynamicAabbTree::kNull;
        unsigned generation = 0;
        bool source = false, dirty = false;
        std::vector<unsigned> neighbours;
    };
    std::vector<Node> nodes;
    std::vector<unsigned> dirty;
    DynamicAabbTree::PairQueryScratch scratch;
    std::vector<int> sources;
    std::size_t byteLimit, neighbourBytes = 0, edges = 0;
    bool ready = false, disabled = false, failed = false;

    void VerifyIfRequested(const DynamicAabbTree& tree) const {
        static const bool verify=std::getenv("JUDAS_VALIDATION_VERIFY_OVERLAPS")!=nullptr;
        if(!verify)return;
        std::vector<int> active;
        for(const auto& node:nodes)if(node.source&&node.proxy!=DynamicAabbTree::kNull)active.push_back(node.proxy);
        DynamicAabbTree::PairQueryScratch oracleScratch;
        std::vector<std::pair<unsigned,unsigned>> expected,actual;
        tree.QueryPairs(active,oracleScratch,[&](int a,int b){
            const auto first=tree.UserData(a),second=tree.UserData(b);
            expected.emplace_back(std::min(first,second),std::max(first,second));
        });
        std::sort(expected.begin(),expected.end());
        Pairs([&](unsigned a,unsigned b){actual.emplace_back(a,b);});
        if(expected!=actual||actual.size()!=edges)
            throw std::logic_error("incremental overlap index differs from native tree oracle");
        for(unsigned a=0;a<nodes.size();++a)for(unsigned b:nodes[a].neighbours)
            if(!std::binary_search(nodes[b].neighbours.begin(),nodes[b].neighbours.end(),a))
                throw std::logic_error("overlap index endpoint adjacency is not symmetric");
    }

    void Disable() {
        nodes.clear(); nodes.shrink_to_fit();
        dirty.clear(); dirty.shrink_to_fit();
        sources.clear(); sources.shrink_to_fit();
        scratch = {};
        neighbourBytes = edges = 0;
        ready = false;
        disabled = true;
    }

    void Add(std::vector<unsigned>& values, unsigned value) {
        if (failed)
            return;
        const auto at = std::lower_bound(values.begin(), values.end(), value);
        if (at != values.end() && *at == value)
            return;
        const auto index = at - values.begin();
        if (values.size() == values.capacity()) {
            const auto capacity = std::max(std::size_t(4), values.size() * 2);
            const auto increase = (capacity - values.capacity()) * sizeof(unsigned);
            if (StorageBytes() + increase > byteLimit) {
                failed = true;
                return;
            }
            const auto before = values.capacity();
            values.reserve(capacity);
            neighbourBytes += (values.capacity() - before) * sizeof(unsigned);
        }
        values.insert(values.begin() + index, value);
    }

    void Insert(unsigned a, unsigned b) {
        if (failed)
            return;
        const auto& values = nodes[a].neighbours;
        if (std::binary_search(values.begin(), values.end(), b))
            return;
        Add(nodes[a].neighbours, b);
        Add(nodes[b].neighbours, a);
        if (!failed)
            ++edges;
    }

    bool Rebuild(const DynamicAabbTree& tree) {
        ++rebuilds;
        for (auto& node : nodes) {
            node.neighbours.clear();
            node.dirty = false;
        }
        dirty.clear(); edges = 0; sources.clear();
        for (const auto& node : nodes)
            if (node.source && node.proxy != DynamicAabbTree::kNull)
                sources.push_back(node.proxy);
        tree.QueryPairs(sources, scratch, [&](int a, int b) {
            Insert(tree.UserData(a), tree.UserData(b));
        });
        if (failed || StorageBytes() > byteLimit) {
            Disable();
            return false;
        }
        ready = true;
        VerifyIfRequested(tree);
        return true;
    }
  public:
    std::size_t rebuilds = 0;
};
} // namespace JudasPhysicsDetail
