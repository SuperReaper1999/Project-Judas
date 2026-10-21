#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

// Milestone 32: Judas's rigid-body broadphase.
//
// An axis-aligned bounding box in simulation space. World axes are used here
// only as a bounding-volume coordinate system — an AABB of a rotated body is
// still the exact bound of that body, and rotating the whole world changes
// which pairs are *candidates* (bounds are conservative) but never which
// pairs the narrowphase reports as touching. No axis is "up".
struct Aabb {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};

    bool Overlaps(const Aabb& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
    bool Contains(const Aabb& inner) const {
        return min.x <= inner.min.x && min.y <= inner.min.y && min.z <= inner.min.z &&
               max.x >= inner.max.x && max.y >= inner.max.y && max.z >= inner.max.z;
    }
    Aabb Union(const Aabb& other) const {
        return Aabb{glm::min(min, other.min), glm::max(max, other.max)};
    }
    Aabb Expanded(float margin) const {
        return Aabb{min - glm::vec3(margin), max + glm::vec3(margin)};
    }
    // Half the surface area: the insertion cost heuristic only compares
    // areas, so the constant factor is irrelevant.
    float Perimeter() const {
        const glm::vec3 d = max - min;
        return d.x * d.y + d.y * d.z + d.z * d.x;
    }
};

// A dynamic AABB tree (the bounding-volume hierarchy of Box2D/Bullet's
// "dbvt" family): every proxy is a leaf holding a *fat* AABB — the body's
// tight bound grown by a margin — so a body that moves a little inside its
// fat box costs nothing, and one that escapes it is removed and reinserted.
// Internal nodes bound their two children; insertion picks the sibling that
// minimises added surface area and AVL-style rotations keep the height
// logarithmic.
//
// Why this structure and not a grid or sweep-and-prune: Judas's bodies span
// four orders of magnitude in size (an 80 m terrain planet, 20 m spheres, a
// 40 m plank, 0.25 m crates) in one scene, which defeats a uniform grid's
// single cell size; sweep-and-prune sorts along world axes and degrades when
// many bodies share a coordinate (a 1,500-crate floor all at one height).
// The tree is indifferent to both, answers arbitrary box queries (reused by
// the player's capsule sweep and PhysicsWorld::QueryBodiesInAabb), and handles
// static and dynamic proxies in one structure.
//
// Candidate generation only: a reported overlap means "the narrowphase must
// look"; it never means "these bodies touch".
class DynamicAabbTree {
public:
    static constexpr int kNull = -1;

    int CreateProxy(const Aabb& fatAabb, unsigned int userData);
    void DestroyProxy(int proxy);
    // Replaces a proxy's fat AABB (remove + reinsert). The caller decides
    // when that is necessary (see PhysicsWorld: only when the tight bound
    // has escaped the current fat one).
    void MoveProxy(int proxy, const Aabb& fatAabb);

    const Aabb& FatAabb(int proxy) const { return m_nodes[static_cast<std::size_t>(proxy)].aabb; }
    unsigned int UserData(int proxy) const { return m_nodes[static_cast<std::size_t>(proxy)].userData; }

    // Calls `callback(proxy)` for every leaf whose fat AABB overlaps `aabb`.
    // Visit order depends on tree shape; callers needing determinism sort.
    template <typename Callback>
    void Query(const Aabb& aabb, Callback&& callback) const {
        if (m_root == kNull) return;
        std::vector<int>& stack = m_queryStack;
        stack.clear();
        stack.push_back(m_root);
        while (!stack.empty()) {
            const int nodeId = stack.back();
            stack.pop_back();
            const Node& node = m_nodes[static_cast<std::size_t>(nodeId)];
            if (!node.aabb.Overlaps(aabb)) continue;
            if (node.IsLeaf()) {
                callback(nodeId);
            } else {
                stack.push_back(node.child1);
                stack.push_back(node.child2);
            }
        }
    }

    struct PairQueryScratch {
        // Callers may retain this storage across queries. Marks scale with
        // allocated tree slots, while the stack holds pending traversal work,
        // not an output array of every overlapping pair.
        std::vector<std::uint64_t> marks;
        std::uint64_t epoch = 0;
        std::vector<std::pair<int, int>> stack;
        bool inUse = false;
    };

    // Enumerate each unordered overlapping leaf pair exactly once when at
    // least one leaf is in activeProxies. User data need not be ordered or
    // unique; callbacks receive the live proxy IDs, never user-data values.
    // Traversal order is tree-dependent. Callers needing order must sort.
    //
    // Mark only active leaves and their ancestors; no per-query full-tree
    // clear is needed. Marks and depth-first traversal storage are lazy and
    // retained by the caller, adding no storage to unrelated trees. Callbacks
    // must not mutate the tree. Ordinary Query and nested QueryPairs with a
    // different scratch object are safe; recursive reuse of this scratch is
    // rejected, and concurrent reuse is unsupported. Distinct scratch objects
    // may query a read-only tree concurrently if callers exclude tree mutation.
    template <typename Callback>
    void QueryPairs(const std::vector<int>& activeProxies, PairQueryScratch& scratch,
                    Callback&& callback) const {
        if (scratch.inUse)
            throw std::logic_error("recursive broadphase pair query");
        struct QueryGuard {
            bool& active;
            explicit QueryGuard(bool& flag) : active(flag) { active = true; }
            ~QueryGuard() { active = false; }
        } guard(scratch.inUse);
        if (activeProxies.empty())
            return;

        scratch.marks.resize(m_nodes.size(), 0);
        if (++scratch.epoch == 0) {
            std::fill(scratch.marks.begin(), scratch.marks.end(), 0);
            scratch.epoch = 1;
        }
        for (const int proxy : activeProxies) {
            if (proxy < 0 || static_cast<std::size_t>(proxy) >= m_nodes.size() ||
                m_nodes[static_cast<std::size_t>(proxy)].height != 0 ||
                !m_nodes[static_cast<std::size_t>(proxy)].IsLeaf())
                throw std::invalid_argument("pair query requires live leaf proxies");
            for (int node = proxy; node != kNull; node = m_nodes[static_cast<std::size_t>(node)].parent) {
                auto& mark = scratch.marks[static_cast<std::size_t>(node)];
                if (mark == scratch.epoch)
                    break;
                mark = scratch.epoch;
            }
        }

        auto& stack = scratch.stack;
        stack.clear();
        stack.emplace_back(m_root, m_root);
        while (!stack.empty()) {
            const auto [first, second] = stack.back();
            stack.pop_back();
            if (scratch.marks[static_cast<std::size_t>(first)] != scratch.epoch &&
                scratch.marks[static_cast<std::size_t>(second)] != scratch.epoch)
                continue;
            const Node& a = m_nodes[static_cast<std::size_t>(first)];
            const Node& b = m_nodes[static_cast<std::size_t>(second)];
            if (first == second) {
                if (!a.IsLeaf()) {
                    // These disjoint sets partition all unordered descendants:
                    // pairs within each child, and pairs between the children.
                    stack.emplace_back(a.child1, a.child1);
                    stack.emplace_back(a.child2, a.child2);
                    stack.emplace_back(a.child1, a.child2);
                }
            } else if (a.aabb.Overlaps(b.aabb)) {
                if (a.IsLeaf() && b.IsLeaf()) {
                    callback(first, second);
                } else if (!a.IsLeaf() && (b.IsLeaf() || a.height >= b.height)) {
                    stack.emplace_back(a.child1, second);
                    stack.emplace_back(a.child2, second);
                } else {
                    stack.emplace_back(first, b.child1);
                    stack.emplace_back(first, b.child2);
                }
            }
        }
    }

    void ShiftOrigin(glm::dvec3 delta) { for(auto& node:m_nodes)if(node.height>=0){node.aabb.min=glm::vec3(glm::dvec3(node.aabb.min)-delta);node.aabb.max=glm::vec3(glm::dvec3(node.aabb.max)-delta);} }
    int Height() const;
    int ProxyCount() const { return m_proxyCount; }
    // Structural self-check for tests: parent links, bounds containment,
    // heights and leaf count. Returns false on the first inconsistency.
    bool Validate() const;

private:
    struct Node {
        Aabb aabb;
        unsigned int userData = 0;
        int parent = kNull;  // doubles as the free-list link
        int child1 = kNull;
        int child2 = kNull;
        int height = -1;     // leaf = 0, free = -1
        bool IsLeaf() const { return child1 == kNull; }
    };

    int AllocateNode();
    void FreeNode(int node);
    void InsertLeaf(int leaf);
    void RemoveLeaf(int leaf);
    int Balance(int node);
    bool ValidateNode(int node, int& leafCount) const;

    std::vector<Node> m_nodes;
    int m_root = kNull;
    int m_freeList = kNull;
    int m_proxyCount = 0;
    mutable std::vector<int> m_queryStack;
};
